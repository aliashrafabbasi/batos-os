#include "task.h"

#include "../arch/x86_64/sched/preempt.h"

#include "runqueue.h"
#include "task_registry.h"

#include "../mm/pmm/pmm.h"
#include "../mm/vmm/vmm.h"

#include <stdint.h>
#include <stddef.h>

extern char __task_stack_start[];
extern char __task_stack_end[];

#define TASK_STACK_VIRTUAL_BASE \
    ((uint64_t)(uintptr_t)__task_stack_start)

#define TASK_STACK_VIRTUAL_END \
    ((uint64_t)(uintptr_t)__task_stack_end)

static uint64_t task_stack_virtual_for(uint64_t id)
{
    if (id == 0 || id > TASK_MAX_TASKS)
        return 0;

    uint64_t slot = id - 1;

    uint64_t offset =
        slot * TASK_KERNEL_STACK_SLOT_SIZE;

    uint64_t base =
        TASK_STACK_VIRTUAL_BASE +
        TASK_KERNEL_STACK_GUARD_SIZE +
        offset;

    uint64_t end =
        base + TASK_KERNEL_STACK_SIZE;

    if (base < TASK_STACK_VIRTUAL_BASE ||
        end < base ||
        end > TASK_STACK_VIRTUAL_END)
    {
        return 0;
    }

    return base;
}

static int task_map_stack(struct task *task)
{
    if (task == NULL)
        return -1;

    uint64_t virtual_base =
        task_stack_virtual_for(task->id);

    if (virtual_base == 0)
        return -1;

    /*
     * Task stacks are currently created only inside the
     * active kernel address space. Address-space switching
     * belongs to a later scheduler/VM integration milestone.
     */
    if (task->address_space != vmm_get_pml4())
        return -1;

    for (uint64_t page = 0;
         page < TASK_KERNEL_STACK_PAGE_COUNT;
         page++)
    {
        task->kernel_stack_pages[page] =
            pmm_alloc_frame();

        if (task->kernel_stack_pages[page] == 0)
            goto rollback;

        uint64_t virtual_address =
            virtual_base +
            page * VMM_PAGE_SIZE;

        if (vmm_map_page(
                task->address_space,
                virtual_address,
                task->kernel_stack_pages[page],
                VMM_WRITABLE
            ) != 0)
        {
            pmm_free_frame(
                task->kernel_stack_pages[page]
            );

            task->kernel_stack_pages[page] = 0;

            goto rollback;
        }
    }

    task->kernel_stack_base =
        virtual_base;

    task->kernel_stack_top =
        virtual_base +
        TASK_KERNEL_STACK_SIZE;

    return 0;

rollback:
    for (uint64_t page = 0;
         page < TASK_KERNEL_STACK_PAGE_COUNT;
         page++)
    {
        uint64_t virtual_address =
            virtual_base +
            page * VMM_PAGE_SIZE;

        uint64_t physical = 0;

        if (vmm_unmap_page(
                task->address_space,
                virtual_address,
                &physical
            ) == 0)
        {
            pmm_free_frame(physical);
        }

        task->kernel_stack_pages[page] = 0;
    }

    return -1;
}

static int task_unmap_stack(struct task *task)
{
    if (task == NULL ||
        task->kernel_stack_base == 0)
    {
        return -1;
    }

    int result = 0;

    for (uint64_t page = 0;
         page < TASK_KERNEL_STACK_PAGE_COUNT;
         page++)
    {
        uint64_t virtual_address =
            task->kernel_stack_base +
            page * VMM_PAGE_SIZE;

        uint64_t physical = 0;

        if (vmm_unmap_page(
                task->address_space,
                virtual_address,
                &physical
            ) != 0)
        {
            result = -1;
            continue;
        }

        pmm_free_frame(physical);

        task->kernel_stack_pages[page] = 0;
    }

    return result;
}

static task_exit_handler_t task_exit_handler = NULL;

int task_transition(
    struct task *task,
    enum task_state new_state
)
{
    if (task == NULL)
        return -1;

    switch (task->state)
    {
        case TASK_STATE_NEW:
            if (new_state != TASK_STATE_READY)
                return -1;
            break;

        case TASK_STATE_READY:
            if (new_state != TASK_STATE_RUNNING)
                return -1;
            break;

        case TASK_STATE_RUNNING:
            if (new_state != TASK_STATE_READY &&
                new_state != TASK_STATE_BLOCKED &&
                new_state != TASK_STATE_SLEEPING &&
                new_state != TASK_STATE_TERMINATED)
            {
                return -1;
            }
            break;

        case TASK_STATE_BLOCKED:
            if (new_state != TASK_STATE_READY)
                return -1;
            break;

        case TASK_STATE_SLEEPING:
            if (new_state != TASK_STATE_READY)
                return -1;
            break;

        case TASK_STATE_TERMINATED:
            return -1;

        default:
            return -1;
    }

    task->state = new_state;
    return 0;
}


extern void x86_64_task_bootstrap_trampoline(void);

