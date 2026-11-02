package io.github.mikan32.nemunoki.platform

import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.view.WindowManager
import org.libsdl.app.SDLActivity

/**
 * アプリの画面（SDL3＋LVGL）。画面そのものは C が描く。
 * ここでは、起動理由を C に伝えることと、アラームのときにロック画面の上に表示することだけを行う。
 */
class MainActivity : SDLActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        // ライブラリの読み込みは SDL（super.onCreate）に任せる
        NativeBridge.init(applicationContext, loadNative = false)
        super.onCreate(savedInstanceState)
        NativeBridge.attachActivity(this)
        handleLaunch(intent)
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        handleLaunch(intent)
    }

    override fun onDestroy() {
        NativeBridge.detachActivity(this)
        super.onDestroy()
    }

    override fun getLibraries(): Array<String> = NativeBridge.LIBRARIES

    private fun handleLaunch(intent: Intent?) {
        if (intent == null) return
        val reason = intent.getIntExtra(EXTRA_REASON, NativeBridge.LAUNCH_NORMAL)
        val id = intent.getIntExtra(EXTRA_ID, -1)
        setShowOverLockScreen(reason == NativeBridge.LAUNCH_ALARM)
        NativeBridge.onLaunch(reason, id)
        // 画面の回転などで同じ理由を 2 回伝えないように、処理済みの値を消す
        intent.removeExtra(EXTRA_REASON)
        intent.removeExtra(EXTRA_ID)
    }

    internal fun setShowOverLockScreen(on: Boolean) {
        if (Build.VERSION.SDK_INT >= 27) {
            setShowWhenLocked(on)
            setTurnScreenOn(on)
        } else {
            @Suppress("DEPRECATION")
            val flags = WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED or WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON
            if (on) window.addFlags(flags) else window.clearFlags(flags)
        }
    }

    companion object {
        internal const val EXTRA_REASON = "io.github.mikan32.nemunoki.extra.REASON"
        internal const val EXTRA_ID = "io.github.mikan32.nemunoki.extra.ID"
    }
}
