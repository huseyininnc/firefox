/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ArrayBufferObject_h
#define vm_ArrayBufferObject_h

#include "mozilla/Maybe.h"

#include <tuple>  // std::tuple

#include "builtin/TypedArrayConstants.h"
#include "gc/Memory.h"
#include "gc/ZoneAllocator.h"
#include "js/ArrayBuffer.h"
#include "js/GCHashTable.h"
#include "vm/JSFunction.h"
#include "vm/JSObject.h"
#include "vm/SharedMem.h"
#include "wasm/WasmMemory.h"

namespace js {

class JS_PUBLIC_API GenericPrinter;
class JSONPrinter;

class ArrayBufferViewObject;
class AutoSetNewObjectMetadata;
class WasmArrayRawBuffer;

namespace wasm {
struct MemoryDesc;
}  // namespace wasm

// Create a new mapping of size `mappedSize` with an initially committed prefix
// of size `initialCommittedSize`.  Both arguments denote bytes and must be
// multiples of the page size, with `initialCommittedSize` <= `mappedSize`.
// Returns nullptr on failure.
void* MapBufferMemory(wasm::AddressType, size_t mappedSize,
                      size_t initialCommittedSize);

// Commit additional memory in an existing mapping.  `dataEnd` must be the
// correct value for the end of the existing committed area, and `delta` must be
// a byte amount to grow the mapping by, and must be a multiple of the page
// size.  Returns false on failure.
bool CommitBufferMemory(void* dataEnd, size_t delta);

// Remove an existing mapping.  `dataStart` must be the pointer to the start of
// the mapping, and `mappedSize` the size of that mapping.
void UnmapBufferMemory(wasm::AddressType t, void* dataStart, size_t mappedSize,
                       size_t committedSize);

// Return the number of bytes currently reserved for WebAssembly memory
uint64_t WasmReservedBytes();

// The inheritance hierarchy for the various classes relating to typed arrays
// is as follows.
//
//
// - JSObject
//   - NativeObject
//     - ArrayBufferObjectMaybeShared
//       - ArrayBufferObject
//         - FixedLengthArrayBufferObject
//         - ResizableArrayBufferObject
//         - ImmutableArrayBufferObject
//       - SharedArrayBufferObject
//         - FixedLengthSharedArrayBufferObject
//         - GrowableSharedArrayBufferObject
//     - ArrayBufferViewObject
//       - DataViewObject
//         - FixedLengthDataViewObject
//         - ResizableDataViewObject
//         - ImmutableDataViewObject
//       - TypedArrayObject (declared in vm/TypedArrayObject.h)
//         - FixedLengthTypedArrayObject
//           - FixedLengthTypedArrayObjectTemplate<NativeType>, also inheriting
//             from TypedArrayObjectTemplate<NativeType>
//             - FixedLengthTypedArrayObjectTemplate<int8_t>
//             - FixedLengthTypedArrayObjectTemplate<uint8_t>
//             - ...
//         - ResizableTypedArrayObject
//           - ResizableTypedArrayObjectTemplate<NativeType>, also inheriting
//             from TypedArrayObjectTemplate<NativeType>
//             - ResizableTypedArrayObjectTemplate<int8_t>
//             - ResizableTypedArrayObjectTemplate<uint8_t>
//             - ...
//         - ImmutableTypedArrayObject
//           - ImmutableTypedArrayObjectTemplate<NativeType>, also inheriting
//             from TypedArrayObjectTemplate<NativeType>
//             - ImmutableTypedArrayObjectTemplate<int8_t>
//             - ImmutableTypedArrayObjectTemplate<uint8_t>
//             - ...
//
// Note that |{FixedLength,Resizable,Immutable}TypedArrayObjectTemplate| is just
// an implementation detail that makes implementing its various subclasses
// easier.
//
// FixedLengthArrayBufferObject, ResizableArrayBufferObject, and
// ImmutableArrayBufferObject are also implementation specific types to
// differentiate between fixed-length, resizable, and immutable ArrayBuffers.
//
// ArrayBufferObject and SharedArrayBufferObject are unrelated data types:
// the racy memory of the latter cannot substitute for the non-racy memory of
// the former; the non-racy memory of the former cannot be used with the
// atomics; the former can be detached and the latter not.  Hence they have been
// separated completely.
//
// Most APIs will only accept ArrayBufferObject.  ArrayBufferObjectMaybeShared
// exists as a join point to allow APIs that can take or use either, notably
// AsmJS.
//
// In contrast with the separation of ArrayBufferObject and
// SharedArrayBufferObject, the TypedArray types can map either.
//
// The possible data ownership and reference relationships with ArrayBuffers
// and related classes are enumerated below. These are the possible locations
// for typed data:
//
// (1) malloc'ed or mmap'ed data owned by an ArrayBufferObject.
// (2) Data allocated inline with an ArrayBufferObject.
// (3) Data allocated inline with a TypedArrayObject.
//
// An ArrayBufferObject may point to any of these sources of data, except (3).
// All array buffer views may point to any of these sources of data, except
// that (3) may only be pointed to by the typed array the data is inline with.
//
// During a minor GC, (3) may move. During a compacting GC, (2) and (3) may
// move.

class ArrayBufferObjectMaybeShared;

wasm::AddressType WasmArrayBufferAddressType(
    const ArrayBufferObjectMaybeShared* buf);
wasm::Pages WasmArrayBufferPages(const ArrayBufferObjectMaybeShared* buf);
wasm::Pages WasmArrayBufferClampedMaxPages(
    const ArrayBufferObjectMaybeShared* buf);
mozilla::Maybe<wasm::Pages> WasmArrayBufferSourceMaxPages(
    const ArrayBufferObjectMaybeShared* buf);
size_t WasmArrayBufferMappedSize(const ArrayBufferObjectMaybeShared* buf);

class ArrayBufferObjectMaybeShared : public NativeObject {
 public:
  inline size_t byteLength() const;
  inline bool isDetached() const;
  inline bool isResizable() const;
  inline bool isImmutable() const;
  inline SharedMem<uint8_t*> dataPointerEither();

