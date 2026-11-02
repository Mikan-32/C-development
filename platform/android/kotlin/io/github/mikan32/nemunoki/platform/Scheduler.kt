package io.github.mikan32.nemunoki.platform

import android.app.AlarmManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.os.Build
import android.util.Log
import org.json.JSONObject

/**
 * AlarmManager への予約の登録と取り消し。
 *
 * 予約の内容（時刻・種類・通知の文面）は、再起動後に登録し直せるよう保存しておく。
 * 保存先はデバイス保護ストレージ（端末のロックを一度も解除していない状態でも読める）にする。
 * いつ予約するか・次の予約をどうするかは C が決める。ここは登録するだけ。
 */
object Scheduler {
    private const val TAG = "NkScheduler"
    private const val PREFS = "nk_schedule"
    internal const val ACTION_FIRE = "io.github.mikan32.nemunoki.action.FIRE"
    internal const val EXTRA_ID = "io.github.mikan32.nemunoki.extra.SCHEDULE_ID"
    internal const val EXTRA_KIND = "io.github.mikan32.nemunoki.extra.SCHEDULE_KIND"

    /** 再起動などで時刻を過ぎてしまったアラームを、まだ鳴らしてよい時間。 */
    private const val MISSED_ALARM_GRACE_MS = 10 * 60 * 1000L
    private const val REQUEST_SHOW = 1

    data class Entry(
        val id: Int,
        val atMs: Long,
        val kind: Int,
        val flags: Int,
        val timeoutMs: Int,
        val title: String,
        val body: String,
    )

    fun schedule(context: Context, entry: Entry): Boolean {
        prefs(context).edit().putString(key(entry.id), toJson(entry)).commit()
        return register(context, entry)
    }

    fun cancel(context: Context, id: Int) {
        prefs(context).edit().remove(key(id)).commit()
        val pending = PendingIntent.getBroadcast(
            context, id, fireIntent(context, id, -1),
            PendingIntent.FLAG_NO_CREATE or PendingIntent.FLAG_IMMUTABLE,
        )
        if (pending != null) {
            alarmManager(context).cancel(pending)
            pending.cancel()
        }
    }

    /** 時刻が来た予約を取り出す（1 回きりなので消す）。保存されていなければ null。 */
    fun take(context: Context, id: Int): Entry? {
        val p = prefs(context)
        val entry = p.getString(key(id), null)?.let(::fromJson)
        p.edit().remove(key(id)).commit()
        return entry
    }

    /** 再起動後・アプリの更新後・正確なアラームの許可が戻ったときに、保存してある予約を登録し直す。 */
    fun restoreAll(context: Context) {
        val p = prefs(context)
        val now = System.currentTimeMillis()
        for ((k, v) in p.all) {
            val entry = (v as? String)?.let(::fromJson)
            when {
                entry == null -> p.edit().remove(k).commit()
                entry.atMs > now -> register(context, entry)
                entry.kind == NativeBridge.SCHED_ALARM && now - entry.atMs <= MISSED_ALARM_GRACE_MS ->
                    register(context, entry.copy(atMs = now + 2_000))
                else -> p.edit().remove(k).commit()
            }
        }
    }

    private fun register(context: Context, entry: Entry): Boolean {
        val am = alarmManager(context)
        val operation = PendingIntent.getBroadcast(
            context, entry.id, fireIntent(context, entry.id, entry.kind),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE,
        )
        return try {
            if (entry.kind == NativeBridge.SCHED_ALARM) {
                // 目覚まし時計として登録する（Doze 中でも時刻どおりに鳴り、ステータスバーに時計の印が出る）
                val show = PendingIntent.getActivity(
                    context, REQUEST_SHOW, Intent(context, MainActivity::class.java),
                    PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE,
                )
                am.setAlarmClock(AlarmManager.AlarmClockInfo(entry.atMs, show), operation)
                true
            } else if (Build.VERSION.SDK_INT >= 31 && !am.canScheduleExactAlarms()) {
                // 正確なアラームの許可がない：だいたいの時刻で登録し、false を返して C に知らせる
                am.setAndAllowWhileIdle(AlarmManager.RTC_WAKEUP, entry.atMs, operation)
                false
            } else {
                am.setExactAndAllowWhileIdle(AlarmManager.RTC_WAKEUP, entry.atMs, operation)
                true
            }
        } catch (e: SecurityException) {
            Log.w(TAG, "exact alarm is not permitted (id=${entry.id})", e)
            false
        }
    }

    private fun fireIntent(context: Context, id: Int, kind: Int): Intent =
        Intent(context, ScheduleReceiver::class.java)
            .setAction(ACTION_FIRE)
            .putExtra(EXTRA_ID, id)
            .putExtra(EXTRA_KIND, kind)

    private fun alarmManager(context: Context): AlarmManager = context.getSystemService(AlarmManager::class.java)

    private fun prefs(context: Context): SharedPreferences =
        context.createDeviceProtectedStorageContext().getSharedPreferences(PREFS, Context.MODE_PRIVATE)

    private fun key(id: Int) = "e$id"

    private fun toJson(e: Entry): String = JSONObject()
        .put("id", e.id).put("at", e.atMs).put("kind", e.kind).put("flags", e.flags)
        .put("timeout", e.timeoutMs).put("title", e.title).put("body", e.body)
        .toString()

    private fun fromJson(s: String): Entry? = try {
        val o = JSONObject(s)
        Entry(
            o.getInt("id"), o.getLong("at"), o.getInt("kind"), o.optInt("flags", 0),
            o.optInt("timeout", 0), o.optString("title", ""), o.optString("body", ""),
        )
    } catch (e: Exception) {
        Log.w(TAG, "broken schedule entry", e)
        null
    }
}
