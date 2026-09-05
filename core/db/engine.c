#include <stdio.h>
#include <string.h>
#include <time.h>
#include "../../include/db.h"

static FILE *database;

bool db_init(const char *path) {
    if (!path || !path[0]) return false;
    database = fopen(path, "a+");
    if (!database) return false;
    fseek(database, 0, SEEK_END);
    if (ftell(database) == 0) fputs("# SimOS event journal v1\n", database);
    fflush(database);
    return true;
}

static void clean_field(char *out, size_t size, const char *in) {
    size_t n = 0; if (!in) in = "";
    while (*in && n + 1 < size) {
        char c = *in++; out[n++] = (c == '\n' || c == '\r' || c == '\t') ? ' ' : c;
    }
    out[n] = '\0';
}

bool db_store_event(const char *node, const char *action, const char *details) {
    if (!database) return false;
    time_t now = time(NULL); struct tm value; char timestamp[32];
    char safe_node[256], safe_action[64], safe_details[1024];
    localtime_r(&now, &value);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S%z", &value);
    clean_field(safe_node, sizeof(safe_node), node); clean_field(safe_action, sizeof(safe_action), action);
    clean_field(safe_details, sizeof(safe_details), details);
    fprintf(database, "%s\tnode=%s\taction=%s\tdetails=%s\n", timestamp, safe_node, safe_action, safe_details);
    return fflush(database) == 0;
}

void db_list_events(void) {
    if (!database) { puts("Event journal is unavailable."); return; }
    fflush(database); rewind(database); puts("\n--- SimOS event journal ---");
    char line[1400]; while (fgets(line, sizeof(line), database)) fputs(line, stdout);
    puts("---------------------------"); fseek(database, 0, SEEK_END);
}

void db_close(void) {
    if (database) fclose(database); database = NULL;
}
