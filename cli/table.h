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

typedef struct {
    char hostname[256];
    FetchState state;
    int has_metrics;
    float user_percent;
    float nice_percent;
    float system_percent;
    float idle_percent;
    float iowait_percent;
    float busy_percent;
} CpuDetailStatus;

typedef struct {
    char hostname[256];
    FetchState state;
    int has_metrics;
    unsigned long mem_total_mb;
    unsigned long mem_avail_mb;
    float mem_percent;
    unsigned long swap_total_mb;
    unsigned long swap_used_mb;
    float swap_percent;
} MemDetailStatus;

typedef struct {
    char hostname[256];
    FetchState state;
    int has_metrics;
    float read_mbps;
    float write_mbps;
    float total_mbps;
    float read_iops;
    float write_iops;
    float total_iops;
} DiskDetailStatus;

typedef struct {
    char hostname[256];
    FetchState state;
    int has_metrics;
    float rx_mbps;
    float tx_mbps;
    float total_mbps;
} NetDetailStatus;

typedef struct {
    char hostname[256];
    FetchState state;
    int latency_ms;
    int bytes_received;
    unsigned long long timestamp_ms;
    int sample_age_ms;
    int connect_ok;
    int read_ok;
} DebugStatus;

void setTableColorEnabled(int enabled);
void printTableHeader();
void printTableRow(const NodeStatus *status);
void printTableFooter(int ok_count, int fail_count);
int parseJsonMetrics(const char *json, NodeStatus *status);

void printCpuDetailHeader();
void printCpuDetailRow(const CpuDetailStatus *status);
int parseJsonCpuDetail(const char *json, CpuDetailStatus *status);

void printMemDetailHeader();
void printMemDetailRow(const MemDetailStatus *status);
int parseJsonMemDetail(const char *json, MemDetailStatus *status);

void printDiskDetailHeader();
void printDiskDetailRow(const DiskDetailStatus *status);
int parseJsonDiskDetail(const char *json, DiskDetailStatus *status);

void printNetDetailHeader();
void printNetDetailRow(const NetDetailStatus *status);
int parseJsonNetDetail(const char *json, NetDetailStatus *status);

void printDebugStatus(const DebugStatus *status);
int parseJsonDebug(const char *json, DebugStatus *status);

#endif // TABLE_H
