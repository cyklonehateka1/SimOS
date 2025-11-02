#include "../include/env.h"
#include "../include/loop.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdbool.h>

Node* load_nodes_from_config(Config *config) {
    if (!config) return NULL;

    int count = 0;
    while (strlen(config->nodes[count].name) > 0) {
        count++;
        if (count >= 100) break;
    }

    if (count == 0) return NULL;

    Node *nodes = malloc(count * sizeof(Node));
    if (!nodes) return NULL;

    for (int i = 0; i < count; i++) {
        strcpy(nodes[i].name, config->nodes[i].name);
        strcpy(nodes[i].address, config->nodes[i].address);
        strcpy(nodes[i].os, config->nodes[i].os);
    }

    return nodes;
}

Node find_node_by_name(char *name){

    int count = 0;
    Node* all_nodes = load_nodes_from_config();

    while (strlen(config->nodes[count].name) > 0) {
        count++;
        if (count >= 100) break;
    }

    if (count == 0) return NULL;

    for (int i = 0; i < count; i++){
        if (all_nodes[i].name == name) {
            return all_nodes[i];
        } else{
            return NULL;
        }
    }


}

char* send_command_to_node(Node node, char *command) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0) { perror("socket"); exit(1); }

    struct sockaddr_in srv = {0};
    srv.sin_family = AF_INET;
    srv.sin_port = htons(9000);                    // port -> network order
    inet_pton(AF_INET, node.address, &srv.sin_addr);

    if (connect(sock, (struct sockaddr*)&srv, sizeof(srv)) < 0) {
        perror("connect"); close(sock); exit(1);
    }

    send(sock, command, sizeof(command), 0);

    // receive (may receive partial message)
    char buf[512];
    ssize_t n = recv(sock, buf, sizeof(buf)-1, 0);
    if (n > 0) { buf[n] = '\0'; printf("got: %s", buf); }

    close(sock); // hang up
    return buf;
}

void execute_node_command(GlobalState global_state, char *input_command){
    char *node_name = get_token(input_command, " ", 0);
    Node node = find_node_by_name(node_name);

    if (node == NULL){
        print("Node not found");
        exit(1);
    }

    char *response = send_command_to_node(node, input_command);
}

bool starts_with(const char *str, const char *prefix) {
    size_t len_prefix = strlen(prefix);
    size_t len_str = strlen(str);
    return len_str < len_prefix ? false : strncmp(str, prefix, len_prefix) == 0;
}

char *get_token(const char *input, const char *delimiter, int index) {
    // Make a modifiable copy of the original string
    char *copy = strdup(input);
    if (!copy) return NULL;

    char *token = strtok(copy, delimiter);
    int i = 0;

    while (token != NULL) {
        if (i == index) {
            char *result = strdup(token); // make a safe copy to return
            free(copy);                   // free the temporary copy
            return result;                // return the result (must be freed by caller)
        }
        token = strtok(NULL, delimiter);
        i++;
    }

    // If index not found
    free(copy);
    return NULL;
}