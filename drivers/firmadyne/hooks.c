#include <linux/binfmts.h>
#include <linux/capability.h>
#include <linux/fs.h>
#include <linux/kmod.h>
#include <linux/kprobes.h>
#include <linux/mman.h>
#include <linux/if_vlan.h>
#include <linux/inetdevice.h>
#include <linux/net.h>
#include <linux/netlink.h>
#include <linux/reboot.h>
#include <linux/security.h>
#include <linux/socket.h>
#include <net/inet_sock.h>

#include "firmadyne.h"
#include "hooks.h"
#include "hooks-private.h"

/* Network related operations; e.g. bind, accept, etc */
#define LEVEL_NETWORK (1 << 0)
/* System operations; e.g. reboot, mount, ioctl, execve, etc */
#define LEVEL_SYSTEM  (1 << 1)
/* Filesystem write operations; e.g. unlink, mknod, etc */
#define LEVEL_FS_W    (1 << 2)
/* Filesystem read operations; e.g. open, close, etc */
#define LEVEL_FS_R    (1 << 3)
/* Process execution operations; e.g. mmap, fork, etc */
#define LEVEL_EXEC    (1 << 4)
/* IGLOO introspection: filenames and binds */
#define LEVEL_IGLOO   (1 << 5)
/* Igloo BIND only */
#define LEVEL_IGLOO_BIND   (1 << 6)

// Note these hooks might be better implemented as an LSM: e.g., with  security_socket_bind over inet_bind

#define SYSCALL_HOOKS \
	/* Hook network binds */ \
	HOOK("inet_bind", bind_hook, bind_probe) \
	/* Hook accepts */ \
	HOOK("inet_accept", accept_hook, accept_probe) \
	/* Hook VLAN creation */ \
	HOOK("register_vlan_dev", vlan_hook, vlan_probe) \
	/* Hook assignments of IP addresses to network interfaces */ \
	HOOK("__inet_insert_ifa", inet_hook, inet_probe) \
	/* Hook adding of interfaces to bridges */ \
	HOOK("br_add_if", br_hook, br_probe) \
	/* Hook socket calls */ \
	HOOK("sys_socket", socket_hook, socket_probe) \
	/* Hook changes in socket options */ \
	HOOK("sys_setsockopt", setsockopt_hook, setsockopt_probe) \
\
	/* Hook mounting of file systems */ \
	HOOK("do_mount", mount_hook, mount_probe) \
	/* Hook creation of device nodes */ \
	HOOK("vfs_mknod", mknod_hook, mknod_probe) \
	/* Hook deletion of files */ \
	HOOK("vfs_unlink", unlink_hook, unlink_probe) \
  \
	/* Hook IOCTL's on files */ \
	/*HOOK("do_vfs_ioctl", ioctl_hook, ioctl_probe)*/ \
	/*HOOK_RET("do_vfs_ioctl", ioctl_hook2, ioctl_hook_ret, ioctl_probe_ret) */\
	/*HOOK_RET("do_vfs_ioctl", NULL, ioctl_hook_ret, ioctl_probe_ret)*/ \
  \
	/* Hook system reboot */ \
	HOOK("sys_reboot", reboot_hook, reboot_probe) \
\
	/* Hook opening of file descriptors */ \
	HOOK("do_sys_open", open_hook, open_probe) \
/*	HOOK_RET("do_sys_open", NULL, open_ret_hook, open_ret_probe) */ \
	/* Hook closing of file descriptors */ \
	HOOK("sys_close", close_hook, close_probe) \
\
	/* Hook execution of programs */ \
	HOOK("do_execveat_common", execve_hook, execve_probe) \
	/* Hook forking of processes */ \
	HOOK("do_fork", fork_hook, fork_probe) \
	HOOK_RET("do_fork", NULL, fork_ret_hook, fork_ret_probe) \
	/* Hook process exit */ \
	HOOK("do_exit", exit_hook, exit_probe) \
	/* Hook sending of signals */ \
	HOOK("do_send_sig_info", signal_hook, signal_probe) \
