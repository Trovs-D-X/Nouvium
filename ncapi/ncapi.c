/* SPDX-License-Identifier: CC0-1.0
   Copyright (c) 2026 stevenetworkesp267
   E-mail: stevenetworkesp267@gmail.com

   NCAPI — implementation for the Carl operating system.
   Part of the Nouvium driver project.

   How register access works
   -------------------------
   NVIDIA GPUs expose their internal registers through BAR0, the first
   Base Address Register in PCI config space.  BAR0 is a memory-mapped
   I/O (MMIO) region typically 16 MB or 32 MB in size.

   To read a register at offset R:
       value = *(volatile uint32_t *)(bar0_base + R)

   Carl's virtual memory layer must have mapped BAR0 into the kernel
   address space before NCAPI is initialised.  The stub
   ncapi_mmio_map_bar0() below is where that call goes.

   Register offsets used here come from public NVIDIA documentation,
   the Nouveau driver source, and the open-gpu-kernel-modules repository
   released by NVIDIA in 2022.  They apply to Pascal (GP10x) and later.
*/

#include "ncapi.h"
#include "nouvium_main.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>   /* malloc/free — replace with Carl's allocator */
#include <string.h>   /* memset, strncpy */

/* -----------------------------------------------------------------------
 * MMIO register offsets (Pascal / Turing / Ampere / Ada)
 *
 * Sources:
 *   - NVIDIA open-gpu-kernel-modules (MIT-licensed headers)
 *   - Nouveau driver (drivers/gpu/drm/nouveau in the Linux kernel tree)
 *   - https://github.com/envytools/envytools (register documentation)
 * --------------------------------------------------------------------- */

/* Temperature sensor */
#define NV_THERM_TEMP                   0x0002045C  /* bits 23:8 = temp*256 */

/* Memory subsystem */
#define NV_FB_PARAMS                    0x0010F200  /* VRAM size encoding   */
#define NV_PFB_NISO_FLUSH_SYSMEM_ADDR   0x00100C10

/* Clock frequencies */
#define NV_PCLOCK_GPCCLK_OUT_DIVIDER    0x00137250  /* graphics clock       */
#define NV_PCLOCK_MCLK_OUT_DIVIDER      0x001373F0  /* memory clock         */
#define NV_PCLOCK_HOSTCLK_OUT_DIVIDER   0x001373D0  /* video clock          */

/* Utilisation counters */
#define NV_PGRAPH_STATUS                0x00400700
#define NV_PMEM_BUSY                    0x001FA800

/* Performance state */
#define NV_PBUS_PERF_PSTATE             0x00101F84

/* Power */
#define NV_PMU_THERM_FBPA_TSENSE        0x0010A614

/* Fan tachometer */
#define NV_PFAN_TACH                    0x00032484

/* BAR0 size: 16 MB for most chips */
#define NCAPI_BAR0_SIZE                 (16u * 1024u * 1024u)

/* -----------------------------------------------------------------------
 * MMIO abstraction
 *
 * Replace ncapi_mmio_map_bar0 / ncapi_mmio_unmap with Carl's virtual
 * memory mapping calls (e.g. carl_vm_map_mmio(phys, size)).
 * ncapi_mmio_read32 / ncapi_mmio_write32 can stay as-is once the
 * mapping returns a valid kernel virtual address.
 * --------------------------------------------------------------------- */

__attribute__((weak))
volatile uint8_t *ncapi_mmio_map_bar0(uintptr_t phys_addr, size_t size)
{
    (void)phys_addr; (void)size;
    return NULL;    /* stub: replace with Carl's MMIO mapping */
}

__attribute__((weak))
void ncapi_mmio_unmap(volatile uint8_t *virt_addr, size_t size)
{
    (void)virt_addr; (void)size;
    /* stub */
}

static inline uint32_t ncapi_mmio_read32(volatile uint8_t *base,
                                          uint32_t          offset)
{
    if (!base) return 0xFFFFFFFFu;
    return *(volatile uint32_t *)(base + offset);
}

/* (write32 reserved for future clock / fan control) */
static inline void ncapi_mmio_write32(volatile uint8_t *base,
                                       uint32_t          offset,
                                       uint32_t          value)
{
    if (!base) return;
    *(volatile uint32_t *)(base + offset) = value;
}

/* -----------------------------------------------------------------------
 * Internal device representation
 * --------------------------------------------------------------------- */

#define NCAPI_MAX_DEVICES  8

struct ncapi_device_internal {
    struct nouvium_device  nouvium_dev;       /* PCI info from Nouvium core */
    volatile uint8_t      *bar0;              /* mapped MMIO base           */
    uintptr_t              bar0_phys;         /* physical address of BAR0   */
    bool                   valid;
};

