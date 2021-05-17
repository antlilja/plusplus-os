#pragma once
#define GDT_KERNEL_CODE_SEGMENT 0x08
#define GDT_KERNEL_DATA_SEGMENT 0x10

#define GDT_USER_DATA_SEGMENT 0x20
#define GDT_USER_CODE_SEGMENT 0x28

#define GDT_TSS_SEGMENT 0x30

void set_tss_kernel_stack(void* stack_ptr);

void setup_gdt_and_tss();
