/* SPDX-License-Identifier: CC0-1.0 */
/* Copyright (c) 2026 stevenetworkesp267 */
/* E-mail: stevenetworkesp267@gmail.com */

   NCAPI — NVIDIA Card API for the Carl operating system.
   Part of the Nouvium driver project.

   Inspired by NVAPI (device management) and NVML (monitoring).
   Implemented from scratch; no NVIDIA libraries required.

   Typical usage
   -------------
     ncapi_status_t s;

     s = ncapiInit();
     if (s != NCAPI_SUCCESS) { ... }

     ncapi_device_t dev;
     s = ncapiDeviceGet(&dev, 0);          // first GPU

     ncapi_device_info_t info;
     s = ncapiDeviceGetInfo(dev, &info);

     uint32_t temp;
     s = ncapiDeviceGetTemperature(dev, &temp);

     ncapiShutdown();
*/

#ifndef NCAPI_H
#define NCAPI_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * Version
 * --------------------------------------------------------------------- */

#define NCAPI_VERSION_MAJOR  0
#define NCAPI_VERSION_MINOR  1
#define NCAPI_VERSION_PATCH  0

/* -----------------------------------------------------------------------
 * Status / error codes
 * --------------------------------------------------------------------- */

typedef enum {
    NCAPI_SUCCESS                = 0,
    NCAPI_ERROR_NOT_INITIALISED  = 1,  /* ncapiInit() not called            */
    NCAPI_ERROR_INVALID_ARG      = 2,  /* NULL pointer or bad index         */
    NCAPI_ERROR_NO_DEVICE        = 3,  /* No NVIDIA GPU found               */
    NCAPI_ERROR_NOT_SUPPORTED    = 4,  /* Feature not supported on this GPU */
    NCAPI_ERROR_MMIO_FAULT       = 5,  /* MMIO read/write failed            */
    NCAPI_ERROR_OUT_OF_MEMORY    = 6,  /* Carl allocator returned NULL      */
    NCAPI_ERROR_ALREADY_INIT     = 7,  /* ncapiInit() called twice          */
    NCAPI_ERROR_UNKNOWN          = 99,
} ncapi_status_t;

/* Returns a human-readable string for a status code. */
const char *ncapiStatusString(ncapi_status_t status);

/* -----------------------------------------------------------------------
 * Opaque device handle
 * --------------------------------------------------------------------- */

typedef struct ncapi_device_internal *ncapi_device_t;

#define NCAPI_DEVICE_NULL  ((ncapi_device_t)NULL)

/* -----------------------------------------------------------------------
 * Device information  (inspired by NVML nvmlDeviceGetName / NVAPI NvAPI_GPU_*)
 * --------------------------------------------------------------------- */

#define NCAPI_NAME_LEN   64

typedef struct {
    char     name[NCAPI_NAME_LEN];  /* e.g. "NVIDIA GeForce RTX 3080"     */
    uint16_t vendor_id;             /* Always 0x10DE                       */
    uint16_t device_id;             /* Chip-specific PCI device ID         */
    uint8_t  pci_bus;
    uint8_t  pci_device;
    uint8_t  pci_function;
} ncapi_device_info_t;

/* -----------------------------------------------------------------------
 * Memory information  (inspired by NVML nvmlDeviceGetMemoryInfo)
 * --------------------------------------------------------------------- */

typedef struct {
    uint64_t total;   /* Total VRAM in bytes  */
    uint64_t used;    /* Used  VRAM in bytes  */
    uint64_t free;    /* Free  VRAM in bytes  */
} ncapi_memory_info_t;

/* -----------------------------------------------------------------------
 * Utilisation  (inspired by NVML nvmlDeviceGetUtilizationRates)
 * --------------------------------------------------------------------- */

typedef struct {
    uint32_t gpu;     /* GPU core utilisation  0-100 % */
    uint32_t memory;  /* Memory bus utilisation 0-100 % */
} ncapi_utilisation_t;

/* -----------------------------------------------------------------------
 * Clock domains  (inspired by NVAPI NvAPI_GPU_GetAllClockFrequencies)
 * --------------------------------------------------------------------- */

typedef enum {
    NCAPI_CLOCK_GRAPHICS = 0,   /* Shader / core clock   */
    NCAPI_CLOCK_MEMORY   = 1,   /* Memory clock          */
    NCAPI_CLOCK_VIDEO    = 2,   /* Video engine clock    */
    NCAPI_CLOCK_COUNT    = 3,
} ncapi_clock_domain_t;

/* -----------------------------------------------------------------------
 * Performance state  (inspired by NVAPI NvAPI_GPU_GetPerfPstate)
 * --------------------------------------------------------------------- */

