#include <stdint.h>
#include <stddef.h>

#include "limine.h"
#include "kernel/arch/x86_64/gdt.h"
#include "kernel/arch/x86_64/tss.h"
#include "kernel/arch/x86_64/pmm.h"
#include "kernel/arch/x86_64/vmm.h"
#include "kernel/arch/x86_64/idt.h"
#include "kernel/arch/x86_64/pic.h"
#include "kernel/arch/x86_64/irq.h"
#include "kernel/arch/x86_64/lapic.h"
#include "kernel/arch/x86_64/ioapic.h"
#include "kernel/arch/x86_64/gsi.h"
#include "kernel/arch/x86_64/pit.h"
#include "kernel/arch/x86_64/time.h"
#include "kernel/arch/x86_64/clock_event.h"
#include "kernel/arch/x86_64/timer.h"
#include "kernel/arch/x86_64/timer_manager.h"
#include "kernel/arch/x86_64/acpi.h"
#include "kernel/arch/x86_64/heap.h"
#include "kernel/console/console.h"
#include "kernel/tests/heap_tests.h"

/* ============================================================
   LIMINE FRAMEBUFFER REQUEST
   ============================================================ */

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request
    limine_framebuffer_request = {
        .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
        .revision = 0,
        .response = NULL
    };

static struct limine_framebuffer *kernel_framebuffer = NULL;

/* ============================================================
   LIMINE EXECUTABLE ADDRESS REQUEST
   ============================================================ */

__attribute__((used, section(".limine_requests")))
static volatile struct limine_executable_address_request
    limine_executable_address_request = {
        .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID,
        .revision = 0,
        .response = NULL
    };

static uint64_t read_cr2(void)
{
    uint64_t value;

    __asm__ volatile (
        "mov %%cr2, %0"
        : "=r"(value)
    );

    return value;
}

/* ============================================================
   EXCEPTION HANDLER
   ============================================================ */

__attribute__((noreturn))
void exception_handler(struct exception_frame *frame)
{
    if (frame->vector == 14)
    {
        serial_write_string(
            "\n================================\n"
        );

        serial_write_string(
            "BATOS KERNEL EXCEPTION\n"
        );

        serial_write_string(
            "================================\n"
        );

        serial_write_string(
            "VECTOR: "
        );

        serial_write_hex(frame->vector);

        serial_write_string("\n");

        serial_write_string(
            "ERROR CODE: "
        );

        serial_write_hex(frame->error_code);

        serial_write_string("\n");

        serial_write_string(
            "RIP: "
        );

        serial_write_hex(frame->rip);

        serial_write_string("\n");

        serial_write_string(
            "CS: "
        );

        serial_write_hex(frame->cs);

        serial_write_string("\n");

        serial_write_string(
            "RFLAGS: "
        );

        serial_write_hex(frame->rflags);

        serial_write_string("\n");

        uint64_t cr2 = read_cr2();

        serial_write_string(
            "EXCEPTION: PAGE FAULT (#PF)\n"
        );

        serial_write_string(
            "PAGE FAULT ADDRESS: "
        );

        serial_write_hex(cr2);

        serial_write_string("\n");

        serial_write_string(
            "FIRST PAGE FAULT HANDLER: ACTIVE\n"
        );

        serial_write_string(
            "TRIGGERING NESTED PAGE FAULT...\n"
        );

        volatile uint64_t *nested_fault =
            (volatile uint64_t *)0x0000000000000000ULL;

        volatile uint64_t nested_value =
            *nested_fault;

        (void)nested_value;

        serial_write_string(
            "ERROR: NESTED PAGE FAULT DID NOT OCCUR\n"
        );
    }
    else if (frame->vector == 8)
    {
        serial_write_string(
            "\n================================\n"
        );

        serial_write_string(
            "BATOS DOUBLE FAULT\n"
        );

        serial_write_string(
            "================================\n"
        );

        serial_write_string(
            "VECTOR: "
        );

        serial_write_hex(frame->vector);

        serial_write_string("\n");

        serial_write_string(
            "ERROR CODE: "
        );

        serial_write_hex(frame->error_code);

        serial_write_string("\n");

        serial_write_string(
            "RIP: "
        );

        serial_write_hex(frame->rip);

        serial_write_string("\n");

        serial_write_string(
            "CS: "
        );

        serial_write_hex(frame->cs);

        serial_write_string("\n");

        serial_write_string(
            "RFLAGS: "
        );

        serial_write_hex(frame->rflags);

        serial_write_string("\n");

        serial_write_string(
            "EXCEPTION: DOUBLE FAULT (#DF)\n"
        );

        serial_write_string(
            "IST1 HANDLER: ACTIVE\n"
        );

        serial_write_string(
            "TSS IST1: "
        );

        serial_write_hex(
            tss_get_ist1()
        );

        serial_write_string("\n");

        serial_write_string(
            "DOUBLE FAULT HANDLING: OK\n"
        );
    }
    else
    {
        serial_write_string(
            "\nUNKNOWN CPU EXCEPTION\n"
        );

        serial_write_string(
            "VECTOR: "
        );

        serial_write_hex(frame->vector);

        serial_write_string("\n");
    }

    serial_write_string(
        "CPU HALTED\n"
    );

    for (;;)
    {
        __asm__ volatile (
            "cli\n"
            "hlt"
        );
    }
}

/* ============================================================
   KERNEL MAIN
   ============================================================ */

