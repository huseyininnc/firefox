/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Parent config file for all test files.
// This should be applied by the configuration before any other test
// configurations.

export default {
  name: "mozilla/general-test",
  plugins: ["mozilla", "@microsoft/sdl", "no-unsanitized"],

  rules: {
    // No using of insecure url, so no http urls.
    // Note: This is turned off for xpcshell-tests as it is not considered
    // necessary for xpcshell level tests.
    "@microsoft/sdl/no-insecure-url": [
      "error",
      {
        exceptions: [
          "^http:\\/\\/mochi\\.test?.*",
          "^http:\\/\\/mochi\\.xorigin-test?.*",
          "^http:\\/\\/localhost?.*",
          "^http:\\/\\/127\\.0\\.0\\.1?.*",
          // Exempt xmlns urls
          "^http:\\/\\/www\\.w3\\.org?.*",
          "^http:\\/\\/www\\.mozilla\\.org\\/keymaster\\/gatekeeper?.*",
          // Exempt urls that start with ftp or ws.
          "^ws:?.*",
          "^ftp:?.*",
        ],
        varExceptions: ["insecure?.*"],
      },
    ],

    "mozilla/import-content-task-globals": "error",
    "mozilla/import-headjs-globals": "error",
    "mozilla/mark-test-function-used": "error",
    "mozilla/no-arbitrary-setTimeout": "error",

    // Bug 1883707 - Turn off no-console in tests at the moment.
    "no-console": "off",
    // Turn off no-unsanitized for tests, as we do want to be able to use
    // these for testing.
    "no-unsanitized/method": "off",
    "no-unsanitized/property": "off",
  },
};
