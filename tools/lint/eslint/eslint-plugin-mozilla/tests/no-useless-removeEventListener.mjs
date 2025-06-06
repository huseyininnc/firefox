/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

import rule from "../lib/rules/no-useless-removeEventListener.mjs";
import { RuleTester } from "eslint";

const ruleTester = new RuleTester();

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code) {
  return { code, errors: [{ messageId: "useOnce", type: "CallExpression" }] };
}

ruleTester.run("no-useless-removeEventListener", rule, {
  valid: [
    // Rule should not throw if arguments for removeEventListener
    // are not provided yet.
    "elt.addEventListener(event1, function listener() {" +
      "  elt.removeEventListener();" +
      "});",

    // Listeners that aren't a function are always valid.
    "elt.addEventListener('click', handler);",
    "elt.addEventListener('click', handler, true);",
    "elt.addEventListener('click', handler, {once: true});",

    // Should not fail on empty functions.
    "elt.addEventListener('click', function() {});",

    // Should not reject when removing a listener for another event.
    "elt.addEventListener('click', function listener() {" +
      "  elt.removeEventListener('keypress', listener);" +
      "});",

    // Should not reject when there's another instruction before
    // removeEventListener.
    "elt.addEventListener('click', function listener() {" +
      "  elt.focus();" +
      "  elt.removeEventListener('click', listener);" +
      "});",

    // Should not reject when wantsUntrusted is true.
    "elt.addEventListener('click', function listener() {" +
      "  elt.removeEventListener('click', listener);" +
      "}, false, true);",

    // Should not reject when there's a literal and a variable
    "elt.addEventListener('click', function listener() {" +
      "  elt.removeEventListener(eventName, listener);" +
      "});",

    // Should not reject when there's 2 different variables
    "elt.addEventListener(event1, function listener() {" +
      "  elt.removeEventListener(event2, listener);" +
      "});",

    // Should not fail if this is a different type of event listener function.
    "myfunc.addEventListener(listener);",
  ],
  invalid: [
    invalidCode(
      "elt.addEventListener('click', function listener() {" +
        "  elt.removeEventListener('click', listener);" +
        "});"
    ),
    invalidCode(
      "elt.addEventListener('click', function listener() {" +
        "  elt.removeEventListener('click', listener, true);" +
        "}, true);"
    ),
    invalidCode(
      "elt.addEventListener('click', function listener() {" +
        "  elt.removeEventListener('click', listener);" +
        "}, {once: true});"
    ),
    invalidCode(
      "elt.addEventListener('click', function listener() {" +
        "  /* Comment */" +
        "  elt.removeEventListener('click', listener);" +
        "});"
    ),
    invalidCode(
      "elt.addEventListener('click', function() {" +
        "  elt.removeEventListener('click', arguments.callee);" +
        "});"
    ),
    invalidCode(
      "elt.addEventListener(eventName, function listener() {" +
        "  elt.removeEventListener(eventName, listener);" +
        "});"
    ),
  ],
});
