#include <stdio.h>
#include <string.h>
#include "table.h"

void printTableHeader() {
    printf("HOST                 CPU     MEM     LOAD\n");
    printf("--------------------------------------------\n");
}

void printTableRow(NodeStatus status) {
    printf("%-20s %5.1f%% %6.1f%% %7.2f\n", 
           status.hostname, 
           status.cpu_percent, 
           status.mem_percent, 
           status.load);
}

void printTableFooter() {
    printf("\n");
}

int parseJsonMetrics(const char *json, NodeStatus *status) {
    // Parse JSON to extract: cpuBusyPercent, memUsedPercent, load1
    // Format: {...,"cpuBusyPercent":1.37,...,"memUsedPercent":94.88,...,"load1":1.05,...}
    
    if (!json || !status) {
        return -1;
    }
    
    int matches = 0;
    
    // Parse cpuBusyPercent
    const char *cpu_ptr = strstr(json, "\"cpuBusyPercent\":");
    if (cpu_ptr) {
        if (sscanf(cpu_ptr, "\"cpuBusyPercent\":%f", &status->cpu_percent) == 1) {
            matches++;
        }
    }
    
    // Parse memUsedPercent
    const char *mem_ptr = strstr(json, "\"memUsedPercent\":");
    if (mem_ptr) {
        if (sscanf(mem_ptr, "\"memUsedPercent\":%f", &status->mem_percent) == 1) {
            matches++;
        }
    }
    
    // Parse load1
    const char *load_ptr = strstr(json, "\"load1\":");
    if (load_ptr) {
        if (sscanf(load_ptr, "\"load1\":%f", &status->load) == 1) {
            matches++;
        }
    }
    
    return (matches == 3) ? 0 : -1;
}