\
	/* Hook memory mapping */ \
	HOOK("mmap_region", mmap_hook, mmap_probe) \
	/* NEW */ \
	HOOK("sys_faccessat", access_hook, access_probe) \
	HOOK("vfs_lstat", lstat_hook, lstat_probe) \

// User pointer filename
#define LOG_FILE(sname, pid, current, filename) \
	if (syscall & LEVEL_IGLOO && current != NULL) { \
		char kp[128]; \
		int len = strnlen_user(filename, 128); \
		if (len < 0) { \
			printk("IGLOO: Failed to read file details in %s: %d, %s\n", sname, pid, current->comm); \
		} else { \
			strncpy_from_user(kp, filename, len); \
			if ((strncmp("/etc/TZ", kp, 128) != 0) && (strncmp("/firmadyne/", kp, 11) != 0)) { \
				printk(KERN_INFO "IGLOO: %s [PID: %d (%s)], file: %s\n", sname, pid, current->comm, kp); \
			} \
		} \
  }

// Kernel pointer filename. Name <= NAME_MAX
#define LOG_FILE_K(sname, pid, current, filename) \
	if (syscall & LEVEL_IGLOO && current != NULL) { \
    if ((strncmp("/etc/TZ", filename, 128) != 0) && (strncmp("/firmadyne/", filename, 11) != 0)) \
			printk(KERN_INFO "IGLOO: %s [PID: %d (%s)], file: %s\n", sname, pid, current->comm, filename); \
  }


#define LOG_BIND(sname, pid, current, family, type, port, ip) \
	if (((syscall & LEVEL_IGLOO) || (syscall & LEVEL_IGLOO_BIND)) && current != NULL) \
		printk(KERN_INFO "IGLOO: %s [PID: %d (%s)], bind: %s:%s:%d IP=%pI4\n", sname, pid, current->comm, type, family, port, ip); \
	else if (syscall & LEVEL_IGLOO) \
		printk(KERN_INFO "IGLOO: %s [PID: %d (??)], bind: %s:%s:%d IP=%pI4\n", sname, pid, type, family, port, ip);

#define LOG_BIND6(sname, pid, current, family, type, port, ip) \
	if (((syscall & LEVEL_IGLOO) || (syscall & LEVEL_IGLOO_BIND)) && current != NULL) \
		printk(KERN_INFO "IGLOO: %s [PID: %d (%s)], bind: %s:%s:%d IP=%pI6\n", sname, pid, current->comm, type, family, port, ip); \
	else if (syscall & LEVEL_IGLOO) \
		printk(KERN_INFO "IGLOO: %s [PID: %d (??)], bind: %s:%s:%d IP=%pI6\n", sname, pid, type, family, port, ip); \

#define LOG_EXECVE(sname, pid, current) \
	if (syscall & LEVEL_IGLOO && current != NULL) \
		printk(KERN_INFO "IGLOO: %s [PID: %d (%s)], file: \n", sname, pid, current->comm); // No file

#define LOG_ARG(value) \
	if (syscall & LEVEL_IGLOO) \
		printk(KERN_INFO "IGLOO: execve ARG %s\n", value);

#define LOG_ENV(value) \
	if (syscall & LEVEL_IGLOO) \
		printk(KERN_INFO "IGLOO: execve ENV: %s\n", value);

#define LOG_END(sc) \
	if (syscall & LEVEL_IGLOO) \
		printk(KERN_INFO "IGLOO: %s END\n", sc);

static char *envp_init[] = {  "PATH=/igloo/utils/:/igloo/bin:/sbin:/bin:/usr/sbin:/usr/bin", "HOME=/", "TERM=linux", "ENV=/igloo/utils/igloo_profile", "LD_PRELOAD=/igloo/utils/libnvram.so", NULL, };

