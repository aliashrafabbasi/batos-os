#include "gdt.h"
#include "tss.h"

/*
 * Standard GDT entry.
 */
struct gdt_entry
{
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

/*
 * 64-bit TSS descriptor.
 *
 * A TSS descriptor occupies 16 bytes in the GDT.
 */
struct gdt_tss_entry
{
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
    uint32_t base_upper;
    uint32_t reserved;
} __attribute__((packed));

struct gdt_ptr
{
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

/*
 * GDT:
 *
 * Entry 0 = Null
 * Entry 1 = Kernel Code
 * Entry 2 = Kernel Data
 * Entry 3-4 = 64-bit TSS descriptor
 */
static uint8_t gdt[
    sizeof(struct gdt_entry) * 3 +
    sizeof(struct gdt_tss_entry)
] __attribute__((aligned(16)));

static struct gdt_ptr gdt_descriptor;

static void gdt_set_entry(
    int index,
    uint32_t base,
    uint32_t limit,
    uint8_t access,
    uint8_t granularity
)
{
    struct gdt_entry *entry =
        (struct gdt_entry *)&gdt[index * 8];

    entry->limit_low =
        (uint16_t)(limit & 0xFFFF);

    entry->base_low =
        (uint16_t)(base & 0xFFFF);

    entry->base_middle =
        (uint8_t)((base >> 16) & 0xFF);

    entry->access = access;

    entry->granularity =
        (uint8_t)(((limit >> 16) & 0x0F) |
                  (granularity & 0xF0));

    entry->base_high =
        (uint8_t)((base >> 24) & 0xFF);
}

static void gdt_set_tss(
    uint64_t base,
    uint32_t limit
)
{
    struct gdt_tss_entry *entry =
        (struct gdt_tss_entry *)&gdt[24];

    entry->limit_low =
        (uint16_t)(limit & 0xFFFF);

    entry->base_low =
        (uint16_t)(base & 0xFFFF);

    entry->base_middle =
        (uint8_t)((base >> 16) & 0xFF);

    /*
     * Present + Ring 0 + Available 64-bit TSS.
     *
     * Type = 1001b
     */
    entry->access = 0x89;

    entry->granularity =
        (uint8_t)((limit >> 16) & 0x0F);

    entry->base_high =
        (uint8_t)((base >> 24) & 0xFF);

    entry->base_upper =
        (uint32_t)((base >> 32) & 0xFFFFFFFF);

    entry->reserved = 0;
}

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
     * Reload CS with kernel code selector.
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
     * Reload kernel data segments.
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

    /*
     * Load Task Register with TSS selector.
     */
    __asm__ volatile (
        "movw $0x18, %%ax\n"
        "ltr %%ax\n"
        :
        :
        : "rax", "memory"
    );
}

void gdt_init(void)
{
    __asm__ volatile ("cli" ::: "memory");

    /*
     * Clear complete GDT.
     */
    for (unsigned int i = 0; i < sizeof(gdt); i++)
    {
        gdt[i] = 0;
    }

    /*
     * Null descriptor.
     */
    gdt_set_entry(
        0,
        0,
        0,
        0,
        0
    );

    /*
     * Kernel code.
     */
    gdt_set_entry(
        1,
        0,
        0,
        0x9A,
        0x20
    );

    /*
     * Kernel data.
     */
    gdt_set_entry(
        2,
        0,
        0,
        0x92,
        0x00
    );

    /*
     * Initialize TSS.
     */
    tss_init();

    /*
     * Install TSS descriptor.
     */
    gdt_set_tss(
        tss_get_base(),
        tss_get_limit()
    );

    /*
     * Build GDTR.
     */
    gdt_descriptor.limit =
        sizeof(gdt) - 1;

    gdt_descriptor.base =
        (uint64_t)&gdt[0];

    /*
     * Load GDT and TSS.
     */
    gdt_load();
}