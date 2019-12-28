/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2019 Chad Dupuis
 *
 * Example multiqueue block driver mainly for understanding how a block
 * driver works.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/genhd.h>
#include <linux/numa.h>
#include <linux/vmalloc.h>
#include "blk_example.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Chad Dupuis");
MODULE_DESCRIPTION("Sample Block Driver");
MODULE_VERSION("1");

int blk_example_major;
blk_example blk_ex;

unsigned int num_sectors;
module_param(num_sectors, uint, S_IRUGO);

/*
 * Work queue callback to perform the deferred processing of a request. This
 * is where the real work of processing the bio_vec's in the request to copy
 * data to/from the backing memory store.
 */
static void blk_example_complete(struct work_struct *work)
{
	struct req_iterator iter;
	struct bio_vec bvec;
	blk_example_cmd *cmd =
		container_of(work, blk_example_cmd, work);
	sector_t sector = blk_rq_pos(cmd->req);
	void *store_addr, *page_addr;

	/*
	 * Actual address into the store depends on which sector we're read/
	 * writing to.
	 */
	store_addr = blk_ex.store + sector * 512;

	mutex_lock(&blk_ex.store_mutex);
	rq_for_each_segment(bvec, cmd->req, iter) {
		/* Get memory of address to use in memcpy */
		page_addr = kmap_atomic(bvec.bv_page);
		if (page_addr == NULL) {
			cmd->status = BLK_STS_IOERR;
			mutex_unlock(&blk_ex.store_mutex);
			goto out;
		}

		/* Adjust page address based on offset in bio_vec */
		page_addr = page_addr + bvec.bv_offset;
		if (op_is_write(req_op(cmd->req))) {
			memcpy(store_addr, page_addr, bvec.bv_len);
		} else {
			memcpy(page_addr, store_addr, bvec.bv_len);
		}

		/* Update the address of the store based on the length */
		store_addr = store_addr + bvec.bv_len;
		kunmap_atomic(page_addr);
	}
	mutex_unlock(&blk_ex.store_mutex);

	/* We're always sunshine and lollipops */
	cmd->status = BLK_STS_OK;

out:
	/* Tell the block layer we're doing with the i/o */
	blk_mq_complete_request(cmd->req);
}

/* Callback block layer uses to queue a request to our driver */
static blk_status_t blk_example_queue_rq(struct blk_mq_hw_ctx *hctx,
	const struct blk_mq_queue_data *bd)
{
	struct request *rq = bd->rq;
	blk_example_cmd *cmd = blk_mq_rq_to_pdu(rq);

	/* Tell the block layer we've started processing this request */
	blk_mq_start_request(rq);

	/* Save the requst back pointer */
	cmd->req = rq;

	/* Queue the actual copy and completion to a work queue */
	INIT_WORK(&cmd->work, blk_example_complete);
	schedule_work(&cmd->work);

	return BLK_STS_OK;
}

/* Callback for when the block layer completes a request */
static void blk_example_complete_rq(struct request *rq)
{
	blk_example_cmd *cmd = blk_mq_rq_to_pdu(rq);

	cmd->req = NULL;
	/* Tell block layer to complete this back to upper layers */
	blk_mq_end_request(rq, cmd->status);
}

static const struct blk_mq_ops blk_example_ops = {
	.queue_rq = blk_example_queue_rq,
	.complete = blk_example_complete_rq
};

static struct kobject *blk_example_probe(dev_t dev, int *part, void *data)
{
	return NULL;
}

/*
 * Block file operation handlers
 */

static int blk_example_open(struct block_device *bdev, fmode_t mode)
{
	return 0;
}

static void blk_example_release(struct gendisk *disk, fmode_t mode)
{}

static int blk_example_ioctl(struct block_device *bdev, fmode_t mode,
	unsigned int cmd, unsigned long arg)
{
	return 0;
}

static const struct block_device_operations blk_ex_fops = {
	.owner =	THIS_MODULE,
	.open =		blk_example_open,
	.release =	blk_example_release,
	.ioctl =	blk_example_ioctl,
};

