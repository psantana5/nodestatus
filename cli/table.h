#ifndef TABLE_H
#define TABLE_H

#include "nodes.h"

typedef struct {
    char hostname[256];
    FetchState state;
    int latency_ms;
    int has_metrics;
    float load;
    float cpu_percent;
    float mem_percent;
    float disk_mb_s;
} NodeStatus;

void printTableHeader();
void printTableRow(const NodeStatus *status);
void printTableFooter(int ok_count, int fail_count);
int parseJsonMetrics(const char *json, NodeStatus *status);

#endif // TABLE_H
