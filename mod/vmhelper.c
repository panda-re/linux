#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <asm/uaccess.h>	/* for get_user and put_user */
#include <linux/proc_fs.h>
#include <linux/mutex.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/mm.h>
#include <asm/page.h>
#include <asm/pgtable.h>

// #define pte_offset(dir, address)					\
	// ((pte_t *) pmd_page_vaddr(*(dir)) + __pte_offset(address))


MODULE_AUTHOR("LUKE CRAIG");
MODULE_VERSION("0.01");
MODULE_LICENSE("SOMETHING");

#define MAJOR_NUM 101
#define DEVICE_NAME "vmhelper"

#define PINLINE

pgd_t *pgd_offset_fn(struct mm_struct *mm, unsigned long address) PINLINE;
pud_t *pud_offset_fn(pgd_t *pgd, unsigned long address) PINLINE;
pmd_t *pmd_offset_fn(pud_t *pud, unsigned long address) PINLINE;
pte_t *pte_offset_fn(pmd_t *pmd, unsigned long address) PINLINE;
unsigned long pte_pfn_fn(pte_t pte) PINLINE;

bool validate_pgd(pgd_t *pgd) PINLINE;
bool validate_pud(pud_t *pud) PINLINE;
bool validate_pmd(pmd_t *pmd) PINLINE;

pgd_t * pgd_offset_fn(struct mm_struct * mm, unsigned long address){
	return pgd_offset(mm, address);
}

pud_t* pud_offset_fn(pgd_t * pgd, unsigned long address){
	return pud_offset(pgd, address);
}

pmd_t*  pmd_offset_fn(pud_t * pud, unsigned long address){
	return pmd_offset(pud, address);
}

bool  validate_pgd(pgd_t * pgd)  {
	return !pgd_none(*pgd) && !pgd_bad(*pgd);
}

bool  validate_pud(pud_t * pud) {
	return !pud_none(*pud) && !pud_bad(*pud);
}

bool  validate_pmd(pmd_t * pmd) {
	return !pmd_none(*pmd) && !pmd_bad(*pmd);
}

pte_t*  pte_offset_map_fn(pmd_t * pmd, unsigned long address) {
	return pte_offset_map(pmd, address);
}

#ifndef pte_offset
#define pte_offset(dir, address)					\
	((pte_t *) pmd_val(*(dir)) + pte_index(address))
#endif

pte_t*  pte_offset_fn(pmd_t * pmd, unsigned long address) {
	return pte_offset(pmd, address);
}

unsigned long  pte_pfn_fn(pte_t pte) {
	return pte_pfn(pte);
}

struct mm_struct *saved_mm;

long helper_without_conditions(int cmd, unsigned long arg){
	unsigned long address = arg;
	pgd_t *pgd = pgd_offset(saved_mm, address);
	pud_t *pud = pud_offset(pgd, address);
	pmd_t *pmd = pmd_offset(pud, address);
	pte_t *pte = pte_offset_map(pmd, address);
	return pte_val(*pte) & 0xfffff000 + address - (address * 0xfff);
}

long helper_with_conditions(int cmd, unsigned long arg){
	unsigned long address = arg;
	pgd_t *pgd = pgd_offset_fn(saved_mm, address);
	if (validate_pgd(pgd)){
		pud_t *pud = pud_offset_fn(pgd, address);
		if (validate_pud(pud)){
			pmd_t *pmd = pmd_offset_fn(pud, address);
			if (validate_pmd(pmd)){
				pte_t *pte = pte_offset_map_fn(pmd, address);
				if (!pte_none(*pte)){
					return pte_val(*pte) & 0xfffff000 + address - (address * 0xfff);
				}
			}
		}
	}
}

long vmhelper_ioctl(struct file* f, unsigned int cmd, unsigned long arg){
	if (helper_with_conditions(cmd, arg) == helper_without_conditions(cmd, arg)){
		return 1;
	}

	return 0;
}

struct file_operations fops = {
	.unlocked_ioctl = vmhelper_ioctl,
};

static int __init hello_world_init(void){
	printk(KERN_INFO "Hello, World!\n");
	saved_mm = current->mm;
	return register_chrdev(MAJOR_NUM, DEVICE_NAME, &fops);
}

static void __exit hello_world_exit(void){
	printk(KERN_INFO "Goodbye, World!\n");
}

module_init(hello_world_init);
module_exit(hello_world_exit);