/* SPDX-License-Identifier: CC0-1.0
Copyright (c) 2026 stevenetworkesp267
E-mail: stevenetworkesp267@gmail.com 

   Nouvium - NVIDIA GPU driver for the Carl operating system.
   PCI enumeration and chip detection, implemented from scratch.

   How detection works
   -------------------
   On x86/x86-64, PCI configuration space is accessed through two I/O ports:
     0xCF8  (CONFIG_ADDRESS) — write a 32-bit address specifying
                               bus / device / function / register offset.
     0xCFC  (CONFIG_DATA)    — read or write the selected register.

   We scan every possible (bus, device, function) tuple, read the Vendor ID
   at offset 0x00, and compare it against 0x10DE (NVIDIA).  When we find a
   match we read the Device ID and look it up in our table.

   NOTE: The I/O port helpers (nouvium_io_outl / nouvium_io_inl) are declared
   as weak stubs here so the file compiles on a hosted build environment.
   In the actual Carl kernel, replace them with your real port I/O intrinsics
   (or inline asm) — the rest of the code does not need to change. */

#include "nouvium_main.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>   /* malloc / free — swap for Carl's allocator */

/* -----------------------------------------------------------------------
 * I/O port abstraction
 *
 * Replace these two functions with Carl's real port I/O primitives.
 * On x86 bare-metal that is typically:
 *
 *   static inline void outl(uint16_t port, uint32_t val) {
 *       __asm__ volatile ("outl %0, %w1" : : "a"(val), "Nd"(port));
 *   }
 *   static inline uint32_t inl(uint16_t port) {
 *       uint32_t val;
 *       __asm__ volatile ("inl %w1, %0" : "=a"(val) : "Nd"(port));
 *       return val;
 *   }
 * --------------------------------------------------------------------- */

/* Weak stubs — overridden by the kernel build. */
__attribute__((weak))
void nouvium_io_outl(uint16_t port, uint32_t value)
{
    (void)port; (void)value;
    /* stub: no-op outside Carl */
}

__attribute__((weak))
uint32_t nouvium_io_inl(uint16_t port)
{
    (void)port;
    return 0xFFFFFFFFu; /* PCI "no device" sentinel */
}

/* -----------------------------------------------------------------------
 * GPU device-ID table
 *
 * Sources: PCI ID repository (https://pci-ids.ucw.cz/) and
 *          NVIDIA product pages.  Extend freely.
 * --------------------------------------------------------------------- */

const nouvium_gpu_entry_t nouvium_gpu_table[] = {
    /* ---- Turing (RTX 20 series) ---- */
    { 0x1E04, "NVIDIA GeForce RTX 2080 Ti" },
    { 0x1E82, "NVIDIA GeForce RTX 2080"    },
    { 0x1E87, "NVIDIA GeForce RTX 2080 SUPER" },
    { 0x1E84, "NVIDIA GeForce RTX 2070 SUPER" },
    { 0x1F02, "NVIDIA GeForce RTX 2070"    },
    { 0x1F08, "NVIDIA GeForce RTX 2060 SUPER" },
    { 0x1F36, "NVIDIA GeForce RTX 2060"    },

    /* ---- Ampere (RTX 30 series) ---- */
    { 0x2204, "NVIDIA GeForce RTX 3090"    },
    { 0x2206, "NVIDIA GeForce RTX 3080"    },
    { 0x2216, "NVIDIA GeForce RTX 3080 Ti" },
    { 0x2208, "NVIDIA GeForce RTX 3070 Ti" },
    { 0x2484, "NVIDIA GeForce RTX 3070"    },
    { 0x2503, "NVIDIA GeForce RTX 3060 Ti" },
    { 0x2504, "NVIDIA GeForce RTX 3060"    },
    { 0x2571, "NVIDIA GeForce RTX 3050"    },

    /* ---- Ada Lovelace (RTX 40 series) ---- */
    { 0x2684, "NVIDIA GeForce RTX 4090"    },
    { 0x2702, "NVIDIA GeForce RTX 4080 SUPER" },
    { 0x2704, "NVIDIA GeForce RTX 4080"    },
    { 0x2782, "NVIDIA GeForce RTX 4070 Ti SUPER" },
    { 0x2783, "NVIDIA GeForce RTX 4070 Ti" },
    { 0x2786, "NVIDIA GeForce RTX 4070 SUPER" },
    { 0x2803, "NVIDIA GeForce RTX 4070"    },
    { 0x2882, "NVIDIA GeForce RTX 4060 Ti" },
    { 0x2860, "NVIDIA GeForce RTX 4060"    },

    /* ---- Pascal (GTX 10 series) ---- */
    { 0x1B80, "NVIDIA GeForce GTX 1080"    },
    { 0x1B81, "NVIDIA GeForce GTX 1070"    },
    { 0x1B82, "NVIDIA GeForce GTX 1070 Ti" },
    { 0x1C02, "NVIDIA GeForce GTX 1060 6GB" },
    { 0x1C82, "NVIDIA GeForce GTX 1050 Ti" },
    { 0x1C81, "NVIDIA GeForce GTX 1050"    },
    { 0x1CB1, "NVIDIA GeForce GTX 1080 Ti" },

    /* ---- Quadro / professional ---- */
    { 0x1CB3, "NVIDIA Quadro P400"         },
    { 0x1CB2, "NVIDIA Quadro P600"         },
    { 0x1CB1, "NVIDIA Quadro P1000"        },
    { 0x1BB1, "NVIDIA Quadro P4000"        },
};

