#ifndef CPU_H
#define CPU_H

// x86 specific helpers

#include <stdint.h>

// MSR (model specific registers) helpers
// https://wiki.osdev.org/Model_Specific_Registers
// https://www.sandpile.org/x86/msr.htm

#define MSR_MTRR_CAP 0xFE
#define MSR_MTRR_DEF_TYPE 0x2FF
#define MSR_MTRR_PHYS_BASE 0x200
#define MSR_MTRR_PHYS_MASK 0x201
#define MSR_PAT 0x277

// read msr
static inline uint64_t rdmsr(uint32_t msr) {
   uint32_t lo;
   uint32_t hi;

   asm volatile (
      "rdmsr"
      : "=a"(lo),
      "=d"(hi)
      : "c"(msr)
   );

   return ((uint64_t)hi << 32) | lo;
}

// write msr
static inline void wrmsr(uint32_t msr, uint64_t value) {
   uint32_t lo = (uint32_t)value;
   uint32_t hi = (uint32_t)(value >> 32);

   asm volatile (
      "wrmsr"
      :: "c"(msr),
      "a"(lo),
      "d"(hi)
      : "memory"
   );
}

// general/misc helpers

// invalidate page
static inline void invlpg(uint32_t addr) {
   asm volatile("invlpg (%0)" :: "r"(addr) : "memory");
}

// invalidate cache
static inline void wbinvd() {
   asm volatile("wbinvd" ::: "memory");
}

static inline uint32_t read_cr2() {
   uint32_t addr;
   asm volatile("mov %%cr2, %0" : "=r" (addr));
   return addr;
}

// cpuid - get cpu info
// https://en.wikipedia.org/wiki/CPUID
// https://www.felixcloutier.com/x86/cpuid

static inline void cpuid(uint32_t leaf, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d) {
   asm volatile(
      "cpuid"
      : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
      : "a"(leaf), "c"(0)
   );
}

// writes 12 chars of cpuid id str to out
static inline void get_cpu_str(char *out) {
   uint32_t a, b, c, d;
   cpuid(0, &a, &b, &c, &d);
   out[0] = b&0xFF;
   out[1] = (b>>8)&0xFF;
   out[2] = (b>>16)&0xFF;
   out[3] = (b>>24)&0xFF;
   out[4] = d&0xFF;
   out[5] = (d>>8)&0xFF;
   out[6] = (d>>16)&0xFF;
   out[7] = (d>>24)&0xFF;
   out[8] = c&0xFF;
   out[9] = (c>>8)&0xFF;
   out[10] = (c>>16)&0xFF;
   out[11] = (c>>24)&0xFF;
}

// max extended (past 0x80000000) leaf
static inline uint32_t get_cpuid_ext_max() {
   uint32_t max, unused;
   cpuid(0x80000000, &max, &unused, &unused, &unused);
   return max;
}

#endif
