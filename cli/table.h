#ifndef TABLE_H
#define TABLE_H

typedef struct {
    char hostname[256];
    float load;
    float cpu_percent;
    float mem_percent;
} NodeStatus;

void printTableHeader();
void printTableRow(NodeStatus status);
void printTableFooter();
int parseJsonMetrics(const char *json, NodeStatus *status);

#endif // TABLE_H
