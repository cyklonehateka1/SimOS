#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include "../../include/logging.h"

static FILE *log_file = NULL;

bool log_init(const char *path) {
    log_file = fopen(path, "a");
    return log_file != NULL;
}

void log_close() {
    if (log_file) fclose(log_file);
}

void log_info(const char *fmt, ...) {
    if (!log_file) return;
    va_list args;
    va_start(args, fmt);
    time_t now = time(NULL);
    fprintf(log_file, "[INFO] %s: ", ctime(&now));
    vfprintf(log_file, fmt, args);
    fprintf(log_file, "\n");
    fflush(log_file);
    va_end(args);
}

void log_error(const char *fmt, ...) {
    if (!log_file) return;
    va_list args;
    va_start(args, fmt);
    time_t now = time(NULL);
    fprintf(log_file, "[ERROR] %s: ", ctime(&now));
    vfprintf(log_file, fmt, args);
    fprintf(log_file, "\n");
    fflush(log_file);
    va_end(args);
}