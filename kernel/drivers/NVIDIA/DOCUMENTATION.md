# Trovna Documentation

## About

Trovna is the experimental NVIDIA hardware subsystem for Carl-OS.

The purpose of Trovna is to provide:
- NVIDIA GPU detection
- PCI hardware communication
- experimental framebuffer support
- future graphics acceleration research
- low-level GPU experiments

Trovna is currently under early development and research.

---

# Goals

Current goals of Trovna include:

- Detect NVIDIA GPUs using PCI
- Read NVIDIA Vendor IDs
- Initialize basic graphics hardware
- Create framebuffer experiments
- Develop modular driver architecture

Future goals may include:
- hardware acceleration
- display management
- GPU memory communication
- graphics rendering support

---

# Architecture

Trovna uses a modular driver architecture inspired by modern operating systems.

Example structure:

```text
drivers/
 └── trovna/
      ├── pci.c
      ├── device.c
      ├── framebuffer.c
      ├── init.c
      ├── probe.c
      └── trovna.h
