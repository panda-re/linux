#include <asm/uaccess.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/path.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/namei.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/netdevice.h>

#include <linux/mm_types.h>
#include <linux/pagemap.h>
#include <asm/pgtable.h>

#include <linux/proc_fs.h>

#include <linux/hypercall.h>
#include <linux/dyndev.h>

#include "hyperutils.h"

static char **device_name;
static int *device_major;
static int num_devices = 0;

static struct class* my_class  = NULL; // The device-driver class struct pointer

bool hook_mtd=false; // Set by dyndev, checked by mtdpart
EXPORT_SYMBOL(hook_mtd);


static int dyndev_open(struct inode *inodep, struct file *filep) {
    return 0;
}

static int dyndev_release(struct inode *inodep, struct file *filep) {
    return 0;
}



static ssize_t _dyndev_read(struct file *filep, char *buffer, size_t len, loff_t *offset, bool kernel) {
    char *full_path;
    char *path_buffer;
    ssize_t result;
    struct path path;

    // Allocate a temporary buffer for the path
    path_buffer = kmalloc(PATH_MAX, GFP_KERNEL);
    if (!path_buffer) {
        return -ENOMEM; // Return error if allocation failed
    }

    // Get the full path of the file
    path = filep->f_path;
    full_path = d_path(&path, path_buffer, PATH_MAX);

    // Check for errors
    if (IS_ERR(full_path)) {
        printk(KERN_ERR "IGLOO dyndev_read error: %ld\n", PTR_ERR(full_path));
        kfree(path_buffer);
        return PTR_ERR(full_path);
    }

    if (kernel) {
        result = hypervisor_read_kernel(full_path, buffer, len, offset);
    } else {
        result = hypervisor_read(full_path, buffer, len, offset);
    }

    // Free the temporary buffer
    kfree(path_buffer);

    return result;
}

static ssize_t dyndev_read(struct file *filep, char *buffer, size_t len, loff_t *offset) {
    // Read into userspace buffer
    return _dyndev_read(filep, buffer, len, offset, false);
}

static ssize_t dyndev_read_kernel(struct file *filep, char *buffer, size_t len, loff_t *offset) {
    // Read into kernel space buffer (for mmap)
    return _dyndev_read(filep, buffer, len, offset, true);
}

static ssize_t dyndev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
    char *full_path;
    char *path_buffer;
    ssize_t result;
    struct path path;

    // Allocate a temporary buffer for the path
    path_buffer = kmalloc(PATH_MAX, GFP_KERNEL);
    if (!path_buffer) {
        return -ENOMEM; // Return error if allocation failed
    }

    // Get the full path of the file
    path = filep->f_path;
    full_path = d_path(&path, path_buffer, PATH_MAX);

    // Check for errors
    if (IS_ERR(full_path)) {
        printk(KERN_ERR "IGLOO dyndev_write error: %ld\n", PTR_ERR(full_path));
        kfree(path_buffer);
        return PTR_ERR(full_path);
    }

    result = hypervisor_write(full_path, buffer, len, offset);

    // Free the temporary buffer
    kfree(path_buffer);

    return result;
}

static long dyndev_ioctl(struct file *filep, unsigned int cmd, unsigned long arg) {
    char *full_path;
    char path_buffer[128];
    struct path path;
    struct hyper_file_op hyper_op;
    hyper_op.type = HYPER_IOCTL;

    // Get the full path of the file
    path = filep->f_path;
    full_path = d_path(&path, path_buffer, 128);

    // Check for errors
    if (IS_ERR(full_path)) {
        printk(KERN_ERR "IGLOO dyndev_ioctl error: %ld\n", PTR_ERR(full_path));
        return PTR_ERR(full_path);
    }

    //hyper_op.device_name = path_buffer; // Can we do this?
    snprintf(hyper_op.device_name, 128, "%s", full_path);

    hyper_op.args.ioctl_args.cmd = cmd;
    hyper_op.args.ioctl_args.arg = arg;

    sync_struct(&hyper_op);

    return hyper_op.rv; // Return the value fetched from the emulator
}

static void dyndev_vma_close(struct vm_area_struct *vma)
{
    struct page *pages = vma->vm_private_data;
    __free_pages(pages, get_order(vma->vm_end - vma->vm_start));
}


static const struct vm_operations_struct dyndev_vm_ops = {
    .close = dyndev_vma_close,
};

