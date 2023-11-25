#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/workqueue.h>

static struct delayed_work my_delayed_work;
static char *path = NULL;

module_param(path, charp, 0000);
MODULE_PARM_DESC(path, "Path to the userspace program to execute");

static void run_my_program(struct work_struct *work)
{
    static char *argv[] = { NULL, NULL }; // Placeholder for the executable path
    static char *envp_init[] = {
        "PATH=/igloo/utils/:/sbin:/bin:/usr/sbin:/usr/bin",
        "HOME=/",
        "TERM=linux",
        "ENV=/igloo/utils/igloo_profile",
        NULL,
    };
    int rv;

    argv[0] = path; // Set the executable path

    //printk(KERN_INFO "Userexec: Attempting to start userspace program: %s\n", path);
    rv = call_usermodehelper(argv[0], argv, envp_init, UMH_WAIT_EXEC);

    if (rv != 0) {
        //printk(KERN_WARNING "Userexec: Userspace program failed to start: error %d, retrying in 1s\n", rv);
        // Correctly reschedule as delayed work
        schedule_delayed_work(&my_delayed_work, msecs_to_jiffies(1000)); 
    }
}

static int __init my_module_init(void)
{
    if (path != NULL) {
        //printk(KERN_WARNING "Userexec: let's gooo\n");
        INIT_DELAYED_WORK(&my_delayed_work, run_my_program);
        // Schedule as delayed work
        schedule_delayed_work(&my_delayed_work, 0); 
    }

    return 0;
}

static void __exit my_module_exit(void)
{
    if (path != NULL) {
        cancel_delayed_work_sync(&my_delayed_work);
    }
}

module_init(my_module_init);
module_exit(my_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Andrew Fasano");
MODULE_DESCRIPTION("Launch userspace program");

