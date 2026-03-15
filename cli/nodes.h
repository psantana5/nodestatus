// Here we will define the struct for nodes
#ifndef NODES_H
#define NODES_H

#define MAX_NODES 128
#define MAX_HOSTNAME 256

typedef struct {
    char hostname[MAX_HOSTNAME];
} Node;

int loadNodes(const char *filename, Node *nodes, int max_nodes);
int fetchStatus(const char *hostname, char *response, int response_size);

#endif // NODES_H
