#ifndef SIMOS_JSON_H
#define SIMOS_JSON_H

char *json_get_string(const char *json, const char *key);
int json_get_integer(const char *json, const char *key, int fallback);
char *json_escape(const char *input);

#endif
