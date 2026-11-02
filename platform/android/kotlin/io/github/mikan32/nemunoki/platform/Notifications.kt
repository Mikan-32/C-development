package io.github.mikan32.nemunoki.platform

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.graphics.drawable.Icon
import android.media.AudioAttributes
import android.media.RingtoneManager
import android.net.Uri
import android.os.Build
import io.github.mikan32.nemunoki.R

/**
 * 通知チャネルと通知の組み立て。
 *
 * - 通知のタイトルと本文は C から受け取る（ADR-0004）。
 * - チャネル名とボタンの文字は Android だけの固定の文言なので、res/values/strings.xml に置く。
 * - アラーム音は、アラーム用チャネルの音として鳴らす（アプリが終了していても鳴るように。platform-boundary.md の 4 章）。
 */
object Notifications {
    const val CH_TRACKING = "nk_tracking"
    const val CH_ALARM = "nk_alarm"
    const val CH_NOTICE = "nk_notice"
    const val CH_REMINDER = "nk_reminder"

    internal const val ACTION_BUTTON = "io.github.mikan32.nemunoki.action.BUTTON"
    internal const val EXTRA_BUTTON = "io.github.mikan32.nemunoki.extra.BUTTON"
    internal const val EXTRA_ID = "io.github.mikan32.nemunoki.extra.NOTICE_ID"

    const val ID_TRACKING = 1
    fun alarmId(scheduleId: Int) = 100_000 + scheduleId
    fun noticeId(id: Int) = 200_000 + id

    /** C から鳴動の時間が渡されなかったときの上限（ADR-0010：10 分で止める）。 */
    private const val DEFAULT_ALARM_TIMEOUT_MS = 10 * 60 * 1000

