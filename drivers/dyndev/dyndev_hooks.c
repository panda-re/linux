#include "dyndev_hooks.h"

// Macro to extract syscall arguments based on architecture
#if defined(CONFIG_X86_64)
#define SYSCALL_ARG1(ctx) ((ctx)->di)
#define SYSCALL_ARG2(ctx) ((ctx)->si)
#elif defined(CONFIG_ARM64) || defined(CONFIG_ARM)
// For both ARM64 and ARM (32-bit), pt_regs_reg can be used, but the register numbers differ
#define SYSCALL_ARG1(ctx) (pt_regs_reg(ctx, 0)) // R0 for ARM, same as ARM64
#define SYSCALL_ARG2(ctx) (pt_regs_reg(ctx, 1)) // R1 for ARM, same as ARM64
#elif defined(CONFIG_MIPS)
// MIPS uses A0-A3 for the first four syscall arguments
#define SYSCALL_ARG1(ctx) ((ctx)->regs[4]) // A0 register for the first argument
#define SYSCALL_ARG2(ctx) ((ctx)->regs[5]) // A1 register for the second argument
#else
#error "Unsupported architecture"
#endif

struct syscall_probe_data {
    struct kprobe p;
    const char *name; // Syscall name
    int arg_pos;      // Position of the filename argument
};

struct syscall_targets {
    const char *name;
    int arg_pos;
};

// Example syscalls data array
static struct syscall_targets syscalls_data[] = {
    {"sys_open", 1},
    {"sys_openat", 2},
    {"sys_creat", 1},
    {"sys_stat", 1},
    {"sys_lstat", 1},
    {"sys_fstat", 1},
    {"sys_newfstatat", 2},
    {"sys_access", 1},
    {"sys_faccessat", 2},
};

// Define a per-CPU variable to store the filename
DEFINE_PER_CPU(char *, per_cpu_filename);

static struct syscall_probe_data kp_array[sizeof(syscalls_data) / sizeof(syscalls_data[0])];

// Adjust the SYSCALL_ARG macro to dynamically select the argument
#define SYSCALL_ARG(ctx, pos) (pos == 1 ? SYSCALL_ARG1(ctx) : SYSCALL_ARG2(ctx))

int entry_handler(struct kprobe *p, struct pt_regs *regs) {
    struct syscall_probe_data *data = container_of(p, struct syscall_probe_data, p);
    int arg_pos = data->arg_pos;

    char **per_cpu_ptr = get_cpu_ptr(&per_cpu_filename);
    char *filename = kmalloc(PATH_MAX, GFP_KERNEL);

    printk(KERN_INFO "Entering %s\n", data->name);

    if (!filename) {
        put_cpu_ptr(&per_cpu_filename);
        return 0; // Allocation failed, safely return
    }

	// assert arg_pos is 1 or 2
    if (copy_from_user(filename, (void __user *)SYSCALL_ARG(regs, arg_pos), PATH_MAX) == 0) {
        *per_cpu_ptr = filename; // Save filename pointer in per-CPU variable
        // XXX is there a race condition here?
        printk(KERN_INFO "Entering %s with filename %s\n", data->name, filename);
    }

    put_cpu_ptr(&per_cpu_filename); // Always release the per-CPU pointer after use
    return 0;
}

// syscal finishes - get the return value and log if it's -ENOENT
void exit_handler(struct kprobe *p, struct pt_regs *regs, unsigned long flags) {
    long retval = regs_return_value(regs);
    if (retval == -ENOENT) { // Check for the specific error code
        struct syscall_probe_data *data = container_of(p, struct syscall_probe_data, p);
        char **filename = get_cpu_ptr(&per_cpu_filename);
        printk(KERN_INFO "Return from %s with filename %s\n", data->name, *filename);
        if (*filename) {
            printk(KERN_ERR "Missing file: %s in sc %s\n", *filename, data->name);
            kfree(*filename);
            *filename = NULL;
        }
        put_cpu_ptr(&per_cpu_filename);
    }
}

int register_all_kprobes(void) {
    int i, ret;
    for (i = 0; i < ARRAY_SIZE(syscalls_data); i++) {
        kp_array[i].p.symbol_name = syscalls_data[i].name;
        kp_array[i].p.pre_handler = entry_handler;
        kp_array[i].p.post_handler = exit_handler;
        kp_array[i].arg_pos = syscalls_data[i].arg_pos;

        kp_array[i].name = syscalls_data[i].name;
        kp_array[i].arg_pos = syscalls_data[i].arg_pos;

        ret = register_kprobe(&kp_array[i].p);

        if (ret < 0) {
            printk(KERN_INFO "Failed to register kprobe for %s\n", syscalls_data[i].name);
            // Unregister any probes that were successfully registered before the error
            while (--i >= 0) {
                unregister_kprobe(&kp_array[i].p);
            }
            return ret;
        }
        printk(KERN_INFO "Registered kprobe for %s\n", syscalls_data[i].name);
    }
    return 0;
}

void unregister_all_kprobes(void) {
    int i;
    for (i = 0; i < ARRAY_SIZE(syscalls_data); i++) {
        unregister_kprobe(&kp_array[i].p);
    }
}