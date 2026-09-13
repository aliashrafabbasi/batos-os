#include "task.h"

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

static void task_bootstrap(void)
{
    /*
     * Scheduler dispatch will invoke this context in a later
     * milestone. The task entry/argument execution protocol
     * is intentionally not dispatched from task creation.
     *
     * Until scheduler dispatch exists, entering this address
     * is considered an invalid execution path.
     */
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
    uint64_t stack_top =
        task->kernel_stack_top -
        sizeof(uint64_t);

    *(uint64_t *)(uintptr_t)stack_top =
        (uint64_t)(uintptr_t)task_bootstrap;

    task->context.rsp =
        stack_top;

    task->context.rip =
        (uint64_t)(uintptr_t)task_bootstrap;

    task->state =
        TASK_STATE_READY;

    return 0;
}

int task_destroy(struct task *task)
{
    if (task == NULL)
        return -1;

    if (task->state == TASK_STATE_RUNNING)
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

    return 0;
}
