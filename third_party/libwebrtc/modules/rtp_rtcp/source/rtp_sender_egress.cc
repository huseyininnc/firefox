/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_sender_egress.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "api/array_view.h"
#include "api/call/transport.h"
#include "api/environment/environment.h"
#include "api/field_trials_view.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "api/sequence_checker.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/task_queue/task_queue_base.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "logging/rtc_event_log/events/rtc_event_rtp_packet_outgoing.h"
#include "modules/include/module_fec_types.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/packet_sequencer.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_history.h"
#include "modules/rtp_rtcp/source/rtp_rtcp_interface.h"
#include "modules/rtp_rtcp/source/rtp_sequence_number_map.h"
#include "rtc_base/bitrate_tracker.h"
#include "rtc_base/checks.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/logging.h"
#include "rtc_base/task_utils/repeating_task.h"

namespace webrtc {
namespace {
constexpr uint32_t kTimestampTicksPerMs = 90;
constexpr TimeDelta kBitrateStatisticsWindow = TimeDelta::Seconds(1);
constexpr size_t kRtpSequenceNumberMapMaxEntries = 1 << 13;
constexpr TimeDelta kUpdateInterval = kBitrateStatisticsWindow;
}  // namespace

RtpSenderEgress::NonPacedPacketSender::NonPacedPacketSender(
    TaskQueueBase& worker_queue,
    RtpSenderEgress* sender,
    PacketSequencer* sequencer)
    : worker_queue_(worker_queue),
      transport_sequence_number_(0),
      sender_(sender),
      sequencer_(sequencer) {
  RTC_DCHECK(sequencer);
}
RtpSenderEgress::NonPacedPacketSender::~NonPacedPacketSender() {
  RTC_DCHECK_RUN_ON(&worker_queue_);
}

void RtpSenderEgress::NonPacedPacketSender::EnqueuePackets(
    std::vector<std::unique_ptr<RtpPacketToSend>> packets) {
  if (!worker_queue_.IsCurrent()) {
    worker_queue_.PostTask(SafeTask(
        task_safety_.flag(), [this, packets = std::move(packets)]() mutable {
          EnqueuePackets(std::move(packets));
        }));
    return;
  }
  RTC_DCHECK_RUN_ON(&worker_queue_);
  for (auto& packet : packets) {
    PrepareForSend(packet.get());
    sender_->SendPacket(std::move(packet), PacedPacketInfo());
  }
  auto fec_packets = sender_->FetchFecPackets();
  if (!fec_packets.empty()) {
    EnqueuePackets(std::move(fec_packets));
  }
}

void RtpSenderEgress::NonPacedPacketSender::PrepareForSend(
    RtpPacketToSend* packet) {
  RTC_DCHECK_RUN_ON(&worker_queue_);
  // Assign sequence numbers, but not for flexfec which is already running on
  // an internally maintained sequence number series.
  if (packet->Ssrc() != sender_->FlexFecSsrc()) {
    sequencer_->Sequence(*packet);
  }
  if (!packet->SetExtension<TransportSequenceNumber>(
          ++transport_sequence_number_)) {
    --transport_sequence_number_;
  }
  packet->ReserveExtension<TransmissionOffset>();
  packet->ReserveExtension<AbsoluteSendTime>();
}

RtpSenderEgress::RtpSenderEgress(const Environment& env,
                                 const RtpRtcpInterface::Configuration& config,
                                 RtpPacketHistory* packet_history)
    : env_(env),
      enable_send_packet_batching_(config.enable_send_packet_batching),
      worker_queue_(TaskQueueBase::Current()),
      ssrc_(config.local_media_ssrc),
      rtx_ssrc_(config.rtx_send_ssrc),
      flexfec_ssrc_(config.fec_generator ? config.fec_generator->FecSsrc()
                                         : std::nullopt),
      populate_network2_timestamp_(config.populate_network2_timestamp),
      packet_history_(packet_history),
      transport_(config.outgoing_transport),
      is_audio_(config.audio),
      need_rtp_packet_infos_(config.need_rtp_packet_infos),
      fec_generator_(config.fec_generator),
      send_packet_observer_(config.send_packet_observer),
      rtp_stats_callback_(config.rtp_stats_callback),
      bitrate_callback_(config.send_bitrate_observer),
      media_has_been_sent_(false),
      force_part_of_allocation_(false),
      timestamp_offset_(0),
      send_rates_(kNumMediaTypes, BitrateTracker(kBitrateStatisticsWindow)),
      rtp_sequence_number_map_(need_rtp_packet_infos_
                                   ? std::make_unique<RtpSequenceNumberMap>(
                                         kRtpSequenceNumberMapMaxEntries)
                                   : nullptr),
      use_ntp_time_for_absolute_send_time_(!env_.field_trials().IsDisabled(
          "WebRTC-UseNtpTimeAbsoluteSendTime")) {
  RTC_DCHECK(worker_queue_);
  RTC_DCHECK(config.transport_feedback_callback == nullptr)
      << "transport_feedback_callback is no longer used and will soon be "
         "deleted.";
  if (bitrate_callback_) {
    update_task_ = RepeatingTaskHandle::DelayedStart(worker_queue_,
                                                     kUpdateInterval, [this]() {
                                                       PeriodicUpdate();
                                                       return kUpdateInterval;
                                                     });
  }
}

RtpSenderEgress::~RtpSenderEgress() {
  RTC_DCHECK_RUN_ON(worker_queue_);
  update_task_.Stop();
}

void RtpSenderEgress::SendPacket(std::unique_ptr<RtpPacketToSend> packet,
                                 const PacedPacketInfo& pacing_info) {
  RTC_DCHECK_RUN_ON(worker_queue_);
  RTC_DCHECK(packet);

  if (packet->Ssrc() == ssrc_ &&
      packet->packet_type() != RtpPacketMediaType::kRetransmission) {
    if (last_sent_seq_.has_value()) {
      RTC_DCHECK_EQ(static_cast<uint16_t>(*last_sent_seq_ + 1),
                    packet->SequenceNumber());
    }
    last_sent_seq_ = packet->SequenceNumber();
  } else if (packet->Ssrc() == rtx_ssrc_) {
    if (last_sent_rtx_seq_.has_value()) {
      RTC_DCHECK_EQ(static_cast<uint16_t>(*last_sent_rtx_seq_ + 1),
                    packet->SequenceNumber());
    }
    last_sent_rtx_seq_ = packet->SequenceNumber();
  }

  RTC_DCHECK(packet->packet_type().has_value());
  RTC_DCHECK(HasCorrectSsrc(*packet));
  if (packet->packet_type() == RtpPacketMediaType::kRetransmission) {
    RTC_DCHECK(packet->retransmitted_sequence_number().has_value());
  }

  const Timestamp now = env_.clock().CurrentTime();
  if (need_rtp_packet_infos_ &&
      packet->packet_type() == RtpPacketToSend::Type::kVideo) {
    // Last packet of a frame, add it to sequence number info map.
    const uint32_t timestamp = packet->Timestamp() - timestamp_offset_;
    rtp_sequence_number_map_->InsertPacket(
        packet->SequenceNumber(),
        RtpSequenceNumberMap::Info(
            timestamp, packet->is_first_packet_of_frame(), packet->Marker()));
  }

  if (fec_generator_ && packet->fec_protect_packet()) {
    // This packet should be protected by FEC, add it to packet generator.
    RTC_DCHECK(fec_generator_);
    RTC_DCHECK(packet->packet_type() == RtpPacketMediaType::kVideo);
    std::optional<std::pair<FecProtectionParams, FecProtectionParams>>
        new_fec_params;
    new_fec_params.swap(pending_fec_params_);
    if (new_fec_params) {
      fec_generator_->SetProtectionParameters(new_fec_params->first,
                                              new_fec_params->second);
    }
    if (packet->is_red()) {
      RtpPacketToSend unpacked_packet(*packet);

      const rtc::CopyOnWriteBuffer buffer = packet->Buffer();
      // Grab media payload type from RED header.
      const size_t headers_size = packet->headers_size();
      unpacked_packet.SetPayloadType(buffer[headers_size]);

      // Copy the media payload into the unpacked buffer.
      uint8_t* payload_buffer =
          unpacked_packet.SetPayloadSize(packet->payload_size() - 1);
      std::copy(&packet->payload()[0] + 1,
                &packet->payload()[0] + packet->payload_size(), payload_buffer);

      fec_generator_->AddPacketAndGenerateFec(unpacked_packet);
    } else {
      // If not RED encapsulated - we can just insert packet directly.
      fec_generator_->AddPacketAndGenerateFec(*packet);
    }
  }

  // Bug webrtc:7859. While FEC is invoked from rtp_sender_video, and not after
  // the pacer, these modifications of the header below are happening after the
  // FEC protection packets are calculated. This will corrupt recovered packets
  // at the same place. It's not an issue for extensions, which are present in
  // all the packets (their content just may be incorrect on recovered packets).
  // In case of VideoTimingExtension, since it's present not in every packet,
  // data after rtp header may be corrupted if these packets are protected by
  // the FEC.
  if (packet->HasExtension<TransmissionOffset>() &&
      packet->capture_time() > Timestamp::Zero()) {
    TimeDelta diff = now - packet->capture_time();
    packet->SetExtension<TransmissionOffset>(kTimestampTicksPerMs * diff.ms());
  }
  if (packet->HasExtension<AbsoluteSendTime>()) {
    if (use_ntp_time_for_absolute_send_time_) {
      packet->SetExtension<AbsoluteSendTime>(AbsoluteSendTime::To24Bits(
          env_.clock().ConvertTimestampToNtpTime(now)));
    } else {
      packet->SetExtension<AbsoluteSendTime>(AbsoluteSendTime::To24Bits(now));
    }
  }
  if (packet->HasExtension<TransportSequenceNumber>() &&
      packet->transport_sequence_number()) {
    packet->SetExtension<TransportSequenceNumber>(
        *packet->transport_sequence_number() & 0xFFFF);
  }

  if (packet->HasExtension<VideoTimingExtension>()) {
    if (populate_network2_timestamp_) {
      packet->set_network2_time(now);
    } else {
      packet->set_pacer_exit_time(now);
    }
  }

  auto compound_packet = Packet{std::move(packet), pacing_info, now};
  if (enable_send_packet_batching_ && !is_audio_) {
    packets_to_send_.push_back(std::move(compound_packet));
  } else {
    CompleteSendPacket(compound_packet, false);
  }
}

void RtpSenderEgress::OnBatchComplete() {
  RTC_DCHECK_RUN_ON(worker_queue_);
  for (auto& packet : packets_to_send_) {
    CompleteSendPacket(packet, &packet == &packets_to_send_.back());
  }
  packets_to_send_.clear();
}

void RtpSenderEgress::CompleteSendPacket(const Packet& compound_packet,
                                         bool last_in_batch) {
  RTC_DCHECK_RUN_ON(worker_queue_);
  auto& [packet, pacing_info, now] = compound_packet;
  RTC_CHECK(packet);

  PacketOptions options;
  options.included_in_allocation = force_part_of_allocation_;
  options.is_media = packet->packet_type() == RtpPacketMediaType::kAudio ||
                     packet->packet_type() == RtpPacketMediaType::kVideo;

  // Set Packet id from transport sequence number header extension if it is
  // used. The source of the header extension is
  // RtpPacketToSend::transport_sequence_number(), but the extension is only 16
  // bit and will wrap. We should be able to use the 64bit value as id, but in
  // order to not change behaviour we use the 16bit extension value if it is
  // used.
  std::optional<uint16_t> packet_id =
      packet->GetExtension<TransportSequenceNumber>();
  if (packet_id.has_value()) {
    options.packet_id = *packet_id;
    options.included_in_feedback = true;
    options.included_in_allocation = true;
  } else if (packet->transport_sequence_number()) {
    options.packet_id = *packet->transport_sequence_number();
  }

  if (packet->packet_type() != RtpPacketMediaType::kPadding &&
      packet->packet_type() != RtpPacketMediaType::kRetransmission &&
      send_packet_observer_ != nullptr && packet->capture_time().IsFinite()) {
    send_packet_observer_->OnSendPacket(packet_id, packet->capture_time(),
                                        packet->Ssrc());
  }
  options.send_as_ect1 = packet->send_as_ect1();
  options.batchable = enable_send_packet_batching_ && !is_audio_;
  options.last_packet_in_batch = last_in_batch;
  const bool send_success = SendPacketToNetwork(*packet, options, pacing_info);

  // Put packet in retransmission history or update pending status even if
  // actual sending fails.
  if (options.is_media && packet->allow_retransmission()) {
    packet_history_->PutRtpPacket(std::make_unique<RtpPacketToSend>(*packet),
                                  now);
  } else if (packet->retransmitted_sequence_number()) {
    packet_history_->MarkPacketAsSent(*packet->retransmitted_sequence_number());
  }

  if (send_success) {
    // `media_has_been_sent_` is used by RTPSender to figure out if it can send
    // padding in the absence of transport-cc or abs-send-time.
    // In those cases media must be sent first to set a reference timestamp.
    media_has_been_sent_ = true;

    // TODO(sprang): Add support for FEC protecting all header extensions, add
    // media packet to generator here instead.

    RTC_DCHECK(packet->packet_type().has_value());
    RtpPacketMediaType packet_type = *packet->packet_type();
    RtpPacketCounter counter(*packet);
    UpdateRtpStats(now, packet->Ssrc(), packet_type, std::move(counter),
                   packet->size());
  }
}

RtpSendRates RtpSenderEgress::GetSendRates(Timestamp now) const {
  RTC_DCHECK_RUN_ON(worker_queue_);
  RtpSendRates current_rates;
  for (size_t i = 0; i < kNumMediaTypes; ++i) {
    RtpPacketMediaType type = static_cast<RtpPacketMediaType>(i);
    current_rates[type] = send_rates_[i].Rate(now).value_or(DataRate::Zero());
  }
  return current_rates;
}

void RtpSenderEgress::GetDataCounters(StreamDataCounters* rtp_stats,
                                      StreamDataCounters* rtx_stats) const {
  RTC_DCHECK_RUN_ON(worker_queue_);
  if (rtp_stats_callback_) {
    *rtp_stats = rtp_stats_callback_->GetDataCounters(ssrc_);
    if (rtx_ssrc_.has_value()) {
      *rtx_stats = rtp_stats_callback_->GetDataCounters(*rtx_ssrc_);
    }
  } else {
    *rtp_stats = rtp_stats_;
    *rtx_stats = rtx_rtp_stats_;
  }
}

void RtpSenderEgress::ForceIncludeSendPacketsInAllocation(
    bool part_of_allocation) {
  RTC_DCHECK_RUN_ON(worker_queue_);
  force_part_of_allocation_ = part_of_allocation;
}

bool RtpSenderEgress::MediaHasBeenSent() const {
  RTC_DCHECK_RUN_ON(worker_queue_);
  return media_has_been_sent_;
}

void RtpSenderEgress::SetMediaHasBeenSent(bool media_sent) {
  RTC_DCHECK_RUN_ON(worker_queue_);
  media_has_been_sent_ = media_sent;
}

void RtpSenderEgress::SetTimestampOffset(uint32_t timestamp) {
  RTC_DCHECK_RUN_ON(worker_queue_);
  timestamp_offset_ = timestamp;
}

std::vector<RtpSequenceNumberMap::Info> RtpSenderEgress::GetSentRtpPacketInfos(
    rtc::ArrayView<const uint16_t> sequence_numbers) const {
  RTC_DCHECK_RUN_ON(worker_queue_);
  RTC_DCHECK(!sequence_numbers.empty());
  if (!need_rtp_packet_infos_) {
    return std::vector<RtpSequenceNumberMap::Info>();
  }

  std::vector<RtpSequenceNumberMap::Info> results;
  results.reserve(sequence_numbers.size());

  for (uint16_t sequence_number : sequence_numbers) {
    const auto& info = rtp_sequence_number_map_->Get(sequence_number);
    if (!info) {
      // The empty vector will be returned. We can delay the clearing
      // of the vector until after we exit the critical section.
      return std::vector<RtpSequenceNumberMap::Info>();
    }
    results.push_back(*info);
  }

  return results;
}

void RtpSenderEgress::SetFecProtectionParameters(
    const FecProtectionParams& delta_params,
    const FecProtectionParams& key_params) {
  RTC_DCHECK_RUN_ON(worker_queue_);
  pending_fec_params_.emplace(delta_params, key_params);
}

std::vector<std::unique_ptr<RtpPacketToSend>>
RtpSenderEgress::FetchFecPackets() {
  RTC_DCHECK_RUN_ON(worker_queue_);
  if (fec_generator_) {
    return fec_generator_->GetFecPackets();
  }
  return {};
}

void RtpSenderEgress::OnAbortedRetransmissions(
    rtc::ArrayView<const uint16_t> sequence_numbers) {
  RTC_DCHECK_RUN_ON(worker_queue_);
  // Mark aborted retransmissions as sent, rather than leaving them in
  // a 'pending' state - otherwise they can not be requested again and
  // will not be cleared until the history has reached its max size.
  for (uint16_t seq_no : sequence_numbers) {
    packet_history_->MarkPacketAsSent(seq_no);
  }
}

bool RtpSenderEgress::HasCorrectSsrc(const RtpPacketToSend& packet) const {
  switch (*packet.packet_type()) {
    case RtpPacketMediaType::kAudio:
    case RtpPacketMediaType::kVideo:
      return packet.Ssrc() == ssrc_;
    case RtpPacketMediaType::kRetransmission:
    case RtpPacketMediaType::kPadding:
      // Both padding and retransmission must be on either the media or the
      // RTX stream.
      return packet.Ssrc() == rtx_ssrc_ || packet.Ssrc() == ssrc_;
    case RtpPacketMediaType::kForwardErrorCorrection:
      // FlexFEC is on separate SSRC, ULPFEC uses media SSRC.
      return packet.Ssrc() == ssrc_ || packet.Ssrc() == flexfec_ssrc_;
  }
  return false;
}

bool RtpSenderEgress::SendPacketToNetwork(const RtpPacketToSend& packet,
                                          const PacketOptions& options,
                                          const PacedPacketInfo& pacing_info) {
  RTC_DCHECK_RUN_ON(worker_queue_);
  if (transport_ == nullptr || !transport_->SendRtp(packet, options)) {
    RTC_LOG(LS_WARNING) << "Transport failed to send packet.";
    return false;
  }

  env_.event_log().Log(std::make_unique<RtcEventRtpPacketOutgoing>(
      packet, pacing_info.probe_cluster_id));
  return true;
}

void RtpSenderEgress::UpdateRtpStats(Timestamp now,
                                     uint32_t packet_ssrc,
                                     RtpPacketMediaType packet_type,
                                     RtpPacketCounter counter,
                                     size_t packet_size) {
  RTC_DCHECK_RUN_ON(worker_queue_);

  // TODO(bugs.webrtc.org/11581): send_rates_ should be touched only on the
  // worker thread.
  RtpSendRates send_rates;

  StreamDataCounters* counters = nullptr;
  if (rtp_stats_callback_) {
    rtp_stats_ = rtp_stats_callback_->GetDataCounters(packet_ssrc);
    counters = &rtp_stats_;
  } else {
    counters = packet_ssrc == rtx_ssrc_ ? &rtx_rtp_stats_ : &rtp_stats_;
  }

  counters->MaybeSetFirstPacketTime(now);

  if (packet_type == RtpPacketMediaType::kForwardErrorCorrection) {
    counters->fec.Add(counter);
  } else if (packet_type == RtpPacketMediaType::kRetransmission) {
    counters->retransmitted.Add(counter);
  }
  counters->transmitted.Add(counter);

  send_rates_[static_cast<size_t>(packet_type)].Update(packet_size, now);
  if (bitrate_callback_) {
    send_rates = GetSendRates(now);
  }

  if (rtp_stats_callback_) {
    rtp_stats_callback_->DataCountersUpdated(*counters, packet_ssrc);
  }

  // The bitrate_callback_ and rtp_stats_callback_ pointers in practice point
  // to the same object, so these callbacks could be consolidated into one.
  if (bitrate_callback_) {
    bitrate_callback_->Notify(
        send_rates.Sum().bps(),
        send_rates[RtpPacketMediaType::kRetransmission].bps(), ssrc_);
  }
}

void RtpSenderEgress::PeriodicUpdate() {
  RTC_DCHECK_RUN_ON(worker_queue_);
  RTC_DCHECK(bitrate_callback_);
  RtpSendRates send_rates = GetSendRates(env_.clock().CurrentTime());
  bitrate_callback_->Notify(
      send_rates.Sum().bps(),
      send_rates[RtpPacketMediaType::kRetransmission].bps(), ssrc_);
}
}  // namespace webrtc
