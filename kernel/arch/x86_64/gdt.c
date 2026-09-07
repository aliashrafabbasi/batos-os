#include "gdt.h"

/*
 * Global Descriptor Table entry.
 *
 * Long mode does not use the base/limit for normal
 * code/data addressing, but the descriptors are still
 * required for the CPU's segment state.
 */
struct gdt_entry
{
    uint16_t limit_low;
    uint16_t base_low;

    uint8_t base_middle;

    uint8_t access;

    uint8_t granularity;

    uint8_t base_high;
} __attribute__((packed));


/*
 * GDTR format used by LGDT.
 */
struct gdt_ptr
{
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));


/*
 * BATOS Global Descriptor Table.
 *
 * Entry 0: Null
 * Entry 1: Kernel Code
 * Entry 2: Kernel Data
 */
static struct gdt_entry gdt[3]
    __attribute__((aligned(16)));

static struct gdt_ptr gdt_descriptor;


/*
 * Set one GDT entry.
 */
static void gdt_set_entry(
    int index,
    uint32_t base,
    uint32_t limit,
    uint8_t access,
    uint8_t granularity
)
{
    gdt[index].limit_low =
        (uint16_t)(limit & 0xFFFF);

    gdt[index].base_low =
        (uint16_t)(base & 0xFFFF);

    gdt[index].base_middle =
        (uint8_t)((base >> 16) & 0xFF);

    gdt[index].access =
        access;

    gdt[index].granularity =
        (uint8_t)(((limit >> 16) & 0x0F) |
                  (granularity & 0xF0));

    gdt[index].base_high =
        (uint8_t)((base >> 24) & 0xFF);
}


/*
 * Load the GDT and reload the segment registers.
 */
static void gdt_load(void)
{
    /*
     * Load GDTR.
     */
    __asm__ volatile (
        "lgdt (%0)"
        :
        : "r"(&gdt_descriptor)
        : "memory"
    );

    /*
     * Reload CS using a far return.
     *
     * We cannot simply mov a value into CS.
     */
    __asm__ volatile (
        "pushq $0x08\n"
        "lea 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        :
        :
        : "rax", "memory"
    );

    /*
     * Reload the data segment registers.
     */
    __asm__ volatile (
        "movw $0x10, %%ax\n"
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%ss\n"
        "xorw %%ax, %%ax\n"
        "movw %%ax, %%fs\n"
        "movw %%ax, %%gs\n"
        :
        :
        : "rax", "memory"
    );
}


/*
 * Initialize the BATOS Global Descriptor Table.
 */
void gdt_init(void)
{
    /*
     * Disable interrupts while changing
     * the processor's descriptor state.
     */
    __asm__ volatile (
        "cli"
        ::: "memory"
    );

    /*
     * Clear the table.
     */
    for (int i = 0; i < 3; i++)
    {
        gdt[i] = (struct gdt_entry){0};
    }

    /*
     * Null descriptor.
     *
     * Required by the x86 architecture.
     */
    gdt_set_entry(
        0,
        0,
        0,
        0,
        0
    );

    /*
     * Kernel Code Segment.
     *
     * Access:
     *   0x9A
     *
     *   Present = 1
     *   DPL     = 0
     *   Code    = 1
     *   Readable= 1
     *
     * Granularity:
     *   0x20
     *
     *   Long mode = 1
     */
    gdt_set_entry(
        1,
        0,
        0,
        0x9A,
        0x20
    );

    /*
     * Kernel Data Segment.
     *
     * Access:
     *   0x92
     *
     *   Present = 1
     *   DPL     = 0
     *   Data     = 1
     *   Writable = 1
     *
     * In 64-bit mode the base and limit are
     * largely ignored for normal data addressing.
     */
    gdt_set_entry(
        2,
        0,
        0,
        0x92,
        0x00
    );

    /*
     * Build GDTR.
     */
    gdt_descriptor.limit =
        sizeof(gdt) - 1;

    gdt_descriptor.base =
        (uint64_t)&gdt[0];

    /*
     * Load the new GDT.
     */
    gdt_load();
}
