#include <linux/kernel.h>
#include <linux/init.h>
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

bool  validate_pgd(pgd_t * pgd)  {
	return !pgd_none(*pgd) && !pgd_bad(*pgd);
}

bool  validate_pud(pud_t * pud) {
	return !pud_none(*pud) && !pud_bad(*pud);
}

bool  validate_pmd(pmd_t * pmd) {
	return !pmd_none(*pmd) && !pmd_bad(*pmd);
}

static inline pte_t *pmd_page_vaddrr(pmd_t pmd)
{
    printk(KERN_EMERG "pmd_val(pmd) & PHYS_MASK & (s32)PAGE_MASK %p\n", pmd_val(pmd) & PHYS_MASK & (s32)PAGE_MASK);
    printk(KERN_EMERG "pmd_val(%x) & %x & (s32)PAGE_MASK %x\n", pmd_val(pmd), PHYS_MASK, PAGE_MASK);
    printk(KERN_EMERG "__va(%p) = %p", pmd_val(pmd) & PHYS_MASK & (s32)PAGE_MASK, __va(pmd_val(pmd) & PHYS_MASK & (s32)PAGE_MASK));
    printk(KERN_EMERG "PHYS_OFFSET %x PAGE_OFFSET %x\n", PHYS_OFFSET, PAGE_OFFSET);
    return __va(pmd_val(pmd) & PHYS_MASK & (s32)PAGE_MASK);
}

static inline unsigned long pte_indexx(unsigned long addr)
{
    printk(KERN_EMERG "addr >> PAGE_SHIFT %x\n", addr >> PAGE_SHIFT);
    printk(KERN_EMERG "PTRS_PER_PTE-1 %x\n", PTRS_PER_PTE-1);
    return (((addr) >> PAGE_SHIFT) & (PTRS_PER_PTE - 1));
}

#ifdef CONFIG_HIGHPTE
#error
#endif

#define __pte_mapp(pmd)		pmd_page_vaddrr(*(pmd))
#define pte_offset_mapp(pmd,addr)	(__pte_mapp(pmd) + pte_indexx(addr))

void* helper_with_conditions(unsigned long arg){
	unsigned long address = arg;
    printk(KERN_EMERG "address: %p\n", address);
    printk(KERN_EMERG"mm->pgd = %p\n", current->mm->pgd);
	pgd_t *pgd = pgd_offset(current->mm, address);
    printk(KERN_EMERG "pgd: %p\n", pgd);
    printk(KERN_EMERG "pgd_index = %p\n", pgd_index(address));
    printk(KERN_EMERG "PGDIR_SHIFT %d\n", PGDIR_SHIFT);
    printk(KERN_EMERG "pgdir size %d\n", sizeof(pgd_t));
    if (validate_pgd(pgd))
    {
        pud_t *pud = pud_offset(pgd, address);
        printk(KERN_EMERG "pud: %p\n", pud);
		if (validate_pud(pud)){
			pmd_t *pmd = pmd_offset(pud, address);
            printk(KERN_EMERG "pmd: %p\n", pmd);
			if (validate_pmd(pmd)){
				pte_t *pte = pte_offset_mapp(pmd, address);
                printk(KERN_EMERG "pte: %p\n", pte);
				if (!pte_none(*pte)){
                    struct page *pg = pte_page(*pte);
                    unsigned long phys = page_to_phys(pg);
                    void *ptr = (void*)(phys + (address & 0xfff));
                    printk(KERN_EMERG "ptr: %p\n", ptr);
                    return (void *) ptr;
                } else {
                    printk(KERN_EMERG "pte is none\n");
                }
			}else {
                printk(KERN_EMERG "pmd is bad\n");
            }
		} else {
            printk(KERN_EMERG "pud is bad\n");
        }
    }
    else
    {
        printk(KERN_EMERG "pgd is bad\n");
    }
}

asmlinkage long sys_hello(const char __user *path)
{
        printk(KERN_EMERG"Hello there\n");
        void* out = helper_with_conditions((unsigned long) path);
        printk(KERN_EMERG"helper_with_conditions out");
        return 0;
}