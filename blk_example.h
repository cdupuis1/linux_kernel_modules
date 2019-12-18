#include <linux/blk-mq.h>

#define DRV_NAME        "blk_example"

#define BLK_EX_TMO		(60 * HZ)

/* Number of sectors */
#define BLK_EX_SIZE		1000
typedef struct {
    struct blk_mq_tag_set tagset;
    struct request_queue *rq_queue;
    struct gendisk *disk;
    struct kobject *kobj;
} blk_example;

typedef struct {
    uint8_t info;
} blk_example_cmd;
