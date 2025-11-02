#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>        // For close(), read(), write(), fork()
#include <sys/types.h>     // For socket types
#include <sys/socket.h>    // For socket functions
#include <netinet/in.h>    // For sockaddr_in
#include <arpa/inet.h> 


char* execute_system_command(const char *command) {
    static char result[BUFFER_SIZE];
    FILE *fp;

    // Open a process to run the command and read its output
    fp = popen(command, "r");
    if (fp == NULL) {
        snprintf(result, sizeof(result), "Failed to run command\n");
        return result;
    }

    // Read command output into buffer
    memset(result, 0, sizeof(result));
    fread(result, 1, sizeof(result) - 1, fp);
    pclose(fp);

    // Return captured output
    return result;
}

// Function to start the TCP server (node agent)
void start_node_agent() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    char *response;

    // 1. Create TCP socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // 2. Configure the server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Listen on any network interface
    server_addr.sin_port = htons(PORT);       // Set port number (5050)

    // 3. Bind the socket to the specified IP and port
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 4. Start listening for incoming connections
    if (listen(server_fd, 5) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Node agent started. Listening on port %d...\n", PORT);

    // 5. Infinite loop to accept and handle connections
    while (1) {
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("Accept failed");
            continue;
        }

        printf("Connection accepted from %s\n", inet_ntoa(client_addr.sin_addr));

        // 6. Read command from client
        memset(buffer, 0, sizeof(buffer));
        read(client_fd, buffer, sizeof(buffer) - 1);
        printf("Received command: %s\n", buffer);

        // 7. Execute the command
        response = execute_system_command(buffer);

        // 8. Send back response to client
        write(client_fd, response, strlen(response));

        // 9. Close connection
        close(client_fd);
    }

    close(server_fd);
}