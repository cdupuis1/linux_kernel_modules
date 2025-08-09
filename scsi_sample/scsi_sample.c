/*
 * Simple SCSI host module
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <linux/stdarg.h>
#include <linux/device.h>
#include "scsi_sample.h"

unsigned int size_in_mb;
module_param_named(size_in_mb, size_in_mb, uint, S_IRUGO);

struct scsi_sample ss;

static const struct bus_type chad_lld_bus;

static const char ss_proc_name[] = DRIVER_NAME;

static struct device_driver ss_driverfs_driver = {
    .name = ss_proc_name,
    .bus = &chad_lld_bus,
};

static void scsi_sample_device_release(struct device *dev)
{
    /* Could use to free memory for specific adapter instance if wanted */
}

static int __init scsi_sample_init(void)
{
    uint32_t backing_store_size;
    int retval;

    SS_DEBUG_INFO("Module version %s", SCSI_SAMPLE_VERSION);

    /* Sanity check size module parameter */
    if (size_in_mb == 0) {
        size_in_mb = SCSI_SAMPLE_DEFAULT_SIZE;
    }

    /*
     * Allocate the backing store.  Use vmalloc since we may be
     * allocating a lot of memory.
     */
    backing_store_size = size_in_mb * (1024 * 1024);
    SS_DEBUG_INFO("Backing store size = %u", backing_store_size);
    ss.backing_store = vmalloc(backing_store_size);
	if (!ss.backing_store) {
		SS_DEBUG_INFO("%s(): store is NULL", __func__);
		return -ENOMEM;
	}

    /* Creates directory under /sys/devices */
    ss.fake_root_device = root_device_register("chad_root_dev");
    if (IS_ERR(ss.fake_root_device)) {
        SS_DEBUG_WARN("Error creating root device");
        retval = -ENOMEM;
        goto free_backing_store;
    }

    /* Create a device subsystem */
	retval = bus_register(&chad_lld_bus);
	if (retval < 0) {
		SS_DEBUG_WARN("bus_register error: %d", retval);
		goto root_unregister;
	}

    /* Register a driver to do with the bus */
	retval = driver_register(&ss_driverfs_driver);
	if (retval < 0) {
		SS_DEBUG_WARN("driver_register error: %d", retval);
		goto bus_unregister;
	}

    /* Fill out device struct */
    ss.dev.bus = &chad_lld_bus;
    ss.dev.parent = ss.fake_root_device;
    ss.dev.release = &scsi_sample_device_release;
    dev_set_name(&ss.dev, "scsi_sample");

    /*
     * Note this call is what causes the driver subsystem to call our probe
     * routine.
     */
    retval = device_register(&ss.dev);
    if (retval > 0)
    {
        SS_DEBUG_WARN("device_register error: %d", retval);
        goto driver_unregister;
    }

    return 0;

driver_unregister:
    driver_unregister(&ss_driverfs_driver);
bus_unregister:
    bus_unregister(&chad_lld_bus);
root_unregister:
    root_device_unregister(ss.fake_root_device);
free_backing_store:
    vfree(ss.backing_store);

    return retval;
}

static void __exit scsi_sample_exit(void)
{
    /* Unregister device */
    device_unregister(&ss.dev);

    /* Unregister driver */
    driver_unregister(&ss_driverfs_driver);

    /* Unregister fake bus */
    bus_unregister(&chad_lld_bus);

    /* Unregister root device */
    root_device_unregister(ss.fake_root_device);

    /* Free backing store*/
    vfree(ss.backing_store);

    SS_DEBUG_INFO("Module unloaded");
}

module_init(scsi_sample_init);
module_exit(scsi_sample_exit);

static int scsi_sample_probe(struct device *dev)
{
    SS_DEBUG_INFO("%s(): Entered", __func__);

    /* Add scsi host alloc/add code here */
    return 0;
}

static void scsi_sample_remove(struct device *dev)
{
    SS_DEBUG_INFO("%s(): Entered", __func__);

    /* Add scsi host remove code here */
}

static const struct bus_type chad_lld_bus = {
    .name = "chad_bus",
    .probe = scsi_sample_probe,
    .remove = scsi_sample_remove,
};

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Chad Dupuis");
MODULE_DESCRIPTION("Simple SCSI host module");
MODULE_VERSION("0.1");
