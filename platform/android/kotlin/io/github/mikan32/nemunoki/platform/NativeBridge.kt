package io.github.mikan32.nemunoki.platform

import android.content.Context
import android.util.Log
import java.lang.ref.WeakReference

/**
 * C と Kotlin の窓口（JNI）。
 *
 * - `native…` で始まる関数は C 側（platform/android/c/）が実装する。
 * - `@JvmStatic` の関数は、C から JNI の CallStatic…Method で呼ぶ。どのスレッドから呼んでもよい。
 * - 定数の値は、C 側の nk_platform.h の列挙と同じにする（platform/android/README.md の表）。
 * - 判断（いつ鳴らすか、スヌーズできるか等）はここに書かない。判断は C が行う（ADR-0001・0004）。
 */
object NativeBridge {
    private const val TAG = "NkNativeBridge"

    /** 読み込む共有ライブラリ。MainActivity（SDL）とヘッドレス起動の両方で使う。 */
    @JvmField
    val LIBRARIES = arrayOf("SDL3", "main")

    // ---- 予約の種類（schedule の kind） ----
    const val SCHED_ALARM = 0
    const val SCHED_CURTAIN_OPEN = 1
    const val SCHED_CURTAIN_CLOSE = 2
    const val SCHED_REMINDER = 3

    /** 予約のフラグ：鳴動の通知に「スヌーズ」を出す。 */
    const val SCHED_FLAG_SNOOZE = 1

    // ---- 能力（capability） ----
    const val CAP_EXACT_ALARM = 0
    const val CAP_FULL_SCREEN_INTENT = 1
    const val CAP_NOTIFICATIONS = 2
    const val CAP_IGNORING_BATTERY_OPTIMIZATIONS = 3

    // ---- 設定画面（openSettings） ----
    const val PAGE_APP_DETAILS = 0
    const val PAGE_EXACT_ALARM = 1
    const val PAGE_FULL_SCREEN_INTENT = 2
    const val PAGE_NOTIFICATIONS = 3
    const val PAGE_BATTERY_OPTIMIZATION = 4

    // ---- 夜間サービス ----
    const val NIGHT_FLAG_MIC = 1
    const val NIGHT_FLAG_PLAYBACK = 2
    const val NIGHT_STARTED = 0
    const val NIGHT_STOPPED = 1
    const val NIGHT_FAILED = 2

    // ---- 起動理由（nativeOnLaunch） ----
    const val LAUNCH_NORMAL = 0
    const val LAUNCH_ALARM = 1
    const val LAUNCH_NOTICE = 2
    const val LAUNCH_TRACKING = 3

    // ---- 通知のボタン（nativeOnNotificationAction） ----
    const val ACT_SNOOZE = 0
    const val ACT_STOP_ALARM = 1
    const val ACT_STOP_TRACKING = 2
    const val ACT_AUTOMODE_SKIP = 3
    const val ACT_AUTOMODE_OFF = 4

    // ---- お知らせの種類（postNotice） ----
    const val NOTICE_INFO = 0
    const val NOTICE_AUTOMODE = 1
    const val NOTICE_REMINDER = 2

    /** ヘッドレス入口に渡す処理時間の上限（platform-boundary.md の 5 章）。 */
    const val HEADLESS_TIMEOUT_MS = 20_000

    @Volatile
    private var appContext: Context? = null

    @Volatile
    private var loaded = false

    @Volatile
    private var activityRef: WeakReference<MainActivity>? = null

    /**
     * 最初に呼ぶ。MainActivity・受信クラス・サービスの入口で呼ぶ。
     * @param loadNative MainActivity では SDL が読み込むので false にする。
     */
    fun init(context: Context, loadNative: Boolean = true) {
        if (appContext == null) appContext = context.applicationContext
        if (loadNative) ensureLoaded()
    }

    internal fun attachActivity(activity: MainActivity) {
        activityRef = WeakReference(activity)
    }

    internal fun detachActivity(activity: MainActivity) {
        if (activityRef?.get() === activity) activityRef = null
    }

    private fun context(): Context = appContext ?: throw IllegalStateException("NativeBridge.init() has not been called")

    private fun ensureLoaded(): Boolean {
        if (loaded) return true
        synchronized(this) {
            if (!loaded) {
                try {
                    for (lib in LIBRARIES) System.loadLibrary(lib)
                    loaded = true
                } catch (e: UnsatisfiedLinkError) {
                    Log.e(TAG, "failed to load native libraries", e)
                }
            }
            return loaded
        }
    }

    // =====================================================================
    // Kotlin → C（C 側で実装する）
    // =====================================================================

