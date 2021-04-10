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
    		__stack_bottom__ = .;
    		. += 0x4000;
    		__stack_top__ = .;
	}	

	/DISCARD/ :
	{
		*(.interp)
	}
}
