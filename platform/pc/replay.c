#include "replay.h"

#include <stdio.h>
#include <string.h>

static uint32_t read_u32_le(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

nk_result nk_replay_file(const char *path, const nk_sensing_config *config,
                         const nk_sensing_callbacks *callbacks, void *user_data) {
    FILE *file;
    unsigned char header[8];
    unsigned char record[4];
    uint32_t count;
    uint32_t i;
    nk_sensing *sensing = NULL;
    nk_result result = NK_ERROR_FORMAT;
    int close_result;

    if (path == NULL || config == NULL || callbacks == NULL) {
        return NK_ERROR_ARGUMENT;
    }

    file = fopen(path, "rb");

    if (file == NULL) {
        fprintf(stderr, "Cannot open replay log: %s\n", path);
        return NK_ERROR_IO;
    }

    if (fread(header, 1, sizeof(header), file) != sizeof(header)) {
        result = ferror(file) ? NK_ERROR_IO : NK_ERROR_FORMAT;
        goto cleanup;
    }

    count = read_u32_le(header + 4);

    if (memcmp(header, "NKS1", 4) != 0 || count == 0 ||
        count > NK_SENSING_MAX_EPOCHS * NK_SENSING_EPOCH_SECONDS) {
        goto cleanup;
    }

    result = nk_sensing_create(&sensing, config, callbacks, user_data);

    if (result != NK_OK) {
        goto cleanup;
    }

    for (i = 0; i < count; ++i) {
        nk_sensing_second second = {0};

        if (fread(record, 1, sizeof(record), file) != sizeof(record)) {
            result = ferror(file) ? NK_ERROR_IO : NK_ERROR_FORMAT;
            goto cleanup;
        }

        if ((record[0] > 100 && record[0] != 255) || (record[1] > 128 && record[1] != 255) ||
            record[2] > 7 || record[3] != 0) {
            result = NK_ERROR_FORMAT;
            goto cleanup;
        }

        if ((record[2] & 4U) != 0U && (record[0] != 255 || record[1] != 255)) {
            result = NK_ERROR_FORMAT;
            goto cleanup;
        }

        second.idx = (int)i;
        second.acc_sample_count = NK_SENSING_ACC_HZ;
        second.acc_hp.has_value = 1;
        second.acc_hp.value = 1.0;

        second.sound_on = (record[2] & 1U) != 0U;
        second.mic_busy = (record[2] & 2U) != 0U;

        if ((record[2] & 4U) == 0U) {
            second.mic_sample_count = NK_SENSING_MIC_HZ;

            if (record[0] != 255) {
                second.snore_like.has_value = 1;
                second.snore_like.value = record[0] / 100.0;
            }

            if (record[1] != 255) {
                second.snd_db.has_value = 1;
                second.snd_db.value = (int)record[1] - 128;
            }
        }

        result = nk_sensing_push_second(sensing, &second);

        if (result != NK_OK) {
            goto cleanup;
        }
    }

    /*
     * 宣言件数を超える余分なデータも拒否する。
     */
    if (fgetc(file) != EOF) {
        result = NK_ERROR_FORMAT;
        goto cleanup;
    }

    if (ferror(file)) {
        result = NK_ERROR_IO;
        goto cleanup;
    }

    close_result = fclose(file);
    file = NULL;

    if (close_result != 0) {
        result = NK_ERROR_IO;
        goto cleanup;
    }

    result = nk_sensing_finish(sensing);

cleanup:
    nk_sensing_destroy(sensing);

    if (file != NULL && fclose(file) != 0) {
        fprintf(stderr, "Failed to close replay log: %s\n", path);

        if (result == NK_OK) {
            result = NK_ERROR_IO;
        }
    }

    return result;
}
