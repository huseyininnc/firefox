// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: pending
description: |
  Mutate an iterator after it has been mapped and returned done.
features:
  - iterator-helpers
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
---*/
//

const array = [1, 2, 3];
const iterator = [1, 2, 3].values().map(x => x * 2);

assert.sameValue(iterator.next().value, 2);
assert.sameValue(iterator.next().value, 4);
assert.sameValue(iterator.next().value, 6);
assert.sameValue(iterator.next().done, true);

array.push(4);
assert.sameValue(iterator.next().done, true);


reportCompare(0, 0);
