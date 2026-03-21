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

typedef enum {
    FILTER_FIELD_STATE,
    FILTER_FIELD_CPU,
    FILTER_FIELD_MEM,
    FILTER_FIELD_LOAD,
    FILTER_FIELD_DISK,
    FILTER_FIELD_RESP
} FilterField;

typedef enum {
    FILTER_OP_EQ,     // =
    FILTER_OP_NEQ,    // !=
    FILTER_OP_GT,     // >
    FILTER_OP_LT,     // <
    FILTER_OP_GTE,    // >=
    FILTER_OP_LTE     // <=
} FilterOperator;

typedef struct {
    FilterField field;
    FilterOperator op;
    union {
        FetchState state_val;
        float numeric_val;
    } value;
} FilterExpr;

int parse_filter(const char *expr, FilterExpr *filter);
int eval_filter(const NodeStatus *status, const FilterExpr *filter);

#endif // TABLE_H
