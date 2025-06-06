// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.since
description: Rounding can cross unit boundaries up to largestUnit
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const earlier = new Temporal.PlainTime();
const later = new Temporal.PlainTime(1, 59, 59);
const duration = earlier.since(later, { largestUnit: "hours", smallestUnit: "minutes", roundingMode: "expand" });
TemporalHelpers.assertDuration(duration, 0, 0, 0, 0, -2, 0, 0, 0, 0, 0, "-1:60 balances to -2 hours");

reportCompare(0, 0);
