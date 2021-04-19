#pragma once
#include <stdint.h>

// Values for these defines can be found here:
// https://en.wikipedia.org/wiki/CPUID#EAX=1:_Processor_Info_and_Feature_Bits

// EBX CPU defines
#define CPU_FEAT_EBX_BRAND_INDEX 0xffUL
#define CPU_FEAT_EBX_CLFLUSH_LINE_SIZE (0xffUL << 8)
#define CPU_FEAT_EBX_LOGICAL_PROCESSOR_COUNT (0xffUL << 16)
#define CPU_FEAT_EBX_LOCAL_APIC_ID (0xffUL << 24)

// ECX CPU defines
#define CPU_FEAT_ECX_SSE3 (1UL << 0)
#define CPU_FEAT_ECX_PCLMUL (1UL << 1)
#define CPU_FEAT_ECX_DTES64 (1UL << 2)
#define CPU_FEAT_ECX_MONITOR (1UL << 3)
#define CPU_FEAT_ECX_DS_CPL (1UL << 4)
#define CPU_FEAT_ECX_VMX (1UL << 5)
#define CPU_FEAT_ECX_SMX (1UL << 6)
#define CPU_FEAT_ECX_EST (1UL << 7)
#define CPU_FEAT_ECX_TM2 (1UL << 8)
#define CPU_FEAT_ECX_SSSE3 (1UL << 9)
#define CPU_FEAT_ECX_CID (1UL << 10)
#define CPU_FEAT_ECX_FMA (1UL << 12)
#define CPU_FEAT_ECX_CX16 (1UL << 13)
#define CPU_FEAT_ECX_ETPRD (1UL << 14)
#define CPU_FEAT_ECX_PDCM (1UL << 15)
#define CPU_FEAT_ECX_PCIDE (1UL << 17)
#define CPU_FEAT_ECX_DCA (1UL << 18)
#define CPU_FEAT_ECX_SSE4_1 (1UL << 19)
#define CPU_FEAT_ECX_SSE4_2 (1UL << 20)
#define CPU_FEAT_ECX_x2APIC (1UL << 21)
#define CPU_FEAT_ECX_MOVBE (1UL << 22)
#define CPU_FEAT_ECX_POPCNT (1UL << 23)
#define CPU_FEAT_ECX_AES (1UL << 25)
#define CPU_FEAT_ECX_XSAVE (1UL << 26)
#define CPU_FEAT_ECX_OSXSAVE (1UL << 27)
#define CPU_FEAT_ECX_AVX (1UL << 28)

// EDX CPU defines
#define CPU_FEAT_EDX_FPU (1UL << 0)
#define CPU_FEAT_EDX_VME (1UL << 1)
#define CPU_FEAT_EDX_DE (1UL << 2)
#define CPU_FEAT_EDX_PSE (1UL << 3)
#define CPU_FEAT_EDX_TSC (1UL << 4)
#define CPU_FEAT_EDX_MSR (1UL << 5)
#define CPU_FEAT_EDX_PAE (1UL << 6)
#define CPU_FEAT_EDX_MCE (1UL << 7)
#define CPU_FEAT_EDX_CX8 (1UL << 8)
#define CPU_FEAT_EDX_APIC (1UL << 9)
#define CPU_FEAT_EDX_SEP (1UL << 11)
#define CPU_FEAT_EDX_MTRR (1UL << 12)
#define CPU_FEAT_EDX_PGE (1UL << 13)
#define CPU_FEAT_EDX_MCA (1UL << 14)
#define CPU_FEAT_EDX_CMOV (1UL << 15)
#define CPU_FEAT_EDX_PAT (1UL << 16)
#define CPU_FEAT_EDX_PSE36 (1UL << 17)
#define CPU_FEAT_EDX_PSN (1UL << 18)
#define CPU_FEAT_EDX_CLF (1UL << 19)
#define CPU_FEAT_EDX_DTES (1UL << 21)
#define CPU_FEAT_EDX_ACPI (1UL << 22)
#define CPU_FEAT_EDX_MMX (1UL << 23)
#define CPU_FEAT_EDX_FXSR (1UL << 24)
#define CPU_FEAT_EDX_SSE (1UL << 25)
#define CPU_FEAT_EDX_SSE2 (1UL << 26)
#define CPU_FEAT_EDX_SS (1UL << 27)
#define CPU_FEAT_EDX_HTT (1UL << 28)
#define CPU_FEAT_EDX_TM1 (1UL << 29)
#define CPU_FEAT_EDX_IA64 (1UL << 30)
#define CPU_FEAT_EDX_PBE (1UL << 31)

// This struct shouldn't be padded to begin with,
// but we add the attribute to make sure
typedef struct {
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
} CPUFeatures;

extern CPUFeatures g_cpu_features;

// Initializes g_cpu_features with values from cpuid eax=1
// We use the naked attribute to avoid the function prologue and epilogue,
// this is not strictly necessary but it saves a couple of instructions.
__attribute__((naked)) void initialize_cpu_features();
