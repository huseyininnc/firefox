// |reftest| shell-option(--enable-explicit-resource-management) skip-if(!(this.hasOwnProperty('getBuildConfiguration')&&getBuildConfiguration('explicit-resource-management'))||!xulRuntime.shell) -- explicit-resource-management is not enabled unconditionally, requires shell-options
// Copyright (C) 2023 Ron Buckton. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-disposablestack.prototype.dispose
description: Does not re-invoke disposal on resources after stack has already been disposed.
info: |
  DisposableStack.prototype.dispose ( )

  1. Let disposableStack be the this value.
  2. Perform ? RequireInternalSlot(disposableStack, [[DisposableState]]).
  3. If disposableStack.[[DisposableState]] is disposed, return undefined.
  4. Set disposableStack.[[DisposableState]] to disposed.
  5. Return DisposeResources(disposableStack.[[DisposeCapability]], NormalCompletion(undefined)).

  DisposeResources ( disposeCapability, completion )

  1. For each resource of disposeCapability.[[DisposableResourceStack]], in reverse list order, do
    a. Let result be Dispose(resource.[[ResourceValue]], resource.[[Hint]], resource.[[DisposeMethod]]).
    b. If result.[[Type]] is throw, then
      i. If completion.[[Type]] is throw, then
        1. Set result to result.[[Value]].
        2. Let suppressed be completion.[[Value]].
        3. Let error be a newly created SuppressedError object.
        4. Perform ! CreateNonEnumerableDataPropertyOrThrow(error, "error", result).
        5. Perform ! CreateNonEnumerableDataPropertyOrThrow(error, "suppressed", suppressed).
        6. Set completion to ThrowCompletion(error).
      ii. Else,
        1. Set completion to result.
  2. Return completion.

  Dispose ( V, hint, method )

  1. If method is undefined, let result be undefined.
  2. Else, let result be ? Call(method, V).
  3. If hint is async-dispose, then
    a. Perform ? Await(result).
  4. Return undefined.

features: [explicit-resource-management]
---*/

var stack = new DisposableStack();
var useCount = 0;
var adoptCount = 0;
var deferCount = 0;
stack.use({ [Symbol.dispose]() { useCount++; } });
stack.adopt({}, _ => { adoptCount++; });
stack.defer(() => { deferCount++; });
stack.dispose();
stack.dispose();
assert.sameValue(useCount, 1);
assert.sameValue(adoptCount, 1);
assert.sameValue(deferCount, 1);

reportCompare(0, 0);
