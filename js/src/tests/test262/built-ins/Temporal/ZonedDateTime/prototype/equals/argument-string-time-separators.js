// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.equals
description: Time separator in string argument can vary
features: [Temporal]
---*/

const tests = [
  ["1970-01-01T00:00+00:00[UTC]", "uppercase T"],
  ["1970-01-01t00:00+00:00[UTC]", "lowercase T"],
  ["1970-01-01 00:00+00:00[UTC]", "space between date and time"],
];

const instance = new Temporal.ZonedDateTime(0n, "UTC");

tests.forEach(([arg, description]) => {
  const result = instance.equals(arg);

  assert.sameValue(
    result,
    true,
    `variant time separators (${description})`
  );
});

reportCompare(0, 0);
