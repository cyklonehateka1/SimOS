#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 2048
#define PORT 5050

char* execute_system_command(const char *command) {
    static char result[BUFFER_SIZE]; 
    FILE *fp;

    memset(result, 0, sizeof(result)); 

    fp = popen(command, "r");
    if (fp == NULL) {
        snprintf(result, sizeof(result), "Failed to run command\n");
        return result;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strlen(result) + strlen(line) < sizeof(result) - 1) {
            strcat(result, line);
        } else {
            break;
        }
    }

    pclose(fp);
    return result;
}

void start_node_agent() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    char *response;
    int opt = 1;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Node agent started. Listening on port %d...\n", PORT);

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("Accept failed");
            continue;
        }

        printf("Connection accepted from %s\n", inet_ntoa(client_addr.sin_addr));

        memset(buffer, 0, sizeof(buffer));
        ssize_t n = read(client_fd, buffer, sizeof(buffer) - 1);
        if (n <= 0) {
            close(client_fd);
            continue;
        }

        printf("Received command: %s\n", buffer);

        response = execute_system_command(buffer);

        write(client_fd, response, strlen(response));

        close(client_fd);
    }

    close(server_fd);
}