static struct ncapi_device_internal g_devices[NCAPI_MAX_DEVICES];
static uint32_t                     g_device_count  = 0;
static bool                         g_initialised   = false;

/* -----------------------------------------------------------------------
 * Status strings
 * --------------------------------------------------------------------- */

const char *ncapiStatusString(ncapi_status_t status)
{
    switch (status) {
    case NCAPI_SUCCESS:               return "Success";
    case NCAPI_ERROR_NOT_INITIALISED: return "NCAPI not initialised";
    case NCAPI_ERROR_INVALID_ARG:     return "Invalid argument";
    case NCAPI_ERROR_NO_DEVICE:       return "No NVIDIA device found";
    case NCAPI_ERROR_NOT_SUPPORTED:   return "Not supported";
    case NCAPI_ERROR_MMIO_FAULT:      return "MMIO fault";
    case NCAPI_ERROR_OUT_OF_MEMORY:   return "Out of memory";
    case NCAPI_ERROR_ALREADY_INIT:    return "Already initialised";
    default:                          return "Unknown error";
    }
}

/* -----------------------------------------------------------------------
 * BAR0 physical address extraction from PCI config space
 *
 * BAR0 is at PCI config offset 0x10.  Bits 3:0 encode the BAR type;
 * for a 32-bit memory BAR, bits 31:4 are the base address (16-byte
 * aligned).  For a 64-bit BAR, the upper 32 bits are in BAR1 (0x14).
 * --------------------------------------------------------------------- */

static uintptr_t ncapi_read_bar0_phys(const nouvium_pci_addr_t *addr)
{
    uint32_t lo = nouvium_pci_read32(addr->bus, addr->device,
                                      addr->function, PCI_BAR0_OFFSET);

    /* Bit 0 = 0 → memory BAR; bits 2:1 = 00 → 32-bit, 10 → 64-bit */
    if ((lo & 0x1u) != 0)
        return 0;   /* I/O BAR — unexpected for a GPU, skip */

    lo &= 0xFFFFFFF0u;  /* mask off the type bits */

    if (((lo >> 1) & 0x3u) == 2) {
        /* 64-bit BAR — upper half in BAR1 */
        uint32_t hi = nouvium_pci_read32(addr->bus, addr->device,
                                          addr->function, PCI_BAR1_OFFSET);
        return ((uintptr_t)hi << 32) | (uintptr_t)lo;
    }

    return (uintptr_t)lo;
}

/* -----------------------------------------------------------------------
 * Lifecycle
 * --------------------------------------------------------------------- */

ncapi_status_t ncapiInit(void)
{
    if (g_initialised)
        return NCAPI_ERROR_ALREADY_INIT;

    if (nouvium_driver_init() != 0)
        return NCAPI_ERROR_UNKNOWN;

    memset(g_devices, 0, sizeof(g_devices));
    g_device_count = 0;

    /*
     * Scan the PCI bus for all NVIDIA GPUs (not just the first one).
     * We reuse Nouvium's low-level PCI helpers directly so we can
     * populate multiple slots.
     */
    for (uint16_t bus = 0; bus < PCI_MAX_BUS && g_device_count < NCAPI_MAX_DEVICES; ++bus) {
        for (uint8_t slot = 0; slot < PCI_MAX_DEVICE && g_device_count < NCAPI_MAX_DEVICES; ++slot) {
            for (uint8_t func = 0; func < PCI_MAX_FUNCTION && g_device_count < NCAPI_MAX_DEVICES; ++func) {

                uint16_t vendor = nouvium_pci_read16((uint8_t)bus, slot, func,
                                                      PCI_VENDOR_ID_OFFSET);
                if (vendor != PCI_VENDOR_NVIDIA)
                    continue;

                uint16_t dev_id = nouvium_pci_read16((uint8_t)bus, slot, func,
                                                      PCI_DEVICE_ID_OFFSET);

                struct ncapi_device_internal *d = &g_devices[g_device_count];
                d->nouvium_dev.vendor_id         = vendor;
                d->nouvium_dev.device_id         = dev_id;
                d->nouvium_dev.pci_addr.bus      = (uint8_t)bus;
                d->nouvium_dev.pci_addr.device   = slot;
                d->nouvium_dev.pci_addr.function = func;
                d->nouvium_dev.name              = nouvium_gpu_name(dev_id);
                d->nouvium_dev.present           = true;

                /* Map BAR0 so register reads work immediately. */
                d->bar0_phys = ncapi_read_bar0_phys(&d->nouvium_dev.pci_addr);
                if (d->bar0_phys != 0)
                    d->bar0 = ncapi_mmio_map_bar0(d->bar0_phys, NCAPI_BAR0_SIZE);

                d->valid = true;
                g_device_count++;
            }
        }
    }

    if (g_device_count == 0) {
        nouvium_driver_shutdown();
        return NCAPI_ERROR_NO_DEVICE;
    }

    g_initialised = true;
    return NCAPI_SUCCESS;
}