const size_t nouvium_gpu_table_size =
    sizeof(nouvium_gpu_table) / sizeof(nouvium_gpu_table[0]);

/* -----------------------------------------------------------------------
 * Internal detection context
 * --------------------------------------------------------------------- */

struct nouvium_detect {
    struct nouvium_device *device;   /* pointer to the caller-owned device */
    bool                   scanned;  /* true after nouvium_detect_scan()   */
};

/* -----------------------------------------------------------------------
 * PCI helpers
 * --------------------------------------------------------------------- */

/*
 * Build the 32-bit address word for PCI CONFIG_ADDRESS.
 *
 * Bit layout (PCI Local Bus Specification 3.0, §3.2.2.3.2):
 *   31    — Enable bit (must be 1)
 *   30:24 — Reserved (0)
 *   23:16 — Bus number
 *   15:11 — Device number
 *   10:8  — Function number
 *    7:2  — Register offset (bits 7:2; bits 1:0 are always 0)
 *    1:0  — Always 0
 */
static uint32_t pci_make_address(uint8_t bus, uint8_t dev,
                                  uint8_t func, uint8_t offset)
{
    return (uint32_t)(1u << 31)
         | ((uint32_t)bus    << 16)
         | ((uint32_t)(dev  & 0x1F) << 11)
         | ((uint32_t)(func & 0x07) <<  8)
         | ((uint32_t)(offset & 0xFC));   /* clear bits 1:0 */
}

uint32_t nouvium_pci_read32(uint8_t bus, uint8_t dev,
                             uint8_t func, uint8_t offset)
{
    nouvium_io_outl(PCI_CONFIG_ADDRESS, pci_make_address(bus, dev, func, offset));
    return nouvium_io_inl(PCI_CONFIG_DATA);
}

uint16_t nouvium_pci_read16(uint8_t bus, uint8_t dev,
                             uint8_t func, uint8_t offset)
{
    uint32_t val = nouvium_pci_read32(bus, dev, func, offset & 0xFC);
    /* Select the correct 16-bit half based on bit 1 of the offset. */
    if (offset & 0x02)
        return (uint16_t)(val >> 16);
    else
        return (uint16_t)(val & 0xFFFF);
}

/* -----------------------------------------------------------------------
 * GPU name lookup
 * --------------------------------------------------------------------- */

const char *nouvium_gpu_name(uint16_t device_id)
{
    for (size_t i = 0; i < nouvium_gpu_table_size; ++i) {
        if (nouvium_gpu_table[i].device_id == device_id)
            return nouvium_gpu_table[i].name;
    }
    return "Unknown NVIDIA GPU";
}

