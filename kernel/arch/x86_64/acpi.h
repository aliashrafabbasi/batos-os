#ifndef BATOS_ACPI_H
#define BATOS_ACPI_H

#include <stdint.h>

int acpi_init(void);

uint8_t acpi_get_rsdp_revision(void);
uint64_t acpi_get_rsdp_address(void);

uint64_t acpi_get_root_table_address(void);
uint32_t acpi_get_root_table_count(void);
uint8_t acpi_root_table_is_xsdt(void);

uint64_t acpi_get_madt_address(void);

#endif
