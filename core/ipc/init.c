#include <stdio.h>
#include "../include/init.h"
#include "../include/loop.h"
#include "../include/logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <poll.h>


int ipc_server_start(GlobalState *state) {

    if (state==NULL || state->config == NULL) {
        log_error("Error loading config");
        return -1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0 );

    if (sock < 0){
        log_error("Error creating connection");
    }

    int opt = 1;

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr;

    addr.sin_family = AF_INET6;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(state->config->listen_port || DEFAULT_NODE_PORT);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 1) {
        log_error("Error binding socket");
        close(sock);
        return -1;
    }

    if (listen(sock, SOMAXCONN)){
        og_error("Error getting socket flags: %s", strerror(errno));
        close(sock);
        return -1;
    }

    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) {
        log_error("Error getting socket flags: %s", strerror(errno));
        close(sock);
        return -1;
    }
    
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) {
        log_error("Error setting socket to non-blocking: %s", strerror(errno));
        close(sock);
        return -1;
    }

    // 8. Store in state and return
    // state->server_fd = sock;  // Assuming GlobalState has server_fd field
    
    log_info("IPC server started successfully on port %d", state->config->listen_port);
    
    return sock;
  
}

int ipc_server_stop(GlobalState *state, int *fd){
    close(fd);
    log_info("IPC server stopped");
     return 0;
}

int ipc_accept_connection(int server_fd){
    size_t addr_len = sizeof( struct sockaddr_in);
    int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);

    if (client_fd < 0){
        log_error("Error accepting client %d", strerror(errno));
        return -1;
    }

    return client_fd;
}

ssize_t ipc_send_full(int fd, const char *buf, size_t len){
    if (buf == NULL || len==0){
        log_error("invalid buf or length");
        return -1;
    }

    bool needs_newline = (buf[len - 1] != '\n');
    size_t total_len = len + (needs_newline ? 1 : 0);

    // Create a buffer with newline if needed
    char *send_buf;
    if (needs_newline) {
        send_buf = malloc(total_len);
        if (send_buf == NULL) {
            log_error("malloc failed");
            return -1;
        }
        memcpy(send_buf, buf, len);
        send_buf[len] = '\n';
    } else {
        send_buf = (char*)buf;  // Use original buffer
    }

    ssize_t total_sent = 0;

    while (total_len < len){
        ssize_t s = send(fd, buf + total_sent, len - total_sent, 0);

        if (s < 1) {
            log_error("Error accepting client %d", strerror(errno));
           if (errno == EINTR) continue;

           log_error("send() failed: %s", strerror(errno)); return -1;
        }
        total_sent += s;
    }

    return total_sent;
}

// char *ipc_recv_line(int fd, int timeout_ms){
//     if (fd == NULL || timeout_ms == NULL){
//         log_error("Error: All properties in ipc_recv_line are required.");
//         reurn NULL;
//     }
//     char tempbuf[256];
//     char *buffer = malloc(1024);
//     size_t buff_len = strlen(buffer);
//     size_t offset = 0;

//     for (int i = 0; i < buff_len; i++){
//         ssize_t n= recv(fd, tempbuf, 1, 0);

//         if (n == 0){
//             free(buffer);
//             return NULL;
//         }
//         memcpy(buffer + offset, tempbuf, n);
//         offset += n;

//         if (buffer[buff_len] == '\n'){
//             break;
//         }

//         if (offset + 256 > buff_len) {
//             buff_len *= 2;
//             buffer = realloc(buffer, buff_len);
//         }
//     }

   
//     return buffer; 

// }



char *ipc_recv_line(int fd, int timeout_ms) {
     if (fd == NULL || timeout_ms == NULL){
        log_error("Error: All properties in ipc_recv_line are required.");
        reurn NULL;
    }
    
    size_t capacity = 1024;
    size_t offset = 0;
    char *buffer = malloc(capacity);
    
    if (buffer == NULL) {
        log_error("malloc failed");
        return NULL;
    }

    if (timeout_ms > 0) {
        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLIN;
        
        int poll_result = poll(&pfd, 1, timeout_ms);
        
        if (poll_result < 0) {
            log_error("poll failed: %s", strerror(errno));
            free(buffer);
            return NULL;
        }
        
        if (poll_result == 0) {
            log_error("timeout waiting for data");
            free(buffer);
            return NULL;
        }
    }

    while (1) {
        char tempbuf[1];
        
        ssize_t n = recv(fd, tempbuf, 1, 0);
        
        if (n == 0) {
            log_error("connection closed by peer");
            free(buffer);
            return NULL;
        }
        
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            
            log_error("recv failed: %s", strerror(errno));
            free(buffer);
            return NULL;
        }
        
        buffer[offset] = tempbuf[0];
        offset++;
        if (tempbuf[0] == '\n') {
            break;
        }
        if (offset >= capacity - 1) {
            capacity *= 2;
            char *new_buffer = realloc(buffer, capacity);
            if (new_buffer == NULL) {
                log_error("realloc failed");
                free(buffer);
                return NULL;
            }
            buffer = new_buffer;
        }
    }
    
    buffer[offset] = '\0';
    if (offset > 0 && buffer[offset - 1] == '\n') {
        buffer[offset - 1] = '\0';
        offset--;
    }
    
    return buffer;
}