  inline bool pinLength(bool pin);

  // WebAssembly support:
  // Note: the eventual goal is to remove this from ArrayBuffer and have
  // (Shared)ArrayBuffers alias memory owned by some wasm::Memory object.

  wasm::AddressType wasmAddressType() const {
    return WasmArrayBufferAddressType(this);
  }
  wasm::Pages wasmPages() const { return WasmArrayBufferPages(this); }
  wasm::Pages wasmClampedMaxPages() const {
    return WasmArrayBufferClampedMaxPages(this);
  }
  mozilla::Maybe<wasm::Pages> wasmSourceMaxPages() const {
    return WasmArrayBufferSourceMaxPages(this);
  }
  size_t wasmMappedSize() const { return WasmArrayBufferMappedSize(this); }

  inline bool isPreparedForAsmJS() const;
  inline bool isWasm() const;
};

class FixedLengthArrayBufferObject;
class ResizableArrayBufferObject;
class ImmutableArrayBufferObject;

/*
 * ArrayBufferObject
 *
 * This class holds the underlying raw buffer that the various ArrayBufferViews
 * (DataViewObject and the TypedArrays) access. It can be created explicitly and
 * used to construct an ArrayBufferView, or can be created lazily when it is
 * first accessed for a TypedArrayObject that doesn't have an explicit buffer.
 *
 * ArrayBufferObject is an abstract base class and has exactly three concrete
 * subclasses: FixedLengthArrayBufferObject, ResizableArrayBufferObject, and
 * ImmutableArrayBufferObject.
 *
 * ArrayBufferObject (or really the underlying memory) /is not racy/: the
 * memory is private to a single worker.
 */
class ArrayBufferObject : public ArrayBufferObjectMaybeShared {
  static bool byteLengthGetterImpl(JSContext* cx, const CallArgs& args);
  static bool maxByteLengthGetterImpl(JSContext* cx, const CallArgs& args);
  static bool resizableGetterImpl(JSContext* cx, const CallArgs& args);
  static bool detachedGetterImpl(JSContext* cx, const CallArgs& args);
#ifdef NIGHTLY_BUILD
  static bool immutableGetterImpl(JSContext* cx, const CallArgs& args);
#endif
  static bool sliceImpl(JSContext* cx, const CallArgs& args);
#ifdef NIGHTLY_BUILD
  static bool sliceToImmutableImpl(JSContext* cx, const CallArgs& args);
#endif
  static bool resizeImpl(JSContext* cx, const CallArgs& args);
  static bool transferImpl(JSContext* cx, const CallArgs& args);
  static bool transferToFixedLengthImpl(JSContext* cx, const CallArgs& args);
#ifdef NIGHTLY_BUILD
  static bool transferToImmutableImpl(JSContext* cx, const CallArgs& args);
#endif

 public:
  static const uint8_t DATA_SLOT = 0;
  static const uint8_t BYTE_LENGTH_SLOT = 1;
  static const uint8_t FIRST_VIEW_SLOT = 2;
  static const uint8_t FLAGS_SLOT = 3;

  static const uint8_t RESERVED_SLOTS = 4;

  // Alignment for ArrayBuffer objects. Must match the largest possible
  // TypedArray scalar to ensure TypedArray and Atomics accesses are always
  // aligned.
  static constexpr size_t ARRAY_BUFFER_ALIGNMENT = 8;

  static_assert(FLAGS_SLOT == JS_ARRAYBUFFER_FLAGS_SLOT,
                "self-hosted code with burned-in constants must get the "
                "right flags slot");

  // The length of an ArrayBuffer or SharedArrayBuffer can be at most INT32_MAX
  // on 32-bit platforms. Allow a larger limit on 64-bit platforms.
  static constexpr size_t ByteLengthLimitForSmallBuffer = INT32_MAX;
#ifdef JS_64BIT
  static constexpr size_t ByteLengthLimit =
      size_t(8) * 1024 * 1024 * 1024;  // 8 GB.
#else
  static constexpr size_t ByteLengthLimit = ByteLengthLimitForSmallBuffer;
#endif

 public:
  enum BufferKind {
    /** Inline data kept in the repurposed slots of this ArrayBufferObject. */
    INLINE_DATA = 0b000,

    /*
     * Data allocated using the SpiderMonkey allocator, created within
     * js::ArrayBufferContentsArena.
     */
    MALLOCED_ARRAYBUFFER_CONTENTS_ARENA = 0b001,

    /**
     * No bytes are associated with this buffer.  (This could be because the
     * buffer is detached, because it's an internal, newborn buffer not yet
     * overwritten with user-exposable semantics, or some other reason.  The
     * point is, don't read precise language semantics into this kind.)
     */
    NO_DATA = 0b010,

    /**
     * User-owned memory.  The associated buffer must be manually detached
     * before the user invalidates (deallocates, reuses the storage of, &c.)
     * the user-owned memory.
     */
    USER_OWNED = 0b011,

