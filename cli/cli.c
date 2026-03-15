#include <stdio.h>
#include <string.h>
#include "nodes.h"

int main()
{
    Node nodes[MAX_NODES];

    int nodeCount = loadNodes("config/nodes.txt", nodes, MAX_NODES);

    if (nodeCount == 0) {
        printf("No nodes found\n");
        return 1;
    }

    printf("Loaded %d nodes\n\n", nodeCount);

    for (int i = 0; i < nodeCount; i++) {
        printf("Querying %s\n", nodes[i].hostname);

        char response[2048];
        int bytes = fetchStatus(nodes[i].hostname, response, sizeof(response));

        if (bytes > 0) {
            // Parse HTTP response to extract JSON body
            // Format: headers\r\n\r\nJSON_body
            char *json_start = strstr(response, "\r\n\r\n");
            if (json_start) {
                printf("Status: %s\n", json_start + 4);
            } else {
                printf("Response: %s\n", response);
            }
        } else {
            printf("Failed to fetch status\n");
        }

        printf("\n");
    }

    return 0;
}
