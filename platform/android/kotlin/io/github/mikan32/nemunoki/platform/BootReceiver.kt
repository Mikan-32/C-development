package io.github.mikan32.nemunoki.platform

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent

/**
 * 再起動・アプリの更新・正確なアラームの許可が戻ったときに、保存してある予約を登録し直す。
 * 端末のロックを一度も解除していない状態（LOCKED_BOOT_COMPLETED）でも動く。
 */
class BootReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        when (intent.action) {
            Intent.ACTION_LOCKED_BOOT_COMPLETED,
            Intent.ACTION_BOOT_COMPLETED,
            Intent.ACTION_MY_PACKAGE_REPLACED,
            ACTION_EXACT_ALARM_PERMISSION_CHANGED,
            -> Scheduler.restoreAll(context)
        }
    }

    private companion object {
        // AlarmManager.ACTION_SCHEDULE_EXACT_ALARM_PERMISSION_STATE_CHANGED（API 31）と同じ値
        const val ACTION_EXACT_ALARM_PERMISSION_CHANGED = "android.app.action.SCHEDULE_EXACT_ALARM_PERMISSION_STATE_CHANGED"
    }
}
