#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
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

typedef enum {
    SORT_BY_HOST = 0,
    SORT_BY_RESP,
    SORT_BY_STATE
} SortMode;

typedef enum {
    VIEW_DEFAULT = 0,
    VIEW_CPU,
    VIEW_MEM,
    VIEW_DISK,
    VIEW_NET
} ViewMode;

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
    int max_host_chars = (int)sizeof(status->hostname) - 7; /* ':' + max 5-digit port + '\0' */
    if (max_host_chars < 1) {
        max_host_chars = 1;
    }
    (void)snprintf(status->hostname, sizeof(status->hostname), "%.*s:%d", max_host_chars, node->hostname, node->port);
    status->state = FETCH_IO_ERROR;
    status->latency_ms = 0;
    status->has_metrics = 0;
    status->cpu_percent = 0.0f;
    status->mem_percent = 0.0f;
    status->load = 0.0f;
    status->disk_mb_s = 0.0f;

    FetchResult fetch_result = (persistent_sockfd == NULL)
        ? fetchStatus(node->hostname, node->port, timeout_ms)
        : fetchStatusWithConnection(node->hostname, node->port, timeout_ms, persistent_sockfd);
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

static int state_sort_rank(FetchState state) {
    switch (state) {
        case FETCH_OK:
            return 0;
        case FETCH_PARSE_ERROR:
            return 1;
        case FETCH_HTTP_ERROR:
            return 2;
        case FETCH_TIMEOUT:
            return 3;
        case FETCH_DNS_ERROR:
            return 4;
        case FETCH_CONNECT_ERROR:
            return 5;
        case FETCH_IO_ERROR:
            return 6;
        default:
            return 7;
    }
}

static int compare_status_by_host(const void *a, const void *b) {
    const NodeStatus *sa = (const NodeStatus *)a;
    const NodeStatus *sb = (const NodeStatus *)b;

    int sa_unhealthy = (sa->state != FETCH_OK);
    int sb_unhealthy = (sb->state != FETCH_OK);
    if (sa_unhealthy != sb_unhealthy) {
        return sb_unhealthy - sa_unhealthy; /* unhealthy first */
    }

    return strcmp(sa->hostname, sb->hostname);
}

static int compare_status_by_resp(const void *a, const void *b) {
    const NodeStatus *sa = (const NodeStatus *)a;
    const NodeStatus *sb = (const NodeStatus *)b;

    int sa_unhealthy = (sa->state != FETCH_OK);
    int sb_unhealthy = (sb->state != FETCH_OK);
    if (sa_unhealthy != sb_unhealthy) {
        return sb_unhealthy - sa_unhealthy; /* unhealthy first */
    }

    int sa_bucket = sa->latency_ms / 10;
    int sb_bucket = sb->latency_ms / 10;
    if (sa_bucket != sb_bucket) {
        return sa_bucket - sb_bucket; /* lower bucket first */
    }

    if (sa->latency_ms != sb->latency_ms) {
        return sa->latency_ms - sb->latency_ms;
    }

    if (sa->state != sb->state) {
        return state_sort_rank(sa->state) - state_sort_rank(sb->state);
    }

    return strcmp(sa->hostname, sb->hostname);
}

static int compare_status_by_state(const void *a, const void *b) {
    const NodeStatus *sa = (const NodeStatus *)a;
    const NodeStatus *sb = (const NodeStatus *)b;
    int sa_unhealthy = (sa->state != FETCH_OK);
    int sb_unhealthy = (sb->state != FETCH_OK);
    if (sa_unhealthy != sb_unhealthy) {
        return sb_unhealthy - sa_unhealthy; /* unhealthy first */
    }

    if (!sa_unhealthy && sa->load != sb->load) {
        return (sa->load < sb->load) ? 1 : -1; /* higher load first for healthy nodes */
    }

    int rank_diff = state_sort_rank(sa->state) - state_sort_rank(sb->state);
    if (rank_diff != 0) {
        return rank_diff;
    }

    if (sa->latency_ms != sb->latency_ms) {
        return sb->latency_ms - sa->latency_ms; /* slower first inside same bucket */
    }
    return strcmp(sa->hostname, sb->hostname);
}

static const char *sort_mode_to_string(SortMode mode) {
    switch (mode) {
        case SORT_BY_RESP:
            return "resp";
        case SORT_BY_STATE:
            return "state";
        case SORT_BY_HOST:
        default:
            return "host";
    }
}

static int parse_sort_mode(const char *value, SortMode *mode_out) {
    if (!value || !mode_out) {
        return -1;
    }
    if (strcmp(value, "host") == 0) {
        *mode_out = SORT_BY_HOST;
        return 0;
    }
    if (strcmp(value, "resp") == 0) {
        *mode_out = SORT_BY_RESP;
        return 0;
    }
    if (strcmp(value, "state") == 0) {
        *mode_out = SORT_BY_STATE;
        return 0;
    }
    return -1;
}

