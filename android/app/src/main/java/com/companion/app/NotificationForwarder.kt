package com.companion.app

import android.app.Notification
import android.content.Context
import android.service.notification.NotificationListenerService
import android.service.notification.StatusBarNotification

/** Forwards phone notifications while the switch in the app is on. The user enables access in system settings. */
class NotificationForwarder : NotificationListenerService() {
    override fun onNotificationPosted(sbn: StatusBarNotification) {
        val prefs = getSharedPreferences("companion", Context.MODE_PRIVATE)
        if (!prefs.getBoolean("forward", false) || sbn.packageName == packageName || sbn.isOngoing) return
        val extras = sbn.notification.extras
        val title = extras.getCharSequence(Notification.EXTRA_TITLE)?.toString() ?: return
        val text = extras.getCharSequence(Notification.EXTRA_TEXT)?.toString() ?: ""
        val icon = when (sbn.notification.category) {
            Notification.CATEGORY_MESSAGE -> "message"
            Notification.CATEGORY_EMAIL -> "mail"
            Notification.CATEGORY_CALL -> "phone"
            Notification.CATEGORY_EVENT, Notification.CATEGORY_REMINDER -> "calendar"
            Notification.CATEGORY_ALARM, Notification.CATEGORY_ERROR -> "warning"
            else -> "bell"
        }
        val client = BleClient.get(this)
        if (client.link.value == LinkState.Authenticated) {
            // Limits keep the line under the 1023 byte protocol maximum even with Cyrillic text.
            client.send(Protocol.command("notify", "title" to title.take(40), "text" to text.take(150), "icon" to icon,
                "time" to sbn.postTime / 1000))
        }
    }
}
