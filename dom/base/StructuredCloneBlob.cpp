/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/StructuredCloneBlob.h"

#include "js/StructuredClone.h"
#include "js/Utility.h"
#include "jswrapper.h"

#include "xpcpublic.h"

#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/StructuredCloneTags.h"

namespace mozilla {
namespace dom {

StructuredCloneBlob::StructuredCloneBlob()
    : StructuredCloneHolder(CloningSupported, TransferringNotSupported,
                            StructuredCloneScope::DifferentProcess)
{};


/* static */ already_AddRefed<StructuredCloneBlob>
StructuredCloneBlob::Constructor(GlobalObject& aGlobal, JS::HandleValue aValue,
                                 JS::HandleObject aTargetGlobal,
                                 ErrorResult& aRv)
{
  JSContext* cx = aGlobal.Context();

  RefPtr<StructuredCloneBlob> holder = new StructuredCloneBlob();

  Maybe<JSAutoCompartment> ac;
  JS::RootedValue value(cx, aValue);

  if (aTargetGlobal) {
    JS::RootedObject targetGlobal(cx, js::CheckedUnwrap(aTargetGlobal));
    if (!targetGlobal) {
      js::ReportAccessDenied(cx);
      aRv.NoteJSContextException(cx);
      return nullptr;
    }

    ac.emplace(cx, targetGlobal);

    if (!JS_WrapValue(cx, &value)) {
      aRv.NoteJSContextException(cx);
      return nullptr;
    }
  } else if (value.isObject()) {
    JS::RootedObject obj(cx, js::CheckedUnwrap(&value.toObject()));
    if (!obj) {
      js::ReportAccessDenied(cx);
      aRv.NoteJSContextException(cx);
      return nullptr;
    }

    ac.emplace(cx, obj);
    value = JS::ObjectValue(*obj);
  }

  holder->Write(cx, value, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  return holder.forget();
}

void
StructuredCloneBlob::Deserialize(JSContext* aCx, JS::HandleObject aTargetScope,
                                      JS::MutableHandleValue aResult, ErrorResult& aRv)
{
  JS::RootedObject scope(aCx, js::CheckedUnwrap(aTargetScope));
  if (!scope) {
    js::ReportAccessDenied(aCx);
    aRv.NoteJSContextException(aCx);
    return;
  }

  {
    JSAutoCompartment ac(aCx, scope);

    Read(xpc::NativeGlobal(scope), aCx, aResult, aRv);
    if (aRv.Failed()) {
      return;
    }
  }

  if (!JS_WrapValue(aCx, aResult)) {
    aResult.set(JS::UndefinedValue());
    aRv.NoteJSContextException(aCx);
  }
}


/* static */ JSObject*
StructuredCloneBlob::ReadStructuredClone(JSContext* aCx, JSStructuredCloneReader* aReader)
{
  JS::RootedObject obj(aCx);
  {
    RefPtr<StructuredCloneBlob> holder = new StructuredCloneBlob();

    if (!holder->ReadStructuredCloneInternal(aCx, aReader) ||
        !holder->WrapObject(aCx, nullptr, &obj)) {
      return nullptr;
    }
  }
  return obj.get();
}

bool
StructuredCloneBlob::ReadStructuredCloneInternal(JSContext* aCx, JSStructuredCloneReader* aReader)
{
  uint32_t length;
  uint32_t version;
  if (!JS_ReadUint32Pair(aReader, &length, &version)) {
    return false;
  }

  JSStructuredCloneData data(length, length, 4096);
  if (!JS_ReadBytes(aReader, data.Start(), length)) {
    return false;
  }

  mBuffer = MakeUnique<JSAutoStructuredCloneBuffer>(mStructuredCloneScope,
                                                    &StructuredCloneHolder::sCallbacks,
                                                    this);
  mBuffer->adopt(Move(data), version, &StructuredCloneHolder::sCallbacks);

  return true;
}

bool
StructuredCloneBlob::WriteStructuredClone(JSContext* aCx, JSStructuredCloneWriter* aWriter)
{
  auto& data = mBuffer->data();
  if (!JS_WriteUint32Pair(aWriter, SCTAG_DOM_STRUCTURED_CLONE_HOLDER, 0) ||
      !JS_WriteUint32Pair(aWriter, data.Size(), JS_STRUCTURED_CLONE_VERSION)) {
    return false;
  }

  auto iter = data.Iter();
  while (!iter.Done()) {
    if (!JS_WriteBytes(aWriter, iter.Data(), iter.RemainingInSegment())) {
      return false;
    }
    iter.Advance(data, iter.RemainingInSegment());
  }

  return true;
}

bool
StructuredCloneBlob::WrapObject(JSContext* aCx, JS::HandleObject aGivenProto, JS::MutableHandleObject aResult)
{
    return StructuredCloneHolderBinding::Wrap(aCx, this, aGivenProto, aResult);
}

} // namespace dom
} // namespace mozilla
