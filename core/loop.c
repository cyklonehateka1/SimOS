#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>

#include "../include/loop.h"
#include "../include/logging.h"
#include "../include/init.h"
#include "../include/node_manager.h"
#include "../include/cli.h"

GlobalState init_global_state(void) {
    GlobalState state;
    state.config = NULL;
    return state;
}

void run_event_loop(GlobalState *state) {

    if (state == NULL || state->config != NULL){ 
        log_error("Invalid config");
        exit(-1);
    }

    int server_fd = ipc_server_start(state);
    if (server_fd < 0){
        log_error("Error creating socket");
        exit(-1);
    }

    node_sessions_init();

    int max_fds = 1;

    struct pollfd pfds[max_fds];

    while(1){
        int nfds = 0;
        pfds[nfds].fd = STDIN_FILENO;
        pfds[nfds].events = POLLIN;
        nfds++;

        pfds[nfds].fd = server_fd;
        pfds[nfds].events = POLLIN;
        nfds++;

        nfds += node_sessions_fill_pollfds(&pfds[nfds], max_fds - nfds);
        int timeout_ms = 1000;

        int rc = poll(pfds, nfds, timeout_ms);
        if (rc < 0){
            if (errno==EINTR) continue;
            log_error("Something went wrong");
            break;
        }

        if (rc == 0){
           continue;
        }
        
        int idx = 0;

        // Check STDIN
        if (pfds[idx].revents & POLLIN) {
            // Read one line from stdin
            char *line = NULL;
            size_t len = 0;
            ssize_t nread = getline(&line, &len, stdin);
            
            if (nread > 0) {
                // Trim newline
                if (line[nread - 1] == '\n') {
                    line[nread - 1] = '\0';
                }
                
                // Parse and execute the command
                parse_cli_command(line);
            }
            
            // Free the line allocated by getline
            free(line);
        }
        idx++;

        if (pfds[idx].revents & POLLIN){
            int new_fd = ipc_accept_connection(server_fd);
            
            if (new_fd >= 0){
                char *hello = ipc_recv_line(new_fd, 2000);
                if (hello == NULL) {
                    close(new_fd);
                }else{
                    // parse hello JSON -> Node hello_meta
                    Node hello_meta = parse_hello(hello); // implement below
                    node_session_add(&hello_meta, new_fd);
                    free(hello);
                }
                   
            }
            idx++;

        }

        for (p = idx..nfds-1){
            if (pfds[p].revents & (POLLIN | POLLPRI)){
                int fd = pfds[p].fd;
                char *msg = ipc_recv_line(fd, 0);

                if (msg == NULL){
                    node_session_remove_by_fd(fd);
                } else{
                handle_node_message(fd, msg, state);
                free(msg);
            } 

            }else if (pfds[p].revents & (POLLHUP | POLLERR | POLLNVAL)){
                node_session_remove_by_fd(pfds[p].fd);
            }
        }
    }

    ipc_server_stop(state, server_fd) || close(server_fd);
    node_sessions_cleanup();
    
}
