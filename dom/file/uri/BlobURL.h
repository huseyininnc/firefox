/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BlobURL_h
#define mozilla_dom_BlobURL_h

#include "nsCOMPtr.h"
#include "nsISerializable.h"
#include "nsSimpleURI.h"
#include "prtime.h"

#define NS_HOSTOBJECTURI_CID \
  {0xf5475c51, 0x59a7, 0x4757, {0xb3, 0xd9, 0xe2, 0x11, 0xa9, 0x41, 0x08, 0x72}}

#define NS_IBLOBURLMUTATOR_IID \
  {0xf91e646d, 0xe87b, 0x485e, {0xbb, 0xc8, 0x0e, 0x8a, 0x2e, 0xe9, 0x87, 0xa9}}

class NS_NO_VTABLE nsIBlobURLMutator : public nsISupports {
 public:
  NS_INLINE_DECL_STATIC_IID(NS_IBLOBURLMUTATOR_IID)
  NS_IMETHOD SetRevoked(bool aRevoked) = 0;
};

inline NS_DEFINE_CID(kHOSTOBJECTURICID, NS_HOSTOBJECTURI_CID);

namespace mozilla::dom {

/**
 * These URIs refer to host objects with "blob" scheme.
 */
class BlobURL final : public mozilla::net::nsSimpleURI {
 private:
  BlobURL();

 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSISERIALIZABLE

  // Override EqualsInternal()
  nsresult EqualsInternal(nsIURI* aOther, RefHandlingEnum aRefHandlingMode,
                          bool* aResult) override;
  NS_IMETHOD_(void) Serialize(mozilla::ipc::URIParams& aParams) override;

  // Override StartClone to hand back a BlobURL with mRevoked set.
  already_AddRefed<mozilla::net::nsSimpleURI> StartClone() override {
    RefPtr<BlobURL> url = new BlobURL();
    url->mRevoked = mRevoked;
    return url.forget();
  }

  bool Revoked() const { return mRevoked; }

  NS_IMETHOD Mutate(nsIURIMutator** _retval) override;

 private:
  ~BlobURL() override = default;

  nsresult SetScheme(const nsACString& aProtocol) override;
  bool Deserialize(const mozilla::ipc::URIParams&);
  nsresult ReadPrivate(nsIObjectInputStream* stream);

  bool mRevoked;

 public:
  class Mutator final : public nsIURIMutator,
                        public BaseURIMutator<BlobURL>,
                        public nsIBlobURLMutator,
                        public nsISerializable {
    NS_DECL_ISUPPORTS
    NS_FORWARD_SAFE_NSIURISETTERS_RET(mURI)
    NS_DEFINE_NSIMUTATOR_COMMON

    NS_IMETHOD
    Write(nsIObjectOutputStream* aOutputStream) override {
      return NS_ERROR_NOT_IMPLEMENTED;
    }

    [[nodiscard]] NS_IMETHOD Read(nsIObjectInputStream* aStream) override {
      return InitFromInputStream(aStream);
    }

    NS_IMETHOD SetRevoked(bool aRevoked) override {
      mURI->mRevoked = aRevoked;
      return NS_OK;
    }

    Mutator() = default;

   private:
    ~Mutator() = default;

    friend class BlobURL;
  };

  friend BaseURIMutator<BlobURL>;
};

#define NS_HOSTOBJECTURIMUTATOR_CID \
  {0xbbe50ef2, 0x80eb, 0x469d, {0xb7, 0x0d, 0x02, 0x85, 0x82, 0x75, 0x38, 0x9f}}

}  // namespace mozilla::dom

#endif /* mozilla_dom_BlobURL_h */
