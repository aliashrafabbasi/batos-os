#include "acpi.h"

#include "../../../limine.h"

#define ACPI_RSDP_SIGNATURE "RSD PTR "

struct acpi_rsdp_v1
{
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
} __attribute__((packed));

struct acpi_rsdp_v2
{
    struct acpi_rsdp_v1 v1;

    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
} __attribute__((packed));

static volatile struct limine_rsdp_request
    limine_rsdp_request = {
        .id = LIMINE_RSDP_REQUEST_ID,
        .revision = 0,
        .response = 0
    };

static uint8_t rsdp_revision = 0;
static uint64_t rsdp_address = 0;

static uint8_t acpi_checksum(
    const uint8_t *data,
    uint64_t length
)
{
    uint8_t sum = 0;

    for (uint64_t i = 0; i < length; i++)
    {
        sum = (uint8_t)(sum + data[i]);
    }

    return sum;
}

int acpi_init(void)
{
    if (limine_rsdp_request.response == 0)
    {
        return -1;
    }

    if (limine_rsdp_request.response->address == 0)
    {
        return -2;
    }

    volatile struct acpi_rsdp_v1 *rsdp =
        (volatile struct acpi_rsdp_v1 *)
        limine_rsdp_request.response->address;

    for (uint64_t i = 0; i < 8; i++)
    {
        if (rsdp->signature[i] != ACPI_RSDP_SIGNATURE[i])
        {
            return -3;
        }
    }

    if (acpi_checksum(
            (const uint8_t *)rsdp,
            sizeof(struct acpi_rsdp_v1)) != 0)
    {
        return -4;
    }

    rsdp_revision = rsdp->revision;
    rsdp_address =
        (uint64_t)(uintptr_t)limine_rsdp_request.response->address;

    /*
     * ACPI 2.0+ RSDP contains an extended structure
     * and an additional checksum covering the complete
     * structure specified by its length field.
     */
    if (rsdp_revision >= 2)
    {
        volatile struct acpi_rsdp_v2 *rsdp_v2 =
            (volatile struct acpi_rsdp_v2 *)rsdp;

        uint32_t length = rsdp_v2->length;

        if (length < sizeof(struct acpi_rsdp_v2))
        {
            return -5;
        }

        if (acpi_checksum(
                (const uint8_t *)rsdp_v2,
                length) != 0)
        {
            return -6;
        }
    }

    return 0;
}

uint8_t acpi_get_rsdp_revision(void)
{
    return rsdp_revision;
}

uint64_t acpi_get_rsdp_address(void)
{
    return rsdp_address;
}
