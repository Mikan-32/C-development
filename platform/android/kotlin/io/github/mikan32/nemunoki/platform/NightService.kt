package io.github.mikan32.nemunoki.platform

import android.Manifest
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.IBinder
import android.os.PowerManager
import android.util.Log

/**
 * 夜間の常駐（フォアグラウンドサービス）。
 *
 * 計測そのものは C のスレッド（src/sensing/）が行う。このサービスは、画面が消えてもプロセスと CPU を
 * 動かし続けるための「器」で、状態の変化を C に知らせるだけ。
 *
 * サービスの種類：
 * - health：加速度での計測（Android 14 以降。HIGH_SAMPLING_RATE_SENSORS を宣言していれば使える）
 * - microphone：マイクを使う場合（マイクの許可があるときだけ付ける）
 * - mediaPlayback：入眠サウンドを鳴らす場合
 */
class NightService : Service() {
    private var wakeLock: PowerManager.WakeLock? = null
    private var running = false

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        NativeBridge.init(applicationContext)
        if (intent?.action == ACTION_START) startNight(intent) else stopSelf()
        // OS に止められても自動では再開しない（マイクは裏から再開できないため）。欠損は C が記録する
        return START_NOT_STICKY
    }

    private fun startNight(intent: Intent) {
        val requested = intent.getIntExtra(EXTRA_FLAGS, 0)
        val notification = Notifications.buildTracking(
            this, intent.getStringExtra(EXTRA_TITLE) ?: "", intent.getStringExtra(EXTRA_BODY) ?: "",
        )
        try {
            val types = serviceTypes(requested)
            if (Build.VERSION.SDK_INT >= 29 && types != 0) {
                startForeground(Notifications.ID_TRACKING, notification, types)
            } else {
                startForeground(Notifications.ID_TRACKING, notification)
            }
        } catch (e: RuntimeException) {
            // SecurityException（許可の不足）や、裏から始めようとしたときの例外
            Log.e(TAG, "startForeground failed", e)
            NativeBridge.onNightService(NativeBridge.NIGHT_FAILED)
            stopSelf()
            return
        }
        if (wakeLock == null) {
            wakeLock = getSystemService(PowerManager::class.java)
                .newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "nemunoki:night")
                .apply {
                    setReferenceCounted(false)
                    acquire(MAX_NIGHT_MS)
                }
        }
        if (!running) {
            running = true
            NativeBridge.onNightService(NativeBridge.NIGHT_STARTED)
        }
    }

    private fun serviceTypes(requested: Int): Int {
        var types = 0
        if (Build.VERSION.SDK_INT >= 34) {
            types = types or ServiceInfo.FOREGROUND_SERVICE_TYPE_HEALTH
        }
        if (Build.VERSION.SDK_INT >= 30 && requested and NativeBridge.NIGHT_FLAG_MIC != 0 &&
            checkSelfPermission(Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED
        ) {
            types = types or ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE
        }
        if (Build.VERSION.SDK_INT >= 29 && requested and NativeBridge.NIGHT_FLAG_PLAYBACK != 0) {
            types = types or ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK
        }
        return types
    }

    override fun onDestroy() {
        wakeLock?.let { if (it.isHeld) it.release() }
        wakeLock = null
        if (running) {
            running = false
            NativeBridge.onNightService(NativeBridge.NIGHT_STOPPED)
        }
        super.onDestroy()
    }

    companion object {
        private const val TAG = "NkNightService"
        private const val ACTION_START = "io.github.mikan32.nemunoki.action.NIGHT_START"
        private const val EXTRA_FLAGS = "io.github.mikan32.nemunoki.extra.NIGHT_FLAGS"
        private const val EXTRA_TITLE = "io.github.mikan32.nemunoki.extra.NIGHT_TITLE"
        private const val EXTRA_BODY = "io.github.mikan32.nemunoki.extra.NIGHT_BODY"

        /** 計測の最長時間（AI-1：14 時間で打ち切る）。WakeLock もこれで自動的に外れる。 */
        private const val MAX_NIGHT_MS = 14 * 60 * 60 * 1000L

        /** 画面の操作（ユーザーが「計測をはじめる」を押した直後）から呼ぶ。裏からは始められない。 */
        fun start(context: Context, flags: Int, title: String, body: String): Boolean = try {
            context.startForegroundService(
                Intent(context, NightService::class.java)
                    .setAction(ACTION_START)
                    .putExtra(EXTRA_FLAGS, flags)
                    .putExtra(EXTRA_TITLE, title)
                    .putExtra(EXTRA_BODY, body),
            )
            true
        } catch (e: RuntimeException) {
            Log.e(TAG, "cannot start NightService", e)
            false
        }

        fun stop(context: Context) {
            context.stopService(Intent(context, NightService::class.java))
        }
    }
}
