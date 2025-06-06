/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/memory/fifo_buffer.h"

#include <algorithm>

#include "rtc_base/thread.h"

namespace rtc {

FifoBuffer::FifoBuffer(size_t size)
    : state_(webrtc::SS_OPEN),
      buffer_(new char[size]),
      buffer_length_(size),
      data_length_(0),
      read_position_(0),
      owner_(webrtc::Thread::Current()) {
  // all events are done on the owner_ thread
}

FifoBuffer::FifoBuffer(size_t size, webrtc::Thread* owner)
    : state_(webrtc::SS_OPEN),
      buffer_(new char[size]),
      buffer_length_(size),
      data_length_(0),
      read_position_(0),
      owner_(owner) {
  // all events are done on the owner_ thread
}

FifoBuffer::~FifoBuffer() {}

bool FifoBuffer::GetBuffered(size_t* size) const {
  RTC_DCHECK_RUN_ON(&callback_sequence_);
  *size = data_length_;
  return true;
}

webrtc::StreamState FifoBuffer::GetState() const {
  RTC_DCHECK_RUN_ON(&callback_sequence_);
  return state_;
}

webrtc::StreamResult FifoBuffer::Read(rtc::ArrayView<uint8_t> buffer,
                                      size_t& bytes_read,
                                      int& error) {
  RTC_DCHECK_RUN_ON(&callback_sequence_);
  const bool was_writable = data_length_ < buffer_length_;
  size_t copy = 0;
  webrtc::StreamResult result = ReadLocked(buffer.data(), buffer.size(), &copy);

  if (result == webrtc::SR_SUCCESS) {
    // If read was successful then adjust the read position and number of
    // bytes buffered.
    read_position_ = (read_position_ + copy) % buffer_length_;
    data_length_ -= copy;
    bytes_read = copy;

    // if we were full before, and now we're not, post an event
    if (!was_writable && copy > 0) {
      PostEvent(webrtc::SE_WRITE, 0);
    }
  }
  return result;
}

webrtc::StreamResult FifoBuffer::Write(rtc::ArrayView<const uint8_t> buffer,
                                       size_t& bytes_written,
                                       int& error) {
  RTC_DCHECK_RUN_ON(&callback_sequence_);

  const bool was_readable = (data_length_ > 0);
  size_t copy = 0;
  webrtc::StreamResult result =
      WriteLocked(buffer.data(), buffer.size(), &copy);

  if (result == webrtc::SR_SUCCESS) {
    // If write was successful then adjust the number of readable bytes.
    data_length_ += copy;
    bytes_written = copy;
    // if we didn't have any data to read before, and now we do, post an event
    if (!was_readable && copy > 0) {
      PostEvent(webrtc::SE_READ, 0);
    }
  }
  return result;
}

void FifoBuffer::Close() {
  RTC_DCHECK_RUN_ON(&callback_sequence_);
  state_ = webrtc::SS_CLOSED;
}

const void* FifoBuffer::GetReadData(size_t* size) {
  RTC_DCHECK_RUN_ON(&callback_sequence_);
  *size = (read_position_ + data_length_ <= buffer_length_)
              ? data_length_
              : buffer_length_ - read_position_;
  return &buffer_[read_position_];
}

void FifoBuffer::ConsumeReadData(size_t size) {
  RTC_DCHECK_RUN_ON(&callback_sequence_);
  RTC_DCHECK_LE(size, data_length_);
  const bool was_writable = data_length_ < buffer_length_;
  read_position_ = (read_position_ + size) % buffer_length_;
  data_length_ -= size;
  if (!was_writable && size > 0) {
    PostEvent(webrtc::SE_WRITE, 0);
  }
}

void* FifoBuffer::GetWriteBuffer(size_t* size) {
  RTC_DCHECK_RUN_ON(&callback_sequence_);

  if (state_ == webrtc::SS_CLOSED) {
    return nullptr;
  }

  // if empty, reset the write position to the beginning, so we can get
  // the biggest possible block
  if (data_length_ == 0) {
    read_position_ = 0;
  }

  const size_t write_position =
      (read_position_ + data_length_) % buffer_length_;
  *size = (write_position > read_position_ || data_length_ == 0)
              ? buffer_length_ - write_position
              : read_position_ - write_position;
  return &buffer_[write_position];
}

void FifoBuffer::ConsumeWriteBuffer(size_t size) {
  RTC_DCHECK_RUN_ON(&callback_sequence_);
  RTC_DCHECK_LE(size, buffer_length_ - data_length_);
  const bool was_readable = (data_length_ > 0);
  data_length_ += size;
  if (!was_readable && size > 0) {
    PostEvent(webrtc::SE_READ, 0);
  }
}

webrtc::StreamResult FifoBuffer::ReadLocked(void* buffer,
                                            size_t bytes,
                                            size_t* bytes_read) {
  if (data_length_ == 0) {
    return (state_ != webrtc::SS_CLOSED) ? webrtc::SR_BLOCK : webrtc::SR_EOS;
  }

  const size_t available = data_length_;
  const size_t read_position = read_position_ % buffer_length_;
  const size_t copy = std::min(bytes, available);
  const size_t tail_copy = std::min(copy, buffer_length_ - read_position);
  char* const p = static_cast<char*>(buffer);
  memcpy(p, &buffer_[read_position], tail_copy);
  memcpy(p + tail_copy, &buffer_[0], copy - tail_copy);

  if (bytes_read) {
    *bytes_read = copy;
  }
  return webrtc::SR_SUCCESS;
}

webrtc::StreamResult FifoBuffer::WriteLocked(const void* buffer,
                                             size_t bytes,
                                             size_t* bytes_written) {
  if (state_ == webrtc::SS_CLOSED) {
    return webrtc::SR_EOS;
  }

  if (data_length_ >= buffer_length_) {
    return webrtc::SR_BLOCK;
  }

  const size_t available = buffer_length_ - data_length_;
  const size_t write_position =
      (read_position_ + data_length_) % buffer_length_;
  const size_t copy = std::min(bytes, available);
  const size_t tail_copy = std::min(copy, buffer_length_ - write_position);
  const char* const p = static_cast<const char*>(buffer);
  memcpy(&buffer_[write_position], p, tail_copy);
  memcpy(&buffer_[0], p + tail_copy, copy - tail_copy);

  if (bytes_written) {
    *bytes_written = copy;
  }
  return webrtc::SR_SUCCESS;
}

}  // namespace rtc
