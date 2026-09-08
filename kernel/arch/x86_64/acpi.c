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

#define ACPI_MADT_TYPE_LOCAL_APIC 0
#define ACPI_MADT_TYPE_IO_APIC    1
#define ACPI_MADT_TYPE_ISO        2

struct acpi_madt_header
{
    struct acpi_sdt_header sdt;

    uint32_t local_apic_address;
    uint32_t flags;
} __attribute__((packed));

struct acpi_madt_entry_header
{
    uint8_t type;
    uint8_t length;
} __attribute__((packed));

struct acpi_madt_local_apic_entry
{
    struct acpi_madt_entry_header header;

    uint8_t processor_uid;
    uint8_t apic_id;
    uint32_t flags;
} __attribute__((packed));

struct acpi_madt_io_apic_entry
{
    struct acpi_madt_entry_header header;

    uint8_t io_apic_id;
    uint8_t reserved;
    uint32_t io_apic_address;
    uint32_t global_system_interrupt_base;
} __attribute__((packed));

struct acpi_madt_iso_entry
{
    struct acpi_madt_entry_header header;

    uint8_t bus;
    uint8_t source;
    uint32_t global_system_interrupt;
    uint16_t flags;
} __attribute__((packed));

static uint32_t madt_local_apic_address = 0;
static uint32_t madt_flags = 0;

static struct acpi_madt_local_apic
    madt_local_apics[ACPI_MAX_MADT_LOCAL_APICS];

static uint32_t madt_local_apic_count = 0;

static struct acpi_madt_io_apic
    madt_io_apics[ACPI_MAX_MADT_IO_APICS];

static uint32_t madt_io_apic_count = 0;

static struct acpi_madt_iso
    madt_isos[ACPI_MAX_MADT_ISOS];

static uint32_t madt_iso_count = 0;

static uint32_t madt_unknown_entry_count = 0;

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
/*
 * Parse the MADT structure.
 *
 * The generic SDT validator has already verified the complete
 * table and checksum before this parser is called.
 *
 * Supported entries:
 *   Type 0 - Processor Local APIC
 *   Type 1 - I/O APIC
 *   Type 2 - Interrupt Source Override
 *
 * Unknown entry types are safely skipped.
 */