static void access_hook(int dfd, const char __user *filename, int mode, int flags) {
	LOG_FILE("access", task_pid_nr(current), current, filename);
	jprobe_return();
}

static void lstat_hook(char* filename, struct kstat *stat) {
	LOG_FILE("lstat", task_pid_nr(current), current, filename);
	jprobe_return();
}


static void socket_hook(int family, int type, int protocol) {
	if (syscall & LEVEL_NETWORK) {
		printk(KERN_INFO MODULE_NAME": sys_socket[PID: %d (%s)]: family:%d, type:%d, protocol:%d\n", task_pid_nr(current), current->comm, family, type, protocol);
	}

	jprobe_return();
}

static void setsockopt_hook(int fd, int level, int optname, char __user *optval, int optlen) {
	if (syscall & LEVEL_NETWORK) {
		printk(KERN_INFO MODULE_NAME": sys_setsockopt[PID: %d (%s)]: fd:%d, level:%d, optname:%d\n", task_pid_nr(current), current->comm, fd, level, optname);
	}

	jprobe_return();
}

static void reboot_hook(int magic1, int magic2, unsigned int cmd, void __user *arg) {
	static char *argv_init[] = { "/sbin/init", NULL };
	kernel_cap_t pE, pP, pI;
	struct cred *new;

	if (reboot || syscall & LEVEL_SYSTEM) {
		printk(KERN_INFO MODULE_NAME": sys_reboot[PID: %d (%s)]: magic1:%x, magic2:%x, cmd:%x\n", task_pid_nr(current), current->comm, magic1, magic2, cmd);
	}

	if (reboot && cmd != LINUX_REBOOT_CMD_CAD_OFF && cmd != LINUX_REBOOT_CMD_CAD_ON) {
		if (security_capget(current, &pE, &pI, &pP)) {
			printk(KERN_WARNING MODULE_NAME": security_capget() failed!\n");
			goto out;
		}

		if (!(new = prepare_creds())) {
			printk(KERN_WARNING MODULE_NAME": prepare_creds() failed!\n");
			goto out;
		}

		cap_lower(pE, CAP_SYS_BOOT);
		cap_lower(pI, CAP_SYS_BOOT);
		cap_lower(pP, CAP_SYS_BOOT);

		if (security_capset(new, current_cred(), &pE, &pI, &pP)) {
			printk(KERN_WARNING MODULE_NAME": security_capset() failed!\n");
			abort_creds(new);
			goto out;
		}

		commit_creds(new);
		printk(KERN_INFO MODULE_NAME": sys_reboot: removed CAP_SYS_BOOT, starting init...\n");

		call_usermodehelper(argv_init[0], argv_init, envp_init, UMH_NO_WAIT);
	}

out:
	jprobe_return();
}

static void mount_hook(char *dev_name, const char __user *dir_name, char* type_page, unsigned long flags, void *data_page) {
	LOG_FILE("mount", task_pid_nr(current), current, dir_name);
	jprobe_return();
}

// Our IOCTL logging is done directly in the syscall handler along with ioctl_faking
#if 0
static void ioctl_hook(struct file *filp, unsigned int cmd, unsigned long arg) {
  // Log IOCTLs: who issued, what path (unless path can't be resolved)
	if (syscall & LEVEL_IGLOO) {
    char buf[256];
		char *path = d_path(flip.f_path, buf, 256);
    if (!IS_ERR(path)) {
      printk(KERN_INFO "IGLOO: ioctl[PID: %d (%s)]: path:%s, cmd:0x%x arg:0x%lx\n", task_pid_nr(current), current->comm, path, cmd, arg);
    }else{
      printk(KERN_INFO "IGLOO: ioctl[PID: %d (%s)]: path:[error], cmd:0x%x arg:0x%lx\n", task_pid_nr(current), current->comm, filp->f_path.dentry, cmd, arg);
    }
	}
	jprobe_return();
}
#endif

