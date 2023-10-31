#ifndef HYPERCALL_H
#define HYPERCALL_H

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

static inline uint32_t igloo_hypercall2(uint32_t num, uint32_t a1, uint32_t a2) {
#if defined(CONFIG_ARM)
  register uint32_t r0 asm("r0") = num;  // Set up r0 with the value of num
  register uint32_t r1 asm("r1") = a1;   // Argument 2
  register uint32_t r2 asm("r2") = a2;   // Argument 3

  asm volatile(
     "mcr p7, 0, r0, c0, c0, 0"
      : "+r"(r0)  // Input and output
      : "r"(r1), "r"(r2)
      :  // Clobber list is empty because r0 is specified as an input-output operand
  );

  // Read the result from r0
  return r0;
#else
#error "No igloo_hypercall2 support for architecture"
#endif
}

#endif
