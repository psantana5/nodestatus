#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "nodes.h"
#include "table.h"

typedef struct {
    const Node *node;
    NodeStatus *status;
} QueryTask;

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

static void query_node(const Node *node, NodeStatus *status) {
    strncpy(status->hostname, node->hostname, sizeof(status->hostname) - 1);
    status->hostname[sizeof(status->hostname) - 1] = '\0';
    status->state = FETCH_IO_ERROR;
    status->has_metrics = 0;
    status->cpu_percent = 0.0f;
    status->mem_percent = 0.0f;
    status->load = 0.0f;
    status->disk_mb_s = 0.0f;

    FetchResult fetch_result = fetchStatus(node->hostname, NODE_TIMEOUT_SECONDS);
    status->state = fetch_result.state;

    if (fetch_result.state == FETCH_OK) {
        const char *json_body = extract_json_body(fetch_result.response);
        if (json_body && parseJsonMetrics(json_body, status) == 0) {
            status->has_metrics = 1;
            return;
        }
        status->state = FETCH_PARSE_ERROR;
    }
}

static void *query_node_thread(void *arg) {
    QueryTask *task = (QueryTask *)arg;
    query_node(task->node, task->status);
    return NULL;
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
    QueryTask tasks[MAX_NODES];
    pthread_t threads[MAX_NODES];
    int thread_started[MAX_NODES];

    for (int i = 0; i < nodeCount; i++) {
        tasks[i].node = &nodes[i];
        tasks[i].status = &statuses[i];
        thread_started[i] = 0;

        if (pthread_create(&threads[i], NULL, query_node_thread, &tasks[i]) == 0) {
            thread_started[i] = 1;
        } else {
            query_node(&nodes[i], &statuses[i]);
        }
    }

    for (int i = 0; i < nodeCount; i++) {
        if (thread_started[i]) {
            (void)pthread_join(threads[i], NULL);
        }
    }

    int ok_count = 0;
    int fail_count = 0;
    for (int i = 0; i < nodeCount; i++) {
        if (statuses[i].has_metrics) {
            ok_count++;
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
