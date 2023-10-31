#ifndef HYPERCALL_H
#define HYPERCALL_H
#include "linux/types.h"

static inline void igloo_hypercall(uint32_t num, uint32_t a1) {
#ifdef CONFIG_MIPS
  asm volatile(
    "movz $0, %[num], %[a1]": : [num] "r" (num), [a1] "r" (a1)
    );
#elif defined(CONFIG_ARM)
  register uint32_t r0 asm("r0") = num;
  register uint32_t r1 asm("r1") = a1;
  asm volatile(
     "mov r0, %0 \t\n\
      mov r1, %1 \t\n\
      mcr p7, 0, r0, c0, c0, 0"
      :
      : "r"(r0), "r"(r1)
      :
  );
#else
#error "No igloo_hypercall support for architecture"
#endif
}

static inline unsigned long igloo_hypercall2(unsigned long num, unsigned long a1, unsigned long a2) {
#if defined(CONFIG_ARM)
    register unsigned long r0 asm("r0") = num;
    register unsigned long r1 asm("r1") = a1;
    register unsigned long r2 asm("r2") = a2;

    asm volatile(
       "mcr p7, 0, r0, c0, c0, 0"
        : "+r"(r0)  // Input and output
        : "r"(r1), "r"(r2)
        :
    );

    return r0;

#elif defined(CONFIG_MIPS)
    // MIPS specific code
    register unsigned long a0 asm("a0") = num;
    register unsigned long a1 asm("a1") = a1;
    register unsigned long a2 asm("a2") = a2;

    asm volatile(
       "movz $0, %[a0], %[a1]"
        : "+r"(a0)  // Input and output
        : [a1] "r" (a1), [a2] "r" (a2)
        :
    );

    return a0;

#else
    #error "No igloo_hypercall2 support for architecture"
#endif
}

#endif
