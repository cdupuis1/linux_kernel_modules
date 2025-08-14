/*
 * Simple SCSI host module
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <linux/stdarg.h>
#include <linux/device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_cmnd.h>

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

static int scsi_sample_queuecommand(struct Scsi_Host *shost, struct scsi_cmnd *cmd)
{
    SS_DEBUG_INFO("cmd opcode=0x%x", cmd->cmnd[0]);

    // Return DID_NO_CONNECT for now as we can't process any commands
    cmd->result = DID_NO_CONNECT << 16;
    scsi_done(cmd);
    return 0;
}

static const struct scsi_host_template scsi_sample_template = {
	.name =			"SCSI_SAMPLE",
	.queuecommand =		scsi_sample_queuecommand,
	.can_queue =		1,
	.this_id =		7,
	.sg_tablesize =		SG_MAX_SEGMENTS,
	.cmd_per_lun =		1,
	.max_sectors =		-1U,
	.max_segment_size =	-1U,
	.module =		THIS_MODULE,
	.skip_settle_delay =	1,
	.track_queue_depth =	0,
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
    int rval = 0;

    SS_DEBUG_INFO("%s(): Entered", __func__);

    ss.shost = scsi_host_alloc(&scsi_sample_template, 0);
    if (ss.shost == NULL) {
        SS_DEBUG_WARN("scsi_host_alloc failed");
        rval = -ENODEV;
        goto out;
    }

    /* Set dma boundary to PAGE_SIZE - 1 so we don't get multiple pages */
    ss.shost->dma_boundary = PAGE_SIZE - 1;

    /* No SCSI multiqueue */
    ss.shost->nr_hw_queues = 1;

    /* Just one target and lun */
    ss.shost->max_id = 1;
    ss.shost->max_lun = 1;

    /* Add the host to the mid-layer*/
    rval = scsi_add_host(ss.shost, &ss.dev);
    if (rval)
    {
        SS_DEBUG_WARN("scsi_host_add failed %d", rval);
        rval = -ENODEV;
        goto free_scsi_host;
    }

    /* Start ur scanning! */
    scsi_scan_host(ss.shost);

    return 0;

free_scsi_host:
    scsi_host_put(ss.shost);
out:
    return rval;
}

static void scsi_sample_remove(struct device *dev)
{
    SS_DEBUG_INFO("%s(): Entered", __func__);

    // Remove shost
    scsi_remove_host(ss.shost);

    // Free Scsi_Host
    scsi_host_put(ss.shost);
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
