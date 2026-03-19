#ifndef NODES_H
#define NODES_H

#define MAX_NODES 128 // Max num of nodes to load from file. Also maxThreads value since it's 1 worker per node.
#define MAX_HOSTNAME 256
#define MAX_GROUPNAME 64
#define RESPONSE_BUFFER_SIZE 2048
#define NODE_CONNECT_TIMEOUT_MS 500
#define NODE_WATCH_INTERVAL_MS 1000
#define NODE_AGENT_DEFAULT_PORT 9002
#define INVENTORY_FILE "config/inventory.yaml" // This is the default inventory file path.

typedef struct {
    char hostname[MAX_HOSTNAME];
    int port;
    char group[MAX_GROUPNAME];
} Node;

typedef enum {
    FETCH_OK = 0,
    FETCH_TIMEOUT,
    FETCH_DNS_ERROR,
    FETCH_CONNECT_ERROR,
    FETCH_HTTP_ERROR,
    FETCH_PARSE_ERROR,
    FETCH_IO_ERROR
} FetchState;

typedef struct {
    FetchState state;
    int bytes_received;
    int latency_ms;
    char response[RESPONSE_BUFFER_SIZE];
} FetchResult;

int loadNodes(const char *filename, Node *nodes, int max_nodes);
int loadNodesByGroup(const char *filename, Node *nodes, int max_nodes, const char *group_filter);
FetchResult fetchStatus(const char *hostname, int port, int timeout_ms);
FetchResult fetchStatusWithConnection(const char *hostname, int port, int timeout_ms, int *persistent_sockfd);

#endif // NODES_H
