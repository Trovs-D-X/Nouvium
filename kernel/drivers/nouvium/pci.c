/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Carl-OS NVIDIA experimental subsystem
 * trovna/pci.c
 *
 * Copyright (c) mauriminuano125-a11y
 * All rights reserved
 */

#include <carl/trovna/device.h>
#include <carl/trovna/driver.h>
#include <carl/trovna/driver_manager.h>
#include <carl/trovna/driver_probe.h>

#define NVIDIA_VENDOR_ID 0x10DE

/* Detect NVIDIA PCI hardware */

void phi_trovna(struct trovna_manager *mgr, struct trovna_probe *probe)
{
    struct trovna_device *dev;

    if (!mgr || !probe) {
        return;
    }

    dev = probe->device;

    if (!dev) {
        return;
    }

    /* Only continue if this is a PCI device */
    if (dev->type != TROVNA_DEVICE_TYPE_PCI) {
        return;
    }

    /* Check NVIDIA Vendor ID */
    if (dev->vendor_id == NVIDIA_VENDOR_ID) {

        struct trovna_driver *nvidia_driver;

        nvidia_driver =
            trovna_driver_manager_get_driver(mgr, "nvidia");

        if (nvidia_driver) {

            /* Probe the NVIDIA driver */
            trovna_driver_probe(nvidia_driver, dev);
        }
    }
}

/* Register NVIDIA driver support */

void trovna_nvidia_drivers(struct trovna_manager *mgr)
{
    struct trovna_driver *nvidia_driver;
    struct trovna_probe *probe;

    if (!mgr) {
        return;
    }

    nvidia_driver =
        trovna_driver_manager_get_driver(mgr, "nvidia");

    if (!nvidia_driver) {
        return;
    }

    /* Create PCI probe */
    probe = trovna_probe_create();

    if (!probe) {
        return;
    }

    trovna_probe_set_device_type(
        probe,
        TROVNA_DEVICE_TYPE_PCI
    );

    trovna_probe_set_vendor_id(
        probe,
        NVIDIA_VENDOR_ID
    );

    /* Add probe into manager */
    trovna_manager_add_probe(mgr, probe);
}

/* Search NVIDIA hardware */

void trovna_nvidia_hardware(struct trovna_manager *mgr)
{
    struct trovna_probe *probe;

    if (!mgr) {
        return;
    }

    probe = trovna_probe_create();

    if (!probe) {
        return;
    }

    trovna_probe_set_device_type(
        probe,
        TROVNA_DEVICE_TYPE_PCI
    );

    trovna_probe_set_vendor_id(
        probe,
        NVIDIA_VENDOR_ID
    );

    trovna_manager_add_probe(mgr, probe);
}
