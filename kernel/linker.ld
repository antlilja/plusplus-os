ENTRY (stage1_kernel_entry)

PDP_MEM_RANGE = 0x8000000000;

KERNEL_PML4_INDEX = 480;
ADDRESS = KERNEL_PML4_INDEX * PDP_MEM_RANGE;
KERNEL_VIRTUAL_START = ADDRESS | ((((ADDRESS >> 47) & 1) * 0xffff) << 48);

STACK_SIZE = 0x4000;

SECTIONS
{
	. = KERNEL_VIRTUAL_START;

	s_kernel_code_start = .;
	.text : ALIGN (4K)
	{
		*(.text*)
	}
	s_kernel_code_end = .;

	s_kernel_rodata_start = .;
	.rodata : ALIGN (4K)
	{
		*(.rodata*)
	}
	s_kernel_rodata_end = .;

	s_kernel_data_start = .;
	.data : ALIGN (4K)
	{
		*(.data*)
	}

	.bss : ALIGN (4K)
	{
		*(COMMON)
		*(.bss*)

		. = ALIGN (16);
    		s_stack_bottom = .;
    		. += STACK_SIZE;
    		s_stack_top = .;
	}	

	. = ALIGN(4K);
	s_kernel_data_end = .;

	/DISCARD/ :
	{
		*(.interp)
	}
}