    WASM = 0b100,
    MAPPED = 0b101,
    EXTERNAL = 0b110,

    /**
     * Data allocated using the SpiderMonkey allocator, created within an
     * unknown memory arena.
     */
    MALLOCED_UNKNOWN_ARENA = 0b111,

    KIND_MASK = 0b111
  };

 public:
  enum ArrayBufferFlags {
    // The flags also store the BufferKind
    BUFFER_KIND_MASK = BufferKind::KIND_MASK,

    DETACHED = 0b1000,

    // Resizable ArrayBuffer.
    RESIZABLE = 0b1'0000,

    // This MALLOCED, MAPPED, or EXTERNAL buffer has been prepared for asm.js
    // and cannot henceforth be transferred/detached.  (WASM, USER_OWNED, and
    // INLINE_DATA buffers can't be prepared for asm.js -- although if an
    // INLINE_DATA buffer is used with asm.js, it's silently rewritten into a
    // MALLOCED buffer which *can* be prepared.)
    FOR_ASMJS = 0b10'0000,

    // The length is temporarily pinned, so it should not be detached. In the
    // future, this will also prevent GrowableArrayBuffer/ResizeableArrayBuffer
    // from modifying the length while this is set.
    PINNED_LENGTH = 0b100'0000,

    // Immutable ArrayBuffer.
    IMMUTABLE = 0b1000'0000,
  };

  static_assert(JS_ARRAYBUFFER_DETACHED_FLAG == DETACHED,
                "self-hosted code with burned-in constants must use the "
                "correct DETACHED bit value");

  static_assert(JS_ARRAYBUFFER_IMMUTABLE_FLAG == IMMUTABLE,
                "self-hosted code with burned-in constants must use the "
                "correct IMMUTABLE bit value");

 protected:
  enum class FillContents { Zero, Uninitialized };

  template <class ArrayBufferType, FillContents FillType>
  static std::tuple<ArrayBufferType*, uint8_t*>
  createUninitializedBufferAndData(JSContext* cx, size_t nbytes,
                                   AutoSetNewObjectMetadata&,
                                   JS::Handle<JSObject*> proto);

  template <class ArrayBufferType, FillContents FillType>
  static std::tuple<ArrayBufferType*, uint8_t*> createBufferAndData(
      JSContext* cx, size_t nbytes, AutoSetNewObjectMetadata& metadata,
      JS::Handle<JSObject*> proto = nullptr);

 public:
  class BufferContents {
    uint8_t* data_;
    BufferKind kind_;
    JS::BufferContentsFreeFunc free_;
    void* freeUserData_;

    friend class ArrayBufferObject;
    friend class ResizableArrayBufferObject;
    friend class ImmutableArrayBufferObject;

    BufferContents(uint8_t* data, BufferKind kind,
                   JS::BufferContentsFreeFunc freeFunc = nullptr,
                   void* freeUserData = nullptr)
        : data_(data),
          kind_(kind),
          free_(freeFunc),
          freeUserData_(freeUserData) {
      MOZ_ASSERT((kind_ & ~KIND_MASK) == 0);
      MOZ_ASSERT_IF(free_ || freeUserData_, kind_ == EXTERNAL);

      // It is the caller's responsibility to ensure that the
      // BufferContents does not outlive the data.
    }

#ifdef DEBUG
    // Checks if the buffer contents are properly aligned.
    //
    // `malloc(0)` is implementation defined and may return a pointer which
    // isn't aligned to `max_align_t`, so we only require proper alignment when
    // `byteLength` is non-zero.
    //
    // jemalloc doesn't implement restriction, but instead uses `sizeof(void*)`
    // for its smallest allocation class. Larger allocations are guaranteed to
    // be eight byte aligned.
    bool isAligned(size_t byteLength) const {
      // `malloc(0)` has implementation defined behavior.
      if (byteLength == 0) {
        return true;
      }

      // Allow jemalloc tiny allocations to have smaller alignment requirements
      // than `std::malloc`.
      if (sizeof(void*) < ArrayBufferObject::ARRAY_BUFFER_ALIGNMENT) {
        if (byteLength <= sizeof(void*)) {
          return true;
        }
      }

      // `std::malloc` returns memory at least as strictly aligned as for
      // max_align_t and the alignment of max_align_t is a multiple of the array
      // buffer alignment.
      static_assert(alignof(std::max_align_t) %
                        ArrayBufferObject::ARRAY_BUFFER_ALIGNMENT ==
                    0);

      // Otherwise the memory must be correctly alignment.
      auto ptr = reinterpret_cast<uintptr_t>(data());
      return ptr % ArrayBufferObject::ARRAY_BUFFER_ALIGNMENT == 0;
    }
#endif

   public:
    static BufferContents createInlineData(void* data) {
      return BufferContents(static_cast<uint8_t*>(data), INLINE_DATA);
    }

    static BufferContents createMallocedArrayBufferContentsArena(void* data) {
      return BufferContents(static_cast<uint8_t*>(data),
                            MALLOCED_ARRAYBUFFER_CONTENTS_ARENA);
    }

    static BufferContents createMallocedUnknownArena(void* data) {
      return BufferContents(static_cast<uint8_t*>(data),
                            MALLOCED_UNKNOWN_ARENA);
    }

    static BufferContents createNoData() {
      return BufferContents(nullptr, NO_DATA);
    }

