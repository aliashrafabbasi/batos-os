#include <stdint.h>
#include <stddef.h>

#include "kernel/boot/boot.h"
#include "kernel/platform/platform.h"
#include "kernel/arch/x86_64/cpu/cpu.h"
#include "kernel/arch/x86_64/cpu/tss.h"
#include "kernel/mm/pmm/pmm.h"
#include "kernel/mm/vmm/vmm.h"
#include "kernel/arch/x86_64/interrupt/idt.h"
#include "kernel/arch/x86_64/cpu/exception.h"
#include "kernel/arch/x86_64/interrupt/pic.h"
#include "kernel/arch/x86_64/interrupt/irq.h"
#include "kernel/mm/heap/heap.h"
#include "kernel/console/console.h"
#include "kernel/tests/heap_tests.h"
#include "kernel/tests/memory_tests.h"
#include "kernel/tests/interrupt_tests.h"
#include "kernel/tests/timer_tests.h"
#include "kernel/tests/timer_bringup_tests.h"
#include "kernel/tests/acpi_tests.h"

void kernel_main(void)
{
    boot_init();

    /*
     * CPU architecture initialization continues below.
     */

    /* --------------------------------------------------------
       CPU
       -------------------------------------------------------- */

    cpu_init();

    serial_write_string(
        "GDT READY\n"
    );

    serial_write_string(
        "TSS LOADED\n"
    );

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

    int platform_result = platform_init();

    if (platform_result != 0)
    {
        serial_write_string(
            "PLATFORM INITIALIZATION FAILED\n"
        );

        serial_write_string(
            "PLATFORM ERROR CODE: "
        );

        serial_write_hex(
            (uint64_t)(uint32_t)(-platform_result)
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

    acpi_tests_run();

    memory_tests_run();

    heap_test_dynamic_kernel_page();
    heap_test_bootstrap();
    heap_test_dynamic_page_ownership();

    interrupt_tests_lapic_bringup();

    timer_bringup_tests_run();

    /*
     * Timer subsystem verification.
     *
     * Run after the complete LAPIC clock-source migration so
     * Timer-4 observes the live periodic LAPIC clock.
     */
    timer_tests_run();

    serial_write_string(
        "TIMER-4 LAPIC CLOCK MIGRATION: ARMED\n"
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
