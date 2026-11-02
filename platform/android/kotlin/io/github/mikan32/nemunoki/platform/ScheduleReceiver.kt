package io.github.mikan32.nemunoki.platform

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent

/**
 * 予約時刻の到来と、通知のボタンを受け取る。
 *
 * アラームのときは、まず鳴動の通知を出す（C の初期化を待たずに、確実に音を鳴らすため）。
 * そのあと、C のヘッドレス入口を作業スレッドで呼ぶ（カーテンを開ける等の判断は C が行う）。
 */
class ScheduleReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        NativeBridge.init(context.applicationContext)
        when (intent.action) {
            Scheduler.ACTION_FIRE -> onFire(context, intent)
            Notifications.ACTION_BUTTON -> onButton(context, intent)
        }
    }

    private fun onFire(context: Context, intent: Intent) {
        val id = intent.getIntExtra(Scheduler.EXTRA_ID, -1)
        val kind = intent.getIntExtra(Scheduler.EXTRA_KIND, -1)
        val entry = Scheduler.take(context, id)
        if (kind == NativeBridge.SCHED_ALARM) {
            val snooze = entry?.let { it.flags and NativeBridge.SCHED_FLAG_SNOOZE != 0 } ?: true
            Notifications.showAlarm(context, id, entry?.title, entry?.body, entry?.timeoutMs ?: 0, snooze)
        }
        runInBackground { NativeBridge.onScheduled(id, kind) }
    }

    private fun onButton(context: Context, intent: Intent) {
        val action = intent.getIntExtra(Notifications.EXTRA_BUTTON, -1)
        val id = intent.getIntExtra(Notifications.EXTRA_ID, -1)
        when (action) {
            // まず音を止める。スヌーズの予約などの判断は C が行う
            NativeBridge.ACT_SNOOZE, NativeBridge.ACT_STOP_ALARM -> NativeBridge.alarmRingingStop(id)
            NativeBridge.ACT_AUTOMODE_SKIP, NativeBridge.ACT_AUTOMODE_OFF ->
                Notifications.cancel(context, Notifications.noticeId(id))
        }
        runInBackground {
            NativeBridge.onNotificationAction(action, id)
            // 計測の停止は C が記録を締めてから行う。C が止めなかった場合に備えて、最後に止めておく
            if (action == NativeBridge.ACT_STOP_TRACKING) NightService.stop(context)
        }
    }

    private fun runInBackground(block: () -> Unit) {
        val pending = goAsync()
        Thread({
            try {
                block()
            } finally {
                pending.finish()
            }
        }, "nk-headless").start()
    }
}
