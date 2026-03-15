#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nodes.h"

int loadNodes(const char *filename, Node nodes[], int max_nodes) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("fopen");
        return -1;
    }

    char line[256];
    int count = 0;

    while (fgets(line, sizeof(line), file) && count < max_nodes) {
        // Remove newline character if present
        line[strcspn(line, "\r\n")] = 0;
        strncpy(nodes[count].hostname, line, MAX_HOSTNAME - 1);
        nodes[count].hostname[MAX_HOSTNAME - 1] = '\0'; // Ensure null-termination
        count++;
    }

    fclose(file);
    return count;
}