    static BufferContents createUserOwned(void* data) {
      return BufferContents(static_cast<uint8_t*>(data), USER_OWNED);
    }

    static BufferContents createWasm(void* data) {
      return BufferContents(static_cast<uint8_t*>(data), WASM);
    }

    static BufferContents createMapped(void* data) {
      return BufferContents(static_cast<uint8_t*>(data), MAPPED);
    }

    static BufferContents createExternal(void* data,
                                         JS::BufferContentsFreeFunc freeFunc,
                                         void* freeUserData = nullptr) {
      MOZ_ASSERT(freeFunc);
      return BufferContents(static_cast<uint8_t*>(data), EXTERNAL, freeFunc,
                            freeUserData);
    }

    static BufferContents createFailed() {
      // There's no harm in tagging this as MALLOCED_ARRAYBUFFER_CONTENTS_ARENA,
      // even tho obviously it isn't. And adding an extra tag purely for this
      // case is a complication that presently appears avoidable.
      return BufferContents(nullptr, MALLOCED_ARRAYBUFFER_CONTENTS_ARENA);
    }

    uint8_t* data() const { return data_; }
    BufferKind kind() const { return kind_; }
    JS::BufferContentsFreeFunc freeFunc() const { return free_; }
    void* freeUserData() const { return freeUserData_; }

    explicit operator bool() const { return data_ != nullptr; }
    WasmArrayRawBuffer* wasmBuffer() const;
  };

  static const JSClass protoClass_;

  static bool byteLengthGetter(JSContext* cx, unsigned argc, Value* vp);

  static bool maxByteLengthGetter(JSContext* cx, unsigned argc, Value* vp);

  static bool resizableGetter(JSContext* cx, unsigned argc, Value* vp);

  static bool detachedGetter(JSContext* cx, unsigned argc, Value* vp);

#ifdef NIGHTLY_BUILD
  static bool immutableGetter(JSContext* cx, unsigned argc, Value* vp);
#endif

  static bool fun_isView(JSContext* cx, unsigned argc, Value* vp);

  static bool slice(JSContext* cx, unsigned argc, Value* vp);

#ifdef NIGHTLY_BUILD
  static bool sliceToImmutable(JSContext* cx, unsigned argc, Value* vp);
#endif

  static bool resize(JSContext* cx, unsigned argc, Value* vp);

  static bool transfer(JSContext* cx, unsigned argc, Value* vp);

  static bool transferToFixedLength(JSContext* cx, unsigned argc, Value* vp);

#ifdef NIGHTLY_BUILD
  static bool transferToImmutable(JSContext* cx, unsigned argc, Value* vp);
#endif

  static bool class_constructor(JSContext* cx, unsigned argc, Value* vp);

  static bool isOriginalByteLengthGetter(Native native) {
    return native == byteLengthGetter;
  }

  static ArrayBufferObject* createForContents(JSContext* cx, size_t nbytes,
                                              BufferContents contents);

 protected:
  template <class ArrayBufferType>
  static ArrayBufferType* copy(JSContext* cx, size_t newByteLength,
                               JS::Handle<ArrayBufferObject*> source);

  template <class ArrayBufferType>
  static ArrayBufferType* copyAndDetach(JSContext* cx, size_t newByteLength,
                                        JS::Handle<ArrayBufferObject*> source);

 private:
  template <class ArrayBufferType>
  static ArrayBufferType* copyAndDetachSteal(
      JSContext* cx, JS::Handle<ArrayBufferObject*> source);

  template <class ArrayBufferType>
  static ArrayBufferType* copyAndDetachRealloc(
      JSContext* cx, size_t newByteLength,
      JS::Handle<ArrayBufferObject*> source);

 public:
  static FixedLengthArrayBufferObject* createZeroed(
      JSContext* cx, size_t nbytes, HandleObject proto = nullptr);

  // Create an ArrayBufferObject that is safely finalizable and can later be
  // initialize()d to become a real, content-visible ArrayBufferObject.
  static FixedLengthArrayBufferObject* createEmpty(JSContext* cx);

  // Create an ArrayBufferObject using the provided buffer and size.  Assumes
  // ownership of |buffer| even in case of failure, i.e. on failure |buffer|
  // is deallocated.
  static ArrayBufferObject* createFromNewRawBuffer(JSContext* cx,
                                                   WasmArrayRawBuffer* buffer,
                                                   size_t initialSize);

  // Create an ArrayBufferObject object (resizable or fixed-length) based
  // on the existing donor object. The |buffer| will be removed from the
  // latter, and its ownership will be assumed by the new object.
  template <typename ArrayBufferType>
  static ArrayBufferType* createFromWasmObject(
      JSContext* cx, Handle<ArrayBufferObject*> donor);

  static void copyData(ArrayBufferObject* toBuffer, size_t toIndex,
                       ArrayBufferObject* fromBuffer, size_t fromIndex,
                       size_t count);

  template <class ArrayBufferType>
  static size_t objectMoved(JSObject* obj, JSObject* old);

  static uint8_t* stealMallocedContents(JSContext* cx,
                                        Handle<ArrayBufferObject*> buffer);

  static BufferContents extractStructuredCloneContents(
      JSContext* cx, Handle<ArrayBufferObject*> buffer);

  static void addSizeOfExcludingThis(JSObject* obj,
                                     mozilla::MallocSizeOf mallocSizeOf,
                                     JS::ClassInfo* info,
                                     JS::RuntimeSizes* runtimeSizes);

