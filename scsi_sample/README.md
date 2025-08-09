Introduction
============

The point of this module is to make a minimal but functional SCSI host module. This is mostly a learning project for myself to try to recall how the SCSI layer works and how to create a low level module for it.

Module Initialization
=====================

When initialization the module we:

* Use vmalloc to allocate a back store to simulate our storage device
* We then register a fake bus to list our device under /sys/devices
* Register the bus and associted driver to the kernel knows our probe and remove routines
* Call device_register so the kernel will call the probe routine

Acknowledgement
===============

Much of the code here is based on the drivers/scsi/scsi_debug.c in the Linux kernel
