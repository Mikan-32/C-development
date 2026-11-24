#include "android_saved_report.h"

#include <limits.h>
#include <sqlcipher/sqlite3.h>
#include <stdio.h>
#include <string.h>

nk_result nk_android_saved_report_read(const char *path, const void *key, size_t key_size,
                                       nk_android_saved_report *out_report) {
    static const char query[] =
        "SELECT s.id,s.start_ms,s.end_ms,s.end_reason,s.battery_start,s.battery_end,"
        "(SELECT COUNT(*) FROM epoch WHERE session_id=s.id),"
        "(SELECT COUNT(*) FROM epoch WHERE session_id=s.id AND acc_cov>=0.8),"
        "(SELECT COUNT(*) FROM epoch WHERE session_id=s.id AND (flags&16)!=0),"
        "(SELECT MIN(idx) FROM epoch WHERE session_id=s.id),"
        "(SELECT MAX(idx) FROM epoch WHERE session_id=s.id) "
        "FROM session AS s ORDER BY s.id DESC LIMIT 1;";
    sqlite3 *database = NULL;
    sqlite3_stmt *statement = NULL;
    nk_android_saved_report report = {0};
    nk_result result = NK_ERROR_DATABASE;
    if (out_report != NULL) {
        *out_report = report;
    }
    if (path == NULL || path[0] == '\0' || key == NULL || key_size == 0 || key_size > INT_MAX ||
        out_report == NULL) {
        return NK_ERROR_ARGUMENT;
    }
    if (sqlite3_open_v2(path, &database, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK ||
        sqlite3_db_readonly(database, "main") != 1 ||
        sqlite3_busy_timeout(database, 5000) != SQLITE_OK ||
        sqlite3_key(database, key, (int)key_size) != SQLITE_OK ||
        sqlite3_prepare_v2(database, query, -1, &statement, NULL) != SQLITE_OK) {
        goto cleanup;
    }
    int step = sqlite3_step(statement);
    if (step == SQLITE_DONE) {
        result = NK_OK;
        goto cleanup;
    }
    if (step != SQLITE_ROW) {
        goto cleanup;
    }
    report.found = 1;
    report.session_id = sqlite3_column_int64(statement, 0);
    report.start_ms = sqlite3_column_int64(statement, 1);
    report.finished = sqlite3_column_type(statement, 2) != SQLITE_NULL;
    report.end_ms = report.finished ? sqlite3_column_int64(statement, 2) : 0;
    const unsigned char *reason = sqlite3_column_text(statement, 3);
    int reason_length = snprintf(report.end_reason, sizeof(report.end_reason), "%s",
                                 reason != NULL ? (const char *)reason : "UNFINISHED");
    report.battery_start =
        sqlite3_column_type(statement, 4) == SQLITE_NULL ? -1 : sqlite3_column_int(statement, 4);
    report.battery_end =
        sqlite3_column_type(statement, 5) == SQLITE_NULL ? -1 : sqlite3_column_int(statement, 5);
    report.epochs = sqlite3_column_int64(statement, 6);
    report.good_acc_epochs = sqlite3_column_int64(statement, 7);
    report.charging_epochs = sqlite3_column_int64(statement, 8);
    if (report.session_id <= 0 || report.start_ms < 0 || reason_length < 0 ||
        (size_t)reason_length >= sizeof(report.end_reason) || report.battery_start < -1 ||
        report.battery_start > 100 || report.battery_end < -1 || report.battery_end > 100 ||
        (report.finished && report.end_ms < report.start_ms)) {
        result = NK_ERROR_FORMAT;
        goto cleanup;
    }
    if (report.finished) {
        int64_t duration = report.end_ms - report.start_ms;
        report.expected_epochs = duration / 30000 + (duration % 30000 != 0);
        if (report.epochs > report.expected_epochs ||
            (report.epochs > 0 &&
             (sqlite3_column_int64(statement, 9) < 0 ||
              sqlite3_column_int64(statement, 10) >= report.expected_epochs))) {
            result = NK_ERROR_FORMAT;
            goto cleanup;
        }
    }
    result = NK_OK;
cleanup:
    if (sqlite3_finalize(statement) != SQLITE_OK) {
        result = NK_ERROR_DATABASE;
    }
    if (database != NULL && sqlite3_close(database) != SQLITE_OK) {
        result = NK_ERROR_DATABASE;
    }
    if (result == NK_OK) {
        *out_report = report;
    }
    return result;
}

nk_result nk_android_saved_report_format(const nk_android_saved_report *report, char *text,
                                         size_t text_size) {
    if (report == NULL || text == NULL || text_size == 0) {
        return NK_ERROR_ARGUMENT;
    }
    int length;
    if (!report->found) {
        length = snprintf(text, text_size, "TM02 stored: no sessions found.");
    } else if (!report->finished) {
        length = snprintf(text, text_size,
                          "TM02 stored: session=%lld finished=0 start_ms=%lld epochs=%lld\n"
                          "No end time is stored. This report did not recover or finish it.",
                          (long long)report->session_id, (long long)report->start_ms,
                          (long long)report->epochs);
    } else {
        char quality[32] = "unknown";
        char battery_start[16] = "unknown";
        char battery_end[16] = "unknown";
        char drop[16] = "unknown";
        if (report->expected_epochs > 0) {
            (void)snprintf(quality, sizeof(quality), "%.3f",
                           100.0 * (double)report->good_acc_epochs /
                               (double)report->expected_epochs);
        }
        if (report->battery_start >= 0) {
            (void)snprintf(battery_start, sizeof(battery_start), "%d", report->battery_start);
        }
        if (report->battery_end >= 0) {
            (void)snprintf(battery_end, sizeof(battery_end), "%d", report->battery_end);
        }
        if (report->battery_start >= 0 && report->battery_end >= 0) {
            (void)snprintf(drop, sizeof(drop), "%d", report->battery_start - report->battery_end);
        }
        length =
            snprintf(text, text_size,
                     "TM02 stored: session=%lld finished=1 end_reason=%s\n"
                     "start_ms=%lld end_ms=%lld duration_ms=%lld\n"
                     "expected_epochs=%lld epochs=%lld good_acc_epochs=%lld\n"
                     "good_acc_pct=%s missing_epochs=%lld charging_epochs=%lld\n"
                     "start_pct=%s end_pct=%s drop_pp=%s\n"
                     "Read-only report; no recovery or measurement was performed.",
                     (long long)report->session_id, report->end_reason, (long long)report->start_ms,
                     (long long)report->end_ms, (long long)(report->end_ms - report->start_ms),
                     (long long)report->expected_epochs, (long long)report->epochs,
                     (long long)report->good_acc_epochs, quality,
                     (long long)(report->expected_epochs - report->epochs),
                     (long long)report->charging_epochs, battery_start, battery_end, drop);
    }
    return length >= 0 && (size_t)length < text_size ? NK_OK : NK_ERROR_ARGUMENT;
}