void task_bootstrap_entry(struct task *current)
{

    if (current == NULL ||
        current->state != TASK_STATE_RUNNING ||
        current->entry == NULL)
    {
        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt\n"
            );
        }
    }

    current->entry(
        current->argument
    );

    if (task_exit(current) != 0)
    {
        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt\n"
            );
        }
    }

    /*
     * A terminated task must never return through its bootstrap
     * stack. The registered execution owner performs the
     * control-flow handoff to the next runnable task.
     */
    if (task_exit_handler == NULL ||
        task_exit_handler(current) != 0)
    {
        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt\n"
            );
        }
    }

    for (;;)
    {
        __asm__ volatile (
            "cli\n"
            "hlt\n"
        );
    }
}

int task_create(
    struct task *task,
    uint64_t id,
    uint64_t address_space,
    task_entry_t entry,
    void *argument
)
{
    if (task == NULL ||
        id == 0 ||
        id > TASK_MAX_TASKS ||
        address_space == 0 ||
        entry == NULL)
    {
        return -1;
    }

    task->id = id;
    task->state = TASK_STATE_NEW;

    task->kernel_stack_base = 0;
    task->kernel_stack_top = 0;

    for (uint64_t page = 0;
         page < TASK_KERNEL_STACK_PAGE_COUNT;
         page++)
    {
        task->kernel_stack_pages[page] = 0;
    }

    task->address_space =
        address_space;

    task->entry = entry;
    task->argument = argument;

    /*
     * The initial task continuation is the established
     * cooperative context ABI. The interrupt-return frame
     * is prepared as an additional architecture-owned
     * first-run representation but is not authoritative yet.
     */
    task->resume_authority =
        TASK_RESUME_CONTEXT;

    task->preempt_state.frame_address = 0;
    task->preempt_state.valid = 0;

    task->context.rbx = 0;
    task->context.rbp = 0;
    task->context.r12 = 0;
    task->context.r13 = 0;
    task->context.r14 = 0;
    task->context.r15 = 0;
    task->context.rsp = 0;
    task->context.rip = 0;

    if (task_map_stack(task) != 0)
        return -1;

    /*
     * Reserve the ABI return-address slot.
     */
    /*
     * Reserve a bootstrap return slot and an explicit task pointer.
     *
     * The task stack top is 16-byte aligned. Using top - 24 gives the
     * trampoline an RSP value congruent to 8 mod 16, matching the
     * SysV AMD64 function-entry contract.
     *
     * The task pointer remains inside the mapped stack rather than
     * being written one word beyond the mapped stack.
     */
    uint64_t stack_pointer =
        task->kernel_stack_top -
        3ULL * sizeof(uint64_t);

    *(uint64_t *)(uintptr_t)stack_pointer = 0;

    *(uint64_t *)(uintptr_t)(
        stack_pointer + sizeof(uint64_t)
    ) = (uint64_t)(uintptr_t)task;

    task->context.rsp = stack_pointer;

    task->context.rip =
        (uint64_t)(uintptr_t)
            x86_64_task_bootstrap_trampoline;

    /*
     * Prepare the architecture-owned interrupt-return state
     * without changing the existing cooperative context ABI.
     */
    if (x86_64_preempt_prepare_first_run(
            task,
            &task->preempt_state
        ) != 0)
    {
        task_unmap_stack(task);

        task->kernel_stack_base = 0;
        task->kernel_stack_top = 0;

        for (uint64_t page = 0;
             page < TASK_KERNEL_STACK_PAGE_COUNT;
             page++)
        {
            task->kernel_stack_pages[page] = 0;
        }

        task->preempt_state.frame_address = 0;
        task->preempt_state.valid = 0;

        return -1;
    }

    if (task_transition(
            task,
            TASK_STATE_READY
        ) != 0)
    {
        return -1;
    }

    return 0;
}

int task_set_exit_handler(
    task_exit_handler_t handler
)
{
    if (handler == NULL)
        return -1;

    task_exit_handler = handler;
    return 0;
}

int task_exit(struct task *task)
{
    if (task == NULL)
        return -1;

    if (task->state != TASK_STATE_RUNNING)
        return -1;

    return task_transition(
        task,
        TASK_STATE_TERMINATED
    );
}

int task_destroy(struct task *task)
{
    if (task == NULL)
        return -1;

    if (task->state == TASK_STATE_RUNNING)
        return -1;

    /*
     * Task-owned resources may only be destroyed after
     * scheduler and registry ownership have been released.
     */
    if (runqueue_contains(task))
        return -1;

    if (task_registry_contains(task))
        return -1;

    if (task->kernel_stack_base != 0)
    {
        if (task_unmap_stack(task) != 0)
            return -1;
    }

    task->kernel_stack_base = 0;
    task->kernel_stack_top = 0;

    for (uint64_t page = 0;
         page < TASK_KERNEL_STACK_PAGE_COUNT;
         page++)
    {
        task->kernel_stack_pages[page] = 0;
    }

    task->state =
        TASK_STATE_TERMINATED;

    task->entry = NULL;
    task->argument = NULL;

    /*
     * The saved interrupt-return frame lived on the task's
     * kernel stack. Once that stack has been destroyed, the
     * architecture-owned preemptive state must no longer be
     * considered resumable.
     */
    task->preempt_state.frame_address = 0;
    task->preempt_state.valid = 0;
    task->resume_authority =
        TASK_RESUME_NONE;

    return 0;
}