static void sort_statuses(NodeStatus statuses[], int count, SortMode sort_mode) {
    if (count <= 1) {
        return;
    }
    switch (sort_mode) {
        case SORT_BY_RESP:
            qsort(statuses, (size_t)count, sizeof(NodeStatus), compare_status_by_resp);
            break;
        case SORT_BY_STATE:
            qsort(statuses, (size_t)count, sizeof(NodeStatus), compare_status_by_state);
            break;
        case SORT_BY_HOST:
        default:
            qsort(statuses, (size_t)count, sizeof(NodeStatus), compare_status_by_host);
            break;
    }
}

static void render_status_table(const Node nodes[], int nodeCount, int timeout_ms, NodeConnection *connections, SortMode sort_mode) {
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

    sort_statuses(statuses, nodeCount, sort_mode);

    printTableHeader();
    for (int i = 0; i < nodeCount; i++) {
        printTableRow(&statuses[i]);
    }
    printTableFooter(ok_count, fail_count);
}

static void render_cpu_detail(Node *nodes, int nodeCount, int timeout_ms, NodeConnection *connections, SortMode sort_mode) {
    CpuDetailStatus statuses[MAX_NODES];
    
    for (int i = 0; i < nodeCount; i++) {
        memset(&statuses[i], 0, sizeof(CpuDetailStatus));
        snprintf(statuses[i].hostname, sizeof(statuses[i].hostname), "%s:%d", 
                 nodes[i].hostname, nodes[i].port);
        
        FetchResult result = fetchStatusWithConnection(nodes[i].hostname, nodes[i].port, timeout_ms, 
                                                        connections ? &connections[i].sockfd : NULL);
        statuses[i].state = result.state;
        
        if (result.state == FETCH_OK && result.bytes_received > 0) {
            const char *json = extract_json_body(result.response);
            if (json && parseJsonCpuDetail(json, &statuses[i]) == 0) {
                statuses[i].has_metrics = 1;
            }
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

    printCpuDetailHeader();
    for (int i = 0; i < nodeCount; i++) {
        printCpuDetailRow(&statuses[i]);
    }
    printTableFooter(ok_count, fail_count);
}

static void render_mem_detail(Node *nodes, int nodeCount, int timeout_ms, NodeConnection *connections, SortMode sort_mode) {
    MemDetailStatus statuses[MAX_NODES];
    
    for (int i = 0; i < nodeCount; i++) {
        memset(&statuses[i], 0, sizeof(MemDetailStatus));
        snprintf(statuses[i].hostname, sizeof(statuses[i].hostname), "%s:%d", 
                 nodes[i].hostname, nodes[i].port);
        
        FetchResult result = fetchStatusWithConnection(nodes[i].hostname, nodes[i].port, timeout_ms, 
                                                        connections ? &connections[i].sockfd : NULL);
        statuses[i].state = result.state;
        
        if (result.state == FETCH_OK && result.bytes_received > 0) {
            const char *json = extract_json_body(result.response);
            if (json && parseJsonMemDetail(json, &statuses[i]) == 0) {
                statuses[i].has_metrics = 1;
            }
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

    printMemDetailHeader();
    for (int i = 0; i < nodeCount; i++) {
        printMemDetailRow(&statuses[i]);
    }
    printTableFooter(ok_count, fail_count);
}

static void render_disk_detail(Node *nodes, int nodeCount, int timeout_ms, NodeConnection *connections, SortMode sort_mode) {
    DiskDetailStatus statuses[MAX_NODES];
    
    for (int i = 0; i < nodeCount; i++) {
        memset(&statuses[i], 0, sizeof(DiskDetailStatus));
        snprintf(statuses[i].hostname, sizeof(statuses[i].hostname), "%s:%d", 
                 nodes[i].hostname, nodes[i].port);
        
        FetchResult result = fetchStatusWithConnection(nodes[i].hostname, nodes[i].port, timeout_ms, 
                                                        connections ? &connections[i].sockfd : NULL);
        statuses[i].state = result.state;
        
        if (result.state == FETCH_OK && result.bytes_received > 0) {
            const char *json = extract_json_body(result.response);
            if (json && parseJsonDiskDetail(json, &statuses[i]) == 0) {
                statuses[i].has_metrics = 1;
            }
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

    printDiskDetailHeader();
    for (int i = 0; i < nodeCount; i++) {
        printDiskDetailRow(&statuses[i]);
    }
    printTableFooter(ok_count, fail_count);
}

static void render_net_detail(Node *nodes, int nodeCount, int timeout_ms, NodeConnection *connections, SortMode sort_mode) {
    NetDetailStatus statuses[MAX_NODES];
    
    for (int i = 0; i < nodeCount; i++) {
        memset(&statuses[i], 0, sizeof(NetDetailStatus));
        snprintf(statuses[i].hostname, sizeof(statuses[i].hostname), "%s:%d", 
                 nodes[i].hostname, nodes[i].port);
        
        FetchResult result = fetchStatusWithConnection(nodes[i].hostname, nodes[i].port, timeout_ms, 
                                                        connections ? &connections[i].sockfd : NULL);
        statuses[i].state = result.state;
        
        if (result.state == FETCH_OK && result.bytes_received > 0) {
            const char *json = extract_json_body(result.response);
            if (json && parseJsonNetDetail(json, &statuses[i]) == 0) {
                statuses[i].has_metrics = 1;
            }
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

    printNetDetailHeader();
    for (int i = 0; i < nodeCount; i++) {
        printNetDetailRow(&statuses[i]);
    }
    printTableFooter(ok_count, fail_count);
}

int main(int argc, char *argv[])
{
    Node nodes[MAX_NODES];
    NodeConnection connections[MAX_NODES];
    const char *group_filter = NULL;
    int watch_mode = 0;
    SortMode sort_mode = SORT_BY_STATE;
    ViewMode view_mode = VIEW_DEFAULT;
    int colors_enabled = 1;

    if (argc == 1) {
        /* default: one-shot status for all nodes */
    } else if (strcmp(argv[1], "status") == 0 || strcmp(argv[1], "watch") == 0) {
        watch_mode = (strcmp(argv[1], "watch") == 0);
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--no-color") == 0) {
                colors_enabled = 0;
                continue;
            }
            if (strcmp(argv[i], "--cpu") == 0) {
                view_mode = VIEW_CPU;
                continue;
            }
            if (strcmp(argv[i], "--mem") == 0) {
                view_mode = VIEW_MEM;
                continue;
            }
            if (strcmp(argv[i], "--disk") == 0) {
                view_mode = VIEW_DISK;
                continue;
            }
            if (strcmp(argv[i], "--net") == 0) {
                view_mode = VIEW_NET;
                continue;
            }
            if (strcmp(argv[i], "--sort") == 0) {
                if (i + 1 >= argc || parse_sort_mode(argv[i + 1], &sort_mode) != 0) {
                    printf("Usage: %s [status|watch] [group] [--sort host|resp|state] [--cpu|--mem|--disk|--net] [--no-color]\n", argv[0]);
                    return 1;
                }
                i++;
                continue;
            }
            if (strncmp(argv[i], "--sort=", 7) == 0) {
                if (parse_sort_mode(argv[i] + 7, &sort_mode) != 0) {
                    printf("Usage: %s [status|watch] [group] [--sort host|resp|state] [--cpu|--mem|--disk|--net] [--no-color]\n", argv[0]);
                    return 1;
                }
                continue;
            }
            if (group_filter == NULL) {
                group_filter = argv[i];
                continue;
            }
            printf("Usage: %s [status|watch] [group] [--sort host|resp|state] [--cpu|--mem|--disk|--net] [--no-color]\n", argv[0]);
            return 1;
        }
    } else {
        printf("Usage: %s [status|watch] [group] [--sort host|resp|state] [--cpu|--mem|--disk|--net] [--no-color]\n", argv[0]);
        return 1;
    }

    setTableColorEnabled(colors_enabled);

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
        switch (view_mode) {
            case VIEW_CPU:
                render_cpu_detail(nodes, nodeCount, NODE_CONNECT_TIMEOUT_MS, NULL, sort_mode);
                break;
            case VIEW_MEM:
                render_mem_detail(nodes, nodeCount, NODE_CONNECT_TIMEOUT_MS, NULL, sort_mode);
                break;
            case VIEW_DISK:
                render_disk_detail(nodes, nodeCount, NODE_CONNECT_TIMEOUT_MS, NULL, sort_mode);
                break;
            case VIEW_NET:
                render_net_detail(nodes, nodeCount, NODE_CONNECT_TIMEOUT_MS, NULL, sort_mode);
                break;
            case VIEW_DEFAULT:
            default:
                render_status_table(nodes, nodeCount, NODE_CONNECT_TIMEOUT_MS, NULL, sort_mode);
                break;
        }
        return 0;
    }

    printf("\033[2J");
    while (1) {
        printf("\033[H");
        if (group_filter) {
            printf("nodectl watch %s (refresh every %d ms, sort=%s)\n\n", group_filter, NODE_WATCH_INTERVAL_MS, sort_mode_to_string(sort_mode));
        } else {
            printf("nodectl watch (refresh every %d ms, sort=%s)\n\n", NODE_WATCH_INTERVAL_MS, sort_mode_to_string(sort_mode));
        }
        
        switch (view_mode) {
            case VIEW_CPU:
                render_cpu_detail(nodes, nodeCount, NODE_CONNECT_TIMEOUT_MS, connections, sort_mode);
                break;
            case VIEW_MEM:
                render_mem_detail(nodes, nodeCount, NODE_CONNECT_TIMEOUT_MS, connections, sort_mode);
                break;
            case VIEW_DISK:
                render_disk_detail(nodes, nodeCount, NODE_CONNECT_TIMEOUT_MS, connections, sort_mode);
                break;
            case VIEW_NET:
                render_net_detail(nodes, nodeCount, NODE_CONNECT_TIMEOUT_MS, connections, sort_mode);
                break;
            case VIEW_DEFAULT:
            default:
                render_status_table(nodes, nodeCount, NODE_CONNECT_TIMEOUT_MS, connections, sort_mode);
                break;
        }

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
