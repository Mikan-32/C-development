package io.github.mikan32.nemunoki.platform

import android.content.Context
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import java.io.File
import java.security.KeyStore
import java.security.SecureRandom
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec

/**
 * DB（SQLCipher）の暗号化キーを守る。
 *
 * 32 バイトの乱数をキーとし、Android Keystore の AES-256-GCM の鍵で包んで、バックアップされないフォルダに保存する。
 * Keystore の鍵は端末の外に出せないので、ファイルだけを持ち出してもキーは取り出せない。
 */
object KeyVault {
    private const val KEYSTORE = "AndroidKeyStore"
    private const val ALIAS = "nk_db_wrap_v1"
    private const val FILE_NAME = "nk_db_key.bin"
    private const val FORMAT_VERSION: Byte = 1
    private const val KEY_BYTES = 32
    private const val GCM_TAG_BITS = 128

    /** キーを返す。初回は作る。包む鍵が失われている（端末の初期化後など）と例外になる。 */
    @Synchronized
    fun getOrCreateDbKey(context: Context): ByteArray {
        val file = File(context.noBackupFilesDir, FILE_NAME)
        if (file.exists()) return unwrap(file.readBytes(), existingWrappingKey() ?: error("wrapping key is missing"))
        val key = ByteArray(KEY_BYTES).also { SecureRandom().nextBytes(it) }
        val wrapped = wrap(key, existingWrappingKey() ?: createWrappingKey())
        val tmp = File(context.noBackupFilesDir, "$FILE_NAME.tmp")
        tmp.writeBytes(wrapped)
        if (!tmp.renameTo(file)) error("cannot save the wrapped key")
        return key
    }

    /** キーを破棄する（DB を作り直すとき）。 */
    @Synchronized
    fun reset(context: Context) {
        File(context.noBackupFilesDir, FILE_NAME).delete()
        KeyStore.getInstance(KEYSTORE).apply { load(null) }.deleteEntry(ALIAS)
    }

    private fun existingWrappingKey(): SecretKey? =
        KeyStore.getInstance(KEYSTORE).apply { load(null) }.getKey(ALIAS, null) as? SecretKey

    private fun createWrappingKey(): SecretKey =
        KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, KEYSTORE).apply {
            init(
                KeyGenParameterSpec.Builder(ALIAS, KeyProperties.PURPOSE_ENCRYPT or KeyProperties.PURPOSE_DECRYPT)
                    .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                    .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                    .setKeySize(256)
                    .build(),
            )
        }.generateKey()

    // 保存形式：[版 1 バイト][IV の長さ 1 バイト][IV][暗号文＋認証タグ]
    private fun wrap(raw: ByteArray, wrappingKey: SecretKey): ByteArray {
        val cipher = Cipher.getInstance("AES/GCM/NoPadding")
        cipher.init(Cipher.ENCRYPT_MODE, wrappingKey)
        val iv = cipher.iv
        return byteArrayOf(FORMAT_VERSION, iv.size.toByte()) + iv + cipher.doFinal(raw)
    }

    private fun unwrap(blob: ByteArray, wrappingKey: SecretKey): ByteArray {
        require(blob.size > 2 && blob[0] == FORMAT_VERSION) { "unknown key file format" }
        val ivLength = blob[1].toInt()
        val iv = blob.copyOfRange(2, 2 + ivLength)
        val cipher = Cipher.getInstance("AES/GCM/NoPadding")
        cipher.init(Cipher.DECRYPT_MODE, wrappingKey, GCMParameterSpec(GCM_TAG_BITS, iv))
        return cipher.doFinal(blob, 2 + ivLength, blob.size - 2 - ivLength)
    }
}
