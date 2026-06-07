/* SPDX-License-Identifier: CC0-1.0 
Copyright (c) 2025 stevenetworkesp267
E-mail: stevenetworkesp267@gmail.com 

Nouvium - NVIDIA GPU driver for the Carl operating system
PCI enumeration and chip detection, implemented from scratch. */

#ifndef NOUVIUM_MAIN_H
#define NOUVIUM_MAIN_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* -----------------------------------------------------------------------
 * PCI constants
 * --------------------------------------------------------------------- */

#define PCI_CONFIG_ADDRESS      0xCF8   /* I/O port: address register     */
#define PCI_CONFIG_DATA         0xCFC   /* I/O port: data register         */

#define PCI_VENDOR_ID_OFFSET    0x00
#define PCI_DEVICE_ID_OFFSET    0x02
#define PCI_CLASS_CODE_OFFSET   0x0B
#define PCI_SUBCLASS_OFFSET     0x0A
#define PCI_BAR0_OFFSET         0x10
#define PCI_BAR1_OFFSET         0x14

#define PCI_VENDOR_NVIDIA       0x10DE  /* NVIDIA Corporation vendor ID    */

#define PCI_CLASS_DISPLAY       0x03    /* Display controller class        */

#define PCI_MAX_BUS             256
#define PCI_MAX_DEVICE          32
#define PCI_MAX_FUNCTION        8

/* -----------------------------------------------------------------------
 * Known NVIDIA device IDs (partial table — extend as needed)
 * --------------------------------------------------------------------- */

typedef struct {
    uint16_t    device_id;
    const char *name;
} nouvium_gpu_entry_t;

/* The table lives in nouvium_main.c; exposed here for external lookups. */
extern const nouvium_gpu_entry_t nouvium_gpu_table[];
extern const size_t              nouvium_gpu_table_size;

/* -----------------------------------------------------------------------
 * Core data structures
 * --------------------------------------------------------------------- */

/* Physical location of a device on the PCI bus. */
typedef struct {
    uint8_t  bus;
    uint8_t  device;
    uint8_t  function;
} nouvium_pci_addr_t;

/* Represents one detected NVIDIA GPU. */
struct nouvium_device {
    uint16_t            vendor_id;   /* Always PCI_VENDOR_NVIDIA when valid */
    uint16_t            device_id;   /* Chip-specific product ID            */
    nouvium_pci_addr_t  pci_addr;    /* Where we found it on the bus        */
    const char         *name;        /* Human-readable chip name, or NULL   */
    bool                present;     /* true once successfully detected     */
};

/* Opaque detection context. */
struct nouvium_detect;

/* -----------------------------------------------------------------------
 * Driver lifecycle
 * --------------------------------------------------------------------- */

/**
 * nouvium_driver_init - Initialise the Nouvium driver subsystem.
 *
 * Must be called before any other Nouvium function.
 *
 * Returns 0 on success, -1 on failure.
 */
int nouvium_driver_init(void);

/**
 * nouvium_driver_shutdown - Tear down the Nouvium driver subsystem.
 *
 * Safe to call even if init failed.
 */
void nouvium_driver_shutdown(void);

/* -----------------------------------------------------------------------
 * Detection API
 * --------------------------------------------------------------------- */

/**
 * nouvium_detect_create - Allocate a detection context for @device.
 *
 * @device: pointer to a nouvium_device to be populated on success.
 *
 * Returns a pointer to the context, or NULL on allocation failure.
 */
struct nouvium_detect *nouvium_detect_create(struct nouvium_device *device);

/**
 * nouvium_detect_destroy - Free a detection context.
 *
 * @detect: context previously returned by nouvium_detect_create().
 */
void nouvium_detect_destroy(struct nouvium_detect *detect);

/**
 * nouvium_detect_scan - Scan the PCI bus for NVIDIA GPUs.
 *
 * Populates the nouvium_device embedded in @detect with the first NVIDIA
 * GPU found. Sets device->present = true on success.
 *
 * @detect: an initialised detection context.
 *
 * Returns 0 if at least one GPU was found, -1 otherwise.
 */
int nouvium_detect_scan(struct nouvium_detect *detect);

/* -----------------------------------------------------------------------
 * PCI helpers (low-level, available to other Nouvium modules)
 * --------------------------------------------------------------------- */

/**
 * nouvium_pci_read16 - Read a 16-bit value from PCI configuration space.
 *
 * @bus / @dev / @func: PCI address.
 * @offset: byte offset within the 256-byte config space (must be aligned).
 *
 * Returns the 16-bit value read.
 */
uint16_t nouvium_pci_read16(uint8_t bus, uint8_t dev,
                             uint8_t func, uint8_t offset);

/**
 * nouvium_pci_read32 - Read a 32-bit value from PCI configuration space.
 */
uint32_t nouvium_pci_read32(uint8_t bus, uint8_t dev,
                             uint8_t func, uint8_t offset);

/**
 * nouvium_gpu_name - Look up a chip name by device ID.
 *
 * Returns a static string, or "Unknown NVIDIA GPU" if not in the table.
 */
const char *nouvium_gpu_name(uint16_t device_id);

#endif /* NOUVIUM_MAIN_H */