/* -----------------------------------------------------------------------
 * Driver lifecycle
 * --------------------------------------------------------------------- */

static bool g_driver_initialised = false;

int nouvium_driver_init(void)
{
    if (g_driver_initialised)
        return 0;   /* idempotent */

    /*
     * Future: initialise MMIO mapping, interrupt handling, power management,
     * etc.  For now we just set the flag so other functions know init ran.
     */
    g_driver_initialised = true;
    return 0;
}

void nouvium_driver_shutdown(void)
{
    if (!g_driver_initialised)
        return;

    /* Future: unmap MMIO regions, disable interrupts, power down GPU. */
    g_driver_initialised = false;
}

/* -----------------------------------------------------------------------
 * Detection context
 * --------------------------------------------------------------------- */

struct nouvium_detect *nouvium_detect_create(struct nouvium_device *device)
{
    if (!device)
        return NULL;

    struct nouvium_detect *detect =
        (struct nouvium_detect *)malloc(sizeof(struct nouvium_detect));
    if (!detect)
        return NULL;

    detect->device  = device;
    detect->scanned = false;
    return detect;
}

void nouvium_detect_destroy(struct nouvium_detect *detect)
{
    if (detect)
        free(detect);
}

/* -----------------------------------------------------------------------
 * PCI scan
 * --------------------------------------------------------------------- */

int nouvium_detect_scan(struct nouvium_detect *detect)
{
    if (!detect || !detect->device)
        return -1;

    struct nouvium_device *dev = detect->device;
    dev->present = false;

    for (uint16_t bus = 0; bus < PCI_MAX_BUS; ++bus) {
        for (uint8_t slot = 0; slot < PCI_MAX_DEVICE; ++slot) {
            for (uint8_t func = 0; func < PCI_MAX_FUNCTION; ++func) {

                uint16_t vendor = nouvium_pci_read16(
                    (uint8_t)bus, slot, func, PCI_VENDOR_ID_OFFSET);

                /* 0xFFFF means no device present at this address. */
                if (vendor == 0xFFFF)
                    continue;

                if (vendor != PCI_VENDOR_NVIDIA)
                    continue;

                /* Found an NVIDIA device. */
                uint16_t device_id = nouvium_pci_read16(
                    (uint8_t)bus, slot, func, PCI_DEVICE_ID_OFFSET);

                dev->vendor_id       = vendor;
                dev->device_id       = device_id;
                dev->pci_addr.bus    = (uint8_t)bus;
                dev->pci_addr.device = slot;
                dev->pci_addr.function = func;
                dev->name            = nouvium_gpu_name(device_id);
                dev->present         = true;

                detect->scanned = true;
                return 0;   /* First GPU found — success. */
            }
        }
    }

    detect->scanned = true;
    return -1;  /* No NVIDIA GPU found. */
}

/* -----------------------------------------------------------------------
 * Entry point
 * --------------------------------------------------------------------- */

int main(void)
{
    struct nouvium_device device = {
        .vendor_id = 0,
        .device_id = 0,
        .present   = false,
    };

    if (nouvium_driver_init() != 0)
        return -1;

    struct nouvium_detect *detect = nouvium_detect_create(&device);
    if (!detect) {
        nouvium_driver_shutdown();
        return -1;
    }

    int result = nouvium_detect_scan(detect);

    if (result == 0 && device.present) {
        /*
         * GPU found.  device.name, device.device_id, and device.pci_addr
         * are all populated.  Hand off to Carl's display / compute stack.
         *
         * Example (replace with Carl's actual logging/console call):
         *   carl_log("Nouvium: found %s at PCI %02x:%02x.%x (ID %04x:%04x)\n",
         *            device.name,
         *            device.pci_addr.bus,
         *            device.pci_addr.device,
         *            device.pci_addr.function,
         *            device.vendor_id,
         *            device.device_id);
         */
    }

    nouvium_detect_destroy(detect);
    nouvium_driver_shutdown();
    return result;
}
