#ifndef LOGGER_H
#define LOGGER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GW_LOG_DEBUG = 0,
    GW_LOG_INFO  = 1,
    GW_LOG_WARN  = 2,
    GW_LOG_ERROR = 3,
    GW_LOG_NONE  = 4
} gw_log_level_t;

/**
 * @brief Initializes the logging subsystem with minimum level and optional file output.
 * @param min_level Minimum severity level to log
 * @param file_path Optional log file path (can be NULL for console only)
 * @return 0 on success, -1 on failure
 */
int gw_log_init(gw_log_level_t min_level, const char *file_path);

/**
 * @brief Sets the current minimum log level at runtime.
 */
void gw_log_set_level(gw_log_level_t min_level);

/**
 * @brief Flushes and closes logging file resources.
 */
void gw_log_close(void);

/**
 * @brief Emits a structured formatted log message.
 */
void gw_log_write(gw_log_level_t level, const char *tag, const char *fmt, ...);

/**
 * @brief Dumps a byte buffer in space-separated uppercase hex format.
 */
void gw_log_hex_dump(gw_log_level_t level, const char *tag, const char *prefix,
                     const uint8_t *buf, size_t len);

/* Helper macros */
#define GW_LOG_D(tag, ...) gw_log_write(GW_LOG_DEBUG, tag, __VA_ARGS__)
#define GW_LOG_I(tag, ...) gw_log_write(GW_LOG_INFO,  tag, __VA_ARGS__)
#define GW_LOG_W(tag, ...) gw_log_write(GW_LOG_WARN,  tag, __VA_ARGS__)
#define GW_LOG_E(tag, ...) gw_log_write(GW_LOG_ERROR, tag, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* LOGGER_H */
