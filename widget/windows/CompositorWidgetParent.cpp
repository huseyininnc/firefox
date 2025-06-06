/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "CompositorWidgetParent.h"

#include "mozilla/Unused.h"
#include "mozilla/StaticPrefs_layers.h"
#include "mozilla/gfx/DeviceManagerDx.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/layers/Compositor.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/webrender/RenderThread.h"
#include "mozilla/widget/PlatformWidgetTypes.h"
#include "nsWindow.h"
#include "VsyncDispatcher.h"
#include "WinCompositorWindowThread.h"
#include "VRShMem.h"
#include "RemoteBackbuffer.h"

#include <ddraw.h>

namespace mozilla {
namespace widget {

using namespace mozilla::gfx;
using namespace mozilla;

CompositorWidgetParent::CompositorWidgetParent(
    const CompositorWidgetInitData& aInitData,
    const layers::CompositorOptions& aOptions)
    : WinCompositorWidget(aInitData.get_WinCompositorWidgetInitData(),
                          aOptions),
      mWnd(reinterpret_cast<HWND>(
          aInitData.get_WinCompositorWidgetInitData().hWnd())),
      mIsFullyOccluded(false) {
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_GPU);
  MOZ_ASSERT(mWnd && ::IsWindow(mWnd));
}

CompositorWidgetParent::~CompositorWidgetParent() = default;

bool CompositorWidgetParent::Initialize(
    const RemoteBackbufferHandles& aRemoteHandles) {
  mRemoteBackbufferClient = std::make_unique<remote_backbuffer::Client>();
  return mRemoteBackbufferClient->Initialize(aRemoteHandles);
}

bool CompositorWidgetParent::PreRender(WidgetRenderingContext* aContext) {
  // This can block waiting for WM_SETTEXT to finish
  // Using PreRender is unnecessarily pessimistic because
  // we technically only need to block during the present call
  // not all of compositor rendering
  mPresentLock.Enter();
  return true;
}

void CompositorWidgetParent::PostRender(WidgetRenderingContext* aContext) {
  mPresentLock.Leave();
}

LayoutDeviceIntSize CompositorWidgetParent::GetClientSize() {
  RECT r;
  if (!::GetClientRect(mWnd, &r)) {
    return LayoutDeviceIntSize();
  }
  return LayoutDeviceIntSize(r.right - r.left, r.bottom - r.top);
}

already_AddRefed<gfx::DrawTarget>
CompositorWidgetParent::StartRemoteDrawingInRegion(
    const LayoutDeviceIntRegion& aInvalidRegion) {
  MOZ_ASSERT(mRemoteBackbufferClient);
  return mRemoteBackbufferClient->BorrowDrawTarget();
}

void CompositorWidgetParent::EndRemoteDrawingInRegion(
    gfx::DrawTarget* aDrawTarget, const LayoutDeviceIntRegion& aInvalidRegion) {
  Unused << mRemoteBackbufferClient->PresentDrawTarget(
      aInvalidRegion.ToUnknownRegion());
}

already_AddRefed<gfx::DrawTarget>
CompositorWidgetParent::GetBackBufferDrawTarget(gfx::DrawTarget* aScreenTarget,
                                                const gfx::IntRect& aRect,
                                                bool* aOutIsCleared) {
  MOZ_CRASH(
      "Unexpected call to GetBackBufferDrawTarget() with remote "
      "backbuffering in use");
}

already_AddRefed<gfx::SourceSurface>
CompositorWidgetParent::EndBackBufferDrawing() {
  MOZ_CRASH(
      "Unexpected call to EndBackBufferDrawing() with remote "
      "backbuffering in use");
}

bool CompositorWidgetParent::InitCompositor(layers::Compositor* aCompositor) {
  return true;
}

bool CompositorWidgetParent::IsHidden() const { return ::IsIconic(mWnd); }

mozilla::ipc::IPCResult CompositorWidgetParent::RecvInitialize(
    const RemoteBackbufferHandles& aRemoteHandles) {
  Unused << Initialize(aRemoteHandles);
  return IPC_OK();
}

mozilla::ipc::IPCResult CompositorWidgetParent::RecvEnterPresentLock() {
  mPresentLock.Enter();
  return IPC_OK();
}

mozilla::ipc::IPCResult CompositorWidgetParent::RecvLeavePresentLock() {
  mPresentLock.Leave();
  return IPC_OK();
}

mozilla::ipc::IPCResult CompositorWidgetParent::RecvNotifyVisibilityUpdated(
    const bool& aIsFullyOccluded) {
  mIsFullyOccluded = aIsFullyOccluded;
  return IPC_OK();
}

bool CompositorWidgetParent::GetWindowIsFullyOccluded() const {
  return mIsFullyOccluded;
}

mozilla::ipc::IPCResult CompositorWidgetParent::RecvUpdateTransparency(
    const TransparencyMode& aMode) {
  SetTransparencyMode(aMode);
  return IPC_OK();
}

nsIWidget* CompositorWidgetParent::RealWidget() { return nullptr; }

void CompositorWidgetParent::ObserveVsync(VsyncObserver* aObserver) {
  if (aObserver) {
    Unused << SendObserveVsync();
  } else {
    Unused << SendUnobserveVsync();
  }
  mVsyncObserver = aObserver;
}

RefPtr<VsyncObserver> CompositorWidgetParent::GetVsyncObserver() const {
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_GPU);
  return mVsyncObserver;
}

void CompositorWidgetParent::UpdateCompositorWnd(const HWND aCompositorWnd,
                                                 const HWND aParentWnd) {
  MOZ_ASSERT(layers::CompositorThreadHolder::IsInCompositorThread());
  MOZ_ASSERT(mRootLayerTreeID.isSome());

  RefPtr<CompositorWidgetParent> self = this;
  SendUpdateCompositorWnd(reinterpret_cast<WindowsHandle>(aCompositorWnd),
                          reinterpret_cast<WindowsHandle>(aParentWnd))
      ->Then(
          layers::CompositorThread(), __func__,
          [self](const bool& aSuccess) {
            if (aSuccess && self->mRootLayerTreeID.isSome() &&
                layers::CompositorThreadHolder::IsActive()) {
              self->mSetParentCompleted = true;
              // Schedule composition after ::SetParent() call in parent
              // process.
              layers::CompositorBridgeParent::ScheduleForcedComposition(
                  self->mRootLayerTreeID.ref(), wr::RenderReasons::WIDGET);
            }
          },
          [self](const mozilla::ipc::ResponseRejectReason&) {});
}

void CompositorWidgetParent::SetRootLayerTreeID(
    const layers::LayersId& aRootLayerTreeId) {
  mRootLayerTreeID = Some(aRootLayerTreeId);
}

void CompositorWidgetParent::ActorDestroy(ActorDestroyReason aWhy) {}

}  // namespace widget
}  // namespace mozilla
