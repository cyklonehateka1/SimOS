#ifndef NODE_AGENT_H
#define NODE_AGENT_H

int node_agent_connect(const char *host, int port);
int node_agent_register(int sock, const char *name, const char *os_name);
void node_agent_run_loop(int sock, const char *name);

#endif
