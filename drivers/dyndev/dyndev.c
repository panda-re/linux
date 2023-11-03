#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/hypercall.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Andrew");
MODULE_DESCRIPTION("Dynamic devices");

#define MAX_DEVICES 64  // Maximum number of devices
static struct class*  my_class  = NULL; // The device-driver class struct pointer

static char *devnames = "";
module_param(devnames, charp, 0000);
MODULE_PARM_DESC(devnames, "A comma-separated list of device names");

static char **device_name;
static int *device_major;
static int num_devices = 0;

#define HYPER_FILE_OP 0x100200
enum request_type {
    HYPER_READ,
    HYPER_WRITE,
    HYPER_IOCTL,
    // Add other operation types as needed
};

struct hyper_read_args {
    char *buffer;
    size_t length;
    loff_t offset;
};

struct hyper_write_args {
    const char *buffer;
    size_t length;
    loff_t offset;
};

struct hyper_ioctl_args {
    unsigned int cmd;
    unsigned long arg;
};

struct hyper_file_op {
    enum request_type type;
    unsigned long rv;
    char device_name[128];
    union {
        struct hyper_read_args read_args;
        struct hyper_write_args write_args;
        struct hyper_ioctl_args ioctl_args;
    } args;
};

void sync_struct(struct hyper_file_op* struct_instance) {
    int i;
    volatile char junk = 0;
    int max_tries = 100;
    while (max_tries-- > 0) {
        if (igloo_hypercall2(HYPER_FILE_OP, (unsigned long)struct_instance, (unsigned long)sizeof(struct hyper_file_op)) == 0)
            break;
        for (i = 0; i < sizeof(struct hyper_file_op); i++) {
            // Ensure we read the entire structure just to make sure it's paged in
            junk += ((char*)struct_instance)[i];
        }

        // Check if it's of type HYPER_READ and if so, copy the data back
        if (struct_instance->type == HYPER_READ) {
            // page in the buffer - read up to length bytes
            for (i = 0; i < struct_instance->args.read_args.length; i++) {
                junk += (struct_instance->args.read_args.buffer + struct_instance->args.read_args.offset)[i];
            }
        } else if (struct_instance->type == HYPER_WRITE) {
            // page in the buffer - read up to length bytes
            for (i = 0; i < struct_instance->args.write_args.length; i++) {
                junk += (struct_instance->args.write_args.buffer + struct_instance->args.write_args.offset)[i];
            }
        }
    }
    (void)junk;  // Suppress possible unused variable warning
    if (max_tries == 0) {
        pr_emerg("dyndev: failed to sync struct\n");
    }
}

static int dev_open(struct inode *inodep, struct file *filep) {
    return 0;
}

static int dev_release(struct inode *inodep, struct file *filep) {
    return 0;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset) {
    char* kernel_buffer;
    struct hyper_file_op hyper_op;
    hyper_op.type = HYPER_READ;
    strncpy(hyper_op.device_name, filep->f_path.dentry->d_iname, 127);

    // Our plugin needs to write a buffer - let's use a kernel buffer and copy at the end
    // Allocate a buffer of size len
    kernel_buffer = kmalloc(len, GFP_KERNEL);
    if (!kernel_buffer) {
        pr_err("Failed to allocate memory for kernel_buffer\n");
        return -ENOMEM;
    }

    hyper_op.args.read_args.buffer = kernel_buffer;
    hyper_op.args.read_args.length = len;
    hyper_op.args.read_args.offset = *offset;
    //printk(KERN_INFO "dyndev: Reading from device %s with len %d and offset %lld\n", hyper_op.device_name, len, *offset);

    sync_struct(&hyper_op);

    //printk(KERN_INFO "dyndev: hyper_op.rv = %ld\n", hyper_op.rv);

    // Now copy from the kernel buffer to the user buffer - use copy_to_user
    if (copy_to_user(buffer, kernel_buffer, len)) {
        pr_err("Failed to copy kernel_buffer to user buffer\n");
        return -EFAULT;
    }

    // Now update the offset
    if (hyper_op.rv > 0) {
        *offset += hyper_op.rv;
    }
    //printk(KERN_INFO "dyndev: after read set offset to %ld\n", *offset);

    // Free our buffer
    kfree(kernel_buffer);

    return hyper_op.rv; // Return the value fetched from the emulator
}

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
    struct hyper_file_op hyper_op;
    hyper_op.type = HYPER_WRITE;
    strncpy(hyper_op.device_name, filep->f_path.dentry->d_iname, 127);
    hyper_op.args.write_args.buffer = buffer;
    hyper_op.args.write_args.length = len;
    hyper_op.args.write_args.offset = *offset;

    //printk(KERN_INFO "dyndev: Writing device %s with len %d and offset %lld\n", hyper_op.device_name, len, *offset);
    sync_struct(&hyper_op);
    //printk(KERN_INFO "dyndev: hyper_op.rv = %ld\n", hyper_op.rv);

    // Now update the offset
    if (hyper_op.rv > 0) {
        *offset += hyper_op.rv;
    }
    //printk(KERN_INFO "dyndev: after write set offset to %lld\n", *offset);
    return hyper_op.rv; // Return the value fetched from the emulator
}

static long dev_ioctl(struct file *filep, unsigned int cmd, unsigned long arg) {
    struct hyper_file_op hyper_op;
    hyper_op.type = HYPER_IOCTL;
    strncpy(hyper_op.device_name, filep->f_path.dentry->d_iname, 127);
    hyper_op.args.ioctl_args.cmd = cmd;
    hyper_op.args.ioctl_args.arg = arg;

    sync_struct(&hyper_op);

    return hyper_op.rv; // Return the value fetched from the emulator
}

static struct file_operations fops = {
    .open = dev_open,
    .read = dev_read,
    .release = dev_release,
    .write = dev_write,
    .unlocked_ioctl = dev_ioctl,
};

static char *rw_devnode(struct device *dev, umode_t *mode) {
    if (mode) {
        *mode = 0666; // read-write permissions for user, group, and others
    }
    return NULL;
}

static int __init hyperdev_init(void) {
    char *str, *token;
    dev_t current_dev;
    int i=0;

    if (devnames == NULL) {
      return -EINVAL;
    }

    pr_emerg("dyndev: Initializing the dyndev module\n");

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

static void __exit hyperdev_exit(void) {
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

module_init(hyperdev_init);
module_exit(hyperdev_exit);
