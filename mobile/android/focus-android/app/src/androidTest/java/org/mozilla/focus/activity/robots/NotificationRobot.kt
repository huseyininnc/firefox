/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity.robots

import android.os.Build
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import org.junit.Assert.assertTrue
import org.mozilla.focus.R
import org.mozilla.focus.helpers.TestHelper.getStringResource
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.waitingTime

class NotificationRobot {

    private val systemNotificationPanelId = "com.android.systemui:id/notification_stack_scroller"
    private val quickSettingsPanelId = "com.android.systemui:id/quick_qs_panel"

    fun clearNotifications() {
        if (clearButton().exists()) {
            clearButton().click()
        } else {
            scrollToEnd()
            if (clearButton().exists()) {
                clearButton().click()
            } else if (notificationTray().exists()) {
                mDevice.pressBack()
            }
        }
    }

    fun expandEraseBrowsingNotification() {
        eraseBrowsingNotification.swipeDown(10)
    }

    fun verifySystemNotificationExists(notificationMessage: String) {
        val notification = mDevice.findObject(UiSelector().text(notificationMessage))
        while (!notification.waitForExists(waitingTime)) {
            UiScrollable(
                UiSelector().resourceId(systemNotificationPanelId),
            ).flingToEnd(1)
        }

        assertTrue(notification.exists())
    }

    fun verifyMediaNotificationExists(notificationMessage: String) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            val notificationInTray = mDevice.wait(
                Until.hasObject(
                    By.res(quickSettingsPanelId).hasDescendant(
                        By.text(notificationMessage),
                    ),
                ),
                waitingTime,
            )

            assertTrue(notificationInTray)
        } else {
            verifySystemNotificationExists(notificationMessage)
        }
    }

    fun verifyNotificationGone(notificationMessage: String) {
        assertTrue(
            mDevice.findObject(UiSelector().text(notificationMessage))
                .waitUntilGone(waitingTime),
        )
    }

    fun clickMediaNotificationControlButton(action: String) {
        mediaNotificationControlButton(action).click()
    }

    fun verifyMediaNotificationButtonState(action: String) {
        mediaNotificationControlButton(action).waitForExists(waitingTime)
    }

    fun verifyDownloadNotification(notificationMessage: String, fileName: String) {
        val notification = UiSelector().text(notificationMessage)
        var notificationFound = mDevice.findObject(notification).waitForExists(waitingTime)
        val downloadFilename = mDevice.findObject(UiSelector().text(fileName))

        while (!notificationFound) {
            notificationTray().swipeUp(2)
            notificationFound = mDevice.findObject(notification).waitForExists(waitingTime)
        }

        assertTrue(notificationFound)
        assertTrue(downloadFilename.exists())
    }

    class Transition {
        fun clickEraseAndOpenNotificationButton(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            notificationEraseAndOpenButton.waitForExists(waitingTime)
            notificationEraseAndOpenButton.click()

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }

        fun clickNotificationOpenButton(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            notificationOpenButton.waitForExists(waitingTime)
            notificationOpenButton.clickAndWaitForNewWindow()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickNotificationMessage(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            eraseBrowsingNotification.waitForExists(waitingTime)
            eraseBrowsingNotification.click()

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }
    }
}

fun notificationTray(interact: NotificationRobot.() -> Unit): NotificationRobot.Transition {
    NotificationRobot().interact()
    return NotificationRobot.Transition()
}

private val eraseBrowsingNotification =
    mDevice.findObject(
        UiSelector().text(getStringResource(R.string.notification_erase_title_android_14)),
    )

private val notificationEraseAndOpenButton =
    mDevice.findObject(
        UiSelector().description(getStringResource(R.string.notification_action_erase_and_open)),
    )

private val notificationOpenButton = mDevice.findObject(
    UiSelector().description(getStringResource(R.string.notification_action_open)),
)

private fun notificationTray() = UiScrollable(
    UiSelector().resourceId("com.android.systemui:id/notification_stack_scroller"),
).setAsVerticalList()

private fun clearButton() =
    mDevice.findObject(UiSelector().resourceId("com.android.systemui:id/dismiss_text"))

private fun scrollToEnd() {
    notificationTray().scrollToEnd(1)
}

private fun mediaNotificationControlButton(action: String) =
    mDevice.findObject(UiSelector().description(action))
