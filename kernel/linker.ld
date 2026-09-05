OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)

ENTRY(kernel_main)

PHDRS
{
    text   PT_LOAD FLAGS(5);
    rodata PT_LOAD FLAGS(4);
    data   PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 0xffffffff80000000;

    .text : ALIGN(4K)
    {
        KEEP(*(.text.entry))
        *(.text*)
    } :text

    . = ALIGN(4K);

    .limine_requests :
    {
        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end))
    } :rodata

    .rodata : ALIGN(4K)
    {
        *(.rodata*)
    } :rodata

    . = ALIGN(4K);

    .data : ALIGN(4K)
    {
        *(.data*)
    } :data

    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(.bss*)
    } :data

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note*)
    }
}
