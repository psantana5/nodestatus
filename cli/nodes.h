// Here we will define the struct for nodes
#ifndef NODES_H
#define NODES_H

#define MAX_NODES 128
#define MAX_HOSTNAME 256

typedef struct {
    char hostname[MAX_HOSTNAME];
} Node;

int loadNodes(const char *filename, Node *nodes, int max_nodes);

#endif // NODES_H