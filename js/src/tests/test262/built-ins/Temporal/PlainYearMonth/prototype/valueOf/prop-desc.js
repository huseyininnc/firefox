// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.valueof
description: The "valueOf" property of Temporal.PlainYearMonth.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.PlainYearMonth.prototype.valueOf,
  "function",
  "`typeof PlainYearMonth.prototype.valueOf` is `function`"
);

verifyProperty(Temporal.PlainYearMonth.prototype, "valueOf", {
  writable: true,
  enumerable: false,
  configurable: true,
});

reportCompare(0, 0);
