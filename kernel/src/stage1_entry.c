extern char __stack_top__;
extern char kernel_entry;

__attribute__((naked)) void stage1_kernel_entry() {
    asm(
        // Setup stack and base pointer
        "mov %[stack], %%rsp\n"
        "mov %[stack], %%rbp\n"

        // Jump to the real kernel entry
        "jmp *%[kernel_entry]\n"
        : // No output
          // Input
        : [stack] "r"(&__stack_top__), [kernel_entry] "r"(&kernel_entry)
        // Clobbers
        : "rsp", "rbp");
}
