#ifndef LOGGING_H
#define LOGGING_H

#include <stdbool.h>

bool log_init(const char *path);
void log_close();
void log_info(const char *fmt, ...);
void log_error(const char *fmt, ...);

#endif