#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../include/json.h"

static const char *find_value(const char *json, const char *key) {
    char needle[160];
    if (!json || !key || snprintf(needle, sizeof(needle), "\"%s\"", key) >= (int)sizeof(needle)) return NULL;
    const char *p = strstr(json, needle);
    if (!p) return NULL;
    p = strchr(p + strlen(needle), ':');
    if (!p) return NULL;
    do { p++; } while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n');
    return p;
}

char *json_get_string(const char *json, const char *key) {
    const char *p = find_value(json, key);
    if (!p || *p++ != '"') return NULL;
    size_t cap = strlen(p) + 1, len = 0;
    char *out = malloc(cap);
    if (!out) return NULL;
    while (*p && *p != '"') {
        if (*p == '\\' && p[1]) {
            p++;
            switch (*p) {
                case 'n': out[len++] = '\n'; break;
                case 'r': out[len++] = '\r'; break;
                case 't': out[len++] = '\t'; break;
                case 'b': out[len++] = '\b'; break;
                case 'f': out[len++] = '\f'; break;
                default: out[len++] = *p; break;
            }
            p++;
        } else out[len++] = *p++;
    }
    if (*p != '"') { free(out); return NULL; }
    out[len] = '\0';
    return out;
}

int json_get_integer(const char *json, const char *key, int fallback) {
    const char *p = find_value(json, key);
    if (!p) return fallback;
    errno = 0; char *end = NULL; long n = strtol(p, &end, 10);
    return errno || end == p ? fallback : (int)n;
}

char *json_escape(const char *input) {
    if (!input) input = "";
    size_t cap = strlen(input) * 2 + 1, len = 0;
    char *out = malloc(cap);
    if (!out) return NULL;
    for (const unsigned char *p = (const unsigned char *)input; *p; p++) {
        const char *replacement = NULL;
        switch (*p) {
            case '"': replacement = "\\\""; break;
            case '\\': replacement = "\\\\"; break;
            case '\n': replacement = "\\n"; break;
            case '\r': replacement = "\\r"; break;
            case '\t': replacement = "\\t"; break;
            case '\b': replacement = "\\b"; break;
            case '\f': replacement = "\\f"; break;
        }
        if (replacement) { out[len++] = replacement[0]; out[len++] = replacement[1]; }
        else if (*p >= 0x20) out[len++] = (char)*p;
    }
    out[len] = '\0';
    return out;
}