static int __init blk_example_init(void) {
	int rc;
	int retval;

	if (num_sectors == 0)
		num_sectors = BLK_EX_SIZE;

	blk_ex.store = vmalloc(num_sectors * 512);
	if (!blk_ex.store) {
		pr_info("%s(): store is NULL\n", __func__);
		return -ENOMEM;
	}

	rc = register_blkdev(0, DRV_NAME);
	if (rc < 0) {
		pr_warn("%s(): register_blkdev() failed rc=%d\n", __func__, rc);
		return rc;
	}

	blk_example_major = rc;

	blk_register_region(MKDEV(blk_example_major, 0), 1, THIS_MODULE,
		blk_example_probe, NULL, NULL);
	
	/* Set up tagset with basic definitions about our queue size and metadata */
	blk_ex.tagset.ops = &blk_example_ops;
	blk_ex.tagset.nr_hw_queues = 1;
	blk_ex.tagset.queue_depth = BLK_EX_Q_DEPTH;
	blk_ex.tagset.numa_node = NUMA_NO_NODE;
	blk_ex.tagset.cmd_size = sizeof(blk_example_cmd);
	blk_ex.tagset.timeout = BLK_EX_TMO;
	blk_ex.tagset.driver_data = &blk_ex;

	rc = blk_mq_alloc_tag_set(&blk_ex.tagset);
	if (rc) {
		pr_warn("%s(): Tag set allocation failed", __func__);
		retval = -ENOMEM;
		goto out_unreg_blk;
	}

	/* Alocate the block request queue based on the tag set */
	blk_ex.rq_queue = blk_mq_init_queue(&blk_ex.tagset);
	if (IS_ERR(&blk_ex.rq_queue)) {
		pr_warn("%s(): Request queue allocation failed\n", __func__);
		retval = -EINVAL;
		goto out_clean_tags;
	}

	/* Set backpointer for reference if needed */
	blk_ex.rq_queue->queuedata = &blk_ex;

	/* Set default max sector parameter */
	blk_queue_max_hw_sectors(blk_ex.rq_queue, BLK_DEF_MAX_SECTORS);

	/* Set no merges so that each request is it's own page */
	blk_queue_flag_set(QUEUE_FLAG_NOMERGES, blk_ex.rq_queue);

	/* 
	 * Allocate the block lqyer gendisk structure with one partition
	 * to fill in
	 */
	blk_ex.disk = alloc_disk(1);
	if (!blk_ex.disk) {
		pr_warn("%s(): alloc_disk failed\n", __func__);
		goto out_free_queue;
	}
	
	/* Setup gendisk object to call add_disk */
	blk_ex.disk->major = blk_example_major;
	blk_ex.disk->first_minor = 1;
	blk_ex.disk->fops = &blk_ex_fops;
	blk_ex.disk->private_data = &blk_ex;
	blk_ex.disk->queue = blk_ex.rq_queue;
	sprintf(blk_ex.disk->disk_name, "blk_example");

	/* Set in number of 1k byte sectors */
	set_capacity(blk_ex.disk, num_sectors);

	/* Announce to the world that I'm here */
	add_disk(blk_ex.disk);

	mutex_init(&blk_ex.store_mutex);

	return 0;

out_free_queue:
	blk_cleanup_queue(blk_ex.rq_queue);		
out_clean_tags:
	blk_mq_free_tag_set(&blk_ex.tagset);
out_unreg_blk:
	blk_unregister_region(MKDEV(blk_example_major, 0), 1);
	unregister_blkdev(blk_example_major, DRV_NAME);

	return retval;
}

static void __exit blk_example_exit(void) {
	del_gendisk(blk_ex.disk);
	blk_cleanup_queue(blk_ex.rq_queue);
	blk_mq_free_tag_set(&blk_ex.tagset);
	put_disk(blk_ex.disk);
	blk_unregister_region(MKDEV(blk_example_major, 0), 1);
	unregister_blkdev(blk_example_major, DRV_NAME);
	vfree(blk_ex.store);
}

module_init(blk_example_init);
module_exit(blk_example_exit);
