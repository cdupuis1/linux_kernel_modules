#include <linux/blk-mq.h>

#define DRV_NAME        "blk_example"

typedef struct {
    struct blk_mq_tag_set tagset;
    struct request_queue *rq_queue;
    struct gendisk *disk;
} blk_example;

typedef struct {
    uint8_t info;
} blk_example_cmd;