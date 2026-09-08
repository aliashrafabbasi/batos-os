#include "acpi.h"

#include "pmm.h"

#include "../../../limine.h"

#define ACPI_RSDP_SIGNATURE "RSD PTR "
#define ACPI_MADT_SIGNATURE "APIC"

#define ACPI_RSDT_ENTRY_SIZE 4ULL
#define ACPI_XSDT_ENTRY_SIZE 8ULL

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

struct acpi_sdt_header
{
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed));

static volatile struct limine_rsdp_request
    limine_rsdp_request = {
        .id = LIMINE_RSDP_REQUEST_ID,
        .revision = 0,
        .response = 0
    };

static uint8_t rsdp_revision = 0;
static uint64_t rsdp_address = 0;

static uint64_t root_table_address = 0;
static uint32_t root_table_count = 0;
static uint8_t root_table_is_xsdt = 0;
static uint64_t madt_address = 0;

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

static int acpi_signature_equals(
    const volatile char *a,
    const char *b,
    uint64_t length
)
{
    for (uint64_t i = 0; i < length; i++)
    {
        if (a[i] != b[i])
        {
            return 0;
        }
    }

    return 1;
}

/*
 * Convert a physical address into the Limine HHDM virtual address.
 *
 * The HHDM offset belongs to PMM because PMM owns the Limine
 * memory-map/HHDM information.
 */
static volatile uint8_t *acpi_phys_to_virt(
    uint64_t physical,
    uint64_t length
)
{
    if (!pmm_is_physical_range_valid(
            physical,
            length))
    {
        return 0;
    }

    uint64_t hhdm = pmm_get_hhdm_offset();

    /*
     * Prevent unsigned overflow in physical + HHDM.
     */
    if (physical > UINT64_MAX - hhdm)
    {
        return 0;
    }

    /*
     * The complete requested range must also remain
     * representable after adding the HHDM offset.
     */
    if (length > UINT64_MAX - physical)
    {
        return 0;
    }

    uint64_t last_physical =
        physical + length - 1;

    if (last_physical > UINT64_MAX - hhdm)
    {
        return 0;
    }

    return (volatile uint8_t *)(uintptr_t)(physical + hhdm);
}

/*
 * Validate an ACPI System Description Table.
 *
 * Only the generic SDT header and complete-table checksum are
 * handled here. Individual table formats are parsed separately.
 */
static int acpi_validate_sdt(
    uint64_t physical_address,
    volatile struct acpi_sdt_header *header
)
{
    if (header == 0)
    {
        return -1;
    }

    /*
     * The header itself must be inside a valid physical
     * memory-map range before reading its length.
     */
    if (!pmm_is_physical_range_valid(
            physical_address,
            sizeof(struct acpi_sdt_header)))
    {
        return -2;
    }

    if (header->length < sizeof(struct acpi_sdt_header))
    {
        return -3;
    }

    /*
     * Validate the complete table before checksum/parsing.
     */
    if (!pmm_is_physical_range_valid(
            physical_address,
            (uint64_t)header->length))
    {
        return -4;
    }

    if (acpi_checksum(
            (const uint8_t *)(uintptr_t)header,
            header->length) != 0)
    {
        return -5;
    }

    return 0;
}

/*
 * Inspect a table referenced by RSDT/XSDT.
 *
 * Returns:
 *   0  valid table
 *  -1  invalid physical address conversion
 *  -2  invalid SDT
 */
static int acpi_inspect_table(
    uint64_t physical_address
)
{
    if (physical_address == 0)
    {
        return -1;
    }

    volatile struct acpi_sdt_header *header =
        (volatile struct acpi_sdt_header *)
        acpi_phys_to_virt(
            physical_address,
            sizeof(struct acpi_sdt_header));

    if (header == 0)
    {
        return -1;
    }

    if (acpi_validate_sdt(
            physical_address,
            header) != 0)
    {
        return -2;
    }

    if (acpi_signature_equals(
            header->signature,
            ACPI_MADT_SIGNATURE,
            4))
    {
        madt_address = physical_address;
    }

    return 0;
}

/*
 * Enumerate RSDT entries.
 *
 * RSDT entries contain 32-bit physical addresses.
 */
