#include "cpuid.h"

CPUFeatures g_cpu_features;

void initialize_cpu_features() {
    asm("movl $0x1, %%eax\n"
        "cpuid\n"
        "mov %%ebx, %[ebx]\n"
        "mov %%ecx, %[ecx]\n"
        "mov %%edx, %[edx]\n"
        // Output
        : [ebx] "=r"(g_cpu_features.ebx),
          [ecx] "=r"(g_cpu_features.ecx),
          [edx] "=r"(g_cpu_features.edx)
        // No input
        :
        // Clobbers
        : "eax", "ebx", "ecx", "edx");
}
