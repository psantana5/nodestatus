#include <stdio.h>
#include <string.h>
#include "nodes.h"
#include "table.h"

int main()
{
    Node nodes[MAX_NODES];

    int nodeCount = loadNodes("config/nodes.txt", nodes, MAX_NODES);

    if (nodeCount == 0) {
        printf("No nodes found\n");
        return 1;
    }

    NodeStatus statuses[MAX_NODES];
    int successful = 0;

    // Query all nodes (silently)
    for (int i = 0; i < nodeCount; i++) {
        char response[2048];
        int bytes = fetchStatus(nodes[i].hostname, response, sizeof(response));

        if (bytes > 0) {
            // Extract JSON body from HTTP response
            char *json_start = strstr(response, "\r\n\r\n");
            if (json_start) {
                json_start += 4;
                
                // Find the start of JSON (first { character)
                while (*json_start && *json_start != '{') {
                    json_start++;
                }
                
                if (*json_start == '{') {
                    // Copy hostname
                    strncpy(statuses[successful].hostname, nodes[i].hostname, sizeof(statuses[successful].hostname) - 1);
                    statuses[successful].hostname[sizeof(statuses[successful].hostname) - 1] = '\0';
                    
                    // Parse metrics from JSON
                    if (parseJsonMetrics(json_start, &statuses[successful]) == 0) {
                        successful++;
                    }
                }
            }
        }
    }

    // Display results as table
    if (successful > 0) {
        printTableHeader();
        for (int i = 0; i < successful; i++) {
            printTableRow(statuses[i]);
        }
        printTableFooter();
    } else {
        printf("No nodes responded\n");
    }

    return 0;
}
