#ifndef DB_H
#define DB_H

#include <stdbool.h>

bool db_init(const char *path);
bool db_store_event(const char *node, const char *action, const char *details);
void db_list_events(void);
void db_close(void);

#endif
