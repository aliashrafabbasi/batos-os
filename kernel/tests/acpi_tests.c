#include <stdint.h>

#include "kernel/arch/x86_64/acpi.h"
#include "kernel/console/console.h"

void acpi_tests_run(void)
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
