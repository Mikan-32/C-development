#ifndef NK_ANDROID_BRIDGE_H
#define NK_ANDROID_BRIDGE_H

#include "result.h"

#include <stddef.h>

#define NK_ANDROID_DB_KEY_SIZE 32

/*
 * NativeBridge.kt の NIGHT_FLAG_* と同じ値。
 * 加速度だけを使う場合は flags=0。
 */
typedef enum nk_android_night_flag {
    NK_ANDROID_NIGHT_MIC = 1,
    NK_ANDROID_NIGHT_PLAYBACK = 2
} nk_android_night_flag;

/*
 * Android Keystoreで保護されたDBキーを取得する。
 *
 * out_keyには呼び出し側が用意した32バイトの領域を渡す。
 * key_sizeはNK_ANDROID_DB_KEY_SIZEを指定する。
 *
 * 取得したキーは、DBを開いた後にclear_keyで消去する。
 * キーの内容をログへ出さないこと。
 */
nk_result nk_android_bridge_db_key(unsigned char *out_key, size_t key_size);

/*
 * 呼び出し側が所有するキーの領域を上書きして消去する。
 * メモリの解放は行わない。
 */
void nk_android_bridge_clear_key(void *key, size_t key_size);

/*
 * 夜間サービスの開始を要求する。
 *
 * flagsはnk_android_night_flagの組み合わせ。
 * titleとbodyは常駐通知に表示する文言。
 *
 * セッション側で同意を確認してから呼ぶこと。
 * マイクを使う場合は、実行時権限の確認も先に行うこと。
 *
 * NK_OKは開始要求が受け付けられたことを示す。
 * 実際の開始結果はサービスからの通知で確認する。
 */
nk_result nk_android_bridge_night_start(unsigned int flags, const char *title, const char *body);

/*
 * 夜間サービスの停止を要求する。
 */
nk_result nk_android_bridge_night_stop(void);

/*
 * NativeBridge.kt の NIGHT_* と同じ値。
 */
typedef enum nk_android_night_state {
    NK_ANDROID_NIGHT_STARTED = 0,
    NK_ANDROID_NIGHT_STOPPED = 1,
    NK_ANDROID_NIGHT_FAILED = 2
} nk_android_night_state;

/*
 * Kotlinから最後に通知された状態。
 * 開始・停止要求を送った直後は、以前の状態の場合がある。
 */
nk_android_night_state nk_android_bridge_night_state(void);

/*
 * 通知が許可されていれば*out_allowedへ1、
 * 許可されていなければ0を返す。
 */
nk_result nk_android_bridge_notifications_allowed(int *out_allowed);

#endif
