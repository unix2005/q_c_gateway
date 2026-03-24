#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#include "../include/log.h"

static FILE *log_file = NULL;
static int log_level = LOG_LEVEL_INFO;

int log_init(const char *log_file_path) {
    // 确保日志目录存在
    char log_dir[256];
    strcpy(log_dir, log_file_path);
    char *last_slash = strrchr(log_dir, '/');
    if (last_slash != NULL) {
        *last_slash = '\0';
        system((char *)malloc(snprintf(NULL, 0, "mkdir -p %s", log_dir) + 1));
    }

    // 打开日志文件
    log_file = fopen(log_file_path, "a");
    if (log_file == NULL) {
        fprintf(stderr, "Failed to open log file: %s\n", log_file_path);
        return -1;
    }

    return 0;
}

void log_cleanup() {
    if (log_file != NULL) {
        fclose(log_file);
        log_file = NULL;
    }
}

static void log_write(int level, const char *format, va_list args) {
    if (level < log_level) {
        return;
    }

    // 获取当前时间
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    // 日志级别字符串
    const char *level_str;
    switch (level) {
        case LOG_LEVEL_DEBUG:
            level_str = "DEBUG";
            break;
        case LOG_LEVEL_INFO:
            level_str = "INFO";
            break;
        case LOG_LEVEL_WARN:
            level_str = "WARN";
            break;
        case LOG_LEVEL_ERROR:
            level_str = "ERROR";
            break;
        default:
            level_str = "UNKNOWN";
            break;
    }

    // 输出到日志文件
    if (log_file != NULL) {
        fprintf(log_file, "[%s] [%s] ", time_str, level_str);
        vfprintf(log_file, format, args);
        fprintf(log_file, "\n");
        fflush(log_file);
    }

    // 输出到标准输出
    fprintf(stdout, "[%s] [%s] ", time_str, level_str);
    vfprintf(stdout, format, args);
    fprintf(stdout, "\n");
    fflush(stdout);
}

void log_debug(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_write(LOG_LEVEL_DEBUG, format, args);
    va_end(args);
}

void log_info(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_write(LOG_LEVEL_INFO, format, args);
    va_end(args);
}

void log_warn(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_write(LOG_LEVEL_WARN, format, args);
    va_end(args);
}

void log_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_write(LOG_LEVEL_ERROR, format, args);
    va_end(args);
}