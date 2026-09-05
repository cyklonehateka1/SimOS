#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <time.h>
#include <netdb.h>
#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

#include "../include/logging.h"
#include "../include/ipc.h"
#include "../include/json.h"

int node_agent_connect(const char *controller_host, int controller_port) {
    if (!controller_host || controller_port <= 0) {
        log_error("node_agent_connect: invalid controller host/port");
        return -1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        log_error("socket() failed: %s", strerror(errno));
        return -1;
    }

    struct addrinfo hints = {0}, *addresses = NULL;
    hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
    char port_text[16]; snprintf(port_text, sizeof(port_text), "%d", controller_port);
    int lookup = getaddrinfo(controller_host, port_text, &hints, &addresses);
    if (lookup != 0 || !addresses) {
        log_error("cannot resolve %s: %s", controller_host, gai_strerror(lookup)); close(sock); return -1;
    }
    struct sockaddr_in srv; memcpy(&srv, addresses->ai_addr, sizeof(srv)); freeaddrinfo(addresses);

    if (connect(sock, (struct sockaddr *)&srv, sizeof(srv)) < 0) {
        log_error("connect to %s:%d failed: %s", controller_host, controller_port, strerror(errno));
        close(sock);
        return -1;
    }

    int flags = fcntl(sock, F_GETFL, 0);
    if (flags >= 0) {
        flags &= ~O_NONBLOCK;
        fcntl(sock, F_SETFL, flags);
    }

    log_info("Connected to controller %s:%d (fd=%d)", controller_host, controller_port, sock);
    return sock;
}

int node_agent_register(int sock, const char *node_name, const char *osstr) {
    if (sock < 0 || !node_name) {
        log_error("node_agent_register: invalid args");
        return -1;
    }

    char hello[1024];
    int n = snprintf(hello, sizeof(hello),
                     "{\"type\":\"hello\",\"name\":\"%s\",\"os\":\"%s\",\"address\":\"%s\"}\n",
                     node_name, osstr ? osstr : "unknown", "127.0.0.1");
    if (n < 0 || (size_t)n >= sizeof(hello)) {
        log_error("hello message truncated");
        return -1;
    }

    if (ipc_send_full(sock, hello, (size_t)n) < 0) {
        log_error("Failed to send hello");
        return -1;
    }

    char *reply = ipc_recv_line(sock, 2000);
    if (!reply) {
        log_info("No ack received after hello (continuing)");
        return 0;
    }

    if (strstr(reply, "\"type\":\"ack\"") && strstr(reply, "\"status\":\"ok\"")) {
        free(reply);
        log_info("Registration acknowledged by controller");
        return 0;
    }

    log_info("Registration reply: %s", reply);
    free(reply);
    return 0;
}


char *execute_system_command_fork(const char *cmd, int *out_exitcode, char **out_stderr) {
    if (!cmd) {
        if (out_exitcode) *out_exitcode = 127;
        if (out_stderr) *out_stderr = strdup("");
        return strdup("");
    }

    FILE *out_file = tmpfile(), *err_file = tmpfile();
    if (!out_file || !err_file) {
        if (out_file) fclose(out_file); if (err_file) fclose(err_file);
        if (out_exitcode) *out_exitcode = 127;
        if (out_stderr) *out_stderr = strdup("cannot create output buffers");
        return strdup("");
    }

    pid_t pid = fork();
    if (pid < 0) {
        fclose(out_file); fclose(err_file);
        log_error("fork() failed: %s", strerror(errno));
        if (out_exitcode) *out_exitcode = 127;
        if (out_stderr) *out_stderr = strdup("fork failed");
        return strdup("");
    }

    if (pid == 0) {
        dup2(fileno(out_file), STDOUT_FILENO);
        dup2(fileno(err_file), STDERR_FILENO);

        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);

        _exit(127);
    } else {
        int status = 0;
        waitpid(pid, &status, 0);
        if (out_exitcode) {
            if (WIFEXITED(status)) *out_exitcode = WEXITSTATUS(status);
            else *out_exitcode = 127;
        }

        /* Keep two worst-case JSON-escaped streams below MAX_MSG_LEN. */
        const size_t limit = 48 * 1024;
        char *out_buf = malloc(limit + 1), *err_buf = malloc(limit + 1);
        rewind(out_file); rewind(err_file);
        size_t out_len = out_buf ? fread(out_buf, 1, limit, out_file) : 0;
        size_t err_len = err_buf ? fread(err_buf, 1, limit, err_file) : 0;
        if (out_buf) out_buf[out_len] = '\0';
        if (err_buf) err_buf[err_len] = '\0';
        fclose(out_file); fclose(err_file);
        if (!out_buf) out_buf = strdup(""); if (!err_buf) err_buf = strdup("");
        if (out_stderr) *out_stderr = err_buf; else free(err_buf);
        return out_buf;
    }
}

