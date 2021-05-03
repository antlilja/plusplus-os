ENTRY (stage1_kernel_entry)

SECTIONS
{
	. = 1M;

	.text : ALIGN (4K)
	{
		*(.text*)
	}

	.rodata : ALIGN (4K)
	{
		*(.rodata*)
	}

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

	/DISCARD/ :
	{
		*(.interp)
	}
}
