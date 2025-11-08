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

#include "../include/logging.h"
#include "../include/ipc.h"

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

    struct sockaddr_in srv;
    memset(&srv, 0, sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_port = htons((uint16_t)controller_port);

    if (inet_pton(AF_INET, controller_host, &srv.sin_addr) != 1) {
        log_error("inet_pton failed for %s", controller_host);
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&srv, sizeof(srv)) < 0) {
        log_error("connect to %s:%d failed: %s", controller_host, controller_port, strerror(errno));
        close(sock);
        return -1;
    }

    // Optionally set blocking mode (we expect blocking recv for run loop)
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags >= 0) {
        flags &= ~O_NONBLOCK;
        fcntl(sock, F_SETFL, flags);
    }

    log_info("Connected to controller %s:%d (fd=%d)", controller_host, controller_port, sock);
    return sock;
}

/*
 * node_agent_register
 *  - sock: connected socket to controller
 *  - node_name: identifier string
 *  - osstr: OS string (e.g. "linux")
 * Sends:
 *   {"type":"hello","name":"node_name","os":"osstr","address":"<ip>"}
 * and waits for optional ack (2s timeout). Returns 0 on success, -1 on failure.
 */
int node_agent_register(int sock, const char *node_name, const char *osstr) {
    if (sock < 0 || !node_name) {
        log_error("node_agent_register: invalid args");
        return -1;
    }

    // Build hello JSON. Address field uses localhost placeholder — the node can later improve this.
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

    // Optionally wait for ack (non-mandatory). We'll wait up to 2000ms for a response.
    char *reply = ipc_recv_line(sock, 2000);
    if (!reply) {
        // no reply — still OK; controller may not ack
        log_info("No ack received after hello (continuing)");
        return 0;
    }

    // If reply contains type:"ack" and status:"ok", treat as success
    if (strstr(reply, "\"type\":\"ack\"") && strstr(reply, "\"status\":\"ok\"")) {
        free(reply);
        log_info("Registration acknowledged by controller");
        return 0;
    }

    // Otherwise it's not an ack — still continue but log it
    log_info("Registration reply: %s", reply);
    free(reply);
    return 0;
}

/*
 * execute_system_command_fork
 *  - cmd: shell command string
 *  - out_exitcode: pointer to int to store exit code (may be NULL)
 *  - out_stderr: pointer to char* to receive heap-allocated stderr (must be freed by caller). May be NULL if user not interested.
 * Returns heap-allocated stdout string (caller must free). On error returns strdup("") and sets exit code to 127.
 */
char *execute_system_command_fork(const char *cmd, int *out_exitcode, char **out_stderr) {
    if (!cmd) {
        if (out_exitcode) *out_exitcode = 127;
        if (out_stderr) *out_stderr = strdup("");
        return strdup("");
    }

    int outpipe[2];
    int errpipe[2];
    if (pipe(outpipe) != 0) {
        log_error("pipe(out) failed: %s", strerror(errno));
        if (out_exitcode) *out_exitcode = 127;
        if (out_stderr) *out_stderr = strdup("pipe failed");
        return strdup("");
    }
    if (pipe(errpipe) != 0) {
        close(outpipe[0]); close(outpipe[1]);
        log_error("pipe(err) failed: %s", strerror(errno));
        if (out_exitcode) *out_exitcode = 127;
        if (out_stderr) *out_stderr = strdup("pipe failed");
        return strdup("");
    }

    pid_t pid = fork();
    if (pid < 0) {
        // fork failed
        close(outpipe[0]); close(outpipe[1]);
        close(errpipe[0]); close(errpipe[1]);
        log_error("fork() failed: %s", strerror(errno));
        if (out_exitcode) *out_exitcode = 127;
        if (out_stderr) *out_stderr = strdup("fork failed");
        return strdup("");
    }

    if (pid == 0) {
        // Child: redirect stdout and stderr and exec shell
        // Close read ends
        close(outpipe[0]);
        close(errpipe[0]);

        // Dup write ends to stdout and stderr
        dup2(outpipe[1], STDOUT_FILENO);
        dup2(errpipe[1], STDERR_FILENO);

        // Close original write fds if they are not STDOUT/STDERR
        close(outpipe[1]);
        close(errpipe[1]);

        // Execute command via shell
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);

        // If exec fails:
        _exit(127);
    } else {
        // Parent: close write ends and read child's stdout/stderr
        close(outpipe[1]);
        close(errpipe[1]);

        // Read stdout
        size_t out_cap = 4096;
        size_t out_len = 0;
        char *out_buf = malloc(out_cap);
        if (!out_buf) {
            out_buf = strdup("");
        } else {
            ssize_t r;
            while ((r = read(outpipe[0], out_buf + out_len, out_cap - out_len - 1)) > 0) {
                out_len += (size_t)r;
                if (out_len + 256 >= out_cap) {
                    out_cap *= 2;
                    char *n = realloc(out_buf, out_cap);
                    if (!n) break;
                    out_buf = n;
                }
            }
            out_buf[out_len] = '\0';
        }
        close(outpipe[0]);

        // Read stderr
        size_t err_cap = 1024;
        size_t err_len = 0;
        char *err_buf = malloc(err_cap);
        if (!err_buf) {
            err_buf = strdup("");
        } else {
            ssize_t r;
            while ((r = read(errpipe[0], err_buf + err_len, err_cap - err_len - 1)) > 0) {
                err_len += (size_t)r;
                if (err_len + 256 >= err_cap) {
                    err_cap *= 2;
                    char *n = realloc(err_buf, err_cap);
                    if (!n) break;
                    err_buf = n;
                }
            }
            err_buf[err_len] = '\0';
        }
        close(errpipe[0]);

        // Wait for child
        int status = 0;
        waitpid(pid, &status, 0);
        if (out_exitcode) {
            if (WIFEXITED(status)) *out_exitcode = WEXITSTATUS(status);
            else *out_exitcode = 127;
        }

        if (out_stderr) *out_stderr = err_buf;
        else free(err_buf);

        if (!out_buf) return strdup("");
        return out_buf;
    }
}

