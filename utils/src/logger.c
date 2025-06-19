#include "../include/logger.h"
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>

// ANSI color codes
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_WHITE   "\033[37m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_DIM     "\033[2m"

// Global logger instance
static logger_config_t g_logger = {.min_level      = LOG_LEVEL_INFO,
                                   .output_file    = NULL,
                                   .use_colors     = 1,
                                   .show_timestamp = 1,
                                   .show_level     = 1,
                                   .show_file_info = 0,
                                   .component_name = {0}};

static pthread_mutex_t g_logger_mutex       = PTHREAD_MUTEX_INITIALIZER;
static int             g_logger_initialized = 0;

// Level names and colors
static const char *level_names[] = {"DEBUG", "INFO", "WARN", "ERROR", "FATAL"};

static const char *level_colors[] = {
    COLOR_CYAN,   // DEBUG
    COLOR_GREEN,  // INFO
    COLOR_YELLOW, // WARN
    COLOR_RED,    // ERROR
    COLOR_MAGENTA // FATAL
};

// Helper function to get basename of file path
static const char *get_basename(const char *path)
{
    const char *base = strrchr(path, '/');
    return base ? base + 1 : path;
}

// Helper function to check if output supports colors
static int supports_color(FILE *file)
{
    return isatty(fileno(file));
}

void logger_init(const char *component_name, log_level_t min_level)
{
    pthread_mutex_lock(&g_logger_mutex);

    g_logger.min_level      = min_level;
    g_logger.output_file    = stdout;
    g_logger.use_colors     = supports_color(stdout);
    g_logger.show_timestamp = 1;
    g_logger.show_level     = 1;
    g_logger.show_file_info = 0;

    if (component_name) {
        strncpy(g_logger.component_name, component_name, sizeof(g_logger.component_name) - 1);
        g_logger.component_name[sizeof(g_logger.component_name) - 1] = '\0';
    }

    g_logger_initialized = 1;
    pthread_mutex_unlock(&g_logger_mutex);

    LOG_INFO("Logger initialized for component '%s' with level %s",
             component_name ? component_name : "UNKNOWN", level_names[min_level]);
}

void logger_init_with_file(const char *component_name, log_level_t min_level, const char *log_file)
{
    pthread_mutex_lock(&g_logger_mutex);

    FILE *file = fopen(log_file, "a");
    if (!file) {
        pthread_mutex_unlock(&g_logger_mutex);
        fprintf(stderr, "Failed to open log file '%s': %s\n", log_file, strerror(errno));
        logger_init(component_name, min_level);
        return;
    }

    g_logger.min_level      = min_level;
    g_logger.output_file    = file;
    g_logger.use_colors     = 0; // No colors for file output
    g_logger.show_timestamp = 1;
    g_logger.show_level     = 1;
    g_logger.show_file_info = 1;

    if (component_name) {
        strncpy(g_logger.component_name, component_name, sizeof(g_logger.component_name) - 1);
        g_logger.component_name[sizeof(g_logger.component_name) - 1] = '\0';
    }

    g_logger_initialized = 1;
    pthread_mutex_unlock(&g_logger_mutex);

    LOG_INFO("Logger initialized for component '%s' with level %s, output to file '%s'",
             component_name ? component_name : "UNKNOWN", level_names[min_level], log_file);
}

void logger_cleanup(void)
{
    pthread_mutex_lock(&g_logger_mutex);

    if (g_logger_initialized && g_logger.output_file && g_logger.output_file != stdout &&
        g_logger.output_file != stderr) {
        LOG_INFO("Logger shutting down");
        fclose(g_logger.output_file);
    }

    g_logger.output_file = NULL;
    g_logger_initialized = 0;

    pthread_mutex_unlock(&g_logger_mutex);
}

void logger_set_level(log_level_t level)
{
    pthread_mutex_lock(&g_logger_mutex);
    g_logger.min_level = level;
    pthread_mutex_unlock(&g_logger_mutex);
}

void logger_enable_colors(int enable)
{
    pthread_mutex_lock(&g_logger_mutex);
    g_logger.use_colors = enable &&
                          (g_logger.output_file == stdout || g_logger.output_file == stderr) &&
                          supports_color(g_logger.output_file);
    pthread_mutex_unlock(&g_logger_mutex);
}

void logger_enable_timestamps(int enable)
{
    pthread_mutex_lock(&g_logger_mutex);
    g_logger.show_timestamp = enable;
    pthread_mutex_unlock(&g_logger_mutex);
}

void logger_enable_file_info(int enable)
{
    pthread_mutex_lock(&g_logger_mutex);
    g_logger.show_file_info = enable;
    pthread_mutex_unlock(&g_logger_mutex);
}

void logger_log(log_level_t level, const char *file, int line, const char *func, const char *format,
                ...)
{
    // Early return if log level is below minimum
    if (!g_logger_initialized || level < g_logger.min_level) {
        return;
    }

    pthread_mutex_lock(&g_logger_mutex);

    if (!g_logger.output_file) {
        g_logger.output_file = stdout;
    }

    // Get current time
    struct timeval tv;
    struct tm     *tm_info;
    gettimeofday(&tv, NULL);
    tm_info = localtime(&tv.tv_sec);

    // Start building the log message
    char  buffer[4096];
    char *buf_ptr   = buffer;
    int   remaining = sizeof(buffer);
    int   written   = 0;

    // Color prefix
    if (g_logger.use_colors) {
        written = snprintf(buf_ptr, remaining, "%s", level_colors[level]);
        buf_ptr += written;
        remaining -= written;
    }

    // Timestamp
    if (g_logger.show_timestamp && remaining > 0) {
        written =
            snprintf(buf_ptr, remaining, "[%04d-%02d-%02d %02d:%02d:%02d.%03d]",
                     tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
                     tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec, (int)(tv.tv_usec / 1000));
        buf_ptr += written;
        remaining -= written;
    }

    // Component name
    if (strlen(g_logger.component_name) > 0 && remaining > 0) {
        written = snprintf(buf_ptr, remaining, "[%s]", g_logger.component_name);
        buf_ptr += written;
        remaining -= written;
    }

    // Log level
    if (g_logger.show_level && remaining > 0) {
        if (g_logger.use_colors) {
            written = snprintf(buf_ptr, remaining, "[%s%s%s]", COLOR_BOLD, level_names[level],
                               COLOR_RESET);
        } else {
            written = snprintf(buf_ptr, remaining, "[%s]", level_names[level]);
        }
        buf_ptr += written;
        remaining -= written;
    }

    // File info
    if (g_logger.show_file_info && remaining > 0) {
        written = snprintf(buf_ptr, remaining, "[%s:%d:%s]", get_basename(file), line, func);
        buf_ptr += written;
        remaining -= written;
    }

    // Separator
    if (remaining > 0) {
        written = snprintf(buf_ptr, remaining, " ");
        buf_ptr += written;
        remaining -= written;
    }

    // Reset color if using colors
    if (g_logger.use_colors && remaining > 0) {
        written = snprintf(buf_ptr, remaining, "%s", COLOR_RESET);
        buf_ptr += written;
        remaining -= written;
    }

    // User message
    if (remaining > 0) {
        va_list args;
        va_start(args, format);
        written = vsnprintf(buf_ptr, remaining, format, args);
        va_end(args);
        buf_ptr += written;
        remaining -= written;
    }

    // Newline
    if (remaining > 0) {
        written = snprintf(buf_ptr, remaining, "\n");
        buf_ptr += written;
        remaining -= written;
    }

    // Output the message
    fprintf(g_logger.output_file, "%s", buffer);
    fflush(g_logger.output_file);

    pthread_mutex_unlock(&g_logger_mutex);
}
