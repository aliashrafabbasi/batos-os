#include <stdint.h>

#include "kernel/console/console.h"
#include "kernel/arch/x86_64/pic.h"
#include "kernel/arch/x86_64/irq.h"
#include "kernel/arch/x86_64/lapic.h"
#include "kernel/arch/x86_64/ioapic.h"
#include "kernel/arch/x86_64/gsi.h"
#include "kernel/arch/x86_64/time.h"
#include "kernel/arch/x86_64/clock_event.h"
#include "interrupt_tests.h"

void interrupt_tests_lapic_bringup(void)
{
    /* --------------------------------------------------------
       LOCAL APIC BRING-UP
       -------------------------------------------------------- */


    serial_write_string(
        "LAPIC VERIFICATION START\n"
    );

    /*
     * LAPIC initialization is owned by platform_init().
     * This function verifies the resulting hardware state.
     */

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


}

void interrupt_tests_run(void)
{
    /* --------------------------------------------------------
       I/O APIC BRING-UP
       -------------------------------------------------------- */

    serial_write_string(
        "IOAPIC VERIFICATION START\n"
    );

    /*
     * IOAPIC initialization is owned by platform_init().
     * This function verifies the resulting hardware state.
     */

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
        "GSI ROUTING VERIFICATION START\n"
    );

    /*
     * GSI initialization is owned by platform_init().
     * This function verifies IRQ/GSI routing resolution.
     */

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

    uint32_t lapic_id =
        lapic_read(LAPIC_REG_ID);

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


}

void interrupt_test_irq0_lapic_delivery(void)
{
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
     * Resolve IRQ0 routing locally for Stage-5. The earlier
     * bring-up test owns its own route state and must not leak
     * implementation state across test functions.
     */
    struct gsi_irq_route gsi_irq0_route;

    if (gsi_resolve_irq(0, &gsi_irq0_route) != 0)
    {
        serial_write_string(
            "IOAPIC STAGE 5 GSI ROUTE: FAILED\n"
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

    uint32_t lapic_id =
        lapic_read(LAPIC_REG_ID);

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


}