#if 0
/* Per hook private data */
typedef struct my_data {
  char path[256];
}

// kretprobe based: hook on enter, store dentry
static int ioctl_hook2(struct kretprobe_instance *ri, struct pt_regs *regs) {
	struct my_data *data;

	if (!current->mm)
		return 1;	// Skip kernel threads

	if (syscall & LEVEL_IGLOO) {
		printk(KERN_INFO "IGLOO: vfs_ioctl[PID: %d (%s)]: file:%pd, cmd:0x%x arg:0x%lx\n", task_pid_nr(current), current->comm, filp->f_path.dentry, cmd, arg);
    data = (struct my_data *)ri->data;
		char *path = d_path(&f.file->f_path, buf, 256);
    strcpy(data->path, path, 256);
	}
	return 0;
}

static int ioctl_hook_ret(struct kretprobe_instance *ri, struct pt_regs *regs) {
	if (syscall & LEVEL_IGLOO) {
		printk(KERN_INFO "IGLOO: ioctl[PID: %d (%s)]: returns %ld\n", task_pid_nr(current), current->comm, regs_return_value(regs));
	}
  return 0;
}
#endif

static void unlink_hook(struct inode *dir, struct dentry *dentry) {
	LOG_FILE_K("unlink", task_pid_nr(current), current, dentry->d_name.name);
	jprobe_return();
}

static void signal_hook(int sig, struct siginfo *info, struct task_struct *p, bool group) {
	if (syscall & LEVEL_EXEC) {
		printk(KERN_INFO MODULE_NAME": do_send_sig_info[PID: %d (%s)]: PID:%d, signal:%u\n", task_pid_nr(current), current->comm, p->pid, sig);
	}

	jprobe_return();
}

static void vlan_hook(struct net_device *dev) {
	if (syscall & LEVEL_NETWORK) {
		printk(KERN_INFO MODULE_NAME": register_vlan_dev[PID: %d (%s)]: dev:%s vlan_id:%d\n", task_pid_nr(current), current->comm, dev->name, vlan_dev_vlan_id(dev));

	}

	jprobe_return();
}

static void bind_hook(struct socket *sock, struct sockaddr *uaddr, int addr_len) {
  // Note we could alternatively hook on return and resolve port=0 to the actual assigned port
	struct sockaddr_in *addr = (struct sockaddr_in *)uaddr;
	unsigned int family = addr->sin_family;
	unsigned int sport = htons(addr->sin_port);

  //printk(KERN_INFO "IGLOO SAW A BIND: [PID: %d (%s)], bind: family=%d, sock_type=%d, port=%d IP=%pI4\n", task_pid_nr(current), current->comm, family, sock->type, sport, &((struct sockaddr_in *)uaddr)->sin_addr);

  // We need FAMILY, TCP/UDPIP:PORT
  if (family == AF_INET) {
	LOG_BIND("bind", task_pid_nr(current), current,
	  "AF_INET",
	  sock->type == SOCK_STREAM ? "SOCK_STREAM" : (sock->type == SOCK_DGRAM ? "SOCK_DGRAM" : "SOCK_OTHER"),
	  sport,
	  &((struct sockaddr_in *)uaddr)->sin_addr);
  } else if (family == AF_INET6) {
	LOG_BIND6("bind", task_pid_nr(current), current,
	  "AF_INET6",
	  sock->type == SOCK_STREAM ? "SOCK_STREAM" : (sock->type == SOCK_DGRAM ? "SOCK_DGRAM" : "SOCK_OTHER"),
	  sport,
	  &((struct sockaddr_in6 *)uaddr)->sin6_addr);
  }

	jprobe_return();
}

static void accept_hook(struct socket *sock, struct socket *newsock, int flags) {
	if (syscall & LEVEL_NETWORK) {
		printk(KERN_INFO MODULE_NAME": inet_accept[PID: %d (%s)]:\n", task_pid_nr(current), current->comm);
	}
	jprobe_return();
}

