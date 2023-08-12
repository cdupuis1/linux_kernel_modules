/*
 * Sample misc driver.  Uses a memory store to save write data that can be read
 * back using a read.  Supports lseek to be able to set the current position
 * where we read/write from.
 * 
 * (c) 2023 Chad Dupuis
 */
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/vmalloc.h>

/* Size of the memory we allocate for our backing store */
#define STORE_SIZE      (100 * 1024 * 1024)

/* Pointer to our backing memory */
uint8_t *store;

/* Where in our backing store our file currently points to */
size_t current_idx;

static int misc_example_open(struct inode *inode, struct file *file)
{
    pr_info("%s(): Entered\n", __func__);
    return 0;
}

static int misc_example_close(struct inode *inode, struct file *file)
{
    pr_info("%s(): Entered\n", __func__);
    return 0;
}

static ssize_t misc_example_read(struct file *file, char __user *buf, size_t len, loff_t *pos)
{
    int error;

    if (len > STORE_SIZE) {
        pr_warn("%s(): len=%zu greater than %d\n", __func__, len, STORE_SIZE);
        /* Note that on error we return zero bytes read and not an error */
        return 0;
    }

    if ((current_idx + len) > STORE_SIZE) {
        pr_info("%s(): Write passed end of device\n", __func__);
        return 0;
    }

    error = copy_to_user(buf, &store[current_idx], len);
    if (error) {
        pr_warn("%s(): copy_to_user failed error=%d", __func__, error);
        return 0;
    }

    /* Increment the current pointer to our backing memory */
    current_idx += len;
    pr_info("%s(): Copied %zu bytes, current_idx=%zu\n", __func__, len, current_idx);

    return len;
}

static ssize_t misc_example_write(struct file *file, const char __user *buf, size_t len, loff_t *pos)
{
    int error;

    if (len > STORE_SIZE) {
        pr_warn("%s(): len=%zu greater than %d\n", __func__, len, STORE_SIZE);
        /* Note that on error we return 0 bytes written */
        return 0;
    }

    if ((current_idx + len) > STORE_SIZE) {
        pr_info("%s(): Write passed end of device\n", __func__);
        return 0;
    }

    error = copy_from_user(&store[current_idx], buf, len);
    if (error) {
        pr_info("%s(): copy_from_user failed error=%d", __func__, error);
        return 0;
    }

    /* Increment the current pointer to our backing memory */
    current_idx += len;
    pr_info("%s(): Copied %zu bytes, current_idx=%zu\n", __func__, len, current_idx);
    
    return len;
}

static loff_t misc_example_llseek(struct file *filp, loff_t offset, int whence)
{
    switch (whence) {
    case SEEK_SET:
        /* Set the absolute position of our backing memory index*/
        if (offset > STORE_SIZE) {
            pr_warn("%s(): Seek out of range\n", __func__);
            return -ESPIPE;
        }
        current_idx = offset;
        break;

    case SEEK_CUR:
        /* Set the relative position of our backing memory index */
        if (current_idx + offset > STORE_SIZE) {
            pr_warn("%s(): seek out of range\n", __func__);
            return -ESPIPE;
        }
        current_idx += offset;
        break;

    case SEEK_END:
        /* Go to the end of our backing memory */
        current_idx = STORE_SIZE;
        break;

    default:
        return -EINVAL;
    }

    pr_info("%s(): current_idx=%zu", __func__, current_idx);
    return current_idx;
}

static const struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = misc_example_open,
    .release = misc_example_close,
    .read = misc_example_read,
    .write = misc_example_write,
    .llseek = misc_example_llseek
};

struct miscdevice misc_example_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "misc_example",
    .fops = &fops
};

static int __init misc_example_init(void)
{
    int error;

    /* Usually vmalloc is better for larger memory allocations */
    store = vmalloc(STORE_SIZE);
    if (store == NULL) {
        return -ENOMEM;
    }
    pr_info("%s(): store=%p\n", __func__, store);

    /* Register our device including our file ops */
    error = misc_register(&misc_example_device);
    if (error) {
        pr_err("misc_register failed, error=%d\n", error);
        return error;
    }

    pr_info("Hello from %s\n", __func__);

    /* Set the current index to the beginning of our memory store */
    current_idx = 0;

    return 0;
}

static void __exit misc_example_exit(void)
{
    pr_info("Good bye from %s\n", __func__);
    vfree(store);
    misc_deregister(&misc_example_device);
}

module_init(misc_example_init);
module_exit(misc_example_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Chad Dupuis");
MODULE_DESCRIPTION("Sample misc driver");
MODULE_VERSION("0.01");