ncapi_status_t ncapiShutdown(void)
{
    if (!g_initialised)
        return NCAPI_ERROR_NOT_INITIALISED;

    for (uint32_t i = 0; i < g_device_count; ++i) {
        if (g_devices[i].bar0)
            ncapi_mmio_unmap(g_devices[i].bar0, NCAPI_BAR0_SIZE);
    }

    memset(g_devices, 0, sizeof(g_devices));
    g_device_count = 0;
    g_initialised  = false;

    nouvium_driver_shutdown();
    return NCAPI_SUCCESS;
}

ncapi_status_t ncapiGetVersion(uint32_t *major, uint32_t *minor, uint32_t *patch)
{
    if (!major || !minor || !patch) return NCAPI_ERROR_INVALID_ARG;
    *major = NCAPI_VERSION_MAJOR;
    *minor = NCAPI_VERSION_MINOR;
    *patch = NCAPI_VERSION_PATCH;
    return NCAPI_SUCCESS;
}

/* -----------------------------------------------------------------------
 * Device enumeration
 * --------------------------------------------------------------------- */

ncapi_status_t ncapiDeviceGetCount(uint32_t *count)
{
    if (!g_initialised) return NCAPI_ERROR_NOT_INITIALISED;
    if (!count)         return NCAPI_ERROR_INVALID_ARG;
    *count = g_device_count;
    return NCAPI_SUCCESS;
}

ncapi_status_t ncapiDeviceGet(ncapi_device_t *device, uint32_t index)
{
    if (!g_initialised)          return NCAPI_ERROR_NOT_INITIALISED;
    if (!device)                 return NCAPI_ERROR_INVALID_ARG;
    if (index >= g_device_count) return NCAPI_ERROR_INVALID_ARG;
    *device = (ncapi_device_t)&g_devices[index];
    return NCAPI_SUCCESS;
}

/* -----------------------------------------------------------------------
 * Validation helper
 * --------------------------------------------------------------------- */

static ncapi_status_t validate(ncapi_device_t device)
{
    if (!g_initialised) return NCAPI_ERROR_NOT_INITIALISED;
    if (!device)        return NCAPI_ERROR_INVALID_ARG;
    struct ncapi_device_internal *d = (struct ncapi_device_internal *)device;
    if (!d->valid)      return NCAPI_ERROR_INVALID_ARG;
    return NCAPI_SUCCESS;
}

/* -----------------------------------------------------------------------
 * Device info
 * --------------------------------------------------------------------- */

ncapi_status_t ncapiDeviceGetInfo(ncapi_device_t device, ncapi_device_info_t *info)
{
    ncapi_status_t s = validate(device);
    if (s != NCAPI_SUCCESS) return s;
    if (!info) return NCAPI_ERROR_INVALID_ARG;

    struct ncapi_device_internal *d = (struct ncapi_device_internal *)device;
    strncpy(info->name, d->nouvium_dev.name
                        ? d->nouvium_dev.name : "Unknown NVIDIA GPU",
            NCAPI_NAME_LEN - 1);
    info->name[NCAPI_NAME_LEN - 1] = '\0';
    info->vendor_id    = d->nouvium_dev.vendor_id;
    info->device_id    = d->nouvium_dev.device_id;
    info->pci_bus      = d->nouvium_dev.pci_addr.bus;
    info->pci_device   = d->nouvium_dev.pci_addr.device;
    info->pci_function = d->nouvium_dev.pci_addr.function;
    return NCAPI_SUCCESS;
}

/* -----------------------------------------------------------------------
 * Temperature
 *
 * NV_THERM_TEMP register (0x0002045C):
 *   bits 23:8 hold temperature as a fixed-point value * 256.
 *   Divide by 256 to get degrees Celsius.
 *   Source: envytools nv_therm.rst / Nouveau therm/temp.c
 * --------------------------------------------------------------------- */

