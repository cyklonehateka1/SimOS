#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../../include/logging.h"

static FILE *log_file;

bool log_init(const char *path) {
    if (!path || !path[0]) { log_file = stderr; return true; }
    log_file = fopen(path, "a");
    if (!log_file) {
        fprintf(stderr, "warning: cannot open log '%s': %s; using stderr\n",
                path, strerror(errno));
        log_file = stderr;
    }
    return true;
}

void log_close(void) {
    if (log_file && log_file != stderr) fclose(log_file);
    log_file = NULL;
}

static void write_log(const char *level, const char *fmt, va_list args) {
    FILE *out = log_file ? log_file : stderr;
    time_t now = time(NULL);
    struct tm value;
    char stamp[32];
    localtime_r(&now, &value);
    strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &value);
    fprintf(out, "[%s] %-5s ", stamp, level);
    vfprintf(out, fmt, args);
    fputc('\n', out);
    fflush(out);
}

void log_info(const char *fmt, ...) {
    va_list args; va_start(args, fmt); write_log("INFO", fmt, args); va_end(args);
}

void log_error(const char *fmt, ...) {
    va_list args; va_start(args, fmt); write_log("ERROR", fmt, args); va_end(args);
}
