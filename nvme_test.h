#pragma once

#include <linux/nvme.h>
#include "nvme.h"

#define	BACKING_STORE_SIZE	(100 * 1024 * 1024)
#define ADMIN_Q_DEPTH       32

struct nvme_test_queue_pair {
    struct nvme_command *sqes;
    struct nvme_completion *cqes;
};

struct nvme_test_regs {
    uint64_t cap; /* Capabilities */
};

struct nvme_test_dev {
    void *store; /* Pointer to backing memory*/
    struct device dev;
    struct nvme_ctrl ctrl;
    struct nvme_test_queue_pair adminq;
    struct nvme_test_regs regs;
};

