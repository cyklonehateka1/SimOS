#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <stdbool.h>

static char g_db_path[256];

bool db_init(const char *db_path) {
    struct stat st;

    strncpy(g_db_path, db_path, sizeof(g_db_path) - 1);
    g_db_path[sizeof(g_db_path) - 1] = '\0';

    if (stat(db_path, &st) != 0) {
        FILE *f = fopen(db_path, "w");
        if (!f) {
            perror("Failed to create DB file");
            return false;
        }
        fprintf(f, "# SimOS command database\n");
        fclose(f);
    }

    FILE *test = fopen(db_path, "a+");
    if (!test) {
        perror("Failed to open DB file for read/write");
        return false;
    }
    fclose(test);

    return true;
}

bool db_store_command(const char *node_name, const char *command, const char *result) {
    FILE *f = fopen(g_db_path, "a");
    if (!f) {
        perror("Failed to open DB file for writing");
        return false;
    }

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);

    fprintf(f, "[%s] node=%s command=\"%s\" result=\"%s\"\n",
            timestamp, node_name, command, result);

    fclose(f);
    return true;
}

void db_list_commands() {
    FILE *f = fopen(g_db_path, "r");
    if (!f) {
        perror("Failed to open DB file for reading");
        return;
    }

    char line[512];
    printf("\n--- Stored Commands ---\n");
    while (fgets(line, sizeof(line), f)) {
        printf("%s", line);
    }
    printf("\n-----------------------\n");

    fclose(f);
}