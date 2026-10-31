package io.github.mikan32.nemunoki.platform

import android.app.AlarmManager
import android.app.NotificationManager
import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.PowerManager
import android.provider.Settings

/**
 * 実行時権限ではない許可（正確なアラーム、全画面通知など）の確認と、その設定画面を開く処理。
 * マイクと通知の実行時権限は、C から SDL3 の SDL_RequestAndroidPermission で求める（ここでは扱わない）。
 */
object SystemSettings {
    fun capability(context: Context, cap: Int): Boolean = when (cap) {
        NativeBridge.CAP_EXACT_ALARM ->
            Build.VERSION.SDK_INT < 31 || context.getSystemService(AlarmManager::class.java).canScheduleExactAlarms()
        NativeBridge.CAP_FULL_SCREEN_INTENT ->
            Build.VERSION.SDK_INT < 34 || context.getSystemService(NotificationManager::class.java).canUseFullScreenIntent()
        NativeBridge.CAP_NOTIFICATIONS -> {
            val nm = context.getSystemService(NotificationManager::class.java)
            nm.areNotificationsEnabled() &&
                nm.getNotificationChannel(Notifications.CH_ALARM)?.importance != NotificationManager.IMPORTANCE_NONE
        }
        NativeBridge.CAP_IGNORING_BATTERY_OPTIMIZATIONS ->
            context.getSystemService(PowerManager::class.java).isIgnoringBatteryOptimizations(context.packageName)
        else -> false
    }

    fun open(context: Context, page: Int): Boolean {
        val packageUri = Uri.parse("package:" + context.packageName)
        val appDetails = Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS, packageUri)
        val intent = when (page) {
            NativeBridge.PAGE_EXACT_ALARM ->
                if (Build.VERSION.SDK_INT >= 31) Intent(Settings.ACTION_REQUEST_SCHEDULE_EXACT_ALARM, packageUri) else appDetails
            NativeBridge.PAGE_FULL_SCREEN_INTENT ->
                if (Build.VERSION.SDK_INT >= 34) Intent(Settings.ACTION_MANAGE_APP_USE_FULL_SCREEN_INTENT, packageUri) else appDetails
            NativeBridge.PAGE_NOTIFICATIONS ->
                Intent(Settings.ACTION_APP_NOTIFICATION_SETTINGS).putExtra(Settings.EXTRA_APP_PACKAGE, context.packageName)
            NativeBridge.PAGE_BATTERY_OPTIMIZATION -> Intent(Settings.ACTION_IGNORE_BATTERY_OPTIMIZATION_SETTINGS)
            else -> appDetails
        }
        return start(context, intent) || start(context, appDetails)
    }

    private fun start(context: Context, intent: Intent): Boolean = try {
        context.startActivity(intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
        true
    } catch (e: ActivityNotFoundException) {
        false
    }
}
