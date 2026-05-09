// SPDX-License-Identifier: GPL-2.0-only
/*
 * carl/drivers/cpu.c
 *
 * Copyright (C) 2026 MDFJ
 * CPU Information and Monitoring Driver
 * Provides CPU core detection and load monitoring
 * across Windows, macOS, and Linux platforms
 */
 
#include "cpu.h"
#include <stdio.h>
 
// Platform-specific includes
#ifdef _WIN32
    #include <windows.h>
#elif __APPLE__
    #include <sys/sysctl.h>
    #include <mach/mach.h>
    #include <mach/processor_info.h>
    #include <mach/mach_host.h>
#else
    #include <unistd.h>
    #include <sys/sysinfo.h>
#endif
 
// Global CPU information structure
static CPUinfo cpu_info;
 
/**
 * cpu_init() - Initialize CPU information
 * 
 * Detects the number of CPU cores available on the system.
 * Uses platform-specific APIs:
 * - Windows: GetSystemInfo()
 * - macOS: sysctl(hw.ncpu)
 * - Linux: sysconf(_SC_NPROCESSORS_ONLN)
 * 
 * Also initializes frequency and usage counters to 0.
 * Must be called during system startup before cpu_get_info().
 */
void cpu_init(void) {
    #ifdef _WIN32
    // Windows: Use GetSystemInfo to retrieve CPU core count
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    cpu_info.cores = sysinfo.dwNumberOfProcessors;
    cpu_info.freq_mhz = 0;  // Not detected on Windows
    
    #elif __APPLE__
    // macOS: Use sysctl to retrieve CPU core count
    int cores;
    size_t size = sizeof(cores);
    sysctlbyname("hw.ncpu", &cores, &size, NULL, 0);
    cpu_info.cores = cores;
    cpu_info.freq_mhz = 0;  // Not detected on macOS
    
    #else
    // Linux: Use sysconf to retrieve CPU core count
    cpu_info.cores = sysconf(_SC_NPROCESSORS_ONLN);
    cpu_info.freq_mhz = 0;  // Not detected on Linux
    #endif
    
    // Initialize usage counter
    cpu_info.usage_percent = 0;
}
 
/**
 * cpu_get_info() - Get current CPU information
 * 
 * @return: CPUinfo structure containing:
 *   - cores: Number of CPU cores detected
 *   - freq_mhz: CPU frequency in MHz (platform-dependent)
 *   - usage_percent: Current CPU utilization percentage (0-100)
 * 
 * Note: cpu_init() must be called before using this function.
 */
CPUinfo cpu_get_info(void) {
    return cpu_info;
}
 
/**
 * cpu_check_load() - Check if CPU load exceeds threshold
 * 
 * @threshold: CPU usage percentage threshold (0.0-100.0)
 * @return: 1 if current CPU usage > threshold, 0 otherwise
 * 
 * Useful for load balancing and thermal management.
 * Example: cpu_check_load(75.0) returns 1 if CPU > 75% loaded
 */
int cpu_check_load(float threshold) {
    return (cpu_info.usage_percent > threshold) ? 1 : 0;
}