  // ArrayBufferObjects (strongly) store the first view added to them, while
  // later views are (weakly) stored in the compartment's InnerViewTable
  // below. Buffers usually only have one view, so this slot optimizes for
  // the common case. Avoiding entries in the InnerViewTable saves memory and
  // non-incrementalized sweep time.
  JSObject* firstView();

  bool addView(JSContext* cx, ArrayBufferViewObject* view);

  // Pin or unpin the length. Returns whether pinned status was changed.
  bool pinLength(bool pin) {
    if (bool(flags() & PINNED_LENGTH) == pin) {
      return false;
    }
    setFlags(flags() ^ PINNED_LENGTH);
    return true;
  }

  static bool ensureNonInline(JSContext* cx, Handle<ArrayBufferObject*> buffer);

  // Detach this buffer from its original memory.  (This necessarily makes
  // views of this buffer unusable for modifying that original memory.)
  static void detach(JSContext* cx, Handle<ArrayBufferObject*> buffer);

  static constexpr size_t offsetOfByteLengthSlot() {
    return getFixedSlotOffset(BYTE_LENGTH_SLOT);
  }
  static constexpr size_t offsetOfFlagsSlot() {
    return getFixedSlotOffset(FLAGS_SLOT);
  }

 protected:
  void setFirstView(ArrayBufferViewObject* view);

 private:
  struct FreeInfo {
    JS::BufferContentsFreeFunc freeFunc;
    void* freeUserData;
  };
  FreeInfo* freeInfo() const;

 public:
  uint8_t* dataPointer() const;
  SharedMem<uint8_t*> dataPointerShared() const;
  size_t byteLength() const;

  BufferContents contents() const {
    if (isExternal()) {
      return BufferContents(dataPointer(), EXTERNAL, freeInfo()->freeFunc,
                            freeInfo()->freeUserData);
    }
    return BufferContents(dataPointer(), bufferKind());
  }

  void releaseData(JS::GCContext* gcx);

  BufferKind bufferKind() const {
    return BufferKind(flags() & BUFFER_KIND_MASK);
  }

  bool isInlineData() const { return bufferKind() == INLINE_DATA; }
  bool isMalloced() const {
    return bufferKind() == MALLOCED_ARRAYBUFFER_CONTENTS_ARENA ||
           bufferKind() == MALLOCED_UNKNOWN_ARENA;
  }
  bool isNoData() const { return bufferKind() == NO_DATA; }
  bool hasUserOwnedData() const { return bufferKind() == USER_OWNED; }

  bool isWasm() const { return bufferKind() == WASM; }
  bool isMapped() const { return bufferKind() == MAPPED; }
  bool isExternal() const { return bufferKind() == EXTERNAL; }

  bool isDetached() const { return flags() & DETACHED; }
  bool isResizable() const { return flags() & RESIZABLE; }
  bool isLengthPinned() const { return flags() & PINNED_LENGTH; }
  bool isPreparedForAsmJS() const { return flags() & FOR_ASMJS; }
  bool isImmutable() const { return flags() & IMMUTABLE; }

  // Only WASM and asm.js buffers have a non-undefined [[ArrayBufferDetachKey]].
  //
  // https://tc39.es/ecma262/#sec-properties-of-the-arraybuffer-instances
  bool hasDefinedDetachKey() const { return isWasm() || isPreparedForAsmJS(); }

  // WebAssembly support:

  /**
   * Prepare this ArrayBuffer for use with asm.js.  Returns true on success,
   * false on failure.  This function reports no errors.
   */
  [[nodiscard]] bool prepareForAsmJS();

  size_t wasmMappedSize() const;

  wasm::AddressType wasmAddressType() const;
  wasm::Pages wasmPages() const;
  wasm::Pages wasmClampedMaxPages() const;
  mozilla::Maybe<wasm::Pages> wasmSourceMaxPages() const;

  [[nodiscard]] static ArrayBufferObject* wasmGrowToPagesInPlace(
      wasm::AddressType t, wasm::Pages newPages,
      Handle<ArrayBufferObject*> oldBuf, JSContext* cx);
  [[nodiscard]] static ArrayBufferObject* wasmMovingGrowToPages(
      wasm::AddressType t, wasm::Pages newPages,
      Handle<ArrayBufferObject*> oldBuf, JSContext* cx);
  static void wasmDiscard(Handle<ArrayBufferObject*> buf, uint64_t byteOffset,
                          uint64_t byteLength);

  static void finalize(JS::GCContext* gcx, JSObject* obj);

  static BufferContents createMappedContents(int fd, size_t offset,
                                             size_t length);

 protected:
  void setDataPointer(BufferContents contents);
  void setByteLength(size_t length);

  /**
   * Return the byte length for fixed-length buffers or the maximum byte length
   * for resizable buffers.
   */
  inline size_t maxByteLength() const;

  size_t wasmClampedMaxByteLength() const {
    MOZ_ASSERT(isWasm());
    return wasmClampedMaxPages().byteLength();
  }

  size_t associatedBytes() const;

  uint32_t flags() const;
  void setFlags(uint32_t flags);

  void setIsDetached() {
    MOZ_ASSERT(!isLengthPinned());
    MOZ_ASSERT(!isImmutable());
    setFlags(flags() | DETACHED);
  }
  void setIsPreparedForAsmJS() {
    MOZ_ASSERT(!isWasm());
    MOZ_ASSERT(!hasUserOwnedData());
    MOZ_ASSERT(!isInlineData());
    MOZ_ASSERT(isMalloced() || isMapped() || isExternal());
    setFlags(flags() | FOR_ASMJS);
  }