ncapi_status_t ncapiDeviceGetTemperature(ncapi_device_t device, uint32_t *temp_c)
{
    ncapi_status_t s = validate(device);
    if (s != NCAPI_SUCCESS) return s;
    if (!temp_c) return NCAPI_ERROR_INVALID_ARG;

    struct ncapi_device_internal *d = (struct ncapi_device_internal *)device;
    if (!d->bar0) return NCAPI_ERROR_MMIO_FAULT;

    uint32_t raw = ncapi_mmio_read32(d->bar0, NV_THERM_TEMP);
    if (raw == 0xFFFFFFFFu) return NCAPI_ERROR_MMIO_FAULT;

    /* bits 23:8 = temp * 256 */
    *temp_c = (raw >> 8) & 0xFFFF;
    *temp_c /= 256;
    return NCAPI_SUCCESS;
}

/* -----------------------------------------------------------------------
 * Clock frequencies
 *
 * The clock divider registers encode frequency as:
 *   freq_khz = (N / M) * reference_clock_khz
 * where N and M are bitfields in the divider register.
 * Reference clock is typically 27 000 kHz (27 MHz) on Pascal/Turing.
 *
 * Source: Nouveau's clk/gk104.c and envytools docs.
 * --------------------------------------------------------------------- */

#define NCAPI_REF_CLK_KHZ  27000u

static uint32_t decode_clock(uint32_t reg_val)
{
    if (reg_val == 0xFFFFFFFFu || reg_val == 0)
        return 0;
    uint32_t N = (reg_val >> 8) & 0xFF;
    uint32_t M = (reg_val & 0xFF);
    if (M == 0) return 0;
    return (N * NCAPI_REF_CLK_KHZ) / M;
}

ncapi_status_t ncapiDeviceGetClockFreq(ncapi_device_t       device,
                                        ncapi_clock_domain_t domain,
                                        uint32_t            *freq_khz)
{
    ncapi_status_t s = validate(device);
    if (s != NCAPI_SUCCESS) return s;
    if (!freq_khz) return NCAPI_ERROR_INVALID_ARG;

    struct ncapi_device_internal *d = (struct ncapi_device_internal *)device;
    if (!d->bar0) return NCAPI_ERROR_MMIO_FAULT;

    uint32_t offset;
    switch (domain) {
    case NCAPI_CLOCK_GRAPHICS: offset = NV_PCLOCK_GPCCLK_OUT_DIVIDER; break;
    case NCAPI_CLOCK_MEMORY:   offset = NV_PCLOCK_MCLK_OUT_DIVIDER;   break;
    case NCAPI_CLOCK_VIDEO:    offset = NV_PCLOCK_HOSTCLK_OUT_DIVIDER; break;
    default: return NCAPI_ERROR_INVALID_ARG;
    }

    uint32_t raw = ncapi_mmio_read32(d->bar0, offset);
    *freq_khz = decode_clock(raw);
    return NCAPI_SUCCESS;
}

/* -----------------------------------------------------------------------
 * Performance state
 *
 * NV_PBUS_PERF_PSTATE (0x00101F84): bits 3:0 encode the current P-state.
 * Source: Nouveau's pstate implementation.
 * --------------------------------------------------------------------- */

ncapi_status_t ncapiDeviceGetPstate(ncapi_device_t device, ncapi_pstate_t *pstate)
{
    ncapi_status_t s = validate(device);
    if (s != NCAPI_SUCCESS) return s;
    if (!pstate) return NCAPI_ERROR_INVALID_ARG;

    struct ncapi_device_internal *d = (struct ncapi_device_internal *)device;
    if (!d->bar0) return NCAPI_ERROR_MMIO_FAULT;

    uint32_t raw = ncapi_mmio_read32(d->bar0, NV_PBUS_PERF_PSTATE);
    if (raw == 0xFFFFFFFFu) return NCAPI_ERROR_MMIO_FAULT;

    uint32_t p = raw & 0xFu;
    switch (p) {
    case 0:  *pstate = NCAPI_PSTATE_P0;  break;
    case 2:  *pstate = NCAPI_PSTATE_P2;  break;
    case 5:  *pstate = NCAPI_PSTATE_P5;  break;
    case 8:  *pstate = NCAPI_PSTATE_P8;  break;
    case 12: *pstate = NCAPI_PSTATE_P12; break;
    default: *pstate = NCAPI_PSTATE_UNKNOWN; break;
    }
    return NCAPI_SUCCESS;
}

/* -----------------------------------------------------------------------
 * Memory info
 *
 * NV_FB_PARAMS (0x0010F200): bits 15:0 encode VRAM size in MB.
 * Used and free memory require tracking allocations — for now we report
 * used = 0 until Carl's VRAM allocator hooks into NCAPI.
 * --------------------------------------------------------------------- */

