#ifndef _SCSI_SAMPLE_H_
#define _SCSI_SAMPLE_H_

/* Default size in MB */
#define SCSI_SAMPLE_DEFAULT_SIZE        100

#define SCSI_SAMPLE_VERSION     "0.1"

/* Debug print macros */
#define SS_DEBUG_INFO(format, ...) \
    pr_info("scsi_sample: " format "\n", ##__VA_ARGS__)

#define SS_DEBUG_WARN(format, ...) \
    pr_warn("scsi_sample: " format "\n", ##__VA_ARGS__)

#define DRIVER_NAME     "scsi_sample"

struct scsi_sample {
    void *backing_store;
    struct device *fake_root_device;
    struct device dev;
};

#endif