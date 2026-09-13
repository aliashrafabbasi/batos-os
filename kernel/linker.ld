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
     * Exact end of the linked kernel image before page alignment.
     */
    __kernel_end = .;

    /*
     * First page-aligned virtual address after the kernel image.
     */
    . = ALIGN(4K);
    __heap_start = .;

    /*
     * Dedicated virtual address window for kernel task stacks.
     *
     * Each task receives:
     *
     *     4 mapped stack pages
     *     1 unmapped guard page
     *
     * Physical backing is allocated dynamically by PMM and
     * mapped through VMM.
     */
    . = __heap_start + (64K);
    . = ALIGN(4K);
    __task_stack_start = .;

    /*
     * Reserve virtual space only.
     *
     * TASK_MAX_TASKS * TASK_KERNEL_STACK_SLOT_SIZE
     * = 256 * 20 KiB
     * = 5 MiB.
     */
    . = __task_stack_start + (5M);
    . = ALIGN(4K);
    __task_stack_end = .;

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