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

static char *device_names_str = "";
module_param(device_names_str, charp, 0000);
MODULE_PARM_DESC(device_names_str, "A comma-separated list of device names");

static char **device_name;
static int *device_major;
static int num_devices = 0;

// Create an enum for the hypercall types
static int hypercall_base = 0x7000;
enum hypercall_type {
    HYPERCALL_READ = 10,
    HYPERCALL_WRITE = 20,
    HYPERCALL_IOCTL = 30,

};

static int dev_open(struct inode *inodep, struct file *filep) {
    return 0;
}

static int dev_release(struct inode *inodep, struct file *filep) {
    return 0;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset) {
    // First allocate kernel memory of the specified size
    int bytes_read;
    char *data = kmalloc(len, GFP_KERNEL);
    if (!data) {
        return -ENOMEM;
    }
    // Write the path of our device into the data buffer
    // get device_name from filep->f_dentry->d_name.name
    strncpy(data, filep->f_path.dentry->d_iname, len);


    // Now ask the emulator to model the read and write into data
    bytes_read = 0; // XXX need to get from HC
    igloo_hypercall(HYPERCALL_READ+0, data); // Start of read. Buffer at input = device name, at output = data to read

    if (bytes_read > 0) {
        if (copy_to_user(buffer, data, bytes_read)) {
            kfree(data);
            return -EFAULT;
        }
    }
    kfree(data);
    return bytes_read;
}


static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
    // First allocate enough memory for the device name, d_name.name. Not len, but the length of the device name.

    igloo_hypercall(HYPERCALL_WRITE+0, &filep->f_path.dentry->d_iname); // Start of write. Device name is in buffer.
    igloo_hypercall(HYPERCALL_WRITE+1, len); // Tell mu len
    igloo_hypercall(HYPERCALL_WRITE+2, offset); // Tell emu offset
    igloo_hypercall(HYPERCALL_WRITE+3, buffer); // Now tell the emulator to do the write
    return 0; // XXX need to get retval from HC
}

static long dev_ioctl(struct file *filep, unsigned int cmd, unsigned long arg) {
    igloo_hypercall(HYPERCALL_IOCTL+0, &filep->f_path.dentry->d_iname); // Start of ioctl. Device name is in buffer.
    igloo_hypercall(HYPERCALL_IOCTL+1, cmd); // ioctl cmd
    igloo_hypercall(HYPERCALL_IOCTL+2, arg);  // ioctl argument. Gets return value
    return 0; // XXX need to get retval from HC
}


static struct file_operations fops = {
    .open = dev_open,
    .read = dev_read,
    .release = dev_release,
    .write = dev_write,
    .unlocked_ioctl = dev_ioctl,
};

static int __init hyperdev_init(void) {
    char *str, *token;
    dev_t current_dev;
    int i=0;

    if (device_names_str == NULL) {
      return -EINVAL;
    }

    pr_emerg("dyndev: Initializing the dyndev module\n");

    // First, count the number of devices to allocate memory
    for (str = device_names_str; *str; str++) {
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


    // Allocate memory with error checking
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
    str = kstrdup(device_names_str, GFP_KERNEL);
    if (!str) {
        pr_err("Failed to duplicate device_names_str\n");
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
            printk(KERN_ALERT "Yay, registered device %s: %d\n", device_name[i], device_major[i]);
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

