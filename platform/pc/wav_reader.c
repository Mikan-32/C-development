#include "wav_reader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#endif

/*
 * WindowsではUTF-8のファイル名をワイド文字へ変換する。
 */
static nk_result open_wav(const char *path, FILE **out_file) {
#if defined(_WIN32)
    wchar_t *wide_path;
    int length;

    length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);

    if (length <= 0 || length > 32767) {
        return NK_ERROR_ARGUMENT;
    }

    wide_path = malloc((size_t)length * sizeof(*wide_path));

    if (wide_path == NULL) {
        return NK_ERROR_MEMORY;
    }

    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide_path, length) == 0) {
        free(wide_path);
        return NK_ERROR_ARGUMENT;
    }

    *out_file = _wfopen(wide_path, L"rb");

    free(wide_path);
#else
    *out_file = fopen(path, "rb");
#endif

    return *out_file != NULL ? NK_OK : NK_ERROR_IO;
}

typedef struct nk_pcm_source {
    FILE *file;
    int channels;
    uint64_t frame_count;
    uint64_t frames_read;
} nk_pcm_source;

static uint16_t read_u16_le(const unsigned char *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t read_u32_le(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static nk_result read_exact(FILE *file, void *buffer, size_t size) {
    if (fread(buffer, 1, size, file) != size) {
        return ferror(file) ? NK_ERROR_IO : NK_ERROR_FORMAT;
    }

    return NK_OK;
}

static nk_result skip_bytes(FILE *file, uint32_t size) {
    unsigned char buffer[512];

    while (size > 0) {
        size_t amount = size < sizeof(buffer) ? (size_t)size : sizeof(buffer);

        nk_result result = read_exact(file, buffer, amount);

        if (result != NK_OK) {
            return result;
        }

        size -= (uint32_t)amount;
    }

    return NK_OK;
}

static int32_t decode_s16(const unsigned char *p) {
    int32_t value = read_u16_le(p);

    return value >= 32768 ? value - 65536 : value;
}

/*
 * 元の音声を1フレーム読み、モノラルへ変換する。
 */
static nk_result read_mono(nk_pcm_source *source, int16_t *sample) {
    unsigned char bytes[4];
    int32_t value;
    nk_result result;

    if (source->frames_read >= source->frame_count) {
        return NK_ERROR_FORMAT;
    }

    result = read_exact(source->file, bytes, (size_t)source->channels * 2);

    if (result != NK_OK) {
        return result;
    }

    value = decode_s16(bytes);

    if (source->channels == 2) {
        value = (value + decode_s16(bytes + 2)) / 2;
    }

    *sample = (int16_t)value;
    source->frames_read++;

    return NK_OK;
}

/*
 * 線形補間による簡易リサンプリング。
 * この初版には高品質なアンチエイリアスフィルターを含めない。
 */
static nk_result replay_pcm(FILE *file, const nk_wav_info *info, nk_wav_pcm_callback callback,
                            void *user_data) {
    nk_pcm_source source = {file, info->channels, info->frame_count, 0};

    int16_t buffer[256];
    int16_t left;
    int16_t right;
    int used = 0;

    uint64_t source_index = 0;
    uint64_t output_index;
    uint64_t output_count;

    nk_result result;

    output_count =
        (info->frame_count * NK_WAV_OUTPUT_SAMPLE_RATE + (uint64_t)info->sample_rate - 1) /
        (uint64_t)info->sample_rate;

    result = read_mono(&source, &left);

    if (result != NK_OK) {
        return result;
    }

    right = left;

    if (info->frame_count > 1) {
        result = read_mono(&source, &right);

        if (result != NK_OK) {
            return result;
        }
    }

    for (output_index = 0; output_index < output_count; ++output_index) {

        uint64_t position = output_index * (uint64_t)info->sample_rate;

        uint64_t wanted = position / NK_WAV_OUTPUT_SAMPLE_RATE;

        int64_t fraction = (int64_t)(position % NK_WAV_OUTPUT_SAMPLE_RATE);

        int64_t weighted;

        while (source_index < wanted) {
            left = right;
            source_index++;

            if (source_index + 1 < info->frame_count) {
                result = read_mono(&source, &right);

                if (result != NK_OK) {
                    return result;
                }
            } else {
                right = left;
            }
        }

        weighted =
            (int64_t)left * (NK_WAV_OUTPUT_SAMPLE_RATE - fraction) + (int64_t)right * fraction;

        buffer[used++] = (int16_t)(weighted / NK_WAV_OUTPUT_SAMPLE_RATE);

        if (used == 256) {
            result = callback(user_data, buffer, used);

            if (result != NK_OK) {
                return result;
            }

            used = 0;
        }
    }

    /*
     * 間引きで出力に使われなかった末尾も読み取り、
     * ファイルの途中切れを確認する。
     */
    while (source.frames_read < source.frame_count) {
        int16_t unused;

        result = read_mono(&source, &unused);

        if (result != NK_OK) {
            return result;
        }
    }

    if (used > 0) {
        return callback(user_data, buffer, used);
    }

    return NK_OK;
}

nk_result nk_wav_replay_file(const char *path, nk_wav_pcm_callback callback, void *user_data,
                             nk_wav_info *out_info) {
    FILE *file;
    unsigned char header[12];
    nk_wav_info info = {0};

    uint64_t remaining;
    int have_format = 0;
    int have_data = 0;
    int block_align = 0;

    nk_result result;

    if (path == NULL || callback == NULL || out_info == NULL) {
        return NK_ERROR_ARGUMENT;
    }

    *out_info = (nk_wav_info){0};

    result = open_wav(path, &file);

    if (result != NK_OK) {
        return result;
    }

    result = read_exact(file, header, sizeof(header));

    if (result != NK_OK) {
        goto cleanup;
    }

    if (memcmp(header, "RIFF", 4) != 0 || memcmp(header + 8, "WAVE", 4) != 0 ||
        read_u32_le(header + 4) < 4) {
        result = NK_ERROR_FORMAT;
        goto cleanup;
    }

    remaining = (uint64_t)read_u32_le(header + 4) - 4;

    while (remaining > 0) {
        unsigned char chunk[8];
        uint32_t size;
        uint64_t padded_size;

        if (remaining < sizeof(chunk)) {
            result = NK_ERROR_FORMAT;
            goto cleanup;
        }

        result = read_exact(file, chunk, sizeof(chunk));

        if (result != NK_OK) {
            goto cleanup;
        }

        remaining -= sizeof(chunk);

        size = read_u32_le(chunk + 4);
        padded_size = (uint64_t)size + (size & 1U);

        if (padded_size > remaining) {
            result = NK_ERROR_FORMAT;
            goto cleanup;
        }

        if (memcmp(chunk, "fmt ", 4) == 0) {
            unsigned char format[16];

            if (have_format || size < sizeof(format)) {
                result = NK_ERROR_FORMAT;
                goto cleanup;
            }

            result = read_exact(file, format, sizeof(format));

            if (result != NK_OK) {
                goto cleanup;
            }

            info.channels = read_u16_le(format + 2);
            info.sample_rate = (int)read_u32_le(format + 4);
            info.bits_per_sample = read_u16_le(format + 14);
            block_align = read_u16_le(format + 12);

            if (read_u16_le(format) != 1 || (info.channels != 1 && info.channels != 2) ||
                info.sample_rate < 8000 || info.sample_rate > 192000 ||
                info.bits_per_sample != 16 || block_align != info.channels * 2 ||
                read_u32_le(format + 8) != (uint32_t)info.sample_rate * (uint32_t)block_align) {
                result = NK_ERROR_FORMAT;
                goto cleanup;
            }

            result = skip_bytes(file, size - (uint32_t)sizeof(format));

            if (result != NK_OK) {
                goto cleanup;
            }

            have_format = 1;
        } else if (memcmp(chunk, "data", 4) == 0) {
            if (!have_format || have_data || size == 0 || size % (uint32_t)block_align != 0) {
                result = NK_ERROR_FORMAT;
                goto cleanup;
            }

            info.frame_count = size / (uint32_t)block_align;

            result = replay_pcm(file, &info, callback, user_data);

            if (result != NK_OK) {
                goto cleanup;
            }

            have_data = 1;
        } else {
            result = skip_bytes(file, size);

            if (result != NK_OK) {
                goto cleanup;
            }
        }

        if ((size & 1U) != 0U) {
            result = skip_bytes(file, 1);

            if (result != NK_OK) {
                goto cleanup;
            }
        }

        remaining -= padded_size;
    }

    if (!have_data || fgetc(file) != EOF) {
        result = NK_ERROR_FORMAT;
        goto cleanup;
    }

    if (ferror(file)) {
        result = NK_ERROR_IO;
        goto cleanup;
    }

    result = NK_OK;

cleanup:
    if (fclose(file) != 0) {
        fprintf(stderr, "Failed to close WAV file\n");

        if (result == NK_OK) {
            result = NK_ERROR_IO;
        }
    }

    if (result == NK_OK) {
        *out_info = info;
    }

    return result;
}
