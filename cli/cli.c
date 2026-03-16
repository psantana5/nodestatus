#include <stdio.h>
#include <string.h>
#include "nodes.h"
#include "table.h"

static const char *extract_json_body(const char *http_response) {
    const char *body = strstr(http_response, "\r\n\r\n");
    if (!body) {
        return NULL;
    }
    body += 4;
    while (*body == ' ' || *body == '\n' || *body == '\r' || *body == '\t') {
        body++;
    }
    return (*body == '{') ? body : NULL;
}

int main()
{
    Node nodes[MAX_NODES];

    int nodeCount = loadNodes("config/nodes.txt", nodes, MAX_NODES);

    if (nodeCount == 0) {
        printf("No nodes found\n");
        return 1;
    }

    NodeStatus statuses[MAX_NODES];
    int ok_count = 0;
    int fail_count = 0;

    for (int i = 0; i < nodeCount; i++) {
        NodeStatus *status = &statuses[i];
        strncpy(status->hostname, nodes[i].hostname, sizeof(status->hostname) - 1);
        status->hostname[sizeof(status->hostname) - 1] = '\0';
        status->state = FETCH_IO_ERROR;
        status->has_metrics = 0;
        status->cpu_percent = 0.0f;
        status->mem_percent = 0.0f;
        status->load = 0.0f;

        FetchResult fetch_result = fetchStatus(nodes[i].hostname, NODE_TIMEOUT_SECONDS);
        status->state = fetch_result.state;

        if (fetch_result.state == FETCH_OK) {
            const char *json_body = extract_json_body(fetch_result.response);
            if (json_body && parseJsonMetrics(json_body, status) == 0) {
                status->has_metrics = 1;
                ok_count++;
            } else {
                status->state = FETCH_PARSE_ERROR;
                fail_count++;
            }
        } else {
            fail_count++;
        }
    }

    printTableHeader();
    for (int i = 0; i < nodeCount; i++) {
        printTableRow(&statuses[i]);
    }
    printTableFooter(ok_count, fail_count);

    return 0;
}