static void mmap_hook(struct file *file, unsigned long addr, unsigned long len, vm_flags_t vm_flags, unsigned long pgoff) {
	if (syscall & LEVEL_EXEC && (vm_flags & VM_EXEC)) {
		if (file && file->f_path.dentry) {
			printk(KERN_INFO MODULE_NAME": mmap_region[PID: %d (%s)]: addr:0x%lx -> 0x%lx, file:%s\n", task_pid_nr(current), current->comm, addr, addr+len, file->f_path.dentry->d_name.name);
		}
		else {
			printk(KERN_INFO MODULE_NAME": mmap_region[PID: %d (%s)]: addr:0x%lx -> 0x%lx\n", task_pid_nr(current), current->comm, addr, addr+len);
		}
	}

	jprobe_return();
}

static void exit_hook(long code) {
	if (syscall & LEVEL_EXEC && strcmp("khelper", current->comm)) {
		printk(KERN_INFO MODULE_NAME": do_exit[PID: %d (%s)]: code:%lu\n", task_pid_nr(current), current->comm, code);
	}

	jprobe_return();
}

static void fork_hook(unsigned long clone_flags, unsigned long stack_start, unsigned long stack_size, int __user *parent_tidptr, int __user *child_tidptr) {
	if (syscall & LEVEL_EXEC && strcmp("khelper", current->comm)) {
		printk(KERN_INFO MODULE_NAME": do_fork[PID: %d (%s)]: clone_flags:0x%lx, stack_size:0x%lx\n", task_pid_nr(current), current->comm, clone_flags, stack_size);
	}

	jprobe_return();
}

static int fork_ret_hook(struct kretprobe_instance *ri, struct pt_regs *regs) {
	if (syscall & LEVEL_EXEC && strcmp("khelper", current->comm)) {
		printk(KERN_INFO MODULE_NAME": do_fork_ret[PID: %d (%s)] = %ld\n", task_pid_nr(current), current->comm, regs_return_value(regs));
	}

	return 0;
}

static void close_hook(unsigned int fd) {
	if (syscall & LEVEL_FS_R) {
		printk(KERN_INFO MODULE_NAME": close[PID: %d (%s)]: fd:%d\n", task_pid_nr(current), current->comm, fd);
	}

	jprobe_return();
}

/*
static int open_ret_hook(struct kretprobe_instance *ri, struct pt_regs *regs) {
	if (syscall & LEVEL_FS_R) {
		printk(KERN_CONT ": close[PID: %d (%s)] = %ld\n", task_pid_nr(current), current->comm, regs_return_value(regs));
	}

	return 0;
}
*/

static void open_hook(int dfd, const char __user *filename, int flags, umode_t mode) {
  static struct filename *open_hook_fname;
	if (syscall & LEVEL_IGLOO) {
		// Normally we just check in the macro, but the call to getname allocates
		// kernel memory, so let's avoid if we're not logging
		open_hook_fname = getname(filename);
		if (!IS_ERR(open_hook_fname)) {
			LOG_FILE_K("open", task_pid_nr(current), current, open_hook_fname->name);
		}
		putname(open_hook_fname);
	}
	jprobe_return();
}

