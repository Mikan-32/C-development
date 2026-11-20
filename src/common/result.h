#ifndef NK_COMMON_RESULT_H
#define NK_COMMON_RESULT_H

/*
 * コンポーネント共通の処理結果。
 *
 * 成功はNK_OK。
 * 呼び出し側は失敗時の値を確認して処理する。
 */
typedef enum nk_result {
    NK_OK = 0,

    /* 引数が不正。 */
    NK_ERROR_ARGUMENT = -1,

    /* メモリを確保できなかった。 */
    NK_ERROR_MEMORY = -2,

    /* DBの読み書きに失敗した。 */
    NK_ERROR_DATABASE = -3,

    /* 必要な同意がない。 */
    NK_ERROR_CONSENT_REQUIRED = -4,

    /* すでに計測中。 */
    NK_ERROR_ALREADY_RUNNING = -5,

    /* 計測が開始されていない。 */
    NK_ERROR_NOT_RUNNING = -6,
    NK_ERROR_IO = -7,
    NK_ERROR_FORMAT = -8,
    NK_ERROR_PERMISSION_REQUIRED = -9
} nk_result;

#endif
