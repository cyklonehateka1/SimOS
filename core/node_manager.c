#include "../include/env.h"
#include "../include/logging.h"
#include "../include/node_manager.h"
#include "../include/ipc.h"
#include "../include/json.h"
#include "../include/db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <poll.h>
#include <time.h>

// static session store
static NodeSession sessions[MAX_SESSIONS];
static int sessions_init = 0;

static void node_session_touch(NodeSession *session) {
    if (session) {
        session->last_seen = time(NULL);
    }
}

void node_sessions_init(void) {
    if (sessions_init) return;
    for (int i = 0; i < MAX_SESSIONS; ++i) {
        sessions[i].fd = -1;
        sessions[i].connected = 0;
        sessions[i].last_seen = 0;
        memset(&sessions[i].meta, 0, sizeof(Node));
    }
    sessions_init = 1;
}

static NodeSession *find_free_slot(void) {
    for (int i = 0; i < MAX_SESSIONS; ++i) {
        if (!sessions[i].connected) return &sessions[i];
    }
    return NULL;
}

NodeSession *node_session_add(const Node *node_meta, int fd) {
    if (!node_meta || fd < 0) return NULL;
    // try find by name
    for (int i = 0; i < MAX_SESSIONS; ++i) {
        if (sessions[i].connected && strcmp(sessions[i].meta.name, node_meta->name) == 0) {
            // update existing session
            if (sessions[i].fd != fd) {
                close(sessions[i].fd);
            }
            sessions[i].fd = fd;
            sessions[i].last_seen = time(NULL);
            sessions[i].connected = 1;
            strncpy(sessions[i].meta.address, node_meta->address, sizeof(sessions[i].meta.address)-1);
            strncpy(sessions[i].meta.os, node_meta->os, sizeof(sessions[i].meta.os)-1);
            log_info("Updated session for node %s (fd=%d)", node_meta->name, fd);
            return &sessions[i];
        }
    }

    NodeSession *slot = find_free_slot();
    if (!slot) {
        log_error("Max sessions reached");
        return NULL;
    }

    strncpy(slot->meta.name, node_meta->name, sizeof(slot->meta.name)-1);
    strncpy(slot->meta.address, node_meta->address, sizeof(slot->meta.address)-1);
    strncpy(slot->meta.os, node_meta->os, sizeof(slot->meta.os)-1);
    slot->fd = fd;
    slot->last_seen = time(NULL);
    slot->connected = 1;
    log_info("Added node session %s (fd=%d)", slot->meta.name, fd);
    return slot;
}

void node_session_remove_by_fd(int fd) {
    if (fd < 0) return;
    for (int i = 0; i < MAX_SESSIONS; ++i) {
        if (sessions[i].connected && sessions[i].fd == fd) {
            log_info("Removing session %s (fd=%d)", sessions[i].meta.name, fd);
            close(sessions[i].fd);
            sessions[i].fd = -1;
            sessions[i].connected = 0;
            memset(&sessions[i].meta, 0, sizeof(Node));
            sessions[i].last_seen = 0;
            return;
        }
    }
}

void node_session_remove_by_name(const char *name) {
    if (!name) return;
    for (int i = 0; i < MAX_SESSIONS; ++i) {
        if (sessions[i].connected && strcmp(sessions[i].meta.name, name) == 0) {
            close(sessions[i].fd);
            sessions[i].fd = -1;
            sessions[i].connected = 0;
            memset(&sessions[i].meta, 0, sizeof(Node));
            sessions[i].last_seen = 0;
            log_info("Removed session for node %s", name);
            return;
        }
    }
}

NodeSession *node_session_find_by_name(const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < MAX_SESSIONS; ++i) {
        if (sessions[i].connected && strcmp(sessions[i].meta.name, name) == 0) {
            return &sessions[i];
        }
    }
    return NULL;
}

NodeSession *node_session_find_by_fd(int fd) {
    if (fd < 0) return NULL;
    for (int i = 0; i < MAX_SESSIONS; ++i) {
        if (sessions[i].connected && sessions[i].fd == fd) return &sessions[i];
    }
    return NULL;
}

int node_sessions_copy(NodeSession *out_array, int max_entries) {
    if (!out_array || max_entries <= 0) return 0;
    int count = 0;
    for (int i = 0; i < MAX_SESSIONS && count < max_entries; ++i) {
        if (sessions[i].connected) {
            out_array[count++] = sessions[i];
        }
    }
    return count;
}

ssize_t node_session_send(NodeSession *s, const char *msg) {
    if (!s || !msg) return -1;
    return ipc_send_full(s->fd, msg, strlen(msg));
}

int node_sessions_fill_pollfds(struct pollfd *pfds, int max_fds) {
    int idx = 0;
    for (int i = 0; i < MAX_SESSIONS && idx < max_fds; ++i) {
        if (sessions[i].connected) {
            pfds[idx].fd = sessions[i].fd;
            pfds[idx].events = POLLIN;
            pfds[idx].revents = 0;
            idx++;
        }
    }
    return idx;
}

void node_sessions_cleanup(void) {
    for (int i = 0; i < MAX_SESSIONS; ++i) {
        if (sessions[i].connected) {
            close(sessions[i].fd);
            sessions[i].fd = -1;
            sessions[i].connected = 0;
            sessions[i].last_seen = 0;
        }
    }
}