static void execve_hook(int fd, const char *filename, struct user_arg_ptr argv,
	struct user_arg_ptr envp, int flags) {

	int i;
	static char *argv_init[] = { "/igloo/utils/console", NULL };
	int rv;

	if (execute > 5) {
		execute = 0;
		printk(KERN_INFO MODULE_NAME": do_execve: %s\n", argv_init[0]);
		rv = call_usermodehelper(argv_init[0], argv_init, envp_init, UMH_WAIT_EXEC);
		if (rv != 0) {
			printk("Firmadyne console failed to start: error %d, will retry again soon\n", rv);
			execute = 1;
		}

		//printk(KERN_WARNING "OFFSETS: offset of pid: 0x%x offset of comm: 0x%x\n", offsetof(struct task_struct, pid), offsetof(struct task_struct, comm));
	}
	else if (execute > 0) {
		execute += 1;
	}


	if (syscall & LEVEL_IGLOO && current != NULL && strcmp("khelper", current->comm) != 0) {
		const char __user *p;
		char kp[256];
		int arg_count;
		int len;
		LOG_EXECVE("execve", task_pid_nr(current), current);
		arg_count = count(argv, MAX_ARG_STRINGS);
		for (i = 0; i >= 0 && i < arg_count; i++) {
			p = get_user_arg_ptr(argv, i);
			if (!p)
				break;
			if (IS_ERR(p))
				break;

			len = strnlen_user(p, 256); //MAX_ARG_STRLEN);
			if (!len)
				break;

			strncpy_from_user(kp, p, len);
			LOG_ARG(kp);
		}

		arg_count = count(envp, MAX_ARG_STRINGS);
		for (i = 0; i >= 0 && i < arg_count; i++) {
			p = get_user_arg_ptr(envp, i);
			if (IS_ERR(p))
				break;

			len = strnlen_user(p, 256); //MAX_ARG_STRLEN);
			if (!len)
				break;

			strncpy_from_user(kp, p, len);
			LOG_ENV(kp);
		}
		LOG_END("execve");
	}

	jprobe_return();
}

static void mknod_hook(struct inode *dir, struct dentry *dentry, umode_t mode, dev_t dev) {
	LOG_FILE_K("mknod", task_pid_nr(current), current, dentry->d_name.name);
	jprobe_return();
}

static void br_hook(struct net_bridge *br, struct net_device *dev) {
	if (syscall & LEVEL_NETWORK) {
		printk(KERN_INFO MODULE_NAME": br_add_if[PID: %d (%s)]: br:%s dev:%s\n", task_pid_nr(current), current->comm, br->dev->name, dev->name);
	}

	jprobe_return();
}

static void inet_hook(struct in_ifaddr *ifa, struct nlmsghdr *nlh, u32 portid) {
	if (syscall & LEVEL_NETWORK) {
		printk(KERN_INFO MODULE_NAME": __inet_insert_ifa[PID: %d (%s)]: device:%s ifa:0x%08x\n", task_pid_nr(current), current->comm, ifa->ifa_dev->dev->name, ifa->ifa_address);
	}

	jprobe_return();
}

#define HOOK_RET(a, b, c, d) \
static struct kretprobe d = { \
	.entry_handler = b, \
	.handler = c, \
	.kp = { \
		.symbol_name = a, \
	}, \
	.maxactive = 2*NR_CPUS, \
};

#define HOOK(a, b, c) \
static struct jprobe c = { \
	.entry = b, \
	.kp = { \
		.symbol_name = a, \
	}, \
};

	SYSCALL_HOOKS
#undef HOOK
#undef HOOK_RET

int register_probes(void) {
	int ret = 0, tmp;

#define HOOK_RET(a, b, c, d) \
	if ((tmp = register_kretprobe(&d)) < 0) { \
		printk(KERN_WARNING MODULE_NAME": register kretprobe: %s = %d\n", d.kp.symbol_name, tmp); \
		ret = tmp; \
	}

#define HOOK(a, b, c) \
	if ((tmp = register_jprobe(&c)) < 0) { \
		printk(KERN_WARNING MODULE_NAME": register jprobe: %s = %d\n", c.kp.symbol_name, tmp); \
		ret = tmp; \
	}

	SYSCALL_HOOKS
#undef HOOK
#undef HOOK_RET

	return ret;
}

void unregister_probes(void) {
#define HOOK_RET(a, b, c, d) \
	unregister_kretprobe(&d);

#define HOOK(a, b, c) \
	unregister_jprobe(&c);

	SYSCALL_HOOKS
#undef HOOK
#undef HOOK_RET
}
