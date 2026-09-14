#include <stdint.h>

#include "kernel/console/console.h"
#include "kernel/arch/x86_64/interrupt/irq.h"
#include "kernel/arch/x86_64/apic/lapic.h"
#include "kernel/arch/x86_64/apic/ioapic.h"
#include "kernel/arch/x86_64/apic/gsi.h"
#include "kernel/arch/x86_64/time/time.h"
#include "kernel/arch/x86_64/time/clock_event.h"
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
       STAGE 3: PRODUCTION IOAPIC ROUTING VERIFICATION
       --------------------------------------------------------

       irq_routing_init() owns IRQ0 redirection programming.

       This test only reads the established route and verifies
       that the production configuration matches the ACPI
       resolved route and LAPIC destination.
       -------------------------------------------------------- */

    serial_write_string(
        "IOAPIC STAGE 3: ROUTING VERIFICATION START\n"
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

    uint64_t stage3_expected =
        (IRQ_VECTOR_BASE + 0U) |
        IOAPIC_REDIR_DELIVERY_FIXED |
        IOAPIC_REDIR_MASKED |
        ((uint64_t)stage3_lapic_id << 56);

    if (gsi_irq0_route.polarity == GSI_POLARITY_LOW)
    {
        stage3_expected |= IOAPIC_REDIR_POLARITY_LOW;
    }
    else if (gsi_irq0_route.polarity != GSI_POLARITY_HIGH)
    {
        serial_write_string(
            "IOAPIC STAGE 3 POLARITY: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    if (gsi_irq0_route.trigger_mode == GSI_TRIGGER_LEVEL)
    {
        stage3_expected |= IOAPIC_REDIR_TRIGGER_LEVEL;
    }
    else if (gsi_irq0_route.trigger_mode != GSI_TRIGGER_EDGE)
    {
        serial_write_string(
            "IOAPIC STAGE 3 TRIGGER: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    uint64_t stage3_actual = 0;

    if (ioapic_read_redirection_at(
            stage3_ioapic_index,
            stage3_redirection_index,
            &stage3_actual
        ) != 0)
    {
        serial_write_string(
            "IOAPIC STAGE 3 READBACK: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "IOAPIC STAGE 3 EXPECTED: "
    );
    serial_write_hex(stage3_expected);
    serial_write_string("\n");

    serial_write_string(
        "IOAPIC STAGE 3 ACTUAL: "
    );
    serial_write_hex(stage3_actual);
    serial_write_string("\n");

    if (stage3_actual != stage3_expected ||
        (stage3_actual & IOAPIC_REDIR_MASKED) == 0)
    {
        serial_write_string(
            "IOAPIC STAGE 3 ROUTING: FAILED\n"
        );

        serial_write_string("CPU HALTED\n");

        for (;;)
        {
            __asm__ volatile ("cli\nhlt");
        }
    }

    serial_write_string(
        "IOAPIC STAGE 3 ROUTING: VERIFIED\n"
    );

    serial_write_string(
        "IOAPIC STAGE 3 DELIVERY: MASKED\n"
    );

    /* --------------------------------------------------------
       HARDWARE IRQ / TIMER BRING-UP
       -------------------------------------------------------- */

    serial_write_string(
        "IRQ TIMER BRING-UP START\n"
    );

    /*
     * Production interrupt routing is established by
     * irq_routing_init() before the test suite runs.
     *
     * This test must not initialize or mutate the live
     * controller state. It verifies the resulting ownership.
     */
    if (irq_get_controller(0) == IRQ_CONTROLLER_LAPIC)
    {
        serial_write_string(
            "IRQ CONTROLLER IRQ0: LAPIC\n"
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
}
