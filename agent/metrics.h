#ifndef METRICS_H
#define METRICS_H

typedef struct {
    float load1;
    float load5;
    float load15;
} LoadMetrics;

LoadMetrics getLoadAverage();

typedef struct {
    unsigned long MemTotal;
    unsigned long MemFree;
    unsigned long MemAvailable;
    unsigned long memUsed;
    float memUsedPercent;
} MemoryMetrics;

MemoryMetrics getMemoryMetrics();

#endif
