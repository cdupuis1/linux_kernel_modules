#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <linux/device.h>
#include <linux/async.h>
#include "nvme_test.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yakko Warner");
MODULE_DESCRIPTION("NVMe test module");
MODULE_VERSION("0.1");

struct nvme_test_dev nvme;
struct nvme_test_regs regs;

static void nvme_test_dev_release(struct device *thedev)
{
	pr_crit("%s(): Entered\n", __func__);
}

static int nvme_test_reg_read32(struct nvme_ctrl *ctrl, u32 off, u32 *val)
{
	*val = 0;
	return 0;
}

static int nvme_test_reg_write32(struct nvme_ctrl *ctrl, u32 off, u32 val)
{
	return 0;
}

static int nvme_test_reg_read64(struct nvme_ctrl *ctrl, u32 off, u64 *val)
{
	struct nvme_test_dev *nvme =
		container_of(ctrl, struct nvme_test_dev, ctrl);

	switch (off) {
	case NVME_REG_CAP:
		*val = nvme->regs.cap;
		break;
	default:
		*val = 0;
	}

	return 0;
}

static int nvme_test_get_address(struct nvme_ctrl *ctrl, char *buf, int size)
{
	return snprintf(buf, size, "nvme_test");
}

static void nvme_test_free_ctrl(struct nvme_ctrl *ctrl) {}

static void nvme_test_submit_async_event(struct nvme_ctrl *ctrl) {}

static const struct nvme_ctrl_ops nvme_test_ctrl_ops = {
	.name				= "nvme_test",
	.module				= THIS_MODULE,
	.flags				= 0,
	.reg_read32			= nvme_test_reg_read32,
	.reg_write32		= nvme_test_reg_write32,
	.reg_read64			= nvme_test_reg_read64,
	.free_ctrl			= nvme_test_free_ctrl,
	.submit_async_event	= nvme_test_submit_async_event,
	.get_address		= nvme_test_get_address,
};

static int nvme_test_alloc_admin_queue(struct nvme_test_dev *nvme)
{
	nvme->adminq.cqes = kmalloc(GFP_KERNEL,
		sizeof(struct nvme_completion) * ADMIN_Q_DEPTH);
	if (!nvme->adminq.cqes) {
		pr_crit("%s(): Failed to allocate admin cqes\n", __func__);
		return -ENOMEM;
	}

	nvme->adminq.sqes = kmalloc(GFP_KERNEL,
		sizeof(struct nvme_command) * ADMIN_Q_DEPTH);
	if (!nvme->adminq.sqes) {
		pr_crit("%s(): Failed to allocate admin sqes\n", __func__);
		return -ENOMEM;
	}

	return 0;
}

static void nvme_test_free_admin_queue(struct nvme_test_dev *nvme)
{
	kfree(nvme->adminq.sqes);
	kfree(nvme->adminq.cqes);
}

static void nvme_test_reset_work(struct work_struct *work)
{
	struct nvme_test_dev *nvme =
		container_of(work, struct nvme_test_dev, ctrl.reset_work);
	int result;

	pr_info("%s(): Entered\n", __func__);

	/*      
     * Introduce CONNECTING state from nvme-fc/rdma transports to mark the
     * initializing procedure here.
     */             
    if (!nvme_change_ctrl_state(&nvme->ctrl, NVME_CTRL_CONNECTING)) {
        pr_info("%s(): failed to mark controller CONNECTING\n", __func__);
        result = -EBUSY;
        goto out;
    }

	/* Allocate admin queues */
	result = nvme_test_alloc_admin_queue(nvme);
	if (result)
		goto free_admin_queues;

    //result = nvme_init_identify(&nvme->ctrl);
	//if (result)
        //goto out;

	nvme_start_ctrl(&nvme->ctrl);
	
	return;
free_admin_queues:
	nvme_test_free_admin_queue(nvme);
out:
	pr_info("%s(): Out reset failed\n", __func__);
}

static void nvme_test_async_probe(void *data, async_cookie_t cookie)
{
	struct nvme_test_dev *nvme = data;

	flush_work(&nvme->ctrl.reset_work);
	flush_work(&nvme->ctrl.scan_work);
	nvme_put_ctrl(&nvme->ctrl);
}

static void nvme_test_set_regs(struct nvme_test_dev *nvme)
{
	nvme->regs.cap = ADMIN_Q_DEPTH; // Bits 0-15 
}

static int __init nvme_test_init(void)
{
	int ret;

	nvme.store = vmalloc(BACKING_STORE_SIZE);
	if (!nvme.store) {
		pr_info("%s(): store is NULL\n", __func__);
		return -ENOMEM;
	}

	/* Bus is null as we are not connected to a physical transport */
	nvme.dev.bus = NULL;
	nvme.dev.release = &nvme_test_dev_release;
	dev_set_name(&nvme.dev, "nvme_test");
	ret = device_register(&nvme.dev);
	if (ret != 0) {
		put_device(&nvme.dev);
		pr_info("%s(): device_register() failed\n", __func__);
		goto free_store;
	}

	INIT_WORK(&nvme.ctrl.reset_work, nvme_test_reset_work);

	ret = nvme_init_ctrl(&nvme.ctrl, &nvme.dev, &nvme_test_ctrl_ops, 0);
	if (ret) {
		pr_info("%s(): nvme_init_ctlr failed\n", __func__);
		goto unreg_dev;
	}

	// Set up dummy registers
	nvme_test_set_regs(&nvme);

	nvme_reset_ctrl(&nvme.ctrl);
	nvme_get_ctrl(&nvme.ctrl);
	async_schedule(nvme_test_async_probe, &nvme);

	/* All good in da hood */
	return 0;

unreg_dev:
	device_unregister(&nvme.dev);
free_store:
	vfree(nvme.store);
	return ret;
}

static void __exit nvme_test_exit(void) {
	nvme_change_ctrl_state(&nvme.ctrl, NVME_CTRL_DELETING);
	nvme_test_free_admin_queue(&nvme);
	nvme_stop_ctrl(&nvme.ctrl);
	nvme_uninit_ctrl(&nvme.ctrl);
	nvme_put_ctrl(&nvme.ctrl);
	device_unregister(&nvme.dev);
	vfree(nvme.store);
}

module_init(nvme_test_init);
module_exit(nvme_test_exit);
