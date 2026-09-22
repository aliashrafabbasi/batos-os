#include "process_tests.h"

#include "../process/process.h"
#include "../process/process_registry.h"
#include "../mm/vmm/vmm.h"
#include "../console/console.h"

#include <stdint.h>
#include <stddef.h>

static void process_test_fail(
    const char *message
)
{
    serial_write_string(message);

    for (;;)
        __asm__ volatile ("cli\nhlt");
}

void process_tests_run(void)
{
    serial_write_string(
        "\nPROCESS FOUNDATION TEST\n"
    );

    if (process_registry_init() != 0)
    {
        process_test_fail(
            "PROCESS REGISTRY INIT: FAILED\n"
        );
    }

    if (process_registry_count() != 0 ||
        process_registry_find(1) != NULL)
    {
        process_test_fail(
            "PROCESS REGISTRY INIT: INVALID\n"
        );
    }

    /*
     * Invalid/unregistered address spaces must not become
     * Process associations.
     */
    struct process invalid = {0};

    if (process_create(
            &invalid,
            1,
            0
        ) == 0)
    {
        process_test_fail(
            "PROCESS CREATE: INVALID ADDRESS SPACE ACCEPTED\n"
        );
    }

    /*
     * Use a dedicated VMM-created address space rather than
     * the active kernel address space. Process only references
     * the address space; VMM remains its sole lifetime owner.
     */
    uint64_t address_space =
        vmm_create_address_space();

    if (address_space == 0)
    {
        process_test_fail(
            "PROCESS TEST: ADDRESS SPACE CREATE FAILED\n"
        );
    }

    uint8_t address_space_state = 0;

    if (vmm_get_address_space_state(
            address_space,
            &address_space_state
        ) != 0 ||
        address_space_state != VMM_ADDRESS_SPACE_CREATED)
    {
        process_test_fail(
            "PROCESS TEST: INVALID ADDRESS SPACE STATE\n"
        );
    }

    struct process process = {0};

    if (process_create(
            &process,
            1,
            address_space
        ) != 0)
    {
        process_test_fail(
            "PROCESS CREATE: FAILED\n"
        );
    }

    if (process.id != 1 ||
        process.state != PROCESS_STATE_ACTIVE ||
        process.address_space != address_space)
    {
        process_test_fail(
            "PROCESS CREATE: INVALID STATE\n"
        );
    }

    if (process_registry_count() != 1 ||
        process_registry_find(1) != &process ||
        process_registry_contains(&process) == 0 ||
        process_registry_find_by_address_space(
            address_space
        ) != &process)
    {
        process_test_fail(
            "PROCESS REGISTRY LOOKUP: FAILED\n"
        );
    }

    /*
     * Process lifecycle transitions into ACTIVE and TERMINATED
     * require Process Registry ownership. A detached Process
     * object must not be able to bypass registry lifecycle control.
     */
    struct process unregistered_process = {0};

    if (process_transition(
            &unregistered_process,
            PROCESS_STATE_ACTIVE
        ) == 0)
    {
        process_test_fail(
            "PROCESS LIFECYCLE BYPASS: UNREGISTERED ACTIVE ACCEPTED\n"
        );
    }

    if (process_transition(
            &unregistered_process,
            PROCESS_STATE_TERMINATED
        ) == 0)
    {
        process_test_fail(
            "PROCESS LIFECYCLE BYPASS: UNREGISTERED TERMINATED ACCEPTED\n"
        );
    }

    if (unregistered_process.state !=
        PROCESS_STATE_NEW)
    {
        process_test_fail(
            "PROCESS LIFECYCLE BYPASS: STATE MUTATED\n"
        );
    }

    /*
     * PID identity and address-space association are separate
     * Process invariants and are tested independently.
     */
    uint64_t second_address_space =
        vmm_create_address_space();

    if (second_address_space == 0)
    {
        process_test_fail(
            "PROCESS TEST: SECOND ADDRESS SPACE CREATE FAILED\n"
        );
    }

    /*
     * Same PID with a different address space must fail because
     * PID identity is already owned by the existing Process.
     */
    struct process duplicate_pid = {0};

    if (process_create(
            &duplicate_pid,
            1,
            second_address_space
        ) == 0)
    {
        process_test_fail(
            "PROCESS DUPLICATE PID: ACCEPTED\n"
        );
    }

    if (process_registry_count() != 1 ||
        process_registry_find(1) != &process)
    {
        process_test_fail(
            "PROCESS DUPLICATE PID: REGISTRY CORRUPTED\n"
        );
    }

    /*
     * A registry-owned Process object must not be repurposed
     * through process_create(). The failed creation must leave
     * its identity, lifecycle, address-space association, and
     * registry membership completely unchanged.
     */
    uint64_t original_process_id = process.id;
    enum process_state original_process_state = process.state;
    uint64_t original_process_address_space =
        process.address_space;

    if (process_create(
            &process,
            2,
            second_address_space
        ) == 0)
    {
        process_test_fail(
            "PROCESS REUSE: REGISTERED OBJECT ACCEPTED\n"
        );
    }

    if (process.id != original_process_id ||
        process.state != original_process_state ||
        process.address_space !=
            original_process_address_space ||
        process_registry_count() != 1 ||
        process_registry_find(1) != &process ||
        process_registry_find(2) != NULL)
    {
        process_test_fail(
            "PROCESS REUSE: REGISTERED OBJECT MUTATED\n"
        );
    }

    /*
     * A different PID must not be allowed to claim an address
     * space already associated with another Process.
     */
    struct process duplicate_address_space = {0};

    if (process_create(
            &duplicate_address_space,
            2,
            address_space
        ) == 0)
    {
        process_test_fail(
            "PROCESS DUPLICATE ADDRESS SPACE: ACCEPTED\n"
        );
    }

    if (process_registry_count() != 1 ||
        process_registry_find(1) != &process ||
        process_registry_find(2) != NULL)
    {
        process_test_fail(
            "PROCESS DUPLICATE ADDRESS SPACE: REGISTRY CORRUPTED\n"
        );
    }

    /*
     * The second address space was never associated with a
     * Process, so reclaim it directly through VMM.
     */
    if (vmm_destroy_address_space(second_address_space) != 0)
    {
        process_test_fail(
            "PROCESS TEST: SECOND ADDRESS SPACE DESTROY FAILED\n"
        );
    }

    if (process_terminate(&process) != 0)
    {
        process_test_fail(
            "PROCESS TERMINATE: FAILED\n"
        );
    }

    if (process.state !=
        PROCESS_STATE_TERMINATED)
    {
        process_test_fail(
            "PROCESS TERMINATE: INVALID STATE\n"
        );
    }

    if (process_terminate(&process) == 0)
    {
        process_test_fail(
            "PROCESS TERMINATE: DOUBLE TERMINATION ACCEPTED\n"
        );
    }

    if (process_destroy(&process) != 0)
    {
        process_test_fail(
            "PROCESS DESTROY: FAILED\n"
        );
    }

    /*
     * Process destruction must not destroy the VMM address
     * space. VMM remains the sole owner of page-table lifetime.
     */
    if (vmm_get_address_space_state(
            address_space,
            &address_space_state
        ) != 0 ||
        address_space_state != VMM_ADDRESS_SPACE_CREATED)
    {
        process_test_fail(
            "PROCESS DESTROY: ADDRESS SPACE OWNERSHIP VIOLATED\n"
        );
    }

    if (process_registry_count() != 0 ||
        process_registry_find(1) != NULL ||
        process_registry_contains(&process) != 0 ||
        process_registry_find_by_address_space(
            address_space
        ) != NULL)
    {
        process_test_fail(
            "PROCESS DESTROY: REGISTRY CORRUPTED\n"
        );
    }

    if (process.id != 0 ||
        process.state != PROCESS_STATE_NEW ||
        process.address_space != 0)
    {
        process_test_fail(
            "PROCESS DESTROY: OBJECT NOT CLEARED\n"
        );
    }

    /*
     * The Process object no longer references the address
     * space. VMM can now independently reclaim the
     * address-space page-table hierarchy.
     */
    if (vmm_destroy_address_space(address_space) != 0)
    {
        process_test_fail(
            "PROCESS TEST: VMM ADDRESS SPACE DESTROY FAILED\n"
        );
    }

    if (vmm_get_address_space_state(
            address_space,
            &address_space_state
        ) == 0)
    {
        process_test_fail(
            "PROCESS TEST: DESTROYED ADDRESS SPACE STILL REGISTERED\n"
        );
    }

    serial_write_string(
        "PROCESS FOUNDATION: VERIFIED\n"
    );
}