typedef enum {
    NCAPI_PSTATE_P0  = 0,   /* Maximum performance */
    NCAPI_PSTATE_P2  = 2,
    NCAPI_PSTATE_P5  = 5,
    NCAPI_PSTATE_P8  = 8,   /* Balanced            */
    NCAPI_PSTATE_P12 = 12,  /* Minimum power       */
    NCAPI_PSTATE_UNKNOWN = 0xFF,
} ncapi_pstate_t;

/* -----------------------------------------------------------------------
 * Power limit  (inspired by NVML nvmlDeviceGetPowerManagementLimit)
 * --------------------------------------------------------------------- */

typedef struct {
    uint32_t current_mw;    /* Current power draw in milliwatts  */
    uint32_t limit_mw;      /* Configured TDP limit in milliwatts */
    uint32_t default_mw;    /* Factory default TDP               */
    uint32_t min_mw;        /* Minimum allowed limit             */
    uint32_t max_mw;        /* Maximum allowed limit             */
} ncapi_power_info_t;

/* -----------------------------------------------------------------------
 * Fan information  (inspired by NVAPI NvAPI_GPU_GetTachReading)
 * --------------------------------------------------------------------- */

typedef struct {
    uint32_t speed_rpm;   /* Current fan speed in RPM       */
    uint32_t duty_pct;    /* Fan duty cycle 0-100 %         */
} ncapi_fan_info_t;

/* -----------------------------------------------------------------------
 * Lifecycle
 * --------------------------------------------------------------------- */

/**
 * ncapiInit - Initialise NCAPI and the underlying Nouvium core.
 *
 * Must be called once before any other NCAPI function.
 * Returns NCAPI_ERROR_ALREADY_INIT if called a second time.
 */
ncapi_status_t ncapiInit(void);

/**
 * ncapiShutdown - Release all NCAPI resources.
 */
ncapi_status_t ncapiShutdown(void);

/**
 * ncapiGetVersion - Fill *major / *minor / *patch with the NCAPI version.
 */
ncapi_status_t ncapiGetVersion(uint32_t *major,
                                uint32_t *minor,
                                uint32_t *patch);

/* -----------------------------------------------------------------------
 * Device enumeration
 * --------------------------------------------------------------------- */

/**
 * ncapiDeviceGetCount - Return the number of detected NVIDIA GPUs.
 *
 * @count: output — number of devices available.
 */
ncapi_status_t ncapiDeviceGetCount(uint32_t *count);

/**
 * ncapiDeviceGet - Obtain a handle for device at index @index.
 *
 * @device: output handle.
 * @index:  0-based device index (0 .. count-1).
 */
ncapi_status_t ncapiDeviceGet(ncapi_device_t *device, uint32_t index);

/* -----------------------------------------------------------------------
 * Device queries  (management — inspired by NVAPI)
 * --------------------------------------------------------------------- */

/** Fill *info with static device information. */
ncapi_status_t ncapiDeviceGetInfo(ncapi_device_t      device,
                                   ncapi_device_info_t *info);

/** Read current GPU temperature in degrees Celsius into *temp_c. */
ncapi_status_t ncapiDeviceGetTemperature(ncapi_device_t device,
                                          uint32_t       *temp_c);

/** Read current clock frequency for @domain into *freq_khz. */
ncapi_status_t ncapiDeviceGetClockFreq(ncapi_device_t      device,
                                        ncapi_clock_domain_t domain,
                                        uint32_t            *freq_khz);

/** Read current performance state into *pstate. */
ncapi_status_t ncapiDeviceGetPstate(ncapi_device_t  device,
                                     ncapi_pstate_t *pstate);

/* -----------------------------------------------------------------------
 * Device queries  (monitoring — inspired by NVML)
 * --------------------------------------------------------------------- */

/** Fill *mem with VRAM usage information. */
ncapi_status_t ncapiDeviceGetMemoryInfo(ncapi_device_t       device,
                                         ncapi_memory_info_t *mem);

/** Fill *util with GPU and memory bus utilisation percentages. */
ncapi_status_t ncapiDeviceGetUtilisation(ncapi_device_t       device,
                                          ncapi_utilisation_t *util);

/** Fill *power with power draw and limit information. */
ncapi_status_t ncapiDeviceGetPowerInfo(ncapi_device_t    device,
                                        ncapi_power_info_t *power);

/** Fill *fan with fan speed and duty cycle. */
ncapi_status_t ncapiDeviceGetFanInfo(ncapi_device_t  device,
                                      ncapi_fan_info_t *fan);

#ifdef __cplusplus
}
#endif

#endif /* NCAPI_H */
