/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.efficiency.helpers

import org.mozilla.fenix.R

data class Selector(
    val strategy: SelectorStrategy,
    val value: String,
    val description: String,
    val groups: List<String> = listOf(),
) {
    fun toResourceId(): Int {
        return try {
            val rClass = R.id::class.java
            val field = rClass.getField(value)
            field.getInt(null)
        } catch (e: Exception) {
            throw IllegalArgumentException("Could not resolve resource ID for selector value: '$value' using R.id", e)
        }
    }
}

enum class SelectorStrategy {
    /**
     * Supported strategies for locating UI elements.
     */
    COMPOSE_BY_CONTENT_DESCRIPTION,
    COMPOSE_BY_TAG,
    COMPOSE_BY_TEXT,
    ESPRESSO_BY_ID,
    ESPRESSO_BY_TEXT,
    ESPRESSO_BY_CONTENT_DESC,
    UIAUTOMATOR2_BY_RES,
    UIAUTOMATOR2_BY_CLASS,
    UIAUTOMATOR2_BY_TEXT,
    UIAUTOMATOR_WITH_TEXT_CONTAINS,
    UIAUTOMATOR_WITH_RES_ID,
    UIAUTOMATOR_WITH_TEXT,
    UIAUTOMATOR_WITH_DESCRIPTION_CONTAINS,
}
