#ifndef BATOS_IOAPIC_H
#define BATOS_IOAPIC_H

#include <stdint.h>

/*
 * Intel 82093AA-compatible I/O APIC register window.
 */
#define IOAPIC_REG_ID       0x00U
#define IOAPIC_REG_VERSION  0x01U
#define IOAPIC_REG_ARB      0x02U

#define IOAPIC_REDIR_BASE   0x10U

/*
 * Redirection-table entry bits.
 */
#define IOAPIC_REDIR_MASKED       (1ULL << 16)
#define IOAPIC_REDIR_TRIGGER_LEVEL (1ULL << 15)
#define IOAPIC_REDIR_REMOTE_IRR   (1ULL << 14)
#define IOAPIC_REDIR_POLARITY_LOW (1ULL << 13)
#define IOAPIC_REDIR_DEST_LOGICAL (1ULL << 11)

/*
 * Delivery mode occupies bits 10:8.
 */
#define IOAPIC_REDIR_DELIVERY_FIXED   (0ULL << 8)
#define IOAPIC_REDIR_DELIVERY_LOWEST  (1ULL << 8)
#define IOAPIC_REDIR_DELIVERY_SMI     (2ULL << 8)
#define IOAPIC_REDIR_DELIVERY_NMI     (4ULL << 8)
#define IOAPIC_REDIR_DELIVERY_INIT    (5ULL << 8)
#define IOAPIC_REDIR_DELIVERY_EXTINT  (7ULL << 8)

#define IOAPIC_MAX_INSTANCES 256U

int ioapic_init(void);

uint32_t ioapic_get_count(void);

uint64_t ioapic_get_physical_address(void);
uint64_t ioapic_get_virtual_address(void);

uint8_t ioapic_get_id(void);
uint8_t ioapic_get_version(void);
uint8_t ioapic_get_max_redirection_entry(void);

uint64_t ioapic_get_physical_address_at(
    uint32_t index
);

uint64_t ioapic_get_virtual_address_at(
    uint32_t index
);

uint8_t ioapic_get_id_at(
    uint32_t index
);

uint8_t ioapic_get_version_at(
    uint32_t index
);

uint8_t ioapic_get_max_redirection_entry_at(
    uint32_t index
);

uint32_t ioapic_get_gsi_base_at(
    uint32_t index
);

int ioapic_gsi_in_range(
    uint32_t index,
    uint32_t gsi
);

uint32_t ioapic_read_register(uint8_t reg);
void ioapic_write_register(uint8_t reg, uint32_t value);

int ioapic_read_redirection_at(
    uint32_t ioapic_index,
    uint8_t index,
    uint64_t *value
);

int ioapic_write_redirection_at(
    uint32_t ioapic_index,
    uint8_t index,
    uint64_t value
);

/*
 * Compatibility API for IOAPIC instance 0.
 */
int ioapic_read_redirection(
    uint8_t index,
    uint64_t *value
);

int ioapic_write_redirection(
    uint8_t index,
    uint64_t value
);

#endif
