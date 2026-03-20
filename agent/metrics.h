#ifndef METRICS_H
#define METRICS_H

#include <stdint.h>

// We will define a struct to hold the load average metrics, including the 1-minute, 5-minute, and 15-minute load averages
typedef struct {
    float load1;
    float load5;
    float load15;
} LoadMetrics;

LoadMetrics getLoadAverage();

// We will define a new struct to hold memory metrics, including total, free, available memory, used memory, and used memory percentage
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

// We will define a new struct to hold CPU usage metrics, including total, idle and I/O wait times.
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