    /** 予約時刻の到来。作業スレッドから呼ぶ。C は timeoutMs 以内に戻る。 */
    @JvmStatic
    external fun nativeOnScheduled(id: Int, kind: Int, timeoutMs: Int)

    /** アプリが開かれた理由。UI スレッドから呼ぶ。C はすぐ戻る。 */
    @JvmStatic
    external fun nativeOnLaunch(reason: Int, id: Int)

    /** 通知のボタンが押された。作業スレッドから呼ぶ。C は timeoutMs 以内に戻る。 */
    @JvmStatic
    external fun nativeOnNotificationAction(action: Int, id: Int, timeoutMs: Int)

    /** 夜間サービスの状態の変化。UI スレッドから呼ぶ。C はすぐ戻る。 */
    @JvmStatic
    external fun nativeOnNightService(state: Int)

    internal fun onScheduled(id: Int, kind: Int) = callNative("nativeOnScheduled") { nativeOnScheduled(id, kind, HEADLESS_TIMEOUT_MS) }

    internal fun onLaunch(reason: Int, id: Int) = callNative("nativeOnLaunch") { nativeOnLaunch(reason, id) }

    internal fun onNotificationAction(action: Int, id: Int) =
        callNative("nativeOnNotificationAction") { nativeOnNotificationAction(action, id, HEADLESS_TIMEOUT_MS) }

    internal fun onNightService(state: Int) = callNative("nativeOnNightService") { nativeOnNightService(state) }

    private inline fun callNative(name: String, block: () -> Unit) {
        if (!ensureLoaded()) {
            Log.w(TAG, "native library is not available; skipped $name")
            return
        }
        try {
            block()
        } catch (e: UnsatisfiedLinkError) {
            Log.e(TAG, "$name is not implemented in C", e)
        }
    }

    // =====================================================================
    // C → Kotlin（C から呼ぶ。例外は C に投げず、戻り値で知らせる）
    // =====================================================================

    @JvmStatic
    fun capability(cap: Int): Boolean = guard("capability", false) { SystemSettings.capability(context(), cap) }

    @JvmStatic
    fun openSettings(page: Int): Boolean = guard("openSettings", false) { SystemSettings.open(context(), page) }

    /** 夜間サービスを始める。title・body は常駐通知の文面（C の文字列表から渡す）。 */
    @JvmStatic
    fun nightServiceStart(flags: Int, title: String, body: String): Boolean =
        guard("nightServiceStart", false) { NightService.start(context(), flags, title, body) }

    @JvmStatic
    fun nightServiceStop() {
        guard("nightServiceStop", Unit) { NightService.stop(context()) }
    }

    /**
     * 予約する。同じ id で呼ぶと上書きする。
     * @return 正確な時刻で登録できたら true。許可がない等で登録できない・不正確になる場合は false。
     */
    @JvmStatic
    fun schedule(
        id: Int,
        atMs: Long,
        kind: Int,
        flags: Int,
        timeoutMs: Int,
        title: String,
        body: String,
    ): Boolean = guard("schedule", false) {
        Scheduler.schedule(context(), Scheduler.Entry(id, atMs, kind, flags, timeoutMs, title, body))
    }

    @JvmStatic
    fun cancel(id: Int) {
        guard("cancel", Unit) { Scheduler.cancel(context(), id) }
    }

    /** 鳴動を止める（通知を消して音を止め、ロック画面の上の表示をやめる）。 */
    @JvmStatic
    fun alarmRingingStop(id: Int) {
        guard("alarmRingingStop", Unit) {
            Notifications.cancel(context(), Notifications.alarmId(id))
            val activity = activityRef?.get()
            activity?.runOnUiThread { activity.setShowOverLockScreen(false) }
        }
    }

    /** お知らせの通知（カーテンの失敗、自動モードの一時停止、おやすみの通知など）。 */
    @JvmStatic
    fun postNotice(kind: Int, id: Int, title: String, body: String) {
        guard("postNotice", Unit) { Notifications.showNotice(context(), kind, id, title, body) }
    }

    /** DB の暗号化キー（32 バイト）。取り出せない場合は null。 */
    @JvmStatic
    fun dbKey(): ByteArray? = guard<ByteArray?>("dbKey", null) { KeyVault.getOrCreateDbKey(context()) }

    /** DB を作り直すときに、キーを破棄する。 */
    @JvmStatic
    fun dbKeyReset() {
        guard("dbKeyReset", Unit) { KeyVault.reset(context()) }
    }

    private inline fun <T> guard(name: String, fallback: T, block: () -> T): T =
        try {
            block()
        } catch (e: Exception) {
            Log.e(TAG, "$name failed", e)
            fallback
        }
}
