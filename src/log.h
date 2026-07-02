// log.h
#ifndef LOG_H
#define LOG_H

void log_init(const char *log_path);
void log_info(const char *fmt, ...);
void log_error(const char *fmt, ...);
void log_debug(const char *fmt, ...);
void log_close(void);

#endif
