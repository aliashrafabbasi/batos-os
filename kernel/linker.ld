ENTRY(kernel_main)

SECTIONS
{
    . = 0xffffffff80000000;

    /*
     * Limine requests must appear early in the kernel image.
     */
    .limine_requests_start : ALIGN(8)
    {
        KEEP(*(.limine_requests_start))
    }

    .limine_requests : ALIGN(8)
    {
        KEEP(*(.limine_requests))
    }

    .limine_requests_end : ALIGN(8)
    {
        KEEP(*(.limine_requests_end))
    }

    /*
     * Executable kernel code.
     */
    .text : ALIGN(4K)
    {
        KEEP(*(.text.boot))
        *(.text .text.*)
    }

    /*
     * Read-only kernel data.
     */
    .rodata : ALIGN(4K)
    {
        *(.rodata .rodata.*)
    }

    /*
     * Writable initialized data.
     */
    .data : ALIGN(4K)
    {
        *(.data .data.*)
    }

    /*
     * Zero-initialized data.
     */
    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(.bss .bss.*)
    }

    /*
     * Kernel does not need these sections.
     */
    /DISCARD/ :
    {
        *(.eh_frame)
        *(.note*)
        *(.comment)
    }
}