void node_agent_run_loop(int sock, const char *node_name) {
    if (sock < 0) {
        log_error("node_agent_run_loop: invalid socket");
        return;
    }

    log_info("Node agent [%s] entering run loop (fd=%d)", node_name ? node_name : "<anon>", sock);

    while (1) {
        char *msg = ipc_recv_line(sock, -1);
        if (!msg) {
            log_info("Controller closed connection or recv error; exiting run loop");
            break;
        }

        char *type = json_get_string(msg, "type");
        if (!type) {
            log_error("Malformed message (no type): %s", msg);
            free(msg);
            continue;
        }

        if (strcmp(type, "exec") == 0) {
            char *id = json_get_string(msg, "id");
            char *cmd = json_get_string(msg, "cmd");
            if (!id || !cmd) {
                log_error("exec missing id or cmd");
                free(id); free(cmd); free(type); free(msg);
                continue;
            }

            int exitcode = -1;
            char *stderr_out = NULL;
            char *stdout_out = execute_system_command_fork(cmd, &exitcode, &stderr_out);
            if (!stdout_out) stdout_out = strdup("");

            char *esc_out = json_escape(stdout_out);
            char *esc_err = json_escape(stderr_out ? stderr_out : "");

            size_t resp_cap = strlen(esc_out) + strlen(esc_err) + strlen(id) + 256;
            char *resp = malloc(resp_cap);
            if (resp) {
                snprintf(resp, resp_cap,
                         "{\"type\":\"result\",\"id\":\"%s\",\"exit\":%d,\"stdout\":\"%s\",\"stderr\":\"%s\"}\n",
                         id, exitcode, esc_out, esc_err);
                ipc_send_full(sock, resp, strlen(resp));
                free(resp);
            } else {
                log_error("Failed to allocate response buffer");
            }

            free(esc_out);
            free(esc_err);
            free(stdout_out);
            if (stderr_out) free(stderr_out);
            free(id);
            free(cmd);
            free(type);
            free(msg);
            continue;
        } else if (strcmp(type, "status") == 0) {
            struct utsname info; char hostname[256] = "unknown";
            uname(&info); gethostname(hostname, sizeof(hostname) - 1);
            long cpus = 1, memory_mb = 0;
#ifdef __APPLE__
            int cpu_value = 1; uint64_t memory_value = 0;
            size_t cpu_size = sizeof(cpu_value), memory_size = sizeof(memory_value);
            if (sysctlbyname("hw.logicalcpu", &cpu_value, &cpu_size, NULL, 0) == 0) cpus = cpu_value;
            if (sysctlbyname("hw.memsize", &memory_value, &memory_size, NULL, 0) == 0) memory_mb = (long)(memory_value / 1024 / 1024);
#else
#ifdef _SC_NPROCESSORS_ONLN
            cpus = sysconf(_SC_NPROCESSORS_ONLN);
#endif
#if defined(_SC_PHYS_PAGES) && defined(_SC_PAGESIZE)
            long pages = sysconf(_SC_PHYS_PAGES), page_size = sysconf(_SC_PAGESIZE);
            memory_mb = pages > 0 && page_size > 0 ? (pages / 1024) * (page_size / 1024) : 0;
#endif
#endif
            char response[1024];
            struct timespec uptime = {0}; clock_gettime(CLOCK_MONOTONIC, &uptime);
            snprintf(response, sizeof(response),
                "{\"type\":\"status\",\"hostname\":\"%s\",\"kernel\":\"%s %s\",\"cpus\":%ld,\"memory_mb\":%ld,\"uptime_s\":%ld}\n",
                hostname, info.sysname, info.release, cpus, memory_mb, (long)uptime.tv_sec);
            ipc_send_full(sock, response, strlen(response));
            free(type); free(msg); continue;
        } else if (strcmp(type, "ping") == 0) {
            const char *pong = "{\"type\":\"pong\"}\n";
            ipc_send_full(sock, pong, strlen(pong));
            free(type);
            free(msg);
            continue;
        } else {
            log_info("Unknown message type from controller: %s", type);
            free(type);
            free(msg);
            continue;
        }
    }

    close(sock);
    log_info("Node agent run loop exiting");
}
