#include "cpuid.h"

CPUFeatures g_cpu_features;

__attribute__((naked)) void initialize_cpu_features() {
    asm("movl $0x1, %%eax\n"
        "cpuid\n"
        "mov %%ebx, (%[cpu_feat])\n"
        "mov %%ecx, 0x4(%[cpu_feat])\n"
        "mov %%edx, 0x8(%[cpu_feat])\n"
        "ret\n"
        // No output
        :
        // No input
        : [cpu_feat] "r"(&g_cpu_features)
        : "eax", "ebx", "ecx", "edx", "memory");
}
