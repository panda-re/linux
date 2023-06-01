#ifndef HYPERCALL_H
#define HYPERCALL_H

static inline void igloo_hypercall(uint32_t num, uint32_t a1) {
#ifdef CONFIG_MIPS
  asm volatile(
    "movz $0, %[num], %[a1]": : [num] "r" (num), [a1] "r" (a1)
    );
#elif defined(CONFIG_ARM)
    asm __volatile__(
      "mov %%r0, %2 \t\n\
       mov %%r1, %3 \t\n\
       mcr p7, 0, r0, c0, c0, 0"
      : : "r" (num), "r" (a1) /* input registers */
      : "r0", "r1" /* clobbered registers */
    );
#else
#error "No igloo_hypercall support for architecture"
#endif
}

#endif
