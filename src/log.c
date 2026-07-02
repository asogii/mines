// log.c
#include "log.h"
#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <stdlib.h>

static FILE *log_fp = NULL;

void log_init(const char *log_path) {
    if (!log_path) {
        char default_path[256];
        snprintf(default_path, sizeof(default_path), "/tmp/mines-tui.log");
        log_fp = fopen(default_path, "a");
    } else {
        log_fp = fopen(log_path, "a");
    }

    if (log_fp) {
        setbuf(log_fp, NULL);
        log_info("=== Mines-TUI started ===");
    }
}

void log_close(void) {
    if (log_fp) {
        log_info("=== Mines-TUI stopped ===");
        fclose(log_fp);
        log_fp = NULL;
    }
}

static void log_write(const char *level, const char *fmt, va_list args) {
    if (!log_fp) return;

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

    fprintf(log_fp, "[%s] [%s] ", timestamp, level);
    vfprintf(log_fp, fmt, args);
    fprintf(log_fp, "\n");
    fflush(log_fp);
}

void log_info(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_write("INFO", fmt, args);
    va_end(args);
}

void log_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_write("ERROR", fmt, args);
    va_end(args);
}

void log_debug(const char *fmt, ...) {
#ifdef DEBUG
    va_list args;
    va_start(args, fmt);
    log_write("DEBUG", fmt, args);
    va_end(args);
#endif
}