static int acpi_parse_madt(
    uint64_t physical_address
)
{
    volatile struct acpi_madt_header *madt =
        (volatile struct acpi_madt_header *)
        acpi_phys_to_virt(
            physical_address,
            sizeof(struct acpi_madt_header));

    if (madt == 0)
    {
        return -1;
    }

    if (madt->sdt.length <
        sizeof(struct acpi_madt_header))
    {
        return -2;
    }

    uint64_t table_start =
        physical_address;

    uint64_t table_length =
        (uint64_t)madt->sdt.length;

    if (table_start >
        UINT64_MAX - table_length)
    {
        return -3;
    }

    uint64_t table_end =
        table_start + table_length;

    madt_local_apic_address =
        madt->local_apic_address;

    madt_flags =
        madt->flags;

    madt_local_apic_count = 0;
    madt_io_apic_count = 0;
    madt_iso_count = 0;
    madt_unknown_entry_count = 0;

    uint64_t entry_physical =
        table_start +
        sizeof(struct acpi_madt_header);

    while (entry_physical < table_end)
    {
        if (entry_physical >
            table_end - sizeof(struct acpi_madt_entry_header))
        {
            return -4;
        }

        volatile struct acpi_madt_entry_header *entry =
            (volatile struct acpi_madt_entry_header *)
            acpi_phys_to_virt(
                entry_physical,
                sizeof(struct acpi_madt_entry_header));

        if (entry == 0)
        {
            return -5;
        }

        uint8_t type = entry->type;
        uint8_t length = entry->length;

        if (length == 0)
        {
            return -6;
        }

        if (length <
            sizeof(struct acpi_madt_entry_header))
        {
            return -7;
        }

        if ((uint64_t)length >
            table_end - entry_physical)
        {
            return -8;
        }

        if (type == ACPI_MADT_TYPE_LOCAL_APIC)
        {
            if (length <
                sizeof(struct acpi_madt_local_apic_entry))
            {
                return -9;
            }

            if (madt_local_apic_count >=
                ACPI_MAX_MADT_LOCAL_APICS)
            {
                return -10;
            }

            volatile struct acpi_madt_local_apic_entry *local_apic =
                (volatile struct acpi_madt_local_apic_entry *)
                acpi_phys_to_virt(
                    entry_physical,
                    sizeof(struct acpi_madt_local_apic_entry));

            if (local_apic == 0)
            {
                return -11;
            }

            madt_local_apics[madt_local_apic_count].processor_uid =
                local_apic->processor_uid;

            madt_local_apics[madt_local_apic_count].apic_id =
                local_apic->apic_id;

            madt_local_apics[madt_local_apic_count].flags =
                local_apic->flags;

            madt_local_apic_count++;
        }
        else if (type == ACPI_MADT_TYPE_IO_APIC)
        {
            if (length <
                sizeof(struct acpi_madt_io_apic_entry))
            {
                return -12;
            }

            if (madt_io_apic_count >=
                ACPI_MAX_MADT_IO_APICS)
            {
                return -13;
            }

            volatile struct acpi_madt_io_apic_entry *io_apic =
                (volatile struct acpi_madt_io_apic_entry *)
                acpi_phys_to_virt(
                    entry_physical,
                    sizeof(struct acpi_madt_io_apic_entry));

            if (io_apic == 0)
            {
                return -14;
            }

            madt_io_apics[madt_io_apic_count].id =
                io_apic->io_apic_id;

            madt_io_apics[madt_io_apic_count].address =
                (uint64_t)io_apic->io_apic_address;

            madt_io_apics[madt_io_apic_count].gsi_base =
                io_apic->global_system_interrupt_base;

            madt_io_apic_count++;
        }
        else if (type == ACPI_MADT_TYPE_ISO)
        {
            if (length <
                sizeof(struct acpi_madt_iso_entry))
            {
                return -15;
            }

            if (madt_iso_count >=
                ACPI_MAX_MADT_ISOS)
            {
                return -16;
            }

            volatile struct acpi_madt_iso_entry *iso =
                (volatile struct acpi_madt_iso_entry *)
                acpi_phys_to_virt(
                    entry_physical,
                    sizeof(struct acpi_madt_iso_entry));

            if (iso == 0)
            {
                return -17;
            }

            madt_isos[madt_iso_count].bus =
                iso->bus;

            madt_isos[madt_iso_count].source =
                iso->source;

            madt_isos[madt_iso_count].gsi =
                iso->global_system_interrupt;

            madt_isos[madt_iso_count].flags =
                iso->flags;

            madt_iso_count++;
        }
        else
        {
            madt_unknown_entry_count++;
        }

        entry_physical += (uint64_t)length;
    }

    if (entry_physical != table_end)
    {
        return -18;
    }

    return 0;
}

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
    madt_local_apic_address = 0;
    madt_flags = 0;
    madt_local_apic_count = 0;
    madt_io_apic_count = 0;
    madt_iso_count = 0;
    madt_unknown_entry_count = 0;

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

            if (madt_address != 0)
            {
                if (acpi_parse_madt(madt_address) != 0)
                {
                    return -19;
                }
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

    if (madt_address != 0)
    {
        if (acpi_parse_madt(madt_address) != 0)
        {
            return -19;
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


uint32_t acpi_get_madt_local_apic_address(void)
{
    return madt_local_apic_address;
}

uint32_t acpi_get_madt_flags(void)
{
    return madt_flags;
}

uint32_t acpi_get_madt_local_apic_count(void)
{
    return madt_local_apic_count;
}

const struct acpi_madt_local_apic *
acpi_get_madt_local_apic(uint32_t index)
{
    if (index >= madt_local_apic_count)
    {
        return 0;
    }

    return &madt_local_apics[index];
}

uint32_t acpi_get_madt_io_apic_count(void)
{
    return madt_io_apic_count;
}

const struct acpi_madt_io_apic *
acpi_get_madt_io_apic(uint32_t index)
{
    if (index >= madt_io_apic_count)
    {
        return 0;
    }

    return &madt_io_apics[index];
}

uint32_t acpi_get_madt_iso_count(void)
{
    return madt_iso_count;
}

const struct acpi_madt_iso *
acpi_get_madt_iso(uint32_t index)
{
    if (index >= madt_iso_count)
    {
        return 0;
    }

    return &madt_isos[index];
}

uint32_t acpi_get_madt_unknown_entry_count(void)
{
    return madt_unknown_entry_count;
}