  void initialize(size_t byteLength, BufferContents contents) {
    MOZ_ASSERT(contents.isAligned(byteLength));
    setByteLength(byteLength);
    setFlags(0);
    setFirstView(nullptr);
    setDataPointer(contents);
  }

 public:
#if defined(DEBUG) || defined(JS_JITSPEW)
  void dumpOwnFields(js::JSONPrinter& json) const;
  void dumpOwnStringContent(js::GenericPrinter& out) const;
#endif
};

/**
 * FixedLengthArrayBufferObject
 *
 * ArrayBuffer object with a fixed length. Its length is unmodifiable, except
 * when zeroing it for detached buffers. Supports all possible memory stores
 * for ArrayBuffer objects, including inline data, malloc'ed memory, mapped
 * memory, and user-owner memory.
 *
 * Fixed-length ArrayBuffers can be used for asm.js and WebAssembly.
 */
class FixedLengthArrayBufferObject : public ArrayBufferObject {
  friend class ArrayBufferObject;

  uint8_t* inlineDataPointer() const;

  bool hasInlineData() const { return dataPointer() == inlineDataPointer(); }

 public:
  // Fixed-length ArrayBuffer objects don't have any additional reserved slots.
  static const uint8_t RESERVED_SLOTS = ArrayBufferObject::RESERVED_SLOTS;

  /** The largest number of bytes that can be stored inline. */
  static constexpr size_t MaxInlineBytes =
      (NativeObject::MAX_FIXED_SLOTS - RESERVED_SLOTS) * sizeof(JS::Value);

  static const JSClass class_;

  static FixedLengthArrayBufferObject* copy(
      JSContext* cx, size_t newByteLength,
      JS::Handle<ArrayBufferObject*> source) {
    return ArrayBufferObject::copy<FixedLengthArrayBufferObject>(
        cx, newByteLength, source);
  }

  static FixedLengthArrayBufferObject* copyAndDetach(
      JSContext* cx, size_t newByteLength,
      JS::Handle<ArrayBufferObject*> source) {
    return ArrayBufferObject::copyAndDetach<FixedLengthArrayBufferObject>(
        cx, newByteLength, source);
  }
};

/**
 * ResizableArrayBufferObject
 *
 * ArrayBuffer object which can both grow and shrink. The maximum byte length it
 * can grow to is set when creating the object. The data of resizable
 * ArrayBuffer object is either stored inline or malloc'ed memory.
 *
 * When a resizable ArrayBuffer object is detached, its maximum byte length
 * slot is set to zero in addition to the byte length slot.
 *
 * Resizable ArrayBuffers can neither be used for asm.js nor WebAssembly.
 */
class ResizableArrayBufferObject : public ArrayBufferObject {
  friend class ArrayBufferObject;

  template <FillContents FillType>
  static std::tuple<ResizableArrayBufferObject*, uint8_t*> createBufferAndData(
      JSContext* cx, size_t byteLength, size_t maxByteLength,
      AutoSetNewObjectMetadata& metadata, Handle<JSObject*> proto);

  static ResizableArrayBufferObject* createEmpty(JSContext* cx);

 public:
  static ResizableArrayBufferObject* createZeroed(
      JSContext* cx, size_t byteLength, size_t maxByteLength,
      Handle<JSObject*> proto = nullptr);

 private:
  uint8_t* inlineDataPointer() const;

  bool hasInlineData() const { return dataPointer() == inlineDataPointer(); }

  void setMaxByteLength(size_t length) {
    MOZ_ASSERT(length <= ArrayBufferObject::ByteLengthLimit);
    setFixedSlot(MAX_BYTE_LENGTH_SLOT, PrivateValue(length));
  }

  void initialize(size_t byteLength, size_t maxByteLength,
                  BufferContents contents) {
    MOZ_ASSERT(contents.isAligned(byteLength));
    setByteLength(byteLength);
    setMaxByteLength(maxByteLength);
    setFlags(RESIZABLE);
    setFirstView(nullptr);
    setDataPointer(contents);
  }

  // Resize this buffer.
  void resize(size_t newByteLength);

  static ResizableArrayBufferObject* copy(
      JSContext* cx, size_t newByteLength,
      JS::Handle<ResizableArrayBufferObject*> source);

 public:
  static const uint8_t MAX_BYTE_LENGTH_SLOT = ArrayBufferObject::RESERVED_SLOTS;

  static const uint8_t RESERVED_SLOTS = ArrayBufferObject::RESERVED_SLOTS + 1;

  /** The largest number of bytes that can be stored inline. */
  static constexpr size_t MaxInlineBytes =
      (NativeObject::MAX_FIXED_SLOTS - RESERVED_SLOTS) * sizeof(JS::Value);

  static const JSClass class_;

  size_t maxByteLength() const {
    return size_t(getFixedSlot(MAX_BYTE_LENGTH_SLOT).toPrivate());
  }

  static ResizableArrayBufferObject* copyAndDetach(
      JSContext* cx, size_t newByteLength,
      JS::Handle<ResizableArrayBufferObject*> source);