    fun ensureChannels(context: Context) {
        val nm = context.getSystemService(NotificationManager::class.java)
        val alarmSound = RingtoneManager.getDefaultUri(RingtoneManager.TYPE_ALARM)
            ?: RingtoneManager.getDefaultUri(RingtoneManager.TYPE_RINGTONE)
        val alarm = NotificationChannel(CH_ALARM, context.getString(R.string.nk_channel_alarm), NotificationManager.IMPORTANCE_HIGH).apply {
            setSound(
                alarmSound,
                AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_ALARM)
                    .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
                    .build(),
            )
            enableVibration(true)
            lockscreenVisibility = Notification.VISIBILITY_PUBLIC
        }
        val tracking = NotificationChannel(CH_TRACKING, context.getString(R.string.nk_channel_tracking), NotificationManager.IMPORTANCE_LOW).apply {
            setShowBadge(false)
        }
        val notice = NotificationChannel(CH_NOTICE, context.getString(R.string.nk_channel_notice), NotificationManager.IMPORTANCE_DEFAULT)
        val reminder = NotificationChannel(CH_REMINDER, context.getString(R.string.nk_channel_reminder), NotificationManager.IMPORTANCE_DEFAULT)
        nm.createNotificationChannels(listOf(alarm, tracking, notice, reminder))
    }

    /** 鳴動の通知。ロック中は全画面で MainActivity（LVGL の鳴動画面）を開き、音は止めるまで繰り返す。 */
    fun showAlarm(context: Context, scheduleId: Int, title: String?, body: String?, timeoutMs: Int, snoozeAllowed: Boolean) {
        ensureChannels(context)
        val nid = alarmId(scheduleId)
        val open = launchIntent(context, NativeBridge.LAUNCH_ALARM, scheduleId)
        val builder = Notification.Builder(context, CH_ALARM)
            .setSmallIcon(R.drawable.nk_ic_stat)
            .setContentTitle(title?.takeIf { it.isNotEmpty() } ?: appLabel(context))
            .setContentText(body ?: "")
            .setCategory(Notification.CATEGORY_ALARM)
            .setVisibility(Notification.VISIBILITY_PUBLIC)
            .setOngoing(true)
            .setAutoCancel(false)
            .setContentIntent(open)
            .setFullScreenIntent(open, true)
            .setTimeoutAfter((if (timeoutMs > 0) timeoutMs else DEFAULT_ALARM_TIMEOUT_MS).toLong())
        if (snoozeAllowed) {
            builder.addAction(action(context, R.string.nk_action_snooze, NativeBridge.ACT_SNOOZE, scheduleId))
        }
        builder.addAction(action(context, R.string.nk_action_stop_alarm, NativeBridge.ACT_STOP_ALARM, scheduleId))
        val notification = builder.build()
        notification.flags = notification.flags or Notification.FLAG_INSISTENT
        context.getSystemService(NotificationManager::class.java).notify(nid, notification)
    }

    /** お知らせ（カーテン・自動モード・おやすみの通知）。 */
    fun showNotice(context: Context, kind: Int, id: Int, title: String, body: String) {
        ensureChannels(context)
        val channel = if (kind == NativeBridge.NOTICE_REMINDER) CH_REMINDER else CH_NOTICE
        val builder = Notification.Builder(context, channel)
            .setSmallIcon(R.drawable.nk_ic_stat)
            .setContentTitle(title)
            .setContentText(body)
            .setStyle(Notification.BigTextStyle().bigText(body))
            .setAutoCancel(true)
            .setContentIntent(launchIntent(context, NativeBridge.LAUNCH_NOTICE, id))
        if (kind == NativeBridge.NOTICE_AUTOMODE) {
            builder.addAction(action(context, R.string.nk_action_automode_skip, NativeBridge.ACT_AUTOMODE_SKIP, id))
            builder.addAction(action(context, R.string.nk_action_automode_off, NativeBridge.ACT_AUTOMODE_OFF, id))
        }
        context.getSystemService(NotificationManager::class.java).notify(noticeId(id), builder.build())
    }

    /** 夜間サービスの常駐通知（「計測を止める」ボタン付き）。 */
    fun buildTracking(context: Context, title: String, body: String): Notification {
        ensureChannels(context)
        val builder = Notification.Builder(context, CH_TRACKING)
            .setSmallIcon(R.drawable.nk_ic_stat)
            .setContentTitle(title.ifEmpty { appLabel(context) })
            .setContentText(body)
            .setCategory(Notification.CATEGORY_SERVICE)
            .setOngoing(true)
            .setContentIntent(launchIntent(context, NativeBridge.LAUNCH_TRACKING, 0))
            .addAction(action(context, R.string.nk_action_stop_tracking, NativeBridge.ACT_STOP_TRACKING, 0))
        if (Build.VERSION.SDK_INT >= 31) {
            builder.setForegroundServiceBehavior(Notification.FOREGROUND_SERVICE_IMMEDIATE)
        }
        return builder.build()
    }

    fun cancel(context: Context, notificationId: Int) {
        context.getSystemService(NotificationManager::class.java).cancel(notificationId)
    }

    private fun action(context: Context, labelRes: Int, button: Int, id: Int): Notification.Action {
        // data を変えて、ボタンごとに別の PendingIntent になるようにする（extras だけの違いは同じものとみなされるため）
        val intent = Intent(context, ScheduleReceiver::class.java)
            .setAction(ACTION_BUTTON)
            .setData(Uri.parse("nemunoki://button/$button/$id"))
            .putExtra(EXTRA_BUTTON, button)
            .putExtra(EXTRA_ID, id)
        val pending = PendingIntent.getBroadcast(
            context, 0, intent, PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE,
        )
        return Notification.Action.Builder(
            Icon.createWithResource(context, R.drawable.nk_ic_stat), context.getString(labelRes), pending,
        ).build()
    }

    private fun launchIntent(context: Context, reason: Int, id: Int): PendingIntent {
        val intent = Intent(context, MainActivity::class.java)
            .setData(Uri.parse("nemunoki://launch/$reason/$id"))
            .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            .putExtra(MainActivity.EXTRA_REASON, reason)
            .putExtra(MainActivity.EXTRA_ID, id)
        return PendingIntent.getActivity(context, 0, intent, PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE)
    }

    private fun appLabel(context: Context): String = context.applicationInfo.loadLabel(context.packageManager).toString()
}
