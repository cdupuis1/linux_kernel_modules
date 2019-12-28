/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2019 Chad Dupuis
 */
#include <linux/blk-mq.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>

#define DRV_NAME        "blk_example"

#define BLK_EX_TMO		(5 * HZ)

/* Number of 512 byte sectors */
#define BLK_EX_SIZE		2000

#define BLK_EX_Q_DEPTH      16

typedef struct {
    struct work_struct work;
    blk_status_t status;
    struct request *req; /* Back pointer to request */
} blk_example_cmd;

typedef struct {
    struct blk_mq_tag_set tagset;
    struct request_queue *rq_queue;
    struct gendisk *disk;
    void *store;
    struct mutex store_mutex;
} blk_example;