static int dyndev_mmap(struct file *filp, struct vm_area_struct *vma) {
    size_t len = vma->vm_end - vma->vm_start;
    struct page *pages;
    unsigned long num_pages;
    unsigned long start = vma->vm_start;
    char *buffer;
    unsigned long pfn;
    int i, ret;

    // Calculate the number of pages
    num_pages = (len + PAGE_SIZE - 1) / PAGE_SIZE;

    // Allocate pages
    pages = alloc_pages(GFP_KERNEL, get_order(len));
    if (!pages)
        return -ENOMEM;

    // Get the buffer from the allocated pages
    buffer = kmap(pages);

    // Perform the "read" operation to fill the buffer
    // Note: Modify dyndev_read to work with this buffer or replicate its functionality here
#if 0
    ret = dyndev_read_kernel(filp, buffer, len, 0);
    if (ret < 0) {
        kunmap(pages);
        __free_pages(pages, get_order(len));
        return ret;
    }
#endif
    // Placeholder, place "foo" into buffer
    strcpy(buffer, "hello_world_this_is_a_test\0");

    // Now map each page to the user space
    for (i = 0; i < num_pages; i++) {
        pfn = page_to_pfn(&pages[i]);
        if (remap_pfn_range(vma, start, pfn, PAGE_SIZE, vma->vm_page_prot)) {
            kunmap(pages);
            __free_pages(pages, get_order(len));
            return -EAGAIN;
        }
        start += PAGE_SIZE;
    }

    // Unmap the buffer and ensure it's not accessed beyond this point
    kunmap(pages);

    // Store the pages pointer for cleanup
    vma->vm_ops = &dyndev_vm_ops;
    vma->vm_private_data = pages;

    return 0;
}

static struct file_operations fops = {
	.owner =	  THIS_MODULE,
    .open = dyndev_open,
    .read = dyndev_read,
    .mmap = dyndev_mmap,
    .release = dyndev_release,
    .write = dyndev_write,
    .unlocked_ioctl = dyndev_ioctl,
};

static char *rw_devnode(struct device *dev, umode_t *mode) {
    if (mode) {
        *mode = 0666; // read-write permissions for user, group, and others
    }
    return NULL;
}

int dyndev_init_devfs(char *devnames) {
    char *str, *token;
    dev_t current_dev;
    int i=0;

    if (!devnames || !(*devnames)) {
        printk(KERN_INFO "dyndev: no dev names provided\n");
        return 0;
    }

    // First, count the number of devices to allocate memory
    for (str = devnames; *str; str++) {
        if (*str == ',') {
            num_devices++;
        }
    }
    num_devices++; // Add one more for the last (or only) device name

    my_class = class_create(THIS_MODULE, "dyndev");
    if (IS_ERR(my_class)) {
        printk(KERN_ALERT "Dyndev: Failed to create class.\n");
        return -EINVAL;
    }

    // Ensure device is can be read & written by all users
    my_class->devnode = rw_devnode;


    // Allocate memory with error checking for device names and major numbers
    device_name = kmalloc(sizeof(char*) * num_devices, GFP_KERNEL);
    if (!device_name) {
        pr_err("Failed to allocate memory for device_name\n");
        return -ENOMEM;
    }
    device_major = kmalloc(sizeof(int) * num_devices, GFP_KERNEL);
    if (!device_major) {
        pr_err("Failed to allocate memory for device_major\n");
        kfree(device_name);
        return -ENOMEM;
    }

    // Now actually tokenize the string
    str = kstrdup(devnames, GFP_KERNEL);
    if (!str) {
        pr_err("Failed to duplicate devnames\n");
        kfree(device_name);
        kfree(device_major);
        return -ENOMEM;
    }

    while ((token = strsep(&str, ",")) != NULL) {
        if (!(*token)) {  // Check if the token is empty
            // We'll hit this if no device name is provided at all
            continue;
        }

        // If this token is 'mtd' we set a flag and skip
        if (strncmp(token, "mtd", 3) == 0) {
            // MTD is a special device that we handle with custom code in mtdpart.c
            // We'll set the static bool in our header so it knows to do special stuff
            hook_mtd = true;
            continue;
        }

        device_name[i] = kstrdup(token, GFP_KERNEL);
        // Initialize device_major[i] appropriately
        device_major[i] = register_chrdev(0, device_name[i], &fops);
        if (device_major[i] < 0) {
            printk(KERN_ALERT "Could not register device %s: %d\n", device_name[i], device_major[i]);
            return device_major[i];
        } else {
            printk(KERN_ALERT "Registered device %s: %d\n", device_name[i], device_major[i]);
            current_dev = MKDEV(device_major[i], 0);
            device_create(my_class, NULL, current_dev, NULL, "%s", device_name[i]);
        }
        i++;
    }
    return 0;
}

void dyndev_free_devfs(void) {
    int i;
    dev_t current_dev;
    for (i = 0; i < num_devices; i++) {
        current_dev = MKDEV(device_major[i], 0);
        device_destroy(my_class, current_dev);

        unregister_chrdev(device_major[i], device_name[i]);
        printk(KERN_INFO "Unregistered device %s\n", device_name[i]);
    }

    // Destroy the class
    class_destroy(my_class);
    kfree(device_major);
}
