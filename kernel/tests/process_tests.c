#include "process_tests.h"

#include "../process/process.h"
#include "../process/process_registry.h"
#include "../sched/task.h"
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

    /*
     * An already-active VMM address space is also a valid logical
     * Process association. Process creation must not perform CR3
     * activation or otherwise alter the VMM lifecycle state.
     *
     * VMM-2C has already activated the BATOS kernel address space
     * before the Process foundation tests execute.
     */
    uint64_t active_address_space =
        vmm_get_pml4();

    if (active_address_space == 0)
    {
        process_test_fail(
            "PROCESS TEST: ACTIVE ADDRESS SPACE INVALID\\n"
        );
    }

    if (vmm_get_address_space_state(
            active_address_space,
            &address_space_state
        ) != 0 ||
        address_space_state != VMM_ADDRESS_SPACE_ACTIVE)
    {
        process_test_fail(
            "PROCESS TEST: ACTIVE ADDRESS SPACE STATE INVALID\\n"
        );
    }

    struct process active_process = {0};

    if (process_create(
            &active_process,
            2,
            active_address_space
        ) != 0)
    {
        process_test_fail(
            "PROCESS CREATE: ACTIVE ADDRESS SPACE FAILED\\n"
        );
    }

    if (active_process.state != PROCESS_STATE_ACTIVE ||
        active_process.address_space != active_address_space ||
        process_registry_find(2) != &active_process ||
        process_registry_find_by_address_space(
            active_address_space
        ) != &active_process)
    {
        process_test_fail(
            "PROCESS CREATE: ACTIVE ADDRESS SPACE INVALID\\n"
        );
    }

    if (process_terminate(&active_process) != 0 ||
        process_destroy(&active_process) != 0)
    {
        process_test_fail(
            "PROCESS ACTIVE ASSOCIATION CLEANUP: FAILED\\n"
        );
    }

    if (vmm_get_address_space_state(
            active_address_space,
            &address_space_state
        ) != 0 ||
        address_space_state != VMM_ADDRESS_SPACE_ACTIVE)
    {
        process_test_fail(
            "PROCESS ACTIVE ASSOCIATION: VMM OWNERSHIP VIOLATED\\n"
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
     * PROCESS <-> TASK OWNERSHIP
     *
     * Task membership is deliberately tested with lightweight Task
     * objects because task_create() requires its address space to
     * be the currently active address space. This membership cluster
     * must not introduce implicit CR3/address-space activation.
     */
    struct task task_a = {0};
    struct task task_b = {0};
    struct task task_other = {0};

    task_a.id = 1;
    task_a.state = TASK_STATE_NEW;
    task_a.address_space = address_space;

    task_b.id = 2;
    task_b.state = TASK_STATE_NEW;
    task_b.address_space = address_space;

    task_other.id = 3;
    task_other.state = TASK_STATE_NEW;
    task_other.address_space = address_space;

    if (process_task_count(&process) != 0 ||
        process_contains_task(&process, &task_a) != 0)
    {
        process_test_fail(
            "PROCESS TASK MEMBERSHIP: INITIAL STATE INVALID\n"
        );
    }

    if (process_attach_task(
            &process,
            &task_a
        ) != 0)
    {
        process_test_fail(
            "PROCESS TASK ATTACH: FAILED\n"
        );
    }

    if (task_a.process != &process ||
        process_task_count(&process) != 1 ||
        process_contains_task(
            &process,
            &task_a
        ) == 0)
    {
        process_test_fail(
            "PROCESS TASK ATTACH: OWNERSHIP INVARIANT FAILED\n"
        );
    }

    /*
     * The same Task cannot be attached twice.
     */
    if (process_attach_task(
            &process,
            &task_a
        ) == 0)
    {
        process_test_fail(
            "PROCESS TASK ATTACH: DUPLICATE ACCEPTED\n"
        );
    }

    /*
     * A Task already owned by one Process cannot be claimed by
     * another Process.
     *
     * Use a separate VMM address space for the temporary Process.
     * Process creation must not implicitly activate it.
     */
    uint64_t second_address_space =
        vmm_create_address_space();

    if (second_address_space == 0)
    {
        process_test_fail(
            "PROCESS TASK TEST: SECOND ADDRESS SPACE CREATE FAILED\n"
        );
    }

    struct process second_process = {0};

    if (process_create(
            &second_process,
            2,
            second_address_space
        ) != 0)
    {
        process_test_fail(
            "PROCESS TASK TEST: SECOND PROCESS CREATE FAILED\n"
        );
    }

    if (process_attach_task(
            &second_process,
            &task_a
        ) == 0)
    {
        process_test_fail(
            "PROCESS TASK ATTACH: DUAL PROCESS OWNERSHIP ACCEPTED\n"
        );
    }

    if (task_a.process != &process ||
        process_contains_task(
            &second_process,
            &task_a
        ) != 0)
    {
        process_test_fail(
            "PROCESS TASK ATTACH: DUAL OWNERSHIP CORRUPTED\n"
        );
    }

    /*
     * The temporary Process has completed its membership test.
     * Reclaim its Process identity now while deliberately keeping
     * the separate VMM address space alive for the subsequent
     * address-space mismatch and duplicate-PID tests.
     */
    if (process_terminate(&second_process) != 0 ||
        process_destroy(&second_process) != 0)
    {
        process_test_fail(
            "PROCESS TASK TEST: SECOND PROCESS CLEANUP FAILED\n"
        );
    }

    if (process_registry_count() != 1 ||
        process_registry_find(1) != &process ||
        process_registry_find(2) != NULL)
    {
        process_test_fail(
            "PROCESS TASK TEST: SECOND PROCESS CLEANUP CORRUPTED REGISTRY\n"
        );
    }

    /*
     * A Task associated with a different address space cannot
     * silently join this Process.
     */
    task_other.address_space =
        second_address_space;

    if (process_attach_task(
            &process,
            &task_other
        ) == 0)
    {
        process_test_fail(
            "PROCESS TASK ATTACH: ADDRESS SPACE MISMATCH ACCEPTED\n"
        );
    }

    if (task_other.process != NULL ||
        process_contains_task(
            &process,
            &task_other
        ) != 0 ||
        process_task_count(&process) != 1)
    {
        process_test_fail(
            "PROCESS TASK ATTACH: MISMATCH MUTATED OWNERSHIP\n"
        );
    }

    /*
     * Attach a second Task and verify Process-owned membership
     * can represent multiple execution entities.
     */
    if (process_attach_task(
            &process,
            &task_b
        ) != 0)
    {
        process_test_fail(
            "PROCESS TASK ATTACH: SECOND TASK FAILED\n"
        );
    }

    if (task_b.process != &process ||
        process_task_count(&process) != 2 ||
        process_contains_task(
            &process,
            &task_b
        ) == 0)
    {
        process_test_fail(
            "PROCESS TASK ATTACH: MULTI-TASK OWNERSHIP FAILED\n"
        );
    }

    /*
     * Process destruction is forbidden while Task membership
     * remains. This protects Task -> Process back-references.
     */
    if (process_terminate(&process) != 0)
    {
        process_test_fail(
            "PROCESS TASK TEST: TERMINATION BEFORE DESTROY FAILED\n"
        );
    }

    if (process_destroy(&process) == 0)
    {
        process_test_fail(
            "PROCESS DESTROY: ATTACHED TASKS ACCEPTED\n"
        );
    }

    if (process_registry_contains(&process) == 0 ||
        process.task_count != 2)
    {
        process_test_fail(
            "PROCESS DESTROY: ATTACHED MEMBERSHIP MUTATED\n"
        );
    }

    /*
     * A Task cannot be destroyed while Process owns it.
     * Keep task_a/task_b in lightweight READY state for this
     * ownership-boundary test; no scheduler ownership exists.
     */
    task_a.state = TASK_STATE_READY;

    if (task_destroy(&task_a) == 0)
    {
        process_test_fail(
            "TASK DESTROY: ATTACHED TASK ACCEPTED\n"
        );
    }

    /*
     * Detach releases both sides of the relationship.
     */
    if (process_detach_task(
            &process,
            &task_a
        ) != 0)
    {
        process_test_fail(
            "PROCESS TASK DETACH: FAILED\n"
        );
    }

    if (task_a.process != NULL ||
        process_contains_task(
            &process,
            &task_a
        ) != 0 ||
        process_task_count(&process) != 1)
    {
        process_test_fail(
            "PROCESS TASK DETACH: OWNERSHIP INVARIANT FAILED\n"
        );
    }

    /*
     * Releasing one member must not corrupt the remaining member.
     */
    if (task_b.process != &process ||
        process_contains_task(
            &process,
            &task_b
        ) == 0)
    {
        process_test_fail(
            "PROCESS TASK DETACH: REMAINING MEMBER CORRUPTED\n"
        );
    }

    if (process_detach_task(
            &process,
            &task_a
        ) == 0)
    {
        process_test_fail(
            "PROCESS TASK DETACH: DUPLICATE DETACH ACCEPTED\n"
        );
    }

    if (process_detach_task(
            &process,
            &task_b
        ) != 0)
    {
        process_test_fail(
            "PROCESS TASK DETACH: SECOND TASK FAILED\n"
        );
    }

    if (task_b.process != NULL ||
        process_task_count(&process) != 0)
    {
        process_test_fail(
            "PROCESS TASK DETACH: FINAL OWNERSHIP INVALID\n"
        );
    }

    /*
     * Reaching TERMINATED with zero Task membership permits
     * Process destruction.
     */
    if (process_destroy(&process) != 0)
    {
        process_test_fail(
            "PROCESS DESTROY: EMPTY MEMBERSHIP FAILED\n"
        );
    }

    /*
     * The original Process object is now detached from the
     * registry and its membership domain is empty. The remainder
     * of the foundation test below uses a fresh Process object.
     */
    struct process lifecycle_process = {0};

    if (process_create(
            &lifecycle_process,
            3,
            address_space
        ) != 0)
    {
        process_test_fail(
            "PROCESS TEST: LIFECYCLE PROCESS RECREATE FAILED\n"
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
     * Process invariants and are tested independently. The
     * second address space created above remains VMM-owned and
     * unassociated with any Process.
     */
    /*
     * Same PID with a different address space must fail because
     * PID identity is already owned by the existing Process.
     * lifecycle_process currently owns PID 3.
     */
    struct process duplicate_pid = {0};

    if (process_create(
            &duplicate_pid,
            3,
            second_address_space
        ) == 0)
    {
        process_test_fail(
            "PROCESS DUPLICATE PID: ACCEPTED\n"
        );
    }

    if (process_registry_count() != 1 ||
        process_registry_find(3) != &lifecycle_process)
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
    uint64_t original_process_id = lifecycle_process.id;
    enum process_state original_process_state = lifecycle_process.state;
    uint64_t original_process_address_space =
        lifecycle_process.address_space;

    if (process_create(
            &lifecycle_process,
            2,
            second_address_space
        ) == 0)
    {
        process_test_fail(
            "PROCESS REUSE: REGISTERED OBJECT ACCEPTED\n"
        );
    }

    if (lifecycle_process.id != original_process_id ||
        lifecycle_process.state != original_process_state ||
        lifecycle_process.address_space !=
            original_process_address_space ||
        process_registry_count() != 1 ||
        process_registry_find(3) != &lifecycle_process ||
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
        process_registry_find(3) != &lifecycle_process ||
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

    if (process_terminate(&lifecycle_process) != 0)
    {
        process_test_fail(
            "PROCESS TERMINATE: FAILED\n"
        );
    }

    if (lifecycle_process.state !=
        PROCESS_STATE_TERMINATED)
    {
        process_test_fail(
            "PROCESS TERMINATE: INVALID STATE\n"
        );
    }

    if (process_terminate(&lifecycle_process) == 0)
    {
        process_test_fail(
            "PROCESS TERMINATE: DOUBLE TERMINATION ACCEPTED\n"
        );
    }

    if (process_destroy(&lifecycle_process) != 0)
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
        process_registry_find(3) != NULL ||
        process_registry_contains(&lifecycle_process) != 0 ||
        process_registry_find_by_address_space(
            address_space
        ) != NULL)
    {
        process_test_fail(
            "PROCESS DESTROY: REGISTRY CORRUPTED\n"
        );
    }

    if (lifecycle_process.id != 0 ||
        lifecycle_process.state != PROCESS_STATE_NEW ||
        lifecycle_process.address_space != 0)
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
