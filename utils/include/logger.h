#ifndef LOGGER_H
#define LOGGER_H

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

// Log levels
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO  = 1,
    LOG_LEVEL_WARN  = 2,
    LOG_LEVEL_ERROR = 3,
    LOG_LEVEL_FATAL = 4
} log_level_t;

// Logger configuration
typedef struct {
    log_level_t min_level;
    FILE       *output_file;
    int         use_colors;
    int         show_timestamp;
    int         show_level;
    int         show_file_info;
    char        component_name[64];
} logger_config_t;

// Initialize the logger
void logger_init(const char *component_name, log_level_t min_level);
void logger_init_with_file(const char *component_name, log_level_t min_level, const char *log_file);
void logger_cleanup(void);

// Configuration functions
void logger_set_level(log_level_t level);
void logger_enable_colors(int enable);
void logger_enable_timestamps(int enable);
void logger_enable_file_info(int enable);

// Core logging function
void logger_log(log_level_t level, const char *file, int line, const char *func, const char *format,
                ...);

// Convenience macros
#define LOG_DEBUG(fmt, ...)                                                                        \
    logger_log(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)                                                                         \
    logger_log(LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)                                                                         \
    logger_log(LOG_LEVEL_WARN, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)                                                                        \
    logger_log(LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...)                                                                        \
    logger_log(LOG_LEVEL_FATAL, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

// Specialized logging macros for common scenarios
#define LOG_CONNECTION(fmt, ...) LOG_INFO("[CONNECTION] " fmt, ##__VA_ARGS__)
#define LOG_HANDSHAKE(fmt, ...)  LOG_INFO("[HANDSHAKE] " fmt, ##__VA_ARGS__)
#define LOG_PROTOCOL(fmt, ...)   LOG_DEBUG("[PROTOCOL] " fmt, ##__VA_ARGS__)
#define LOG_CLIENT(fmt, ...)     LOG_INFO("[CLIENT] " fmt, ##__VA_ARGS__)
#define LOG_SERVER(fmt, ...)     LOG_INFO("[SERVER] " fmt, ##__VA_ARGS__)
#define LOG_NETWORK(fmt, ...)    LOG_DEBUG("[NETWORK] " fmt, ##__VA_ARGS__)

// Error logging with errno
#define LOG_ERRNO(fmt, ...) LOG_ERROR(fmt ": %s", ##__VA_ARGS__, strerror(errno))

// Conditional logging (only logs if condition is true)
#define LOG_IF(condition, level, fmt, ...)                                                         \
    do {                                                                                           \
        if (condition)                                                                             \
            logger_log(level, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__);                   \
    } while (0)

// Performance logging macros
#define LOG_PERF_START(name)                                                                       \
    struct timeval _perf_start_##name;                                                             \
    gettimeofday(&_perf_start_##name, NULL);                                                       \
    LOG_DEBUG("[PERF] Starting %s", #name)

#define LOG_PERF_END(name)                                                                         \
    do {                                                                                           \
        struct timeval _perf_end_##name;                                                           \
        gettimeofday(&_perf_end_##name, NULL);                                                     \
        double _elapsed = (_perf_end_##name.tv_sec - _perf_start_##name.tv_sec) * 1000.0 +         \
                          (_perf_end_##name.tv_usec - _perf_start_##name.tv_usec) / 1000.0;        \
        LOG_DEBUG("[PERF] %s completed in %.2f ms", #name, _elapsed);                              \
    } while (0)

#endif // LOGGER_H
