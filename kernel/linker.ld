ENTRY (stage1_kernel_entry)

KERNEL_PML4_INDEX = 480;
KERNEL_VIRTUAL_START = KERNEL_PML4_INDEX * 0x8000000000 + 0x1000;

SECTIONS
{
	. = KERNEL_VIRTUAL_START;

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
	}	

	/DISCARD/ :
	{
		*(.interp)
	}
}
