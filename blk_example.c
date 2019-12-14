#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/genhd.h>
#include <linux/numa.h>
#include "blk_example.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yakko Warner");
MODULE_DESCRIPTION("Sample Block Driver");
MODULE_VERSION("1");

int blk_example_major;
blk_example blk_ex;

static blk_status_t blk_example_queue_rq(struct blk_mq_hw_ctx *hctx,
	const struct blk_mq_queue_data *bd)
{
	return BLK_STS_IOERR;
}

static void blk_example_complete_rq(struct request *rq)
{}

static const struct blk_mq_ops blk_example_ops = {
	.queue_rq = blk_example_queue_rq,
	.complete = blk_example_complete_rq
};

static struct kobject *blk_example_probe(dev_t dev, int *part, void *data)
{
	pr_info("%s(): Entered\n", __func__);
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

	rc = register_blkdev(0, DRV_NAME);
	if (rc < 0) {
		pr_warn("%s(): register_blkdev() failed rc=%d\n", __func__, rc);
		return rc;
	}

	blk_example_major = rc;
	pr_info("%s(): Major number: %d\n", __func__, blk_example_major);

	blk_register_region(MKDEV(blk_example_major, 0), 1, THIS_MODULE,
		blk_example_probe, NULL, NULL);
	
	/* Set up tagset with basic definitions about our queue size and metadata */
	blk_ex.tagset.ops = &blk_example_ops;
	blk_ex.tagset.nr_hw_queues = 1;
	blk_ex.tagset.queue_depth = 1;
	blk_ex.tagset.numa_node = NUMA_NO_NODE;
	blk_ex.tagset.cmd_size = sizeof(blk_example_cmd);
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
	add_disk(blk_ex.disk);

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
	pr_info("%s(): Enter\n", __func__);

	del_gendisk(blk_ex.disk);
	blk_cleanup_queue(blk_ex.rq_queue);
	blk_mq_free_tag_set(&blk_ex.tagset);
	put_disk(blk_ex.disk);
	blk_unregister_region(MKDEV(blk_example_major, 0), 1);
	unregister_blkdev(blk_example_major, DRV_NAME);
}

module_init(blk_example_init);
module_exit(blk_example_exit);
