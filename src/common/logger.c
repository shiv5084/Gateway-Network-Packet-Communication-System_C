#include "common/logger.h"
#include "common/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
#else
    #include <sys/time.h>
#endif

static gw_log_level_t s_min_level = GW_LOG_INFO;
static FILE          *s_log_file  = NULL;
static gw_mutex_t     s_log_mutex;
static bool           s_mutex_initialized = false;

static const char* level_to_string(gw_log_level_t level) {
    switch (level) {
        case GW_LOG_DEBUG: return "DEBUG";
        case GW_LOG_INFO:  return "INFO ";
        case GW_LOG_WARN:  return "WARN ";
        case GW_LOG_ERROR: return "ERROR";
        default:           return "LOG  ";
    }
}

static void get_current_time_str(char *buf, size_t size) {
#if defined(_WIN32) || defined(_WIN64)
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(buf, size, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
             st.wYear, st.wMonth, st.wDay,
             st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm tm_buf;
    localtime_r(&tv.tv_sec, &tm_buf);
    int ms = (int)(tv.tv_usec / 1000);
    snprintf(buf, size, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
             tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
             tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec, ms);
#endif
}

int gw_log_init(gw_log_level_t min_level, const char *file_path) {
    if (!s_mutex_initialized) {
        if (gw_mutex_init(&s_log_mutex) == 0) {
            s_mutex_initialized = true;
        }
    }

    s_min_level = min_level;

    if (file_path && file_path[0] != '\0') {
        if (s_log_file) {
            fclose(s_log_file);
            s_log_file = NULL;
        }
        s_log_file = fopen(file_path, "a");
        if (!s_log_file) {
            return -1;
        }
    }
    return 0;
}

void gw_log_set_level(gw_log_level_t min_level) {
    s_min_level = min_level;
}

void gw_log_close(void) {
    if (s_mutex_initialized) {
        gw_mutex_lock(&s_log_mutex);
    }
    if (s_log_file) {
        fflush(s_log_file);
        fclose(s_log_file);
        s_log_file = NULL;
    }
    if (s_mutex_initialized) {
        gw_mutex_unlock(&s_log_mutex);
        gw_mutex_destroy(&s_log_mutex);
        s_mutex_initialized = false;
    }
}

void gw_log_write(gw_log_level_t level, const char *tag, const char *fmt, ...) {
    if (level < s_min_level || level >= GW_LOG_NONE) {
        return;
    }

    char time_str[32];
    get_current_time_str(time_str, sizeof(time_str));

    char msg_buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg_buf, sizeof(msg_buf), fmt, args);
    va_end(args);

    if (s_mutex_initialized) {
        gw_mutex_lock(&s_log_mutex);
    }

    FILE *out = (level >= GW_LOG_ERROR) ? stderr : stdout;
    fprintf(out, "[%s] [%s] [%-6s] %s\n", time_str, level_to_string(level), tag ? tag : "SYS", msg_buf);
    fflush(out);

    if (s_log_file) {
        fprintf(s_log_file, "[%s] [%s] [%-6s] %s\n", time_str, level_to_string(level), tag ? tag : "SYS", msg_buf);
        fflush(s_log_file);
    }

    if (s_mutex_initialized) {
        gw_mutex_unlock(&s_log_mutex);
    }
}

void gw_log_hex_dump(gw_log_level_t level, const char *tag, const char *prefix,
                     const uint8_t *buf, size_t len) {
    if (level < s_min_level || level >= GW_LOG_NONE || !buf) {
        return;
    }

    /* Build formatted hex string */
    /* Each byte needs 3 characters ("XX ") + NULL */
    size_t hex_str_size = (len * 3) + 64;
    char *hex_str = (char*)malloc(hex_str_size);
    if (!hex_str) {
        return;
    }

    size_t pos = 0;
    for (size_t i = 0; i < len; i++) {
        pos += (size_t)snprintf(hex_str + pos, hex_str_size - pos, "%02X ", buf[i]);
        if (pos >= hex_str_size - 1) break;
    }
    if (pos > 0 && hex_str[pos - 1] == ' ') {
        hex_str[pos - 1] = '\0'; /* Remove trailing space */
    } else {
        hex_str[pos] = '\0';
    }

    if (prefix && prefix[0] != '\0') {
        gw_log_write(level, tag, "%s (%zu bytes): %s", prefix, len, hex_str);
    } else {
        gw_log_write(level, tag, "(%zu bytes): %s", len, hex_str);
    }

    free(hex_str);
}