 private:
  static ResizableArrayBufferObject* copyAndDetachSteal(
      JSContext* cx, size_t newByteLength,
      JS::Handle<ResizableArrayBufferObject*> source);
};

/**
 * ImmutableArrayBufferObject
 *
 * ArrayBuffer object with immutable length and contents. Supports all possible
 * memory stores for ArrayBuffer objects, including inline data, malloc'ed
 * memory, mapped memory, and user-owner memory.
 *
 * Immutable ArrayBuffers can neither be used for asm.js nor WebAssembly.
 */
class ImmutableArrayBufferObject : public ArrayBufferObject {
  friend class ArrayBufferObject;

  static ImmutableArrayBufferObject* createEmpty(JSContext* cx);

 public:
  static ImmutableArrayBufferObject* createZeroed(
      JSContext* cx, size_t byteLength, Handle<JSObject*> proto = nullptr);

 private:
  uint8_t* inlineDataPointer() const;

  bool hasInlineData() const { return dataPointer() == inlineDataPointer(); }

  void initialize(size_t byteLength, BufferContents contents) {
    MOZ_ASSERT(contents.isAligned(byteLength));
    setByteLength(byteLength);
    setFlags(IMMUTABLE);
    setFirstView(nullptr);
    setDataPointer(contents);
  }

 public:
  // Immutable ArrayBuffer objects don't have any additional reserved slots.
  static const uint8_t RESERVED_SLOTS = ArrayBufferObject::RESERVED_SLOTS;

  /** The largest number of bytes that can be stored inline. */
  static constexpr size_t MaxInlineBytes =
      (NativeObject::MAX_FIXED_SLOTS - RESERVED_SLOTS) * sizeof(JS::Value);

  static const JSClass class_;

  static ImmutableArrayBufferObject* copy(
      JSContext* cx, size_t newByteLength,
      JS::Handle<ArrayBufferObject*> source) {
    return ArrayBufferObject::copy<ImmutableArrayBufferObject>(
        cx, newByteLength, source);
  }

  static ImmutableArrayBufferObject* copyAndDetach(
      JSContext* cx, size_t newByteLength,
      JS::Handle<ArrayBufferObject*> source) {
    return ArrayBufferObject::copyAndDetach<ImmutableArrayBufferObject>(
        cx, newByteLength, source);
  }

  static ImmutableArrayBufferObject* slice(
      JSContext* cx, size_t newByteLength,
      JS::Handle<ArrayBufferObject*> source, size_t sourceByteOffset);
};

size_t ArrayBufferObject::maxByteLength() const {
  if (isResizable()) {
    return as<ResizableArrayBufferObject>().maxByteLength();
  }
  return byteLength();
}

// Create a buffer for a wasm memory, whose type is determined by
// memory.addressType().
ArrayBufferObjectMaybeShared* CreateWasmBuffer(JSContext* cx,
                                               const wasm::MemoryDesc& memory);

// Per-compartment table that manages the relationship between array buffers
// and the views that use their storage.
class InnerViewTable {
  // Store views in a vector such that all the tenured views come before any
  // nursery views. Maintain the index of the first nursery view so there is an
  // efficient way to access only the nursery views.
  using ViewVector =
      GCVector<UnsafeBarePtr<ArrayBufferViewObject*>, 1, ZoneAllocPolicy>;
  struct Views {
    ViewVector views;  // List of views with tenured views at the front.
    size_t firstNurseryView = 0;

    explicit Views(JS::Zone* zone) : views(zone) {}
    bool empty();
    bool hasNurseryViews();
    bool addView(ArrayBufferViewObject* view);

    bool traceWeak(JSTracer* trc, size_t startIndex = 0);
    bool sweepAfterMinorGC(JSTracer* trc);

    void check();
  };

  // For all objects sharing their storage with some other view, this maps
  // the object to the list of such views. All entries in this map are weak.
  //
  // This key is a raw pointer and not a WeakHeapPtr because the post-barrier
  // would hold nursery-allocated entries live unconditionally. It is a very
  // common pattern in low-level and performance-oriented JavaScript to create
  // hundreds or thousands of very short lived temporary views on a larger
  // buffer; having to tenure all of these would be a catastrophic performance
  // regression. Thus, it is vital that nursery pointers in this map not be held
  // live. Special support is required in the minor GC, implemented in
  // sweepAfterMinorGC.
  using ArrayBufferViewMap =
      GCHashMap<UnsafeBarePtr<ArrayBufferObject*>, Views,
                StableCellHasher<JSObject*>, ZoneAllocPolicy>;
  ArrayBufferViewMap map;

  // List of keys from map where either the source or at least one target is in
  // the nursery. The raw pointer to a JSObject is allowed here because this
  // vector is cleared after every minor collection. Users in sweepAfterMinorGC
  // must be careful to use MaybeForwarded before touching these pointers.
  using NurseryKeysVector =
      GCVector<UnsafeBarePtr<ArrayBufferObject*>, 0, SystemAllocPolicy>;
  NurseryKeysVector nurseryKeys;

  // Whether nurseryKeys is a complete list.
  bool nurseryKeysValid = true;

  bool sweepMapEntryAfterMinorGC(UnsafeBarePtr<JSObject*>& buffer,
                                 ViewVector& views);

 public:
  explicit InnerViewTable(Zone* zone) : map(zone) {}

  // Remove references to dead objects in the table and update table entries
  // to reflect moved objects.
  bool traceWeak(JSTracer* trc);
  void sweepAfterMinorGC(JSTracer* trc);

  bool empty() const { return map.empty(); }

  bool needsSweepAfterMinorGC() const {
    return !nurseryKeys.empty() || !nurseryKeysValid;
  }

  size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf);

