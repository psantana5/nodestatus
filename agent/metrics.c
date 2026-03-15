#include <stdio.h>
#include <stdlib.h>
#include "metrics.h"

LoadMetrics getLoadAverage() {
    LoadMetrics metrics = {0, 0, 0};
    FILE *fp = fopen("/proc/loadavg", "r");
    
    if (fp == NULL) {
        perror("fopen");
        return metrics;
    }
    
    int result = fscanf(fp, "%f %f %f", &metrics.load1, &metrics.load5, &metrics.load15);
    
    if (result != 3) {
        perror("fscanf");
    }
    
    fclose(fp);
    return metrics;
}

MemoryMetrics getMemoryMetrics() {
    MemoryMetrics metrics = {0, 0, 0};
    FILE *fp = fopen("/proc/meminfo", "r");
    
    if (fp == NULL) {
        perror("fopen");
        return metrics;
    }
    

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "MemTotal: %lu kB", &metrics.MemTotal) == 1) {
            continue;
        }
        if (sscanf(line, "MemFree: %lu kB", &metrics.MemFree) == 1) {
            continue;
        }
        if (sscanf(line, "MemAvailable: %lu kB", &metrics.MemAvailable) == 1) {
            continue;
        }
    }

     // We will calculate memUsed and memUsedPercent after we have read the total and free memory values
    metrics.memUsed = metrics.MemTotal - metrics.MemFree;
    metrics.memUsedPercent = (metrics.MemTotal > 0) ? ((float)metrics.memUsed / metrics.MemTotal) * 100 : 0;
    
    fclose(fp);
    return metrics;
}