void kernel_main(void)
{
    serial_init();

    serial_write_string(
        "BATOS KERNEL STARTING...\n"
    );

    /* --------------------------------------------------------
       LIMINE EXECUTABLE ADDRESS VERIFICATION
       -------------------------------------------------------- */

    serial_write_string(
        "\nEXECUTABLE ADDRESS VERIFICATION\n"
    );

    if (limine_executable_address_request.response)
    {
        uint64_t executable_physical =
            limine_executable_address_request.response->physical_base;

        uint64_t executable_virtual =
            limine_executable_address_request.response->virtual_base;

        serial_write_string(
            "EXECUTABLE PHYSICAL BASE: "
        );

        serial_write_hex(
            executable_physical
        );

        serial_write_string("\n");

        serial_write_string(
            "EXECUTABLE VIRTUAL BASE: "
        );

        serial_write_hex(
            executable_virtual
        );

        serial_write_string("\n");

        serial_write_string(
            "EXECUTABLE ADDRESS: OK\n"
        );
    }
    else
    {
        serial_write_string(
            "EXECUTABLE ADDRESS: FAILED\n"
        );
    }

    /* --------------------------------------------------------
       FRAMEBUFFER
       -------------------------------------------------------- */

    if (limine_framebuffer_request.response &&
        limine_framebuffer_request.response->framebuffer_count > 0)
    {
        kernel_framebuffer =
            limine_framebuffer_request.response->framebuffers[0];

        console_set_framebuffer(kernel_framebuffer);

        framebuffer_clear(0x00000000);

        draw_text(
            40,
            40,
            "BATOS OS",
            0x00FFFFFF,
            5
        );

        draw_text(
            40,
            110,
            "KERNEL RUNNING",
            0x00FFFFFF,
            3
        );

        framebuffer_console_init();

        serial_write_string(
            "FRAMEBUFFER OK\n"
        );
    }
    else
    {
        serial_write_string(
            "FRAMEBUFFER ERROR\n"
        );
    }

    /* --------------------------------------------------------
       GDT
       -------------------------------------------------------- */

    gdt_init();

    serial_write_string(
        "GDT READY\n"
    );

    serial_write_string(
        "TSS LOADED\n"
    );

    /* --------------------------------------------------------
       IDT
       -------------------------------------------------------- */

    idt_init();

    serial_write_string(
        "IDT READY\n"
    );

    /* --------------------------------------------------------
       TSS / IST VERIFICATION
       -------------------------------------------------------- */

    serial_write_string(
        "\nTSS/IST VERIFICATION\n"
    );

    serial_write_string(
        "TR: "
    );

    serial_write_hex(
        tss_get_selector()
    );

    serial_write_string("\n");

    serial_write_string(
        "IDT[8] IST: "
    );

    serial_write_hex(
        idt_get_ist(8)
    );

    serial_write_string("\n");

    serial_write_string(
        "IDT[8] SELECTOR: "
    );

    serial_write_hex(
        idt_get_selector(8)
    );

    serial_write_string("\n");

    serial_write_string(
        "IST1 STACK: "
    );

    serial_write_hex(
        tss_get_ist1()
    );

    serial_write_string("\n");

    serial_write_string(
        "TSS/IST READY\n"
    );

    /* --------------------------------------------------------
       PHYSICAL MEMORY MANAGER
       -------------------------------------------------------- */

    serial_write_string(
        "\n================================\n"
    );

    serial_write_string(
        "BATOS PMM INITIALIZING...\n"
    );

    serial_write_string(
        "================================\n"
    );

    pmm_init();

    serial_write_string(
        "PMM MEMORY MAP: OK\n"
    );

    serial_write_string(
        "HHDM OFFSET: "
    );

    serial_write_hex(
        pmm_get_hhdm_offset()
    );

    serial_write_string("\n");

    serial_write_string(
        "TOTAL FRAMES: "
    );

    serial_write_hex(
        pmm_get_total_frames()
    );

    serial_write_string("\n");

    serial_write_string(
        "FREE FRAMES: "
    );

    serial_write_hex(
        pmm_get_free_frames()
    );

    serial_write_string("\n");

    serial_write_string(
        "USED FRAMES: "
    );

    serial_write_hex(
        pmm_get_used_frames()
    );

    serial_write_string("\n");

    serial_write_string(
        "PMM BITMAP PHYSICAL: "
    );

    serial_write_hex(
        pmm_get_bitmap_physical()
    );

    serial_write_string("\n");

    serial_write_string(
        "PMM BITMAP SIZE: "
    );

    serial_write_hex(
        pmm_get_bitmap_size()
    );

    serial_write_string("\n");

    serial_write_string(
        "PMM READY\n"
    );

    /* --------------------------------------------------------
       ACPI RSDP DISCOVERY
       -------------------------------------------------------- */

    serial_write_string(
        "\nACPI DISCOVERY START\n"
    );

    int acpi_result = acpi_init();

    if (acpi_result == 0)
    {
        serial_write_string(
            "RSDP: FOUND\n"
        );

        serial_write_string(
            "RSDP SIGNATURE: OK\n"
        );

        serial_write_string(
            "RSDP REVISION: "
        );

        serial_write_hex(
            (uint64_t)acpi_get_rsdp_revision()
        );

        serial_write_string("\n");

        serial_write_string(
            "RSDP ADDRESS: "
        );

        serial_write_hex(
            acpi_get_rsdp_address()
        );

        serial_write_string("\n");

        serial_write_string(
            "RSDP BASE CHECKSUM: OK\n"
        );

        if (acpi_get_rsdp_revision() >= 2)
        {
            serial_write_string(
                "RSDP EXTENDED CHECKSUM: OK\n"
            );
        }

        serial_write_string(
            "ACPI RSDP: VERIFIED\n"
        );

        serial_write_string(
            "\nACPI ROOT TABLE DISCOVERY\n"
        );

        if (acpi_root_table_is_xsdt())
        {
            serial_write_string(
                "ACPI ROOT TYPE: XSDT\n"
            );
        }
        else
        {
            serial_write_string(
                "ACPI ROOT TYPE: RSDT\n"
            );
        }

        serial_write_string(
            "ACPI ROOT ADDRESS: "
        );

        serial_write_hex(
            acpi_get_root_table_address()
        );

        serial_write_string(
            "\n"
        );

        serial_write_string(
            "ACPI ROOT TABLE COUNT: "
        );

        serial_write_hex(
            (uint64_t)acpi_get_root_table_count()
        );

        serial_write_string(
            "\n"
        );

        if (acpi_get_madt_address() != 0)
        {
            serial_write_string(
                "ACPI MADT: FOUND\n"
            );

            serial_write_string(
                "ACPI MADT ADDRESS: "
            );

            serial_write_hex(
                acpi_get_madt_address()
            );

            serial_write_string(
                "\n"
            );

            serial_write_string(
                "MADT LOCAL APIC ADDRESS: "
            );

            serial_write_hex(
                acpi_get_madt_local_apic_address()
            );

            serial_write_string(
                "\n"
            );

            serial_write_string(
                "MADT FLAGS: "
            );

            serial_write_hex(
                acpi_get_madt_flags()
            );

            serial_write_string(
                "\n"
            );

            uint32_t local_apic_count =
                acpi_get_madt_local_apic_count();

            serial_write_string(
                "MADT LOCAL APIC COUNT: "
            );

            serial_write_hex(
                (uint64_t)local_apic_count
            );

            serial_write_string(
                "\n"
            );

            for (uint32_t i = 0;
                 i < local_apic_count;
                 i++)
            {
                const struct acpi_madt_local_apic *local_apic =
                    acpi_get_madt_local_apic(i);

                serial_write_string(
                    "MADT LAPIC UID: "
                );

                serial_write_hex(
                    (uint64_t)local_apic->processor_uid
                );

                serial_write_string(
                    " APIC ID: "
                );

                serial_write_hex(
                    (uint64_t)local_apic->apic_id
                );

                serial_write_string(
                    " FLAGS: "
                );

                serial_write_hex(
                    (uint64_t)local_apic->flags
                );

                serial_write_string(
                    "\n"
                );
            }

            uint32_t io_apic_count =
                acpi_get_madt_io_apic_count();

            serial_write_string(
                "MADT IO APIC COUNT: "
            );

            serial_write_hex(
                (uint64_t)io_apic_count
            );

            serial_write_string(
                "\n"
            );

            for (uint32_t i = 0;
                 i < io_apic_count;
                 i++)
            {
                const struct acpi_madt_io_apic *io_apic =
                    acpi_get_madt_io_apic(i);

                serial_write_string(
                    "MADT IOAPIC ID: "
                );

                serial_write_hex(
                    (uint64_t)io_apic->id
                );

                serial_write_string(
                    " ADDRESS: "
                );

                serial_write_hex(
                    io_apic->address
                );

                serial_write_string(
                    " GSI BASE: "
                );

                serial_write_hex(
                    (uint64_t)io_apic->gsi_base
                );

                serial_write_string(
                    "\n"
                );
            }

            uint32_t iso_count =
                acpi_get_madt_iso_count();

            serial_write_string(
                "MADT ISO COUNT: "
            );

            serial_write_hex(
                (uint64_t)iso_count
            );

            serial_write_string(
                "\n"
            );

            for (uint32_t i = 0;
                 i < iso_count;
                 i++)
            {
                const struct acpi_madt_iso *iso =
                    acpi_get_madt_iso(i);

                serial_write_string(
                    "MADT ISO BUS: "
                );

                serial_write_hex(
                    (uint64_t)iso->bus
                );

                serial_write_string(
                    " SOURCE: "
                );

                serial_write_hex(
                    (uint64_t)iso->source
                );

                serial_write_string(
                    " GSI: "
                );

                serial_write_hex(
                    (uint64_t)iso->gsi
                );

                serial_write_string(
                    " FLAGS: "
                );

                serial_write_hex(
                    (uint64_t)iso->flags
                );

                serial_write_string(
                    "\n"
                );
            }

            serial_write_string(
                "MADT UNKNOWN ENTRY COUNT: "
            );

            serial_write_hex(
                (uint64_t)acpi_get_madt_unknown_entry_count()
            );

            serial_write_string(
                "\n"
            );

            serial_write_string(
                "MADT STRUCTURE: VERIFIED\n"
            );
        }
        else
        {
            serial_write_string(
                "ACPI MADT: NOT FOUND\n"
            );
        }

        serial_write_string(
            "ACPI ROOT TABLE DISCOVERY: VERIFIED\n"
        );
    }
    else
    {
        serial_write_string(
            "ACPI RSDP: FAILED\n"
        );

        serial_write_string(
            "ACPI ERROR CODE: "
        );

        serial_write_hex(
            (uint64_t)(uint32_t)(-acpi_result)
        );

        serial_write_string("\n");

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    /* --------------------------------------------------------
       FRAME ALLOCATOR TEST
       -------------------------------------------------------- */

    serial_write_string(
        "\nPMM ALLOCATOR TEST\n"
    );

    uint64_t frame_a =
        pmm_alloc_frame();

    uint64_t frame_b =
        pmm_alloc_frame();

    serial_write_string(
        "ALLOC FRAME A: "
    );

    serial_write_hex(frame_a);

    serial_write_string("\n");

    serial_write_string(
        "ALLOC FRAME B: "
    );

    serial_write_hex(frame_b);

    serial_write_string("\n");

    serial_write_string(
        "FREE FRAMES AFTER ALLOC: "
    );

    serial_write_hex(
        pmm_get_free_frames()
    );

    serial_write_string("\n");

    pmm_free_frame(frame_a);

    serial_write_string(
        "FRAME A FREED\n"
    );

    serial_write_string(
        "FREE FRAMES AFTER FREE: "
    );

    serial_write_hex(
        pmm_get_free_frames()
    );

    serial_write_string("\n");

    serial_write_string(
        "PMM ALLOCATOR: OK\n"
    );

    /* --------------------------------------------------------
       VIRTUAL MEMORY MANAGER
       -------------------------------------------------------- */

    serial_write_string(
        "\n================================\n"
    );

    serial_write_string(
        "BATOS VMM INITIALIZING...\n"
    );

    serial_write_string(
        "================================\n"
    );

    vmm_init();

    uint64_t pml4 =
        vmm_get_pml4();

    serial_write_string(
        "VMM PML4: "
    );

    serial_write_hex(
        pml4
    );

    serial_write_string("\n");

    uint64_t test_frame =
        pmm_alloc_frame();

    serial_write_string(
        "VMM TEST FRAME: "
    );

    serial_write_hex(
        test_frame
    );

    serial_write_string("\n");

    /*
     * VMM-2A test virtual address.
     *
     * This address is used inside the software
     * page-table structure.
     */
    uint64_t test_virtual =
        0x0000000040000000ULL;

    int map_result =
        vmm_map_page(
            pml4,
            test_virtual,
            test_frame,
            VMM_WRITABLE
        );

    serial_write_string(
        "VMM MAP RESULT: "
    );

    serial_write_hex(
        (uint64_t)map_result
    );

    serial_write_string("\n");

    if (map_result == 0)
    {
        serial_write_string(
            "VMM PAGE TABLES: OK\n"
        );
    }
    else
    {
        serial_write_string(
            "VMM PAGE TABLES: FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    /* --------------------------------------------------------
       VMM-2A SOFTWARE TRANSLATION TEST
       -------------------------------------------------------- */

    serial_write_string(
        "\nVMM-2A TRANSLATION TEST\n"
    );

    uint64_t translated_address = 0;

    int translate_result =
        vmm_translate(
            pml4,
            test_virtual,
            &translated_address
        );

    serial_write_string(
        "TRANSLATION RESULT: "
    );

    serial_write_hex(
        (uint64_t)translate_result
    );

    serial_write_string("\n");

    serial_write_string(
        "VIRTUAL ADDRESS: "
    );

    serial_write_hex(
        test_virtual
    );

    serial_write_string("\n");

    serial_write_string(
        "EXPECTED PHYSICAL: "
    );

    serial_write_hex(
        test_frame
    );

    serial_write_string("\n");

    serial_write_string(
        "TRANSLATED PHYSICAL: "
    );

    serial_write_hex(
        translated_address
    );

    serial_write_string("\n");

    if (translate_result == 0 &&
        translated_address == test_frame)
    {
        serial_write_string(
            "VMM TRANSLATION: OK\n"
        );
    }
    else
    {
        serial_write_string(
            "VMM TRANSLATION: FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    /*
     * Do NOT free test_frame.
     *
     * The page-table entry still references this frame.
     */
    serial_write_string(
        "VMM-2A: SOFTWARE WALKER VERIFIED\n"
    );

    /* --------------------------------------------------------
       VMM-2B CR3 READ VERIFICATION
       -------------------------------------------------------- */

    serial_write_string(
        "\nVMM-2B CR3 VERIFICATION\n"
    );

    uint64_t current_cr3 =
        vmm_read_cr3();

    uint64_t current_cr3_pml4 =
        current_cr3 &
        0x000FFFFFFFFFF000ULL;

    serial_write_string(
        "CURRENT CR3: "
    );

    serial_write_hex(
        current_cr3
    );

    serial_write_string("\n");

    serial_write_string(
        "CURRENT CR3 PML4: "
    );

    serial_write_hex(
        current_cr3_pml4
    );

    serial_write_string("\n");

    serial_write_string(
        "BATOS PML4: "
    );

    serial_write_hex(
        pml4
    );

    serial_write_string("\n");

    if (current_cr3_pml4 != 0)
    {
        serial_write_string(
            "CR3 READ: OK\n"
        );
    }
    else
    {
        serial_write_string(
            "CR3 READ: FAILED\n"
        );
    }

    uint64_t cr3_after_read =
        vmm_read_cr3();

    uint64_t cr3_after_read_pml4 =
        cr3_after_read &
        0x000FFFFFFFFFF000ULL;

    if (cr3_after_read_pml4 == current_cr3_pml4)
    {
        serial_write_string(
            "CR3 UNCHANGED: OK\n"
        );
    }
    else
    {
        serial_write_string(
            "CR3 UNCHANGED: FAILED\n"
        );
    }

    serial_write_string(
        "CR3 WRITE: NOT EXECUTED\n"
    );

    if (current_cr3_pml4 != 0 &&
        cr3_after_read_pml4 == current_cr3_pml4)
    {
        serial_write_string(
            "VMM-2B: CR3 READ VERIFIED\n"
        );
    }
    else
    {
        serial_write_string(
            "VMM-2B: CR3 READ FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "VMM-2B: ADDRESS SPACE NOT ACTIVATED\n"
    );

    /* --------------------------------------------------------
       VMM-2C SAFE ADDRESS-SPACE PREPARATION
       -------------------------------------------------------- */

    serial_write_string(
        "\nVMM-2C SAFE ADDRESS-SPACE ACTIVATION\n"
    );

    /*
     * Prepare BATOS's PML4.
     *
     * vmm_prepare_address_space():
     *
     *     1. Recursively clones the currently active
     *        Limine page-table hierarchy.
     *
     *     2. Keeps the cloned hierarchy independent.
     *
     *     3. Merges the cloned mappings into BATOS's
     *        own PML4.
     *
     *     4. Preserves existing BATOS-owned mappings.
     */
    int prepare_result =
        vmm_prepare_address_space();

    serial_write_string(
        "ADDRESS SPACE PREPARE RESULT: "
    );

    serial_write_hex(
        (uint64_t)(uint32_t)prepare_result
    );

    serial_write_string("\n");

    if (prepare_result != 0)
    {
        serial_write_string(
            "VMM-2C PREPARE: FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "VMM-2C PREPARE: OK\n"
    );

    /* --------------------------------------------------------
       VMM-3.1 RECURSIVE PAGE-TABLE CLONE VERIFICATION
       -------------------------------------------------------- */

    serial_write_string(
        "\nVMM-3.1 RECURSIVE PAGE-TABLE CLONE\n"
    );

    /*
     * vmm_prepare_address_space() creates a standalone
     * recursively cloned PML4 from the currently active
     * Limine hierarchy.
     *
     * The standalone clone root is exposed by:
     *
     *     vmm_get_last_cloned_pml4()
     *
     * IMPORTANT:
     *
     * We verify this standalone clone directly against
     * the original Limine PML4.
     *
     * We do NOT use test_virtual (0x40000000) here because
     * that mapping belongs to BATOS's own address space and
     * is not necessarily present in the Limine source tree.
     */
    uint64_t cloned_pml4 =
        vmm_get_last_cloned_pml4();

    serial_write_string(
        "SOURCE LIMINE PML4: "
    );

    serial_write_hex(
        current_cr3_pml4
    );

    serial_write_string("\n");

    serial_write_string(
        "CLONED PML4: "
    );

    serial_write_hex(
        cloned_pml4
    );

    serial_write_string("\n");

    if (cloned_pml4 == 0)
    {
        serial_write_string(
            "VMM-3.1 CLONED PML4: INVALID\n"
        );

        serial_write_string(
            "VMM-3.1: RECURSIVE CLONE FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    /*
     * Full recursive verification.
     *
     * This verifies the complete page-table hierarchy:
     *
     *     PML4
     *       ↓
     *     PDPT
     *       ↓
     *     PD
     *       ↓
     *     PT
     *       ↓
     *     PTE
     *
     * For normal 4 KiB mappings, the table pages must be
     * physically independent while their mapping entries
     * remain equivalent.
     */
    int clone_result =
        vmm_verify_clone(
            current_cr3_pml4,
            cloned_pml4
        );

    serial_write_string(
        "FULL CLONE VERIFICATION RESULT: "
    );

    serial_write_hex(
        (uint64_t)(uint32_t)clone_result
    );

    serial_write_string("\n");

    if (clone_result != 0)
    {
        serial_write_string(
            "VMM-3.1: RECURSIVE CLONE FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "VMM-3.1: FULL RECURSIVE CLONE VERIFIED\n"
    );

    serial_write_string(
        "VMM-3.1: PAGE-TABLE LEVEL INDEPENDENCE VERIFIED\n"
    );

    serial_write_string(
        "VMM-3.1: PHYSICAL MAPPINGS PRESERVED\n"
    );

    serial_write_string(
        "VMM-3.1: RECURSIVE CLONE VERIFIED\n"
    );

    /* --------------------------------------------------------
       VERIFY BATOS-OWNED MAPPING SURVIVED PREPARATION
       -------------------------------------------------------- */

    /*
     * Verify that BATOS's own VMM-2A mapping survived
     * the address-space preparation.
     */
    uint64_t prepared_physical = 0;

    int prepared_result =
        vmm_translate(
            pml4,
            test_virtual,
            &prepared_physical
        );

    serial_write_string(
        "PREPARED TRANSLATION RESULT: "
    );

    serial_write_hex(
        (uint64_t)(uint32_t)prepared_result
    );

    serial_write_string("\n");

    serial_write_string(
        "PREPARED PHYSICAL: "
    );

    serial_write_hex(
        prepared_physical
    );

    serial_write_string("\n");

    if (prepared_result != 0 ||
        prepared_physical != test_frame)
    {
        serial_write_string(
            "BATOS TEST MAPPING: FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "BATOS TEST MAPPING: PRESERVED\n"
    );

    /* --------------------------------------------------------
       ACTIVATE BATOS ADDRESS SPACE
       -------------------------------------------------------- */

    /*
     * Disable maskable interrupts before changing CR3.
     *
     * Hardware interrupt routing is not active yet.
     */
    __asm__ volatile (
        "cli"
        :
        :
        : "memory"
    );

    serial_write_string(
        "INTERRUPTS: DISABLED\n"
    );

    /*
     * BATOS-owned PML4 that will become the active
     * address-space root.
     */
    uint64_t batos_pml4 =
        vmm_get_pml4();

    serial_write_string(
        "SWITCHING CR3 TO BATOS PML4: "
    );

    serial_write_hex(
        batos_pml4
    );

    serial_write_string("\n");

    /*
     * VMM-3.2B owns the address-space lifecycle.
     *
     * The kernel keeps interrupts disabled while the VMM
     * performs and verifies the hardware CR3 transition.
     */
    int activation_result =
        vmm_activate_address_space(
            batos_pml4
        );

    /*
     * Read CR3 back after the VMM activation API.
     */
    uint64_t activated_cr3 =
        vmm_read_cr3();

    uint64_t activated_pml4 =
        activated_cr3 &
        0x000FFFFFFFFFF000ULL;

    serial_write_string(
        "CR3 AFTER SWITCH: "
    );

    serial_write_hex(
        activated_cr3
    );

    serial_write_string("\n");

    serial_write_string(
        "CR3 PML4 AFTER SWITCH: "
    );

    serial_write_hex(
        activated_pml4
    );

    serial_write_string("\n");

    if (activation_result == 0 &&
        activated_pml4 == batos_pml4)
    {
        serial_write_string(
            "CR3 SWITCH: OK\n"
        );

        serial_write_string(
            "VMM-2C: ADDRESS SPACE ACTIVATED\n"
        );
    }
    else
    {
        serial_write_string(
            "CR3 SWITCH: FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "BATOS ADDRESS SPACE IS NOW ACTIVE\n"
    );

    /* --------------------------------------------------------
       VMM-3.2B ADDRESS-SPACE LIFECYCLE VERIFICATION
       -------------------------------------------------------- */

    serial_write_string(
        "\nVMM-3.2B ADDRESS-SPACE LIFECYCLE\n"
    );

    serial_write_string(
        "VERIFYING BATOS ADDRESS-SPACE REGISTRATION...\n"
    );

    if (vmm_verify_address_space_state(
            batos_pml4,
            VMM_ADDRESS_SPACE_ACTIVE
        ) != 0)
    {
        serial_write_string(
            "VMM-3.2B: ADDRESS-SPACE REGISTRATION FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "BATOS ADDRESS SPACE: ACTIVE\n"
    );

    serial_write_string(
        "VERIFYING ACTIVE ADDRESS-SPACE IDENTITY...\n"
    );

    if (activated_pml4 != batos_pml4)
    {
        serial_write_string(
            "ACTIVE ADDRESS-SPACE IDENTITY: FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "ACTIVE ADDRESS-SPACE IDENTITY: OK\n"
    );

    serial_write_string(
        "VMM-3.2B: ADDRESS-SPACE LIFECYCLE VERIFIED\n"
    );

    /* --------------------------------------------------------
       VMM-2D ADDRESS-SPACE INSPECTION
       -------------------------------------------------------- */

    serial_write_string(
        "\nVMM-2D ADDRESS-SPACE INSPECTION\n"
    );

    uint64_t active_cr3 =
        vmm_read_cr3();

    uint64_t active_pml4 =
        active_cr3 &
        0x000FFFFFFFFFF000ULL;

    serial_write_string(
        "ACTIVE CR3: "
    );

    serial_write_hex(
        active_cr3
    );

    serial_write_string("\n");

    serial_write_string(
        "ACTIVE PML4: "
    );

    serial_write_hex(
        active_pml4
    );

    serial_write_string("\n");

    serial_write_string(
        "BATOS PML4: "
    );

    serial_write_hex(
        pml4
    );

    serial_write_string("\n");

    if (active_pml4 != pml4)
    {
        serial_write_string(
            "VMM-2D ACTIVE PML4: FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    uint64_t present_entries = 0;

    int inspect_result =
        vmm_inspect_address_space(
            active_pml4,
            &present_entries
        );

    serial_write_string(
        "INSPECTION RESULT: "
    );

    serial_write_hex(
        (uint64_t)(uint32_t)inspect_result
    );

    serial_write_string("\n");

    serial_write_string(
        "ACTIVE PML4 PRESENT ENTRIES: "
    );

    serial_write_hex(
        present_entries
    );

    serial_write_string("\n");

    if (inspect_result == 0 &&
        present_entries > 0)
    {
        serial_write_string(
            "VMM-2D ADDRESS SPACE: OK\n"
        );
    }
    else
    {
        serial_write_string(
            "VMM-2D ADDRESS SPACE: FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    /* --------------------------------------------------------
       VMM-3.2A PAGE-TABLE OWNERSHIP VERIFICATION
       -------------------------------------------------------- */

    serial_write_string(
        "\nVMM-3.2A PAGE-TABLE OWNERSHIP\n"
    );

    serial_write_string(
        "VERIFYING BATOS PML4 OWNERSHIP...\n"
    );

    if (vmm_verify_page_table_root(pml4) != 0)
    {
        serial_write_string(
            "VMM-3.2A: BATOS PML4 OWNERSHIP FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "BATOS PML4 OWNERSHIP: OK\n"
    );

    serial_write_string(
        "VERIFYING BATOS PAGE-TABLE PATH...\n"
    );

    if (vmm_verify_page_table_ownership(
            pml4,
            test_virtual
        ) != 0)
    {
        serial_write_string(
            "VMM-3.2A: PAGE-TABLE PATH OWNERSHIP FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "BATOS PML4 -> PDPT -> PD -> PT OWNERSHIP: OK\n"
    );

    serial_write_string(
        "VERIFYING CLONED PML4 OWNERSHIP...\n"
    );

    if (vmm_verify_page_table_root(cloned_pml4) != 0)
    {
        serial_write_string(
            "VMM-3.2A: CLONED PML4 OWNERSHIP FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "CLONED PML4 OWNERSHIP: OK\n"
    );

    if (pml4 != cloned_pml4)
    {
        serial_write_string(
            "OWNERSHIP ROOTS DISTINCT: OK\n"
        );

        serial_write_string(
            "VMM-3.2A: PAGE-TABLE OWNERSHIP VERIFIED\n"
        );
    }
    else
    {
        serial_write_string(
            "VMM-3.2A: OWNERSHIP ROOTS NOT DISTINCT\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    /* --------------------------------------------------------
       VMM-2F PAGE UNMAP + FRAME LIFECYCLE TEST
       -------------------------------------------------------- */

    serial_write_string(
        "\nVMM-2F PAGE UNMAP + FRAME LIFECYCLE TEST\n"
    );

    /*
     * Use a dedicated virtual address for the unmap test.
     *
     * This must not overlap BATOS's existing VMM-2A mapping
     * at 0x40000000.
     */
    uint64_t unmap_virtual =
        0x0000000040001000ULL;

    uint64_t unmap_check_physical = 0;

    /*
     * First prove that the candidate address is actually
     * unmapped in the active BATOS address space.
     */
    int initial_translate =
        vmm_translate(
            pml4,
            unmap_virtual,
            &unmap_check_physical
        );

    serial_write_string(
        "INITIAL TRANSLATION RESULT: "
    );

    serial_write_hex(
        (uint64_t)(uint32_t)initial_translate
    );

    serial_write_string("\n");

    if (initial_translate == 0)
    {
        serial_write_string(
            "VMM-2F: TEST ADDRESS ALREADY MAPPED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "VMM-2F: TEST ADDRESS IS UNMAPPED\n"
    );

    /*
     * Record PMM state before acquiring the test frame.
     */
    uint64_t free_frames_before =
        pmm_get_free_frames();

    serial_write_string(
        "PMM FREE FRAMES BEFORE ALLOCATION: "
    );

    serial_write_hex(
        free_frames_before
    );

    serial_write_string("\n");

    /*
     * Acquire one real physical frame from PMM.
     */
    uint64_t unmap_frame =
        pmm_alloc_frame();

    if (unmap_frame == 0)
    {
        serial_write_string(
            "VMM-2F: FRAME ALLOCATION FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "VMM-2F TEST FRAME: "
    );

    serial_write_hex(
        unmap_frame
    );

    serial_write_string("\n");

    /*
     * Map the dedicated virtual address.
     */
    int unmap_map_result =
        vmm_map_page(
            pml4,
            unmap_virtual,
            unmap_frame,
            VMM_WRITABLE
        );

    serial_write_string(
        "UNMAP TEST MAP RESULT: "
    );

    serial_write_hex(
        (uint64_t)(uint32_t)unmap_map_result
    );

    serial_write_string("\n");

    if (unmap_map_result != 0)
    {
        pmm_free_frame(unmap_frame);

        serial_write_string(
            "VMM-2F: MAP FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    /*
     * Verify software translation after mapping.
     */
    uint64_t mapped_physical = 0;

    int mapped_translate_result =
        vmm_translate(
            pml4,
            unmap_virtual,
            &mapped_physical
        );

    if (mapped_translate_result != 0 ||
        mapped_physical != unmap_frame)
    {
        uint64_t cleanup_physical = 0;

        if (vmm_unmap_page(
                pml4,
                unmap_virtual,
                &cleanup_physical
            ) == 0)
        {
            pmm_free_frame(cleanup_physical);
        }
        else
        {
            pmm_free_frame(unmap_frame);
        }

        serial_write_string(
            "VMM-2F: MAP TRANSLATION FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "VMM-2F: MAP TRANSLATION VERIFIED\n"
    );

    /*
     * Perform a REAL CPU memory access through the new mapping.
     */
    volatile uint64_t *unmap_test_address =
        (volatile uint64_t *)unmap_virtual;

    uint64_t unmap_test_pattern =
        0x4241544F532D3246ULL;

    *unmap_test_address =
        unmap_test_pattern;

    uint64_t unmap_readback =
        *unmap_test_address;

    serial_write_string(
        "VMM-2F CPU WRITE/READ: "
    );

    serial_write_hex(
        unmap_readback
    );

    serial_write_string("\n");

    if (unmap_readback != unmap_test_pattern)
    {
        uint64_t cleanup_physical = 0;

        if (vmm_unmap_page(
                pml4,
                unmap_virtual,
                &cleanup_physical
            ) == 0)
        {
            pmm_free_frame(cleanup_physical);
        }
        else
        {
            pmm_free_frame(unmap_frame);
        }

        serial_write_string(
            "VMM-2F: CPU MEMORY ACCESS FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "VMM-2F: CPU MEMORY ACCESS VERIFIED\n"
    );

    /*
     * Remove only the virtual mapping.
     *
     * vmm_unmap_page() returns the physical frame but does
     * not free it. PMM remains responsible for frame lifetime.
     */
    uint64_t unmapped_physical = 0;

    int unmap_result =
        vmm_unmap_page(
            pml4,
            unmap_virtual,
            &unmapped_physical
        );

    serial_write_string(
        "VMM UNMAP RESULT: "
    );

    serial_write_hex(
        (uint64_t)(uint32_t)unmap_result
    );

    serial_write_string("\n");

    serial_write_string(
        "UNMAPPED PHYSICAL: "
    );

    serial_write_hex(
        unmapped_physical
    );

    serial_write_string("\n");

    if (unmap_result != 0 ||
        unmapped_physical != unmap_frame)
    {
        serial_write_string(
            "VMM-2F: UNMAP FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }


    /*
     * The virtual mapping must now be absent.
     *
     * We deliberately do NOT dereference unmap_virtual here:
     * a page fault would be expected and page-fault recovery
     * is a separate kernel milestone.
     */
    uint64_t after_unmap_physical = 0;

    int after_unmap_result =
        vmm_translate(
            pml4,
            unmap_virtual,
            &after_unmap_physical
        );

    serial_write_string(
        "AFTER UNMAP TRANSLATION RESULT: "
    );

    serial_write_hex(
        (uint64_t)(uint32_t)after_unmap_result
    );

    serial_write_string("\n");

    if (after_unmap_result == 0)
    {
        serial_write_string(
            "VMM-2F: TRANSLATION STILL PRESENT\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "VMM-2F: TRANSLATION REMOVED\n"
    );

    /*
     * Return the physical frame to PMM.
     */
    pmm_free_frame(
        unmapped_physical
    );

    uint64_t free_frames_after =
        pmm_get_free_frames();

    serial_write_string(
        "PMM FREE FRAMES AFTER RELEASE: "
    );

    serial_write_hex(
        free_frames_after
    );

    serial_write_string("\n");

    if (free_frames_after !=
        free_frames_before)
    {
        serial_write_string(
            "VMM-2F: PMM FRAME LIFECYCLE FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "VMM-2F: PMM FRAME RELEASE VERIFIED\n"
    );

    serial_write_string(
        "VMM-2F: PAGE UNMAP + FRAME LIFECYCLE VERIFIED\n"
    );


#ifdef BATOS_VMM_TEST
    /* --------------------------------------------------------
       VMM-3.2C TRANSACTIONAL MAP ROLLBACK TEST
       -------------------------------------------------------- */

    serial_write_string(
        "\nVMM-3.2C TRANSACTIONAL MAP ROLLBACK TEST\n"
    );

    /*
     * This VA is deliberately outside the addresses already
     * exercised by the earlier VMM tests.
     */
    uint64_t rollback_test_virtual =
        0x0000400000000000ULL;

    uint64_t rollback_test_frame =
        pmm_alloc_frame();

    if (rollback_test_frame == 0)
    {
        serial_write_string(
            "VMM-3.2C: TEST FRAME ALLOCATION FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    uint64_t rollback_free_before =
        pmm_get_free_frames();

    serial_write_string(
        "ROLLBACK TEST PMM BASELINE: "
    );
    serial_write_hex(
        rollback_free_before
    );
    serial_write_string("\n");

    extern int vmm_test_is_pml4_slot_empty(
        uint64_t pml4_physical,
        uint64_t virtual_address
    );

    int rollback_pml4_empty =
        vmm_test_is_pml4_slot_empty(
            pml4,
            rollback_test_virtual
        );

    if (!rollback_pml4_empty)
    {
        serial_write_string(
            "VMM-3.2C: TEST VA PML4 SLOT NOT FRESH\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "VMM-3.2C: TEST PML4 SLOT EMPTY\n"
    );

    extern int vmm_test_fail_pt_allocation;

    vmm_test_fail_pt_allocation = 1;

    int rollback_result =
        vmm_map_page(
            pml4,
            rollback_test_virtual,
            rollback_test_frame,
            VMM_WRITABLE
        );

    vmm_test_fail_pt_allocation = 0;

    serial_write_string(
        "FORCED PT FAILURE RESULT: "
    );
    serial_write_hex(
        (uint64_t)(uint32_t)rollback_result
    );
    serial_write_string("\n");

    if (rollback_result == 0)
    {
        serial_write_string(
            "VMM-3.2C: FAILURE INJECTION DID NOT FAIL\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    uint64_t rollback_free_after =
        pmm_get_free_frames();

    serial_write_string(
        "ROLLBACK TEST PMM AFTER FAILURE: "
    );
    serial_write_hex(
        rollback_free_after
    );
    serial_write_string("\n");

    if (rollback_free_after !=
        rollback_free_before)
    {
        serial_write_string(
            "VMM-3.2C: PMM ROLLBACK LEAK DETECTED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    uint64_t rollback_physical = 0;

    int rollback_translate_result =
        vmm_translate(
            pml4,
            rollback_test_virtual,
            &rollback_physical
        );

    serial_write_string(
        "ROLLBACK TEST TRANSLATION RESULT: "
    );
    serial_write_hex(
        (uint64_t)(uint32_t)
            rollback_translate_result
    );
    serial_write_string("\n");

    if (rollback_translate_result == 0)
    {
        serial_write_string(
            "VMM-3.2C: MAPPING SURVIVED ROLLBACK\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "VMM-3.2C: PAGE-TABLE ROLLBACK VERIFIED\n"
    );

    /*
     * The failed mapping never took ownership of the test
     * frame because the final PTE was never installed.
     * Release the frame allocated specifically for this test.
     */
    pmm_free_frame(
        rollback_test_frame
    );

    /*
     * Recovery test:
     *
     * With failure injection disabled, the same fresh VA
     * must be mappable normally.
     */
    uint64_t recovery_frame =
        pmm_alloc_frame();

    if (recovery_frame == 0)
    {
        serial_write_string(
            "VMM-3.2C: RECOVERY FRAME ALLOCATION FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    int recovery_result =
        vmm_map_page(
            pml4,
            rollback_test_virtual,
            recovery_frame,
            VMM_WRITABLE
        );

    serial_write_string(
        "RECOVERY MAP RESULT: "
    );
    serial_write_hex(
        (uint64_t)(uint32_t)recovery_result
    );
    serial_write_string("\n");

    if (recovery_result != 0)
    {
        serial_write_string(
            "VMM-3.2C: RECOVERY MAP FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    uint64_t recovery_physical = 0;

    if (vmm_translate(
            pml4,
            rollback_test_virtual,
            &recovery_physical
        ) != 0 ||
        recovery_physical != recovery_frame)
    {
        serial_write_string(
            "VMM-3.2C: RECOVERY TRANSLATION FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    volatile uint64_t *recovery_address =
        (volatile uint64_t *)
            rollback_test_virtual;

    uint64_t recovery_pattern =
        0x4241544F532D3343ULL;

    *recovery_address = recovery_pattern;

    if (*recovery_address != recovery_pattern)
    {
        serial_write_string(
            "VMM-3.2C: RECOVERY HARDWARE ACCESS FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    uint64_t unmapped_recovery_frame = 0;

    if (vmm_unmap_page(
            pml4,
            rollback_test_virtual,
            &unmapped_recovery_frame
        ) != 0 ||
        unmapped_recovery_frame != recovery_frame)
    {
        serial_write_string(
            "VMM-3.2C: RECOVERY UNMAP FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    pmm_free_frame(
        unmapped_recovery_frame
    );

    serial_write_string(
        "VMM-3.2C: RECOVERY MAP + CPU ACCESS VERIFIED\n"
    );

    serial_write_string(
        "VMM-3.2C: TRANSACTIONAL ROLLBACK VERIFIED\n"
    );

#endif
    /* --------------------------------------------------------
       VMM-2E HARDWARE PAGE TRANSLATION TEST
       -------------------------------------------------------- */

    serial_write_string(
        "\nVMM-2E HARDWARE PAGE TRANSLATION TEST\n"
    );

    /*
     * At this point:
     *
     *     CR3
     *       ↓
     *     BATOS PML4
     *       ↓
     *     PDPT
     *       ↓
     *     PD
     *       ↓
     *     PT
     *       ↓
     *     PTE
     *       ↓
     *     test_frame
     *
     * The software walker already verified this mapping.
     *
     * VMM-2E performs a REAL memory access through
     * test_virtual.
     *
     * This forces the CPU/MMU to perform the hardware
     * page-table translation.
     */

    volatile uint64_t *hardware_test_address =
        (volatile uint64_t *)test_virtual;

    uint64_t test_pattern =
        0x4241544F532D3245ULL;

    serial_write_string(
        "HARDWARE TEST VIRTUAL: "
    );

    serial_write_hex(
        (uint64_t)hardware_test_address
    );

    serial_write_string("\n");

    serial_write_string(
        "HARDWARE TEST PHYSICAL: "
    );

    serial_write_hex(
        test_frame
    );

    serial_write_string("\n");

    serial_write_string(
        "WRITING TEST PATTERN...\n"
    );

    /*
     * REAL CPU MEMORY ACCESS.
     *
     * Because the pointer is volatile, the compiler must
     * emit an actual memory store.
     */
    *hardware_test_address =
        test_pattern;

    serial_write_string(
        "READING TEST PATTERN...\n"
    );

    /*
     * REAL CPU MEMORY ACCESS.
     *
     * This load must travel through the active BATOS
     * page-table hierarchy.
     */
    uint64_t readback =
        *hardware_test_address;

    serial_write_string(
        "EXPECTED VALUE: "
    );

    serial_write_hex(
        test_pattern
    );

    serial_write_string("\n");

    serial_write_string(
        "READBACK VALUE: "
    );

    serial_write_hex(
        readback
    );

    serial_write_string("\n");

    if (readback != test_pattern)
    {
        serial_write_string(
            "VMM-2E: HARDWARE TRANSLATION FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "HARDWARE MEMORY ACCESS: OK\n"
    );

    serial_write_string(
        "VMM-2E: CPU PAGE TRANSLATION VERIFIED\n"
    );

    serial_write_string(
        "BATOS HARDWARE ADDRESS TRANSLATION: OK\n"
    );

    heap_test_dynamic_kernel_page();
    heap_test_bootstrap();
    heap_test_dynamic_page_ownership();


    /* --------------------------------------------------------
       LOCAL APIC BRING-UP
       -------------------------------------------------------- */


    serial_write_string(
        "LAPIC BRING-UP START\n"
    );

    int lapic_result = lapic_init();

    if (lapic_result != 0)
    {
        serial_write_string(
            "LAPIC: INITIALIZATION FAILED\n"
        );

        serial_write_string(
            "LAPIC ERROR: "
        );

        serial_write_hex(
            (uint64_t)(uint32_t)(-lapic_result)
        );

        serial_write_string("\n");
        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\nhlt"
            );
        }
    }

    serial_write_string(
        "LAPIC PHYSICAL ADDRESS: "
    );

    serial_write_hex(
        lapic_get_physical_address()
    );

    serial_write_string("\n");

    serial_write_string(
        "LAPIC VIRTUAL ADDRESS: "
    );

    serial_write_hex(
        lapic_get_virtual_address()
    );

    serial_write_string("\n");

    uint32_t lapic_id =
        lapic_read(LAPIC_REG_ID);

    uint32_t lapic_version =
        lapic_read(LAPIC_REG_VERSION);

    uint32_t lapic_svr =
        lapic_read(LAPIC_REG_SVR);

    serial_write_string(
        "LAPIC ID: "
    );

    serial_write_hex(
        (uint64_t)(lapic_id >> 24)
    );

    serial_write_string("\n");

    serial_write_string(
        "LAPIC VERSION: "
    );

    serial_write_hex(
        (uint64_t)(lapic_version & 0xFFU)
    );

    serial_write_string("\n");

    serial_write_string(
        "LAPIC MAX LVT: "
    );

    serial_write_hex(
        (uint64_t)((lapic_version >> 16) & 0xFFU)
    );

    serial_write_string("\n");

    serial_write_string(
        "LAPIC SVR: "
    );

    serial_write_hex(
        (uint64_t)lapic_svr
    );

    serial_write_string("\n");

    if ((lapic_svr & LAPIC_SVR_ENABLE) == 0)
    {
        serial_write_string(
            "LAPIC SOFTWARE ENABLE: FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\nhlt"
            );
        }
    }

    serial_write_string(
        "LAPIC MMIO: OK\n"
    );

    serial_write_string(
        "LAPIC SOFTWARE ENABLE: OK\n"
    );

    serial_write_string(
        "LAPIC EOI: ISSUED\n"
    );

    serial_write_string(
        "BATOS LAPIC: VERIFIED\n"
    );

    /* --------------------------------------------------------
       LAPIC TIMER BRING-UP
       -------------------------------------------------------- */

    serial_write_string(
        "LAPIC TIMER BRING-UP START\n"
    );

    /*
     * Configure a masked, one-shot LAPIC timer.
     *
     * Interrupt delivery remains disabled. The initial
     * count is intentionally large so the countdown can
     * be observed before the timer reaches zero.
     */
    const uint32_t lapic_timer_initial =
        0xFFFFFFFFU;

    int lapic_timer_result =
        lapic_timer_init(
            lapic_timer_initial
        );

    if (lapic_timer_result != 0)
    {
        serial_write_string(
            "LAPIC TIMER: INITIALIZATION FAILED\n"
        );

        serial_write_string(
            "LAPIC TIMER ERROR: "
        );

        serial_write_hex(
            (uint64_t)(uint32_t)(-lapic_timer_result)
        );

        serial_write_string("\n");

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    uint32_t lapic_timer_lvt =
        lapic_read(
            LAPIC_REG_LVT_TIMER
        );

    uint32_t lapic_timer_divide =
        lapic_read(
            LAPIC_REG_TIMER_DIVIDE
        );

    uint32_t lapic_timer_current_before =
        lapic_read(
            LAPIC_REG_TIMER_CURRENT
        );

    serial_write_string(
        "LAPIC TIMER LVT: "
    );

    serial_write_hex(
        (uint64_t)lapic_timer_lvt
    );

    serial_write_string("\n");

    serial_write_string(
        "LAPIC TIMER DIVIDE: "
    );

    serial_write_hex(
        (uint64_t)lapic_timer_divide
    );

    serial_write_string("\n");

    serial_write_string(
        "LAPIC TIMER CURRENT BEFORE: "
    );

    serial_write_hex(
        (uint64_t)lapic_timer_current_before
    );

    serial_write_string("\n");

    /*
     * Give the hardware timer a short interval to count down.
     *
     * Interrupts remain disabled, so this test observes
     * only the LAPIC timer counter and cannot enter the
     * LAPIC timer interrupt handler.
     */
    for (volatile uint32_t delay = 0;
         delay < 1000000U;
         delay++)
    {
        __asm__ volatile ("pause");
    }

    uint32_t lapic_timer_current_after =
        lapic_read(
            LAPIC_REG_TIMER_CURRENT
        );

    serial_write_string(
        "LAPIC TIMER CURRENT AFTER: "
    );

    serial_write_hex(
        (uint64_t)lapic_timer_current_after
    );

    serial_write_string("\n");

    /*
     * Verify:
     *
     *   1. Timer vector is 0xF0.
     *   2. Timer remains masked.
     *   3. Periodic mode is disabled.
     *   4. Divide configuration is divide-by-16.
     *   5. Current count decreased.
     */
    uint32_t expected_lapic_timer_lvt =
        LAPIC_LVT_TIMER_VECTOR |
        LAPIC_LVT_TIMER_MASK;

    if (lapic_timer_lvt != expected_lapic_timer_lvt)
    {
        serial_write_string(
            "LAPIC TIMER LVT CONFIG: FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    if (lapic_timer_divide != 0x3U)
    {
        serial_write_string(
            "LAPIC TIMER DIVIDE CONFIG: FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    if (lapic_timer_current_after >=
        lapic_timer_current_before)
    {
        serial_write_string(
            "LAPIC TIMER COUNTDOWN: FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "LAPIC TIMER CONFIG: VERIFIED\n"
    );

    serial_write_string(
        "LAPIC TIMER COUNTDOWN: VERIFIED\n"
    );

    serial_write_string(
        "LAPIC TIMER INTERRUPT: MASKED\n"
    );

    serial_write_string(
        "LAPIC TIMER BRING-UP: VERIFIED\n"
    );

    /*
     * Stop and reset the masked LAPIC timer after the
     * controlled countdown test. This guarantees that
     * later calibration starts from a known timer state.
     */
    int lapic_timer_stop_result =
        lapic_timer_stop();

    if (lapic_timer_stop_result != 0)
    {
        serial_write_string(
            "LAPIC TIMER STOP: FAILED\n"
        );

        serial_write_string(
            "LAPIC TIMER STOP ERROR: "
        );

        serial_write_hex(
            (uint64_t)(uint32_t)(-lapic_timer_stop_result)
        );

        serial_write_string("\n");

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "LAPIC TIMER STOP: VERIFIED\n"
    );

    /* --------------------------------------------------------
       I/O APIC BRING-UP
       -------------------------------------------------------- */

    serial_write_string(
        "IOAPIC BRING-UP START\n"
    );

    int ioapic_result = ioapic_init();

    if (ioapic_result != 0)
    {
        serial_write_string(
            "IOAPIC: INITIALIZATION FAILED\n"
        );

        serial_write_string(
            "IOAPIC ERROR: "
        );

        serial_write_hex(
            (uint64_t)(uint32_t)(-ioapic_result)
        );

        serial_write_string("\n");

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "IOAPIC PHYSICAL ADDRESS: "
    );

    serial_write_hex(
        ioapic_get_physical_address()
    );

    serial_write_string("\n");

    serial_write_string(
        "IOAPIC VIRTUAL ADDRESS: "
    );

    serial_write_hex(
        ioapic_get_virtual_address()
    );

    serial_write_string("\n");

    serial_write_string(
        "IOAPIC ID: "
    );

    serial_write_hex(
        (uint64_t)ioapic_get_id()
    );

    serial_write_string("\n");

    serial_write_string(
        "IOAPIC VERSION: "
    );

    serial_write_hex(
        (uint64_t)ioapic_get_version()
    );

    serial_write_string("\n");

    serial_write_string(
        "IOAPIC MAX REDIRECTION ENTRY: "
    );

    serial_write_hex(
        (uint64_t)ioapic_get_max_redirection_entry()
    );

    serial_write_string("\n");

    /*
     * Read the first redirection entry without changing it.
     *
     * This verifies the IOREGSEL/IOWIN mechanism and the
     * 64-bit redirection-table access path.
     */
    uint64_t ioapic_redir0 = 0;

    int ioapic_redir_result =
        ioapic_read_redirection(
            0,
            &ioapic_redir0
        );

    if (ioapic_redir_result != 0)
    {
        serial_write_string(
            "IOAPIC REDIRECTION READ: FAILED\n"
        );

        serial_write_string(
            "IOAPIC ERROR: "
        );

        serial_write_hex(
            (uint64_t)(uint32_t)(-ioapic_redir_result)
        );

        serial_write_string("\n");

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "IOAPIC REDIR[0]: "
    );

    serial_write_hex(
        ioapic_redir0
    );

    serial_write_string("\n");

    serial_write_string(
        "IOAPIC MMIO: OK\n"
    );

    serial_write_string(
        "IOAPIC REDIRECTION READ: OK\n"
    );

    serial_write_string(
        "BATOS IOAPIC: VERIFIED\n"
    );

    /* --------------------------------------------------------
       GSI ROUTING INFORMATION BRING-UP

       This stage resolves ACPI IRQ/GSI routing only.
       No IOAPIC redirection entry is modified and no
       APIC interrupt is enabled here.
       -------------------------------------------------------- */

    serial_write_string(
        "GSI ROUTING BRING-UP START\n"
    );

    int gsi_result = gsi_init();

    if (gsi_result != 0)
    {
        serial_write_string(
            "GSI: INITIALIZATION FAILED\n"
        );

        serial_write_string(
            "GSI ERROR: "
        );

        serial_write_hex(
            (uint64_t)(uint32_t)(-gsi_result)
        );

        serial_write_string("\n");

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    struct gsi_irq_route gsi_irq0_route;

    if (gsi_resolve_irq(
            0,
            &gsi_irq0_route
        ) != 0)
    {
        serial_write_string(
            "GSI IRQ0 ROUTE: FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "GSI IRQ0 ROUTE: GSI="
    );

    serial_write_hex(
        (uint64_t)gsi_irq0_route.gsi
    );

    serial_write_string(
        " IOAPIC="
    );

    serial_write_hex(
        (uint64_t)gsi_irq0_route.ioapic_index
    );

    serial_write_string(
        " REDIR="
    );

    serial_write_hex(
        (uint64_t)
        gsi_irq0_route.ioapic_redirection_index
    );

    serial_write_string(
        " POLARITY="
    );

    serial_write_hex(
        (uint64_t)gsi_irq0_route.polarity
    );

    serial_write_string(
        " TRIGGER="
    );

    serial_write_hex(
        (uint64_t)gsi_irq0_route.trigger_mode
    );

    serial_write_string(
        " ISO="
    );

    serial_write_hex(
        (uint64_t)gsi_irq0_route.has_iso
    );

    serial_write_string("\n");

    /*
     * QEMU's current MADT reports:
     *
     *   ISA IRQ0 -> GSI2
     *
     * with conforming polarity/trigger flags, which resolve
     * to the ISA defaults:
     *
     *   active-high + edge-triggered.
     */
    if (gsi_irq0_route.gsi != 2 ||
        gsi_irq0_route.ioapic_index != 0 ||
        gsi_irq0_route.ioapic_redirection_index != 2 ||
        gsi_irq0_route.polarity != GSI_POLARITY_HIGH ||
        gsi_irq0_route.trigger_mode != GSI_TRIGGER_EDGE ||
        gsi_irq0_route.has_iso != 1)
    {
        serial_write_string(
            "GSI IRQ0 ROUTE: FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "GSI IRQ0 ROUTE: VERIFIED\n"
    );

    serial_write_string(
        "GSI ROUTING: VERIFIED\n"
    );

    /* --------------------------------------------------------
       STAGE 3: MASKED IOAPIC REDIRECTION PROGRAMMING
       --------------------------------------------------------

       Program the already-verified IRQ0 route:

           IRQ0 -> GSI2 -> IOAPIC0 -> REDIR[2]

       The entry is deliberately kept MASKED.
       This stage verifies only redirection programming and
       exact hardware readback. Interrupt delivery is NOT
       enabled or migrated here.
       -------------------------------------------------------- */

    serial_write_string(
        "IOAPIC STAGE 3: MASKED REDIRECTION START\n"
    );

    uint32_t stage3_ioapic_index =
        gsi_irq0_route.ioapic_index;

    uint8_t stage3_redirection_index =
        (uint8_t)(
            gsi_irq0_route.ioapic_redirection_index
        );

    uint8_t stage3_lapic_id =
        (uint8_t)(lapic_id >> 24);

    /*
     * Construct the complete 64-bit redirection entry.
     *
     * Vector:
     *     bits 7:0 = 0x50
     *
     * Delivery mode:
     *     bits 10:8 = Fixed
     *
     * Destination mode:
     *     bit 11 = Physical
     *
     * Polarity:
     *     bit 13 = Active High
     *
     * Trigger:
     *     bit 15 = Edge
     *
     * Mask:
     *     bit 16 = MASKED
     *
     * Destination:
     *     bits 63:56 = current LAPIC ID
     */
    uint64_t stage3_expected =
        IOAPIC_STAGE3_TEST_VECTOR |
        IOAPIC_REDIR_DELIVERY_FIXED |
        IOAPIC_REDIR_MASKED |
        ((uint64_t)stage3_lapic_id << 56);

    serial_write_string(
        "IOAPIC STAGE 3 ROUTE: IRQ0 -> GSI2 -> IOAPIC="
    );

    serial_write_hex(
        (uint64_t)stage3_ioapic_index
    );

    serial_write_string(
        " REDIR="
    );

    serial_write_hex(
        (uint64_t)stage3_redirection_index
    );

    serial_write_string("\n");

    serial_write_string(
        "IOAPIC STAGE 3 VECTOR: "
    );

    serial_write_hex(
        (uint64_t)IOAPIC_STAGE3_TEST_VECTOR
    );

    serial_write_string("\n");

    serial_write_string(
        "IOAPIC STAGE 3 LAPIC DESTINATION: "
    );

    serial_write_hex(
        (uint64_t)stage3_lapic_id
    );

    serial_write_string("\n");

    serial_write_string(
        "IOAPIC STAGE 3 EXPECTED: "
    );

    serial_write_hex(
        stage3_expected
    );

    serial_write_string("\n");

    /*
     * The expected value explicitly contains MASKED=1.
     * Verify that before touching hardware.
     */
    if ((stage3_expected & IOAPIC_REDIR_MASKED) == 0)
    {
        serial_write_string(
            "IOAPIC STAGE 3 MASK: FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "IOAPIC STAGE 3 MASK: PRESERVED\n"
    );

    /*
     * Program the entry.
     *
     * ioapic_write_redirection_at() writes the HIGH dword
     * first and the LOW dword second.
     *
     * The LOW dword contains MASKED=1, so interrupt delivery
     * remains disabled after programming.
     */
    int stage3_write_result =
        ioapic_write_redirection_at(
            stage3_ioapic_index,
            stage3_redirection_index,
            stage3_expected
        );

    if (stage3_write_result != 0)
    {
        serial_write_string(
            "IOAPIC STAGE 3 WRITE: FAILED\n"
        );

        serial_write_string(
            "IOAPIC ERROR: "
        );

        serial_write_hex(
            (uint64_t)(uint32_t)(-stage3_write_result)
        );

        serial_write_string("\n");

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "IOAPIC STAGE 3 WRITE: OK\n"
    );

    /*
     * Read the complete entry back from hardware.
     */
    uint64_t stage3_actual = 0;

    int stage3_read_result =
        ioapic_read_redirection_at(
            stage3_ioapic_index,
            stage3_redirection_index,
            &stage3_actual
        );

    if (stage3_read_result != 0)
    {
        serial_write_string(
            "IOAPIC STAGE 3 READBACK: FAILED\n"
        );

        serial_write_string(
            "IOAPIC ERROR: "
        );

        serial_write_hex(
            (uint64_t)(uint32_t)(-stage3_read_result)
        );

        serial_write_string("\n");

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "IOAPIC STAGE 3 ACTUAL: "
    );

    serial_write_hex(
        stage3_actual
    );

    serial_write_string("\n");

    /*
     * Exact 64-bit comparison.
     */
    if (stage3_actual != stage3_expected)
    {
        serial_write_string(
            "IOAPIC STAGE 3 READBACK: FAILED\n"
        );

        serial_write_string(
            "IOAPIC EXPECTED: "
        );

        serial_write_hex(
            stage3_expected
        );

        serial_write_string("\n");

        serial_write_string(
            "IOAPIC ACTUAL: "
        );

        serial_write_hex(
            stage3_actual
        );

        serial_write_string("\n");

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    /*
     * Independently verify the hardware readback still has
     * the MASKED bit set.
     */
    if ((stage3_actual & IOAPIC_REDIR_MASKED) == 0)
    {
        serial_write_string(
            "IOAPIC STAGE 3 MASK READBACK: FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "IOAPIC STAGE 3 READBACK: EXACT MATCH\n"
    );

    serial_write_string(
        "IOAPIC STAGE 3 MASK READBACK: VERIFIED\n"
    );

    serial_write_string(
        "IOAPIC STAGE 3 INTERRUPT DELIVERY: DISABLED\n"
    );

    serial_write_string(
        "IOAPIC REDIRECTION: VERIFIED\n"
    );

    /* --------------------------------------------------------
       HARDWARE IRQ / TIMER BRING-UP
       -------------------------------------------------------- */

    serial_write_string(
        "IRQ TIMER BRING-UP START\n"
    );

    /*
     * Initialize and remap the legacy 8259 PIC.
     *
     * Keep every IRQ masked while the interrupt subsystem
     * and PIT are being configured.
     */
    pic_init();

    for (uint8_t irq = 0; irq < IRQ_COUNT; irq++)
    {
        pic_set_mask(irq);
    }

    /*
     * Register BATOS IRQ handlers.
     *
     * IRQ0 is handled by the timer handler in irq.c.
     */
    irq_init();

    /*
     * Stage 4: verify that the live IRQ0 source is explicitly
     * assigned to the legacy PIC controller.
     *
     * APIC delivery remains disabled until the controlled
     * migration stage.
     */
    if (irq_get_controller(0) == IRQ_CONTROLLER_PIC)
    {
        serial_write_string(
            "IRQ CONTROLLER IRQ0: PIC\n"
        );

        serial_write_string(
            "IRQ CONTROLLER DISPATCH: VERIFIED\n"
        );
    }
    else
    {
        serial_write_string(
            "IRQ CONTROLLER IRQ0: FAILED\n"
        );
    }

    /*
     * Program PIT channel 0 for 100 Hz.
     */
    pit_init(100);

    /*
     * Initialize the system timekeeping layer.
     *
     * The initial reference source is the PIT at 100 Hz.
     * Hardware IRQ0 will advance the timekeeping tick.
     */
    time_init(100);

    /*
     * Initialize the clock-event abstraction.
     *
     * The current hardware source remains the PIT at 100 Hz.
     * IRQ0 delivery is routed through the IOAPIC/LAPIC path,
     * while the clock-event layer decouples the hardware source
     * from system timekeeping.
     */
    if (clock_event_init(
            CLOCK_EVENT_SOURCE_PIT,
            100
        ) != 0)
    {
        serial_write_string(
            "CLOCK EVENT INIT: FAILED\n"
        );

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "CLOCK EVENT SOURCE: PIT\n"
    );

    serial_write_string(
        "CLOCK EVENT: READY\n"
    );

    /* --------------------------------------------------------
       STAGE 5: CONTROLLED IRQ0 MIGRATION TO LAPIC
       --------------------------------------------------------

       PIT IRQ0 is migrated from:

           PIT -> PIC -> vector 32 -> irq_dispatch()

       to:

           PIT -> IRQ0 -> MADT ISO -> GSI
               -> IOAPIC -> LAPIC -> vector 32
               -> irq_stub_0 -> irq_dispatch()
               -> LAPIC EOI

       PIC IRQ0 remains masked throughout the transition.
       The IOAPIC entry is programmed and verified while
       masked, controller ownership is switched to LAPIC,
       and only then is IOAPIC delivery enabled.
       -------------------------------------------------------- */

    serial_write_string(
        "IOAPIC STAGE 5: IRQ0 LAPIC MIGRATION START\n"
    );

    /*
     * Block CPU interrupt delivery while controller ownership
     * and IOAPIC routing are changed.
     */
    __asm__ volatile (
        "cli"
        :
        :
        : "memory"
    );

    /*
     * Keep the legacy PIC IRQ0 masked.
     */
    pic_set_mask(0);

    serial_write_string(
        "IOAPIC STAGE 5 PIC IRQ0: MASKED\n"
    );

    uint32_t stage5_ioapic_index =
        gsi_irq0_route.ioapic_index;

    uint8_t stage5_redirection_index =
        (uint8_t)(
            gsi_irq0_route.ioapic_redirection_index
        );

    uint8_t stage5_lapic_id =
        (uint8_t)(lapic_id >> 24);

    /*
     * Program vector 32 so the existing irq_stub_0 path
     * remains unchanged.
     */
    uint64_t stage5_expected =
        IOAPIC_IRQ0_VECTOR |
        IOAPIC_REDIR_DELIVERY_FIXED |
        IOAPIC_REDIR_MASKED |
        ((uint64_t)stage5_lapic_id << 56);

    /*
     * Preserve the electrical characteristics resolved
     * from the ACPI MADT interrupt source override.
     */
    if (gsi_irq0_route.polarity == GSI_POLARITY_LOW)
    {
        stage5_expected |= IOAPIC_REDIR_POLARITY_LOW;
    }
    else if (gsi_irq0_route.polarity != GSI_POLARITY_HIGH)
    {
        serial_write_string(
            "IOAPIC STAGE 5 POLARITY: FAILED\n"
        );
        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    if (gsi_irq0_route.trigger_mode == GSI_TRIGGER_LEVEL)
    {
        stage5_expected |= IOAPIC_REDIR_TRIGGER_LEVEL;
    }
    else if (gsi_irq0_route.trigger_mode != GSI_TRIGGER_EDGE)
    {
        serial_write_string(
            "IOAPIC STAGE 5 TRIGGER: FAILED\n"
        );
        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "IOAPIC STAGE 5 ROUTE: IRQ0 -> GSI="
    );
    serial_write_hex(
        (uint64_t)gsi_irq0_route.gsi
    );
    serial_write_string(
        " -> IOAPIC="
    );
    serial_write_hex(
        (uint64_t)stage5_ioapic_index
    );
    serial_write_string(
        " -> REDIR="
    );
    serial_write_hex(
        (uint64_t)stage5_redirection_index
    );
    serial_write_string("\n");

    serial_write_string(
        "IOAPIC STAGE 5 VECTOR: "
    );
    serial_write_hex(
        (uint64_t)IOAPIC_IRQ0_VECTOR
    );
    serial_write_string("\n");

    serial_write_string(
        "IOAPIC STAGE 5 LAPIC DESTINATION: "
    );
    serial_write_hex(
        (uint64_t)stage5_lapic_id
    );
    serial_write_string("\n");

    serial_write_string(
        "IOAPIC STAGE 5 EXPECTED MASKED: "
    );
    serial_write_hex(stage5_expected);
    serial_write_string("\n");

    /*
     * Program the IOAPIC entry while it is still masked.
     */
    int stage5_write_result =
        ioapic_write_redirection_at(
            stage5_ioapic_index,
            stage5_redirection_index,
            stage5_expected
        );

    if (stage5_write_result != 0)
    {
        serial_write_string(
            "IOAPIC STAGE 5 WRITE: FAILED\n"
        );
        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    uint64_t stage5_actual = 0;

    int stage5_read_result =
        ioapic_read_redirection_at(
            stage5_ioapic_index,
            stage5_redirection_index,
            &stage5_actual
        );

    if (stage5_read_result != 0)
    {
        serial_write_string(
            "IOAPIC STAGE 5 READBACK: FAILED\n"
        );
        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "IOAPIC STAGE 5 ACTUAL MASKED: "
    );
    serial_write_hex(stage5_actual);
    serial_write_string("\n");

    if (stage5_actual != stage5_expected ||
        (stage5_actual & IOAPIC_REDIR_MASKED) == 0)
    {
        serial_write_string(
            "IOAPIC STAGE 5 MASKED READBACK: FAILED\n"
        );
        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "IOAPIC STAGE 5 MASKED READBACK: VERIFIED\n"
    );

    /*
     * Switch software IRQ ownership BEFORE unmasking the
     * IOAPIC entry.
     */
    if (irq_set_controller(
            0,
            IRQ_CONTROLLER_LAPIC
        ) != 0)
    {
        serial_write_string(
            "IOAPIC STAGE 5 CONTROLLER SWITCH: FAILED\n"
        );
        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    if (irq_get_controller(0) != IRQ_CONTROLLER_LAPIC)
    {
        serial_write_string(
            "IOAPIC STAGE 5 CONTROLLER VERIFY: FAILED\n"
        );
        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "IOAPIC STAGE 5 CONTROLLER: LAPIC\n"
    );

    /*
     * Remove ONLY the mask bit.
     */
    uint64_t stage5_unmasked =
        stage5_expected &
        ~IOAPIC_REDIR_MASKED;

    if (ioapic_write_redirection_at(
            stage5_ioapic_index,
            stage5_redirection_index,
            stage5_unmasked
        ) != 0)
    {
        serial_write_string(
            "IOAPIC STAGE 5 UNMASK: WRITE FAILED\n"
        );
        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    uint64_t stage5_unmasked_actual = 0;

    if (ioapic_read_redirection_at(
            stage5_ioapic_index,
            stage5_redirection_index,
            &stage5_unmasked_actual
        ) != 0)
    {
        serial_write_string(
            "IOAPIC STAGE 5 UNMASK: READ FAILED\n"
        );
        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    if (stage5_unmasked_actual != stage5_unmasked ||
        (stage5_unmasked_actual & IOAPIC_REDIR_MASKED) != 0)
    {
        serial_write_string(
            "IOAPIC STAGE 5 UNMASK READBACK: FAILED\n"
        );
        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "IOAPIC STAGE 5 UNMASK READBACK: VERIFIED\n"
    );

    serial_write_string(
        "IOAPIC STAGE 5 INTERRUPT DELIVERY: ENABLED\n"
    );

    serial_write_string(
        "IOAPIC STAGE 5: LAPIC IRQ0 PATH ARMED\n"
    );

    serial_write_string(
        "PIT 100HZ READY\n"
    );

    uint64_t start_ticks = irq_get_ticks();
    uint64_t start_time_ticks = time_get_ticks();
    uint64_t start_uptime_ms = time_get_uptime_ms();

    serial_write_string(
        "IRQ0 TEST WAITING\n"
    );

    serial_write_string(
        "TIMEKEEPING START TICKS: "
    );
    serial_write_hex(start_time_ticks);
    serial_write_string("\n");

    serial_write_string(
        "TIMEKEEPING START UPTIME MS: "
    );
    serial_write_hex(start_uptime_ms);
    serial_write_string("\n");

    /*
     * Enable maskable hardware interrupts only after
     * PIC, IRQ handlers and PIT are completely ready.
     */
    __asm__ volatile (
        "sti"
        :
        :
        : "memory"
    );

    /*
     * Wait for 100 real IRQ0 timer ticks.
     *
     * At 100 Hz this should take approximately one second.
     */
    while (irq_get_ticks() < start_ticks + 100)
    {
        __asm__ volatile (
            "hlt"
            :
            :
            : "memory"
        );
    }

    /*
     * Stop maskable interrupts before reporting the result.
     */
    __asm__ volatile (
        "cli"
        :
        :
        : "memory"
    );

    uint64_t end_ticks = irq_get_ticks();
    uint64_t end_time_ticks = time_get_ticks();
    uint64_t end_uptime_ms = time_get_uptime_ms();

    serial_write_string(
        "IRQ0 TEST START TICKS: "
    );
    serial_write_hex(start_ticks);
    serial_write_string("\n");

    serial_write_string(
        "IRQ0 TEST END TICKS: "
    );
    serial_write_hex(end_ticks);
    serial_write_string("\n");

    serial_write_string(
        "TIMEKEEPING END TICKS: "
    );
    serial_write_hex(end_time_ticks);
    serial_write_string("\n");

    serial_write_string(
        "TIMEKEEPING END UPTIME MS: "
    );
    serial_write_hex(end_uptime_ms);
    serial_write_string("\n");

    if (end_ticks >= start_ticks + 100 &&
        end_time_ticks >= start_time_ticks + 100 &&
        end_uptime_ms >= start_uptime_ms + 1000)
    {
        serial_write_string(
            "IRQ0 TIMER: VERIFIED\n"
        );

        serial_write_string(
            "HARDWARE INTERRUPTS: VERIFIED\n"
        );
    }
    else
    {
        serial_write_string(
            "IRQ0 TIMER: FAILED\n"
        );

        serial_write_string(
            "HARDWARE INTERRUPTS: FAILED\n"
        );
    }

    /* --------------------------------------------------------
       TIMER-1 SOFTWARE TIMER SUBSYSTEM
       -------------------------------------------------------- */


    /* --------------------------------------------------------
       TIMER-4: LAPIC TIMER FREQUENCY CALIBRATION
       --------------------------------------------------------

       The PIT remains the temporary reference clock.

       Calibration interval:
           PIT = 100 Hz
           20 PIT ticks = 200 ms

       LAPIC timer:
           one-shot
           masked
           divide-by-16
           initial count = 0xFFFFFFFF

       The LAPIC counter decrement over the known PIT
       interval gives the measured LAPIC timer frequency.
       -------------------------------------------------------- */

    serial_write_string(
        "LAPIC TIMER CALIBRATION START\n"
    );

    /*
     * Ensure the previous LAPIC timer bring-up countdown
     * cannot participate in calibration.
     */
    if (lapic_timer_stop() != 0)
    {
        serial_write_string(
            "LAPIC TIMER CALIBRATION STOP: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    const uint32_t lapic_calibration_initial =
        0xFFFFFFFFU;

    /*
     * Configure a fresh masked one-shot countdown.
     * Interrupt delivery remains disabled during the
     * measurement interval.
     */
    if (lapic_timer_init(
            lapic_calibration_initial
        ) != 0)
    {
        serial_write_string(
            "LAPIC TIMER CALIBRATION INIT: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    uint32_t lapic_calibration_start_count =
        lapic_timer_get_current_count();

    uint64_t lapic_calibration_start_ticks =
        irq_get_ticks();

    serial_write_string(
        "LAPIC CALIBRATION START COUNT: "
    );
    serial_write_hex(
        lapic_calibration_start_count
    );
    serial_write_string("\n");

    serial_write_string(
        "LAPIC CALIBRATION START PIT TICKS: "
    );
    serial_write_hex(
        lapic_calibration_start_ticks
    );
    serial_write_string("\n");

    /*
     * Enable only the already-proven PIT reference path.
     * The LAPIC timer remains masked.
     */
    __asm__ volatile (
        "sti"
        :
        :
        : "memory"
    );

    const uint64_t lapic_calibration_reference_ticks =
        20;

    while (
        irq_get_ticks() <
        lapic_calibration_start_ticks +
        lapic_calibration_reference_ticks
    )
    {
        __asm__ volatile (
            "hlt"
            :
            :
            : "memory"
        );
    }

    /*
     * Stop interrupt delivery before reading the final
     * calibration state.
     */
    __asm__ volatile (
        "cli"
        :
        :
        : "memory"
    );

    uint32_t lapic_calibration_end_count =
        lapic_timer_get_current_count();

    uint64_t lapic_calibration_end_ticks =
        irq_get_ticks();

    serial_write_string(
        "LAPIC CALIBRATION END COUNT: "
    );
    serial_write_hex(
        lapic_calibration_end_count
    );
    serial_write_string("\n");

    serial_write_string(
        "LAPIC CALIBRATION END PIT TICKS: "
    );
    serial_write_hex(
        lapic_calibration_end_ticks
    );
    serial_write_string("\n");

    /*
     * The timer must still be counting down. Reaching zero
     * would mean the selected calibration interval was too
     * long for the chosen initial count.
     */
    if (lapic_calibration_end_count == 0 ||
        lapic_calibration_end_count >=
            lapic_calibration_start_count ||
        lapic_calibration_end_ticks <=
            lapic_calibration_start_ticks)
    {
        serial_write_string(
            "LAPIC TIMER CALIBRATION: FAILED\n"
        );

        lapic_timer_stop();

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    uint64_t lapic_calibration_elapsed_count =
        (uint64_t)lapic_calibration_start_count -
        (uint64_t)lapic_calibration_end_count;

    uint64_t lapic_calibration_elapsed_pit_ticks =
        lapic_calibration_end_ticks -
        lapic_calibration_start_ticks;

    /*
     * PIT reference frequency is exactly the configured
     * 100 Hz clock-event frequency.
     *
     * Measured LAPIC frequency:
     *
     *     delta_count * PIT_HZ
     *     -------------------
     *       elapsed_ticks
     */
    const uint64_t lapic_calibration_pit_hz =
        100ULL;

    uint64_t lapic_timer_frequency =
        (
            lapic_calibration_elapsed_count *
            lapic_calibration_pit_hz
        ) /
        lapic_calibration_elapsed_pit_ticks;

    if (lapic_timer_frequency == 0 ||
        lapic_timer_frequency > 0xFFFFFFFFULL)
    {
        serial_write_string(
            "LAPIC TIMER FREQUENCY: INVALID\n"
        );

        lapic_timer_stop();

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    /*
     * Calculate the reload required for a 100 Hz periodic
     * LAPIC clock event.
     *
     * Rounded rather than truncated.
     */
    uint64_t lapic_timer_reload =
        (
            lapic_timer_frequency +
            50ULL
        ) /
        100ULL;

    if (lapic_timer_reload == 0 ||
        lapic_timer_reload > 0xFFFFFFFFULL)
    {
        serial_write_string(
            "LAPIC TIMER RELOAD: INVALID\n"
        );

        lapic_timer_stop();

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "LAPIC CALIBRATION ELAPSED COUNT: "
    );
    serial_write_hex(
        lapic_calibration_elapsed_count
    );
    serial_write_string("\n");

    serial_write_string(
        "LAPIC CALIBRATION ELAPSED PIT TICKS: "
    );
    serial_write_hex(
        lapic_calibration_elapsed_pit_ticks
    );
    serial_write_string("\n");

    serial_write_string(
        "LAPIC TIMER MEASURED FREQUENCY: "
    );
    serial_write_hex(
        lapic_timer_frequency
    );
    serial_write_string("\n");

    serial_write_string(
        "LAPIC TIMER 100HZ RELOAD: "
    );
    serial_write_hex(
        lapic_timer_reload
    );
    serial_write_string("\n");

    /*
     * Calibration is complete. Do not switch the clock-event
     * source yet; that is the next controlled step.
     */
    if (lapic_timer_stop() != 0)
    {
        serial_write_string(
            "LAPIC TIMER CALIBRATION CLEANUP: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "LAPIC TIMER CALIBRATION: VERIFIED\n"
    );

    /*
     * --------------------------------------------------------
     * TIMER-4: LAPIC TIMER CLOCK-SOURCE MIGRATION
     *
     * The calibrated LAPIC timer becomes the active system
     * clock-event source at 100 Hz.
     *
     * The PIT hardware is kept intact for now, but its IRQ0
     * IOAPIC delivery is masked so it can no longer generate
     * system clock events.
     *
     * Ordering:
     *
     *   1. CLI
     *   2. Configure LAPIC periodic timer while masked
     *   3. Switch clock-event ownership to LAPIC
     *   4. Mask PIT IRQ0 at IOAPIC
     *   5. Verify PIT IRQ0 is masked
     *   6. Unmask LAPIC timer
     *   7. STI
     *
     * No PIT removal or hardware shutdown is performed here.
     * --------------------------------------------------------
     */

    serial_write_string(
        "TIMER-4 LAPIC CLOCK MIGRATION START\n"
    );

    __asm__ volatile (
        "cli"
        :
        :
        : "memory"
    );

    /*
     * Program the calibrated LAPIC timer in periodic mode.
     * The timer remains masked until the complete transition
     * has been verified.
     */
    if (lapic_timer_configure_periodic(
            (uint32_t)lapic_timer_reload
        ) != 0)
    {
        serial_write_string(
            "TIMER-4 LAPIC PERIODIC CONFIG: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMER-4 LAPIC PERIODIC CONFIG: VERIFIED\n"
    );

    /*
     * Switch the clock-event abstraction to the calibrated
     * LAPIC source. Timekeeping ticks are intentionally
     * preserved across the source transition.
     */
    if (clock_event_init(
            CLOCK_EVENT_SOURCE_LAPIC,
            100
        ) != 0)
    {
        serial_write_string(
            "TIMER-4 CLOCK SOURCE SWITCH: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    if (clock_event_get_source() !=
            CLOCK_EVENT_SOURCE_LAPIC ||
        clock_event_get_frequency() != 100)
    {
        serial_write_string(
            "TIMER-4 CLOCK SOURCE VERIFY: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMER-4 CLOCK SOURCE: LAPIC 100HZ\n"
    );

    /*
     * Mask PIT IRQ0 at the IOAPIC.
     *
     * Preserve the complete existing routing entry and change
     * only the mask bit. The ACPI-resolved GSI route is reused.
     */
    uint64_t timer4_pit_redirection = 0;

    if (ioapic_read_redirection_at(
            gsi_irq0_route.ioapic_index,
            (uint8_t)gsi_irq0_route.ioapic_redirection_index,
            &timer4_pit_redirection
        ) != 0)
    {
        serial_write_string(
            "TIMER-4 PIT IRQ0 READ: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    timer4_pit_redirection |= IOAPIC_REDIR_MASKED;

    if (ioapic_write_redirection_at(
            gsi_irq0_route.ioapic_index,
            (uint8_t)gsi_irq0_route.ioapic_redirection_index,
            timer4_pit_redirection
        ) != 0)
    {
        serial_write_string(
            "TIMER-4 PIT IRQ0 MASK: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    uint64_t timer4_pit_readback = 0;

    if (ioapic_read_redirection_at(
            gsi_irq0_route.ioapic_index,
            (uint8_t)gsi_irq0_route.ioapic_redirection_index,
            &timer4_pit_readback
        ) != 0)
    {
        serial_write_string(
            "TIMER-4 PIT IRQ0 READBACK: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    if ((timer4_pit_readback & IOAPIC_REDIR_MASKED) == 0)
    {
        serial_write_string(
            "TIMER-4 PIT IRQ0 MASK READBACK: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMER-4 PIT IRQ0: MASKED\n"
    );

    /*
     * Enable the already-programmed periodic LAPIC timer.
     */
    if (lapic_timer_set_masked(0) != 0)
    {
        serial_write_string(
            "TIMER-4 LAPIC TIMER UNMASK: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMER-4 LAPIC TIMER: UNMASKED\n"
    );

    __asm__ volatile (
        "sti"
        :
        :
        : "memory"
    );

    serial_write_string(
        "TIMER-4 LAPIC CLOCK SOURCE: ACTIVE\n"
    );

    serial_write_string(
        "TIMER-4 LAPIC CLOCK MIGRATION: ARMED\n"
    );

    serial_write_string(
        "TIMER-1 TEST START\n"
    );

    struct timer one_shot_timer;
    struct timer periodic_timer;

    timer_init(&one_shot_timer);
    timer_init(&periodic_timer);

    /*
     * Basic initialization.
     */
    if (timer_get_deadline(&one_shot_timer) != 0 ||
        timer_is_expired(&one_shot_timer) != 0)
    {
        serial_write_string(
            "TIMER-1 INIT: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMER-1 INIT: VERIFIED\n"
    );

    /*
     * Zero-delay one-shot must be rejected.
     */
    if (timer_start(&one_shot_timer, 0) == 0)
    {
        serial_write_string(
            "TIMER-1 ZERO DELAY: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    /*
     * Start a real one-shot timer for 5 PIT timekeeping ticks.
     */
    uint64_t one_shot_start =
        time_get_ticks();

    if (timer_start(&one_shot_timer, 5) != 0)
    {
        serial_write_string(
            "TIMER-1 ONE-SHOT START: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    uint64_t one_shot_deadline =
        timer_get_deadline(&one_shot_timer);

    if (one_shot_deadline !=
        one_shot_start + 5)
    {
        serial_write_string(
            "TIMER-1 ONE-SHOT DEADLINE: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    /*
     * It must not be expired immediately after starting.
     */
    if (timer_is_expired(&one_shot_timer) != 0)
    {
        serial_write_string(
            "TIMER-1 PRE-DEADLINE: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMER-1 ONE-SHOT START: VERIFIED\n"
    );

    /*
     * Wait until the real timekeeping clock reaches
     * the one-shot deadline.
     */
    while (time_get_ticks() < one_shot_deadline)
    {
        __asm__ volatile (
            "sti\n"
            "hlt\n"
            "cli"
            :
            :
            : "memory"
        );
    }

    if (timer_is_expired(&one_shot_timer) == 0)
    {
        serial_write_string(
            "TIMER-1 EXPIRY: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMER-1 EXPIRY: VERIFIED\n"
    );

    /*
     * Rearming a one-shot timer completes/deactivates it.
     */
    if (timer_rearm(&one_shot_timer) != 0 ||
        timer_is_expired(&one_shot_timer) != 0)
    {
        serial_write_string(
            "TIMER-1 ONE-SHOT REARM: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMER-1 ONE-SHOT REARM: VERIFIED\n"
    );

    /*
     * Periodic timer: first deadline must be one period
     * after the current clock.
     */
    uint64_t periodic_start =
        time_get_ticks();

    if (timer_start_periodic(&periodic_timer, 3) != 0)
    {
        serial_write_string(
            "TIMER-1 PERIODIC START: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    uint64_t periodic_deadline_1 =
        timer_get_deadline(&periodic_timer);

    if (periodic_deadline_1 !=
        periodic_start + 3)
    {
        serial_write_string(
            "TIMER-1 PERIODIC DEADLINE: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMER-1 PERIODIC START: VERIFIED\n"
    );

    /*
     * Wait for first periodic expiry.
     */
    while (time_get_ticks() < periodic_deadline_1)
    {
        __asm__ volatile (
            "sti\n"
            "hlt\n"
            "cli"
            :
            :
            : "memory"
        );
    }

    if (timer_is_expired(&periodic_timer) == 0)
    {
        serial_write_string(
            "TIMER-1 PERIODIC EXPIRY: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    /*
     * Rearm must advance exactly one period.
     */
    if (timer_rearm(&periodic_timer) != 0)
    {
        serial_write_string(
            "TIMER-1 PERIODIC REARM: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    uint64_t periodic_deadline_2 =
        timer_get_deadline(&periodic_timer);

    if (periodic_deadline_2 !=
        periodic_deadline_1 + 3 ||
        timer_is_expired(&periodic_timer) != 0)
    {
        serial_write_string(
            "TIMER-1 PERIODIC REARM: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMER-1 PERIODIC REARM: VERIFIED\n"
    );

    /*
     * Verify a second periodic cycle.
     */
    while (time_get_ticks() < periodic_deadline_2)
    {
        __asm__ volatile (
            "sti\n"
            "hlt\n"
            "cli"
            :
            :
            : "memory"
        );
    }

    if (timer_is_expired(&periodic_timer) == 0)
    {
        serial_write_string(
            "TIMER-1 PERIODIC SECOND EXPIRY: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    if (timer_rearm(&periodic_timer) != 0)
    {
        serial_write_string(
            "TIMER-1 PERIODIC SECOND REARM: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMER-1 PERIODIC SECOND CYCLE: VERIFIED\n"
    );

    /*
     * Cancellation must make an active timer non-expiring.
     */
    timer_cancel(&periodic_timer);

    if (timer_is_expired(&periodic_timer) != 0)
    {
        serial_write_string(
            "TIMER-1 CANCEL: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMER-1 CANCEL: VERIFIED\n"
    );

    /*
     * Zero-period periodic timer must be rejected.
     */
    if (timer_start_periodic(&periodic_timer, 0) == 0)
    {
        serial_write_string(
            "TIMER-1 ZERO PERIOD: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMER-1 INVALID INPUTS: VERIFIED\n"
    );

    serial_write_string(
        "TIMER-1 SOFTWARE TIMER: VERIFIED\n"
    );

    /* --------------------------------------------------------
       TIMER-2 TIMER MANAGER / EXPIRY ENGINE
       -------------------------------------------------------- */

    serial_write_string(
        "TIMER-2 TEST START\n"
    );

    struct timer manager_one_shot;
    struct timer manager_periodic;

    timer_manager_init();

    /*
     * A fresh manager must start empty.
     */
    if (timer_manager_get_count() != 0)
    {
        serial_write_string(
            "TIMER-2 INIT: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMER-2 INIT: VERIFIED\n"
    );

    /*
     * NULL registration must be rejected.
     */
    if (timer_manager_add(0) == 0)
    {
        serial_write_string(
            "TIMER-2 NULL ADD: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    /*
     * Prepare a real one-shot timer.
     */
    timer_init(&manager_one_shot);

    if (timer_start(&manager_one_shot, 3) != 0)
    {
        serial_write_string(
            "TIMER-2 ONE-SHOT START: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    /*
     * Registration must succeed exactly once.
     */
    if (timer_manager_add(&manager_one_shot) != 0)
    {
        serial_write_string(
            "TIMER-2 ONE-SHOT ADD: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    if (timer_manager_get_count() != 1)
    {
        serial_write_string(
            "TIMER-2 COUNT AFTER ADD: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    /*
     * Duplicate registration must be rejected.
     */
    if (timer_manager_add(&manager_one_shot) == 0)
    {
        serial_write_string(
            "TIMER-2 DUPLICATE ADD: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMER-2 REGISTRATION: VERIFIED\n"
    );

    /*
     * Before the deadline, the manager must report
     * no expired timers.
     */
    if (timer_manager_process() != 0)
    {
        serial_write_string(
            "TIMER-2 PRE-EXPIRY: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    /*
     * Wait for the actual PIT timekeeping clock.
     */
    uint64_t manager_one_shot_deadline =
        timer_get_deadline(&manager_one_shot);

    while (time_get_ticks() <
           manager_one_shot_deadline)
    {
        __asm__ volatile (
            "sti\n"
            "hlt\n"
            "cli"
            :
            :
            : "memory"
        );
    }

    /*
     * The expired one-shot must be processed exactly once
     * and must become inactive.
     */
    if (timer_manager_process() != 1)
    {
        serial_write_string(
            "TIMER-2 ONE-SHOT EXPIRY: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    if (timer_is_expired(&manager_one_shot) != 0)
    {
        serial_write_string(
            "TIMER-2 ONE-SHOT DEACTIVATE: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    /*
     * A processed one-shot must not fire again.
     */
    if (timer_manager_process() != 0)
    {
        serial_write_string(
            "TIMER-2 ONE-SHOT REPEAT: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMER-2 ONE-SHOT: VERIFIED\n"
    );

    /*
     * Remove the one-shot and verify manager accounting.
     */
    if (timer_manager_remove(&manager_one_shot) != 0 ||
        timer_manager_get_count() != 0)
    {
        serial_write_string(
            "TIMER-2 REMOVE: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMER-2 REMOVE: VERIFIED\n"
    );

    /*
     * Prepare a periodic timer.
     */
    timer_init(&manager_periodic);

    if (timer_start_periodic(&manager_periodic, 2) != 0)
    {
        serial_write_string(
            "TIMER-2 PERIODIC START: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    if (timer_manager_add(&manager_periodic) != 0 ||
        timer_manager_get_count() != 1)
    {
        serial_write_string(
            "TIMER-2 PERIODIC ADD: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    /*
     * First periodic cycle.
     */
    uint64_t manager_periodic_deadline_1 =
        timer_get_deadline(&manager_periodic);

    while (time_get_ticks() <
           manager_periodic_deadline_1)
    {
        __asm__ volatile (
            "sti\n"
            "hlt\n"
            "cli"
            :
            :
            : "memory"
        );
    }

    if (timer_manager_process() != 1)
    {
        serial_write_string(
            "TIMER-2 PERIODIC EXPIRY: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    uint64_t manager_periodic_deadline_2 =
        timer_get_deadline(&manager_periodic);

    if (manager_periodic_deadline_2 !=
        manager_periodic_deadline_1 + 2)
    {
        serial_write_string(
            "TIMER-2 PERIODIC REARM: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMER-2 PERIODIC REARM: VERIFIED\n"
    );

    /*
     * Second periodic cycle proves that the manager can
     * continue processing the same registered timer.
     */
    while (time_get_ticks() <
           manager_periodic_deadline_2)
    {
        __asm__ volatile (
            "sti\n"
            "hlt\n"
            "cli"
            :
            :
            : "memory"
        );
    }

    if (timer_manager_process() != 1)
    {
        serial_write_string(
            "TIMER-2 PERIODIC SECOND EXPIRY: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    /*
     * Cancel the periodic timer and remove it from the
     * manager. No registered timers should remain.
     */
    timer_cancel(&manager_periodic);

    if (timer_manager_remove(&manager_periodic) != 0 ||
        timer_manager_get_count() != 0)
    {
        serial_write_string(
            "TIMER-2 PERIODIC REMOVE: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "TIMER-2 PERIODIC: VERIFIED\n"
    );

    serial_write_string(
        "TIMER-2 TIMER MANAGER: VERIFIED\n"
    );

    /* --------------------------------------------------------
       TIMER-4 LAPIC CLOCK-SOURCE VERIFICATION
       -------------------------------------------------------- */

    serial_write_string(
        "TIMER-4 LAPIC CLOCK VERIFICATION START\n"
    );

    /*
     * The LAPIC timer is already configured by Timer-4 as
     * a periodic 100 Hz clock source.
     *
     * Do not reprogram, stop, or mask it here.
     * This test observes the live clock path.
     */

    uint32_t timer4_lvt =
        lapic_read(
            LAPIC_REG_LVT_TIMER
        );

    if ((timer4_lvt & 0xFFU) !=
        LAPIC_LVT_TIMER_VECTOR)
    {
        serial_write_string(
            "TIMER-4 LAPIC LVT VECTOR: FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    if ((timer4_lvt & LAPIC_LVT_TIMER_PERIODIC) == 0)
    {
        serial_write_string(
            "TIMER-4 LAPIC MODE: NOT PERIODIC\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    if ((timer4_lvt & LAPIC_LVT_TIMER_MASK) != 0)
    {
        serial_write_string(
            "TIMER-4 LAPIC MASK: FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "TIMER-4 LAPIC LVT: PERIODIC + UNMASKED\n"
    );

    if (clock_event_get_source() !=
        CLOCK_EVENT_SOURCE_LAPIC ||
        clock_event_get_frequency() != 100)
    {
        serial_write_string(
            "TIMER-4 CLOCK-EVENT STATE: FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "TIMER-4 CLOCK-EVENT SOURCE: LAPIC 100HZ\n"
    );

    uint64_t timer4_interrupts_before =
        lapic_timer_get_interrupt_count();

    uint64_t timer4_clock_events_before =
        clock_event_get_count();

    uint64_t timer4_time_ticks_before =
        time_get_ticks();

    /*
     * Enable interrupts and wait for the live periodic
     * LAPIC timer to deliver at least one clock event.
     */
    __asm__ volatile (
        "sti"
        :
        :
        : "memory"
    );

    while (lapic_timer_get_interrupt_count() ==
           timer4_interrupts_before)
    {
        __asm__ volatile (
            "hlt"
            :
            :
            : "memory"
        );
    }

    /*
     * Stop maskable interrupts while validating the
     * resulting clock/time state.
     */
    __asm__ volatile (
        "cli"
        :
        :
        : "memory"
    );

    uint64_t timer4_interrupts_after =
        lapic_timer_get_interrupt_count();

    uint64_t timer4_clock_events_after =
        clock_event_get_count();

    uint64_t timer4_time_ticks_after =
        time_get_ticks();

    if (timer4_interrupts_after <=
        timer4_interrupts_before)
    {
        serial_write_string(
            "TIMER-4 LAPIC INTERRUPT PROGRESSION: FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    if (timer4_clock_events_after <=
        timer4_clock_events_before)
    {
        serial_write_string(
            "TIMER-4 CLOCK-EVENT PROGRESSION: FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    if (timer4_time_ticks_after <=
        timer4_time_ticks_before)
    {
        serial_write_string(
            "TIMER-4 TIMEKEEPING PROGRESSION: FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    /*
     * Re-read the LVT after real interrupt delivery.
     * The periodic clock source must remain active.
     */
    uint32_t timer4_final_lvt =
        lapic_read(
            LAPIC_REG_LVT_TIMER
        );

    if ((timer4_final_lvt & 0xFFU) !=
            LAPIC_LVT_TIMER_VECTOR ||
        (timer4_final_lvt &
            LAPIC_LVT_TIMER_PERIODIC) == 0 ||
        (timer4_final_lvt &
            LAPIC_LVT_TIMER_MASK) != 0)
    {
        serial_write_string(
            "TIMER-4 LAPIC FINAL STATE: FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "TIMER-4 LAPIC INTERRUPT PROGRESSION: VERIFIED\n"
    );

    serial_write_string(
        "TIMER-4 CLOCK-EVENT PROGRESSION: VERIFIED\n"
    );

    serial_write_string(
        "TIMER-4 TIMEKEEPING PROGRESSION: VERIFIED\n"
    );

    serial_write_string(
        "TIMER-4 LAPIC FINAL STATE: PERIODIC + UNMASKED\n"
    );

    serial_write_string(
        "TIMER-4 LAPIC CLOCK SOURCE: VERIFIED\n"
    );

    /* --------------------------------------------------------
       FINAL HALT
       -------------------------------------------------------- */

    serial_write_string(
        "CPU HALTED\n"
    );

    for (;;)
    {
        __asm__ volatile (
            "hlt"
        );
    }
}
