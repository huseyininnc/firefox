/**
 * @file Provides utilities for setting up environments.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import path from "path";
import helpers from "../helpers.mjs";
import globals from "../globals.mjs";

/**
 * Obtains the globals for a list of files.
 *
 * @param {string} environmentName
 *   The name of the environment that globals are being obtained for.
 * @param {string[]} files
 *   The array of files to get globals for. The paths are relative to the topsrcdir.
 * @returns {object}
 *   Returns an object with keys of the global names and values of if they are
 *   writable or not.
 */
function getGlobalsForScripts(environmentName, files, extraDefinitions) {
  let fileGlobals = extraDefinitions;
  const root = helpers.rootDir;
  for (const file of files) {
    const fileName = path.join(root, file);
    try {
      fileGlobals = fileGlobals.concat(globals.getGlobalsForFile(fileName));
    } catch (e) {
      console.error(`Could not load globals from file ${fileName}: ${e}`);
      console.error(
        `You may need to update the mappings for the ${environmentName} environment`
      );
      throw new Error(`Could not load globals from file ${fileName}: ${e}`);
    }
  }

  var globalObjects = {};
  for (let { name: globalName, writable } of fileGlobals) {
    globalObjects[globalName] = writable;
  }
  return globalObjects;
}

export function getScriptGlobals(
  environmentName,
  files,
  extraDefinitions = [],
  extraEnv = {}
) {
  if (helpers.isMozillaCentralBased()) {
    return {
      globals: getGlobalsForScripts(environmentName, files, extraDefinitions),
      ...extraEnv,
    };
  }
  return helpers.getSavedEnvironmentItems(environmentName);
}