 private:
  friend class ArrayBufferObject;
  friend class ResizableArrayBufferObject;
  bool addView(JSContext* cx, ArrayBufferObject* buffer,
               ArrayBufferViewObject* view);
  ViewVector* maybeViewsUnbarriered(ArrayBufferObject* buffer);
  void removeViews(ArrayBufferObject* buffer);

  bool sweepViewsAfterMinorGC(JSTracer* trc, ArrayBufferObject* buffer,
                              Views& views);
};

template <typename Wrapper>
class MutableWrappedPtrOperations<InnerViewTable, Wrapper>
    : public WrappedPtrOperations<InnerViewTable, Wrapper> {
  InnerViewTable& table() { return static_cast<Wrapper*>(this)->get(); }

 public:
  size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) {
    return table().sizeOfExcludingThis(mallocSizeOf);
  }
};

class WasmArrayRawBuffer {
  wasm::AddressType addressType_;
  wasm::Pages clampedMaxPages_;
  mozilla::Maybe<wasm::Pages> sourceMaxPages_;
  size_t mappedSize_;  // See comment on mappedSize().
  size_t length_;

 protected:
  WasmArrayRawBuffer(wasm::AddressType addressType, uint8_t* buffer,
                     wasm::Pages clampedMaxPages,
                     const mozilla::Maybe<wasm::Pages>& sourceMaxPages,
                     size_t mappedSize, size_t length)
      : addressType_(addressType),
        clampedMaxPages_(clampedMaxPages),
        sourceMaxPages_(sourceMaxPages),
        mappedSize_(mappedSize),
        length_(length) {
    // Assert that this WasmArrayRawBuffer was allocated in the correct place
    // relative to its data.
    MOZ_ASSERT(buffer == dataPointer());
  }

 public:
  static WasmArrayRawBuffer* AllocateWasm(
      wasm::AddressType addressType, wasm::Pages initialPages,
      wasm::Pages clampedMaxPages,
      const mozilla::Maybe<wasm::Pages>& sourceMaxPages,
      const mozilla::Maybe<size_t>& mappedSize);
  static void Release(void* mem);

  uint8_t* dataPointer() {
    uint8_t* ptr = reinterpret_cast<uint8_t*>(this);
    return ptr + sizeof(WasmArrayRawBuffer);
  }

  static const WasmArrayRawBuffer* fromDataPtr(const uint8_t* dataPtr) {
    return reinterpret_cast<const WasmArrayRawBuffer*>(
        dataPtr - sizeof(WasmArrayRawBuffer));
  }

  static WasmArrayRawBuffer* fromDataPtr(uint8_t* dataPtr) {
    return reinterpret_cast<WasmArrayRawBuffer*>(dataPtr -
                                                 sizeof(WasmArrayRawBuffer));
  }

  wasm::AddressType addressType() const { return addressType_; }

  uint8_t* basePointer() { return dataPointer() - gc::SystemPageSize(); }

  /*
   * The actual mmapped size. Access in the range [0, mappedSize) will either
   * succeed, or be handled by the wasm signal handlers. The mapped size will be
   * aligned to the system allocation granularity such that we can
   * optimistically map other regions following it, in order to reduce copies
   * when growing memory.
   *
   * Note that this does NOT include the header page in which this buffer itself
   * is allocated.
   */
  size_t mappedSize() const { return mappedSize_; }

  /*
   * The wasm-visible current length of the buffer in bytes. Accesses in the
   * range [0, byteLength) will succeed. May only increase.
   *
   * For more info see "WASM Linear Memory structure" in ArrayBufferObject.cpp.
   */
  size_t byteLength() const { return length_; }

  wasm::Pages pages() const {
    return wasm::Pages::fromByteLengthExact(length_);
  }

  /*
   * The maximum size on how far the byteLength can grow in pages. This value
   * respects implementation limits and is always representable as a byte
   * length. Every memory has a clampedMaxSize, even if no maximum was specified
   * in source. When a memory has no sourceMaxSize, the clampedMaxSize will be
   * the maximum amount of memory that can be grown to while still respecting
   * implementation limits.
   *
   * For more info see "WASM Linear Memory structure" in ArrayBufferObject.cpp.
   */
  wasm::Pages clampedMaxPages() const { return clampedMaxPages_; }

  /*
   * The optional declared limit on how far byteLength can grow in pages. This
   * is the unmodified maximum size from the source module or JS-API invocation.
   * This may not be representable in byte lengths, nor feasible for a module to
   * actually grow to due to implementation limits. It is used for correct
   * linking checks and js-types reflection.
   *
   * For more info see "WASM Linear Memory structure" in ArrayBufferObject.cpp.
   */
  mozilla::Maybe<wasm::Pages> sourceMaxPages() const { return sourceMaxPages_; }

  [[nodiscard]] bool growToPagesInPlace(wasm::Pages newPages);

  // Discard a region of memory, zeroing the pages and releasing physical memory
  // back to the operating system. byteOffset and byteLen must be wasm page
  // aligned and in bounds. A discard of zero bytes will have no effect.
  void discard(size_t byteOffset, size_t byteLen);
};

}  // namespace js

template <>
inline bool JSObject::is<js::ArrayBufferObject>() const {
  return is<js::FixedLengthArrayBufferObject>() ||
         is<js::ResizableArrayBufferObject>() ||
         is<js::ImmutableArrayBufferObject>();
}

template <>
bool JSObject::is<js::ArrayBufferObjectMaybeShared>() const;

#endif  // vm_ArrayBufferObject_h
