#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include "nodes.h"
#include "table.h"

typedef struct {
    const Node *node;
    NodeStatus *status;
    int timeout_ms;
    int *persistent_sockfd;
} QueryTask;

typedef struct {
    const Node *node;
    int sockfd;
} NodeConnection;

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

static void query_node(const Node *node, NodeStatus *status, int timeout_ms, int *persistent_sockfd) {
    strncpy(status->hostname, node->hostname, sizeof(status->hostname) - 1);
    status->hostname[sizeof(status->hostname) - 1] = '\0';
    status->state = FETCH_IO_ERROR;
    status->latency_ms = 0;
    status->has_metrics = 0;
    status->cpu_percent = 0.0f;
    status->mem_percent = 0.0f;
    status->load = 0.0f;
    status->disk_mb_s = 0.0f;

    FetchResult fetch_result = (persistent_sockfd == NULL)
        ? fetchStatus(node->hostname, timeout_ms)
        : fetchStatusWithConnection(node->hostname, timeout_ms, persistent_sockfd);
    status->state = fetch_result.state;
    status->latency_ms = fetch_result.latency_ms;

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
    query_node(task->node, task->status, task->timeout_ms, task->persistent_sockfd);
    return NULL;
}

static void render_status_table(const Node nodes[], int nodeCount, int timeout_ms, NodeConnection *connections) {
    NodeStatus statuses[MAX_NODES];
    QueryTask tasks[MAX_NODES];
    pthread_t threads[MAX_NODES];
    int thread_started[MAX_NODES];

    for (int i = 0; i < nodeCount; i++) {
        tasks[i].node = &nodes[i];
        tasks[i].status = &statuses[i];
        tasks[i].timeout_ms = timeout_ms;
        tasks[i].persistent_sockfd = connections ? &connections[i].sockfd : NULL;
        thread_started[i] = 0;

        if (pthread_create(&threads[i], NULL, query_node_thread, &tasks[i]) == 0) {
            thread_started[i] = 1;
        } else {
            query_node(
                &nodes[i],
                &statuses[i],
                timeout_ms,
                connections ? &connections[i].sockfd : NULL
            );
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
}

int main(int argc, char *argv[])
{
    Node nodes[MAX_NODES];
    NodeConnection connections[MAX_NODES];
    const char *group_filter = NULL;
    int watch_mode = 0;

    if (argc == 1) {
        /* default: one-shot status for all nodes */
    } else if (strcmp(argv[1], "status") == 0) {
        if (argc == 3) {
            group_filter = argv[2];
        } else if (argc > 3) {
            printf("Usage: %s [status [group]|watch [group]]\n", argv[0]);
            return 1;
        }
    } else if (strcmp(argv[1], "watch") == 0) {
        watch_mode = 1;
        if (argc == 3) {
            group_filter = argv[2];
        } else if (argc > 3) {
            printf("Usage: %s [status [group]|watch [group]]\n", argv[0]);
            return 1;
        }
    } else {
        printf("Usage: %s [status [group]|watch [group]]\n", argv[0]);
        return 1;
    }

    int nodeCount = loadNodesByGroup(INVENTORY_FILE, nodes, MAX_NODES, group_filter);
    if (nodeCount <= 0) {
        if (group_filter) {
            printf("No nodes found for group '%s'\n", group_filter);
        } else {
            printf("No nodes found\n");
        }
        return 1;
    }

    for (int i = 0; i < nodeCount; i++) {
        connections[i].node = &nodes[i];
        connections[i].sockfd = -1;
    }

    if (!watch_mode) {
        render_status_table(nodes, nodeCount, NODE_CONNECT_TIMEOUT_MS, NULL);
        return 0;
    }

    printf("\033[2J");
    while (1) {
        printf("\033[H");
        if (group_filter) {
            printf("nodectl watch %s (refresh every %d ms)\n\n", group_filter, NODE_WATCH_INTERVAL_MS);
        } else {
            printf("nodectl watch (refresh every %d ms)\n\n", NODE_WATCH_INTERVAL_MS);
        }
        render_status_table(nodes, nodeCount, NODE_CONNECT_TIMEOUT_MS, connections);

        struct timespec delay = {
            NODE_WATCH_INTERVAL_MS / 1000,
            (NODE_WATCH_INTERVAL_MS % 1000) * 1000000L
        };
        (void)nanosleep(&delay, NULL);
    }

    for (int i = 0; i < nodeCount; i++) {
        if (connections[i].sockfd >= 0) {
            close(connections[i].sockfd);
        }
    }
    return 0;
}
