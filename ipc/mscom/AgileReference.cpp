/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mscom/AgileReference.h"

#include <utility>
#include "mozilla/Assertions.h"
#include "mozilla/mscom/Utils.h"

namespace mozilla::mscom::detail {

HRESULT AgileReference_CreateImpl(RefPtr<IAgileReference>& aRefPtr, REFIID riid,
                                  IUnknown* aObject) {
  MOZ_ASSERT(aObject);
  MOZ_ASSERT(IsCOMInitializedOnCurrentThread());
  return ::RoGetAgileReference(AGILEREFERENCE_DEFAULT, riid, aObject,
                               getter_AddRefs(aRefPtr));
}

HRESULT AgileReference_ResolveImpl(RefPtr<IAgileReference> const& aRefPtr,
                                   REFIID riid, void** aOutInterface) {
  MOZ_ASSERT(aRefPtr);
  MOZ_ASSERT(aOutInterface);
  MOZ_ASSERT(IsCOMInitializedOnCurrentThread());

  if (!aRefPtr || !aOutInterface) {
    return E_INVALIDARG;
  }

  *aOutInterface = nullptr;
  return aRefPtr->Resolve(riid, aOutInterface);
}

}  // namespace mozilla::mscom::detail
