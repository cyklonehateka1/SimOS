#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <ctype.h>

#include "../include/ipc.h"
#include "../include/logging.h"
#include "../include/node_agent.h"

static void usage(const char *program) {
    fprintf(stderr, "Usage: %s --name NAME [--host IPv4] [--port PORT] [--log PATH]\n", program);
}

static int valid_name(const char *name) {
    if (!name || !name[0] || strlen(name) > 63) return 0;
    for (const unsigned char *p = (const unsigned char *)name; *p; p++)
        if (!isalnum(*p) && *p != '-' && *p != '_') return 0;
    return 1;
}

int main(int argc, char **argv) {
    const char *name = NULL, *host = "127.0.0.1", *log_path = NULL;
    int port = 9000;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--name") && i + 1 < argc) name = argv[++i];
        else if (!strcmp(argv[i], "--host") && i + 1 < argc) host = argv[++i];
        else if (!strcmp(argv[i], "--port") && i + 1 < argc) port = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--log") && i + 1 < argc) log_path = argv[++i];
        else if (!strcmp(argv[i], "--help")) { usage(argv[0]); return 0; }
        else { usage(argv[0]); return 2; }
    }
    if (!valid_name(name) || port < 1 || port > 65535) { usage(argv[0]); return 2; }
    log_init(log_path);
    struct utsname info; const char *os_name = uname(&info) == 0 ? info.sysname : "unknown";
    int sock = node_agent_connect(host, port);
    if (sock < 0) { log_close(); return 1; }
    if (node_agent_register(sock, name, os_name) < 0) { close(sock); log_close(); return 1; }
    printf("SimOS child node '%s' online at %s:%d\n", name, host, port); fflush(stdout);
    node_agent_run_loop(sock, name); log_close(); return 0;
}
