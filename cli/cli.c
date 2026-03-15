// Author: Pau Santana
// Date: 2026-03-15
// Latest revision: 2026-03-15


// Functions of this file are:
// 1. Read nodes from the config/nodes.txt file
// 2. Loop over the nodes
// 3. For each node, get the metrics
// 4. Send the results to the table.c file
// 5. Print the table.

#include <stdio.h>
#include "nodes.h"
#include "table.h"

int main(int argc, char *argv[]) {
    const char *nodes_file = (argc > 1) ? argv[1] : "config/nodes.txt";

    Node nodes[MAX_NODES];
    int node_count = loadNodes(nodes_file, nodes, MAX_NODES);
    
    if (node_count < 0) {
        fprintf(stderr, "Failed to load nodes from %s\n", nodes_file);
        return 1;
    }

    printf("Loaded %d nodes:\n", node_count);
    for (int i = 0; i < node_count; i++) {
        printf("%s\n", nodes[i].hostname);
    }

    // Here we would loop over the nodes and get the metrics for each node
    // For simplicity, we will just print the hostnames for now

    return 0;
}
