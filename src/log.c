#include "log.h"
#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <stdlib.h>

#ifdef DEBUG

static FILE *log_fp = NULL;

static void log_write(const char *level, const char *fmt, va_list args) {
    if (!log_fp) return;

    time_t now = time(NULL);
    struct tm tm_info;
    localtime_r(&now, &tm_info);

    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_info);

    flockfile(log_fp);
    fprintf(log_fp, "[%s] [%s] ", timestamp, level);
    vfprintf(log_fp, fmt, args);
    fprintf(log_fp, "\n");
    funlockfile(log_fp);
}
#endif

void log_init(void) {
#ifdef DEBUG
    log_fp = fopen("/tmp/mines-tui.log", "a");
    if (log_fp) {
        setbuf(log_fp, NULL);
        log_info("=== Mines-TUI started ===");
    }
#endif
}

void log_close(void) {
#ifdef DEBUG
    if (log_fp) {
        log_info("=== Mines-TUI stopped ===");
        fclose(log_fp);
        log_fp = NULL;
    }
#endif
}

void log_info(const char *fmt, ...) {
#ifdef DEBUG
    va_list args;
    va_start(args, fmt);
    log_write("INFO", fmt, args);
    va_end(args);
#endif
}

void log_error(const char *fmt, ...) {
#ifdef DEBUG
    va_list args;
    va_start(args, fmt);
    log_write("ERROR", fmt, args);
    va_end(args);
#endif
}

void log_debug(const char *fmt, ...) {
#ifdef DEBUG
    va_list args;
    va_start(args, fmt);
    log_write("DEBUG", fmt, args);
    va_end(args);
#endif
}