static int acpi_enumerate_rsdt(
    uint64_t physical_address,
    uint32_t *count
)
{
    volatile struct acpi_sdt_header *header =
        (volatile struct acpi_sdt_header *)
        acpi_phys_to_virt(
            physical_address,
            sizeof(struct acpi_sdt_header));

    if (header == 0)
    {
        return -1;
    }

    if (acpi_validate_sdt(
            physical_address,
            header) != 0)
    {
        return -2;
    }

    uint64_t payload_length =
        (uint64_t)header->length -
        sizeof(struct acpi_sdt_header);

    if ((payload_length % ACPI_RSDT_ENTRY_SIZE) != 0)
    {
        return -3;
    }

    uint32_t entry_count =
        (uint32_t)(payload_length / ACPI_RSDT_ENTRY_SIZE);

    volatile uint32_t *entries =
        (volatile uint32_t *)(
            (uintptr_t)header +
            sizeof(struct acpi_sdt_header)
        );

    uint32_t valid_count = 0;

    for (uint32_t i = 0; i < entry_count; i++)
    {
        uint64_t table_address =
            (uint64_t)entries[i];

        if (table_address == 0)
        {
            continue;
        }

        if (acpi_inspect_table(table_address) == 0)
        {
            valid_count++;
        }
    }

    *count = valid_count;

    return 0;
}

/*
 * Enumerate XSDT entries.
 *
 * XSDT entries contain 64-bit physical addresses.
 */
static int acpi_enumerate_xsdt(
    uint64_t physical_address,
    uint32_t *count
)
{
    volatile struct acpi_sdt_header *header =
        (volatile struct acpi_sdt_header *)
        acpi_phys_to_virt(
            physical_address,
            sizeof(struct acpi_sdt_header));

    if (header == 0)
    {
        return -1;
    }

    if (acpi_validate_sdt(
            physical_address,
            header) != 0)
    {
        return -2;
    }

    uint64_t payload_length =
        (uint64_t)header->length -
        sizeof(struct acpi_sdt_header);

    if ((payload_length % ACPI_XSDT_ENTRY_SIZE) != 0)
    {
        return -3;
    }

    uint32_t entry_count =
        (uint32_t)(payload_length / ACPI_XSDT_ENTRY_SIZE);

    volatile uint64_t *entries =
        (volatile uint64_t *)(
            (uintptr_t)header +
            sizeof(struct acpi_sdt_header)
        );

    uint32_t valid_count = 0;

    for (uint32_t i = 0; i < entry_count; i++)
    {
        uint64_t table_address = entries[i];

        if (table_address == 0)
        {
            continue;
        }

        if (acpi_inspect_table(table_address) == 0)
        {
            valid_count++;
        }
    }

    *count = valid_count;

    return 0;
}

int acpi_init(void)
{
    root_table_address = 0;
    root_table_count = 0;
    root_table_is_xsdt = 0;
    madt_address = 0;

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
            (const uint8_t *)(uintptr_t)rsdp,
            sizeof(struct acpi_rsdp_v1)) != 0)
    {
        return -4;
    }

    rsdp_revision = rsdp->revision;

    rsdp_address =
        (uint64_t)(uintptr_t)
        limine_rsdp_request.response->address;

    /*
     * ACPI 2.0+ RSDP contains an extended structure and
     * checksum covering the complete structure.
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
                (const uint8_t *)(uintptr_t)rsdp_v2,
                length) != 0)
        {
            return -6;
        }

        if (rsdp_v2->xsdt_address != 0)
        {
            root_table_address =
                rsdp_v2->xsdt_address;

            root_table_is_xsdt = 1;

            if (acpi_enumerate_xsdt(
                    root_table_address,
                    &root_table_count) != 0)
            {
                return -7;
            }

            return 0;
        }
    }

    /*
     * ACPI 1.x uses RSDT.
     *
     * For ACPI 2+ systems where XSDT is unavailable,
     * the RSDT remains the fallback.
     */
    if (rsdp->rsdt_address == 0)
    {
        return -8;
    }

    root_table_address =
        (uint64_t)rsdp->rsdt_address;

    root_table_is_xsdt = 0;

    if (acpi_enumerate_rsdt(
            root_table_address,
            &root_table_count) != 0)
    {
        return -9;
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

uint64_t acpi_get_root_table_address(void)
{
    return root_table_address;
}

uint32_t acpi_get_root_table_count(void)
{
    return root_table_count;
}

uint8_t acpi_root_table_is_xsdt(void)
{
    return root_table_is_xsdt;
}

uint64_t acpi_get_madt_address(void)
{
    return madt_address;
}
