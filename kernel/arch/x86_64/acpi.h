#ifndef BATOS_ACPI_H
#define BATOS_ACPI_H

#include <stdint.h>

int acpi_init(void);

uint8_t acpi_get_rsdp_revision(void);
uint64_t acpi_get_rsdp_address(void);

#endif
