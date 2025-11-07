#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *get_token(const char *input, const char *delimiter, int index) {
    if (!input || !delimiter) return NULL;
    char *copy = strdup(input);
    if (!copy) return NULL;

    char *saveptr = NULL;
    char *tok = strtok_r(copy, delimiter, &saveptr);
    int i = 0;
    while (tok) {
        if (i == index) {
            char *result = strdup(tok);
            free(copy);
            return result;
        }
        tok = strtok_r(NULL, delimiter, &saveptr);
        i++;
    }

    free(copy);
    return NULL;
}