// Very simple JSON-like parse for "hello" messages: {"type":"hello","name":"node1",...}
// Only extracts name, address, os. Returns 0 on success, -1 on error.
int parse_hello_message(const char *msg, Node *out_node) {
    if (!msg || !out_node) return -1;
    char *message_type = json_get_string(msg, "type");
    if (!message_type || strcmp(message_type, "hello") != 0) {
        free(message_type);
        return -1;
    }
    free(message_type);
    const char *p = msg;
    // naive parse: look for "name":"...","address":"...","os":"..."
    // extract name
    char tmp[256];
    const char *found = strstr(p, "\"name\"");
    if (found) {
        const char *colon = strchr(found, ':');
        if (!colon) return -1;
        const char *first_quote = strchr(colon, '\"');
        if (!first_quote) return -1;
        const char *second_quote = strchr(first_quote + 1, '\"');
        if (!second_quote) return -1;
        size_t nlen = (size_t)(second_quote - (first_quote + 1));
        if (nlen >= sizeof(tmp)) nlen = sizeof(tmp)-1;
        memcpy(tmp, first_quote + 1, nlen);
        tmp[nlen] = '\0';
        strncpy(out_node->name, tmp, sizeof(out_node->name)-1);
    } else {
        return -1;
    }

    found = strstr(p, "\"address\"");
    if (found) {
        const char *colon = strchr(found, ':');
        if (!colon) return -1;
        const char *first_quote = strchr(colon, '\"');
        if (!first_quote) return -1;
        const char *second_quote = strchr(first_quote + 1, '\"');
        if (!second_quote) return -1;
        size_t nlen = (size_t)(second_quote - (first_quote + 1));
        if (nlen >= sizeof(tmp)) nlen = sizeof(tmp)-1;
        memcpy(tmp, first_quote + 1, nlen);
        tmp[nlen] = '\0';
        strncpy(out_node->address, tmp, sizeof(out_node->address)-1);
    } else {
        out_node->address[0] = '\0';
    }

    found = strstr(p, "\"os\"");
    if (found) {
        const char *colon = strchr(found, ':');
        if (!colon) return -1;
        const char *first_quote = strchr(colon, '\"');
        if (!first_quote) return -1;
        const char *second_quote = strchr(first_quote + 1, '\"');
        if (!second_quote) return -1;
        size_t nlen = (size_t)(second_quote - (first_quote + 1));
        if (nlen >= sizeof(tmp)) nlen = sizeof(tmp)-1;
        memcpy(tmp, first_quote + 1, nlen);
        tmp[nlen] = '\0';
        strncpy(out_node->os, tmp, sizeof(out_node->os)-1);
    } else {
        out_node->os[0] = '\0';
    }

    return 0;
}

void handle_node_message(int fd, const char *msg, GlobalState *state) {
    (void)state;
    if (!msg) return;

    NodeSession *session = node_session_find_by_fd(fd);
    if (!session) {
        log_error("Received message from unknown fd=%d", fd);
        return;
    }

    node_session_touch(session);

    char *type = json_get_string(msg, "type");
    if (!type) {
        log_error("Malformed message from %s: %s", session->meta.name, msg);
        return;
    }

    if (strcmp(type, "pong") == 0) {
        log_info("Received pong from %s", session->meta.name);
    } else if (strcmp(type, "result") == 0) {
        char *id = json_get_string(msg, "id");
        char *stdout_raw = json_get_string(msg, "stdout");
        char *stderr_raw = json_get_string(msg, "stderr");
        int exit_code = json_get_integer(msg, "exit", -1);

        char *stdout_unesc = strdup(stdout_raw ? stdout_raw : "");
        char *stderr_unesc = strdup(stderr_raw ? stderr_raw : "");

        printf("\n[%s] command result (id=%s, exit=%d)\n",
               session->meta.name,
               id ? id : "unknown",
               exit_code);
        if (stdout_unesc && stdout_unesc[0] != '\0') {
            printf("stdout:\n%s\n", stdout_unesc);
        } else {
            printf("stdout: <empty>\n");
        }
        if (stderr_unesc && stderr_unesc[0] != '\0') {
            printf("stderr:\n%s\n", stderr_unesc);
        } else {
            printf("stderr: <empty>\n");
        }
        fflush(stdout);

        log_info("Command result from %s (id=%s, exit=%d)", session->meta.name, id ? id : "unknown", exit_code);
        char journal[256]; snprintf(journal, sizeof(journal), "process=%s exit=%d", id ? id : "unknown", exit_code);
        db_store_event(session->meta.name, "result", journal);

        free(id);
        free(stdout_raw);
        free(stderr_raw);
        free(stdout_unesc);
        free(stderr_unesc);
    } else if (strcmp(type, "status") == 0) {
        char *hostname = json_get_string(msg, "hostname");
        char *kernel = json_get_string(msg, "kernel");
        int cpus = json_get_integer(msg, "cpus", 0);
        int memory = json_get_integer(msg, "memory_mb", 0);
        int uptime = json_get_integer(msg, "uptime_s", 0);
        printf("\n[%s] node status\n  host: %s\n  kernel: %s\n  CPUs: %d\n  memory: %d MiB\n  uptime: %ds\n",
               session->meta.name, hostname ? hostname : "unknown", kernel ? kernel : "unknown", cpus, memory, uptime);
        fflush(stdout); db_store_event(session->meta.name, "status", "snapshot received");
        free(hostname); free(kernel);
    } else {
        log_info("Unhandled message type '%s' from %s: %s", type, session->meta.name, msg);
    }

    free(type);
}