ncapi_status_t ncapiDeviceGetMemoryInfo(ncapi_device_t       device,
                                         ncapi_memory_info_t *mem)
{
    ncapi_status_t s = validate(device);
    if (s != NCAPI_SUCCESS) return s;
    if (!mem) return NCAPI_ERROR_INVALID_ARG;

    struct ncapi_device_internal *d = (struct ncapi_device_internal *)device;
    if (!d->bar0) return NCAPI_ERROR_MMIO_FAULT;

    uint32_t raw = ncapi_mmio_read32(d->bar0, NV_FB_PARAMS);
    if (raw == 0xFFFFFFFFu) return NCAPI_ERROR_MMIO_FAULT;

    uint64_t size_mb = (raw & 0xFFFFu);
    mem->total = size_mb * 1024u * 1024u;
    mem->used  = 0;                    /* TODO: hook into Carl VRAM allocator */
    mem->free  = mem->total;
    return NCAPI_SUCCESS;
}

/* -----------------------------------------------------------------------
 * Utilisation
 *
 * NV_PGRAPH_STATUS (0x00400700): bit 31 = GPC busy.
 * A proper utilisation percentage requires sampling over time.
 * This implementation returns a binary 0/100 as a starting point;
 * add a timer-based sampler in Carl when the scheduler is ready.
 * --------------------------------------------------------------------- */

ncapi_status_t ncapiDeviceGetUtilisation(ncapi_device_t       device,
                                          ncapi_utilisation_t *util)
{
    ncapi_status_t s = validate(device);
    if (s != NCAPI_SUCCESS) return s;
    if (!util) return NCAPI_ERROR_INVALID_ARG;

    struct ncapi_device_internal *d = (struct ncapi_device_internal *)device;
    if (!d->bar0) return NCAPI_ERROR_MMIO_FAULT;

    uint32_t graph = ncapi_mmio_read32(d->bar0, NV_PGRAPH_STATUS);
    uint32_t pmem  = ncapi_mmio_read32(d->bar0, NV_PMEM_BUSY);

    /* bit 31 = engine busy */
    util->gpu    = (graph & (1u << 31)) ? 100u : 0u;
    util->memory = (pmem  & (1u << 31)) ? 100u : 0u;

    /* TODO: replace with a proper time-averaged sampling loop. */
    return NCAPI_SUCCESS;
}

/* -----------------------------------------------------------------------
 * Power info
 *
 * NV_PMU_THERM_FBPA_TSENSE (0x0010A614): encodes current power in mW.
 * TDP limits require PMU firmware communication — stubbed for now.
 * --------------------------------------------------------------------- */

ncapi_status_t ncapiDeviceGetPowerInfo(ncapi_device_t     device,
                                        ncapi_power_info_t *power)
{
    ncapi_status_t s = validate(device);
    if (s != NCAPI_SUCCESS) return s;
    if (!power) return NCAPI_ERROR_INVALID_ARG;

    struct ncapi_device_internal *d = (struct ncapi_device_internal *)device;
    if (!d->bar0) return NCAPI_ERROR_MMIO_FAULT;

    uint32_t raw = ncapi_mmio_read32(d->bar0, NV_PMU_THERM_FBPA_TSENSE);

    power->current_mw = (raw != 0xFFFFFFFFu) ? (raw & 0xFFFFu) * 100u : 0u;
    power->limit_mw   = 0;   /* TODO: read from PMU firmware straps */
    power->default_mw = 0;
    power->min_mw     = 0;
    power->max_mw     = 0;
    return NCAPI_SUCCESS;
}

/* -----------------------------------------------------------------------
 * Fan info
 *
 * NV_PFAN_TACH (0x00032484): bits 15:0 = tachometer reading.
 * duty cycle requires PWM register access — stubbed.
 * --------------------------------------------------------------------- */

ncapi_status_t ncapiDeviceGetFanInfo(ncapi_device_t   device,
                                      ncapi_fan_info_t *fan)
{
    ncapi_status_t s = validate(device);
    if (s != NCAPI_SUCCESS) return s;
    if (!fan) return NCAPI_ERROR_INVALID_ARG;

    struct ncapi_device_internal *d = (struct ncapi_device_internal *)device;
    if (!d->bar0) return NCAPI_ERROR_MMIO_FAULT;

    uint32_t raw = ncapi_mmio_read32(d->bar0, NV_PFAN_TACH);
    fan->speed_rpm = (raw != 0xFFFFFFFFu) ? (raw & 0xFFFFu) : 0u;
    fan->duty_pct  = 0;   /* TODO: read PWM duty cycle register */
    return NCAPI_SUCCESS;
}
