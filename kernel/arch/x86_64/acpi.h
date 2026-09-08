#ifndef BATOS_ACPI_H
#define BATOS_ACPI_H

#include <stdint.h>

#define ACPI_MAX_MADT_LOCAL_APICS 256
#define ACPI_MAX_MADT_IO_APICS    256
#define ACPI_MAX_MADT_ISOS        256

struct acpi_madt_local_apic
{
    uint8_t processor_uid;
    uint8_t apic_id;
    uint32_t flags;
};

struct acpi_madt_io_apic
{
    uint8_t id;
    uint64_t address;
    uint32_t gsi_base;
};

struct acpi_madt_iso
{
    uint8_t bus;
    uint8_t source;
    uint32_t gsi;
    uint16_t flags;
};

int acpi_init(void);

uint8_t acpi_get_rsdp_revision(void);
uint64_t acpi_get_rsdp_address(void);

uint64_t acpi_get_root_table_address(void);
uint32_t acpi_get_root_table_count(void);
uint8_t acpi_root_table_is_xsdt(void);

uint64_t acpi_get_madt_address(void);

uint32_t acpi_get_madt_local_apic_address(void);
uint32_t acpi_get_madt_flags(void);

uint32_t acpi_get_madt_local_apic_count(void);
const struct acpi_madt_local_apic *
acpi_get_madt_local_apic(uint32_t index);

uint32_t acpi_get_madt_io_apic_count(void);
const struct acpi_madt_io_apic *
acpi_get_madt_io_apic(uint32_t index);

uint32_t acpi_get_madt_iso_count(void);
const struct acpi_madt_iso *
acpi_get_madt_iso(uint32_t index);

uint32_t acpi_get_madt_unknown_entry_count(void);

#endif
