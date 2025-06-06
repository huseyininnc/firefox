/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { searchSourceForHighlight } from "../utils/editor/index";

import {
  getSelectedSourceTextContent,
  getSearchOptions,
} from "../selectors/index";

import { closeActiveSearch, clearHighlightLineRange } from "./ui";

export function doSearchForHighlight(query, editor) {
  return async ({ getState, dispatch }) => {
    const sourceTextContent = getSelectedSourceTextContent(getState());
    if (!sourceTextContent) {
      return;
    }

    dispatch(searchContentsForHighlight(query, editor));
  };
}

// Expose an action to the React component, so that it can call the searchWorker.
export function querySearchWorker(query, text, modifiers) {
  return ({ searchWorker }) => {
    return searchWorker.getMatches(query, text, modifiers);
  };
}

export function searchContentsForHighlight(query, editor) {
  return async ({ getState }) => {
    const modifiers = getSearchOptions(getState(), "file-search");
    const sourceTextContent = getSelectedSourceTextContent(getState());

    if (!query || !editor || !sourceTextContent || !modifiers) {
      return;
    }

    const ctx = { editor, cm: editor.codeMirror };
    searchSourceForHighlight(ctx, false, query, true, modifiers);
  };
}

export function closeFileSearch() {
  return ({ dispatch }) => {
    dispatch(closeActiveSearch());
    dispatch(clearHighlightLineRange());
  };
}
