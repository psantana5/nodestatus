#ifndef METRICS_H
#define METRICS_H

#include <stdint.h>

// LoadMetrics struct holds load average metrics: 1-minute, 5-minute, and 15-minute load averages
typedef struct {
    float load1;
    float load5;
    float load15;
} LoadMetrics;

LoadMetrics getLoadAverage();

// MemoryMetrics struct holds memory statistics: total, free, available memory, and usage percentages for both RAM and swap
typedef struct {
    unsigned long MemTotal;
    unsigned long MemFree;
    unsigned long MemAvailable;
    unsigned long memUsed;
    float memUsedPercent;
    unsigned long SwapTotal;
    unsigned long SwapFree;
    unsigned long SwapUsed;
    float swapUsedPercent;
} MemoryMetrics;

MemoryMetrics getMemoryMetrics();

// CpuMetrics struct holds CPU usage data including idle, I/O wait times, and calculated busy metrics with percentage
typedef struct {
    unsigned long long user;
    unsigned long long nice;
    unsigned long long system;
    unsigned long long idle;
    unsigned long long iowait;

    // Derived values (calculated from the primary counters)
    unsigned long long busy;
    float busyPercent;
} CpuMetrics;

CpuMetrics getCpuMetrics();

typedef struct {
    unsigned long long readSectors;
    unsigned long long writeSectors;
    unsigned long long readOps;
    unsigned long long writeOps;
    float readMBps;
    float writeMBps;
    float totalMBps;
    float readIOPS;
    float writeIOPS;
    float totalIOPS;
} DiskMetrics;

DiskMetrics getDiskMetrics();

typedef struct {
    unsigned long long rxBytes;
    unsigned long long txBytes;
    float rxMBps;
    float txMBps;
    float totalMBps;
} NetworkMetrics;

NetworkMetrics getNetworkMetrics();

typedef struct {
    LoadMetrics load;
    MemoryMetrics memory;
    CpuMetrics cpu;
    DiskMetrics disk;
    NetworkMetrics network;
    uint64_t sampleTsMs;
    uint64_t sampleAgeMs;
} SystemMetrics;

int initMetricsSampler(void);
void stopMetricsSampler(void);
int getMetricsSnapshot(SystemMetrics *out_metrics);

#endif