/*
 * naive helper: escape JSON string (replace " \ and newline)
 * returns heap-allocated escaped string (caller frees)
 */
static char *escape_json_string(const char *in) {
    if (!in) return strdup("");
    size_t len = strlen(in);
    size_t cap = len * 2 + 16;
    char *out = malloc(cap);
    if (!out) return strdup("");
    size_t o = 0;
    for (size_t i = 0; i < len; ++i) {
        char c = in[i];
        if (c == '"') {
            if (o + 2 >= cap) { cap *= 2; out = realloc(out, cap); if (!out) return strdup(""); }
            out[o++] = '\\'; out[o++] = '"';
        } else if (c == '\\') {
            if (o + 2 >= cap) { cap *= 2; out = realloc(out, cap); if (!out) return strdup(""); }
            out[o++] = '\\'; out[o++] = '\\';
        } else if (c == '\n') {
            if (o + 2 >= cap) { cap *= 2; out = realloc(out, cap); if (!out) return strdup(""); }
            out[o++] = '\\'; out[o++] = 'n';
        } else {
            if (o + 1 >= cap) { cap *= 2; out = realloc(out, cap); if (!out) return strdup(""); }
            out[o++] = c;
        }
    }
    out[o] = '\0';
    return out;
}

/*
 * naive JSON extraction helpers (look for "key":"value")
 * returns heap-allocated value or NULL
 */
static char *json_get_str(const char *json, const char *key) {
    if (!json || !key) return NULL;
    char needle[128];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    char *pos = strstr(json, needle);
    if (!pos) return NULL;
    char *colon = strchr(pos, ':');
    if (!colon) return NULL;
    // find first quote after colon
    char *firstq = strchr(colon, '\"');
    if (!firstq) return NULL;
    char *secondq = strchr(firstq + 1, '\"');
    if (!secondq) return NULL;
    size_t n = (size_t)(secondq - (firstq + 1));
    char *out = malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, firstq + 1, n);
    out[n] = '\0';
    return out;
}

/*
 * node_agent_run_loop
 *  - sock: connected controller socket
 *  - node_name: node identifier (used in logs)
 *
 * This will block and run until controller closes connection or an error occurs.
 */
void node_agent_run_loop(int sock, const char *node_name) {
    if (sock < 0) {
        log_error("node_agent_run_loop: invalid socket");
        return;
    }

    log_info("Node agent [%s] entering run loop (fd=%d)", node_name ? node_name : "<anon>", sock);

    while (1) {
        char *msg = ipc_recv_line(sock, -1); // block indefinitely
        if (!msg) {
            log_info("Controller closed connection or recv error; exiting run loop");
            break;
        }

        // parse "type"
        char *type = json_get_str(msg, "type");
        if (!type) {
            log_error("Malformed message (no type): %s", msg);
            free(msg);
            continue;
        }

        if (strcmp(type, "exec") == 0) {
            char *id = json_get_str(msg, "id");
            char *cmd = json_get_str(msg, "cmd");
            if (!id || !cmd) {
                log_error("exec missing id or cmd");
                free(id); free(cmd); free(type); free(msg);
                continue;
            }

            int exitcode = -1;
            char *stderr_out = NULL;
            char *stdout_out = execute_system_command_fork(cmd, &exitcode, &stderr_out);
            if (!stdout_out) stdout_out = strdup("");

            // escape outputs
            char *esc_out = escape_json_string(stdout_out);
            char *esc_err = escape_json_string(stderr_out ? stderr_out : "");

            // build response JSON: {"type":"result","id":"...", "exit":0, "stdout":"...", "stderr":"..."}
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
        } else if (strcmp(type, "ping") == 0) {
            // send pong
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