#include <linux/kprobes.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/errno.h>

int register_all_kprobes(void);
void unregister_all_kprobes(void);
