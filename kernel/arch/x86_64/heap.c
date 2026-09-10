#include "kernel/arch/x86_64/heap.h"

#include "kernel/arch/x86_64/pmm.h"
#include "kernel/arch/x86_64/vmm.h"

#include <stddef.h>
#include <stdint.h>

/*
 * HEAP-1A
 *
 * Bootstrap kernel heap allocator.
 *
 * This stage intentionally uses a statically reserved arena.
 * PMM/VMM-backed expansion will be introduced in Heap-1B.
 */

#define HEAP_ALIGNMENT       16ULL
#define HEAP_ARENA_SIZE      (64ULL * 1024ULL)
#define HEAP_MIN_SPLIT_SIZE  32ULL

#define HEAP_BLOCK_FREE      0x46524545ULL
#define HEAP_BLOCK_USED      0x55534544ULL

/*
 * HEAP-1B.2
 *
 * First PMM/VMM-backed dynamic heap page.
 *
 * This address is the first page after the currently linked
 * kernel image. It is a managed heap virtual address, not a
 * direct physical address.
 */
#define HEAP_DYNAMIC_PAGE_VIRTUAL  0xffffffff80042000ULL

struct heap_block
{
    uint64_t size;
    uint64_t state;
    struct heap_block *previous;
    struct heap_block *next;
};

static uint8_t heap_arena[HEAP_ARENA_SIZE]
    __attribute__((aligned(HEAP_ALIGNMENT)));

static struct heap_block *heap_first_block = NULL;

static uint64_t heap_allocated_bytes = 0;
static uint64_t heap_free_bytes = 0;

/*
 * HEAP-1B.2 dynamic page ownership state.
 *
 * Only one dynamic page is managed in this milestone.
 */
static uint64_t heap_dynamic_page_physical = 0;
static uint64_t heap_dynamic_page_virtual = 0;
static int heap_dynamic_page_mapped = 0;

static uint64_t align_up_safe(uint64_t value)
{
    uint64_t mask = HEAP_ALIGNMENT - 1;

    if (value > UINT64_MAX - mask)
        return 0;

    return (value + mask) & ~mask;
}

static uint8_t *block_payload(
    struct heap_block *block
)
{
    return (uint8_t *)block + sizeof(struct heap_block);
}

void heap_init(void)
{
    heap_first_block =
        (struct heap_block *)heap_arena;

    heap_first_block->size =
        HEAP_ARENA_SIZE -
        sizeof(struct heap_block);

    heap_first_block->state =
        HEAP_BLOCK_FREE;

    heap_first_block->previous = NULL;
    heap_first_block->next = NULL;

    heap_allocated_bytes = 0;
    heap_free_bytes =
        heap_first_block->size;
}

void *kmalloc(uint64_t size)
{
    if (size == 0)
        return NULL;

    uint64_t aligned_size =
        align_up_safe(size);

    if (aligned_size == 0)
        return NULL;

    if (heap_first_block == NULL)
        return NULL;

    struct heap_block *block =
        heap_first_block;

    while (block != NULL)
    {
        if (block->state == HEAP_BLOCK_FREE &&
            block->size >= aligned_size)
        {
            uint64_t remaining =
                block->size - aligned_size;

            if (remaining >=
                sizeof(struct heap_block) +
                HEAP_MIN_SPLIT_SIZE)
            {
                struct heap_block *split =
                    (struct heap_block *)(
                        block_payload(block) +
                        aligned_size
                    );

                split->size =
                    remaining -
                    sizeof(struct heap_block);

                split->state =
                    HEAP_BLOCK_FREE;

                split->previous = block;
                split->next = block->next;

                if (split->next != NULL)
                    split->next->previous =
                        split;

                block->next = split;
                block->size = aligned_size;

                /*
                 * Creating the split header consumes space
                 * from the free payload capacity.
                 */
                heap_free_bytes -=
                    sizeof(struct heap_block);
            }

            block->state =
                HEAP_BLOCK_USED;

            heap_allocated_bytes +=
                block->size;

            heap_free_bytes -=
                block->size;

            return block_payload(block);
        }

        block = block->next;
    }

    return NULL;
}

void kfree(void *ptr)
{
    if (ptr == NULL)
        return;

    if (heap_first_block == NULL)
        return;

    /*
     * Only a pointer returned by kmalloc() may be freed.
     *
     * Walk the allocator's own block list and require
     * an exact payload match. This prevents forged/interior
     * pointers from being interpreted as block headers.
     */
    struct heap_block *block =
        heap_first_block;

    while (block != NULL)
    {
        if (block_payload(block) == ptr)
            break;

        block = block->next;
    }

    if (block == NULL)
        return;

    if (block->state != HEAP_BLOCK_USED)
        return;

    uintptr_t block_address =
        (uintptr_t)block;

    uintptr_t arena_start =
        (uintptr_t)&heap_arena[0];

    if (arena_start >
        UINTPTR_MAX - HEAP_ARENA_SIZE)
        return;

    uintptr_t arena_end =
        arena_start + HEAP_ARENA_SIZE;

    if (block_address < arena_start ||
        block_address >= arena_end)
        return;

    if (block->size == 0)
        return;

    if (block->size >
        HEAP_ARENA_SIZE -
        sizeof(struct heap_block))
        return;

    uintptr_t payload_start =
        (uintptr_t)block_payload(block);

    if (payload_start > arena_end ||
        block->size > arena_end - payload_start)
        return;

    block->state =
        HEAP_BLOCK_FREE;

    heap_allocated_bytes -=
        block->size;

    heap_free_bytes +=
        block->size;

    /*
     * Coalesce with the next free block.
     */
    if (block->next != NULL &&
        block->next->state ==
            HEAP_BLOCK_FREE)
    {
        struct heap_block *next =
            block->next;

        block->size +=
            sizeof(struct heap_block) +
            next->size;

        /*
         * The removed next-block header becomes
         * usable free payload capacity.
         */
        heap_free_bytes +=
            sizeof(struct heap_block);

        block->next =
            next->next;

        if (block->next != NULL)
            block->next->previous =
                block;
    }

    /*
     * Coalesce with the previous free block.
     */
    if (block->previous != NULL &&
        block->previous->state ==
            HEAP_BLOCK_FREE)
    {
        struct heap_block *previous =
            block->previous;

        previous->size +=
            sizeof(struct heap_block) +
            block->size;

        /*
         * The removed block header becomes
         * usable free payload capacity.
         */
        heap_free_bytes +=
            sizeof(struct heap_block);

        previous->next =
            block->next;

        if (previous->next != NULL)
            previous->next->previous =
                previous;
    }
}

/*
 * HEAP-1B.2
 *
 * Acquire one PMM/VMM-backed dynamic heap page.
 *
 * Lifecycle:
 *
 *     virtual address availability
 *              ↓
 *         PMM frame
 *              ↓
 *         VMM mapping
 *              ↓
 *       translation verify
 *              ↓
 *       page becomes owned
 *
 * kmalloc()/kfree() do not use this primitive yet.
 */
uint64_t heap_dynamic_page_acquire(void)
{
    /*
     * This milestone manages exactly one dynamic page.
     */
    if (heap_dynamic_page_mapped)
        return 0;

    uint64_t virtual_address =
        HEAP_DYNAMIC_PAGE_VIRTUAL;

    /*
     * The candidate virtual page must not already
     * have a mapping.
     */
    uint64_t existing_physical = 0;

    if (vmm_translate(
            vmm_get_pml4(),
            virtual_address,
            &existing_physical
        ) == 0)
    {
        return 0;
    }

    /*
     * Acquire the real physical backing frame.
     */
    uint64_t physical_address =
        pmm_alloc_frame();

    if (physical_address == 0)
        return 0;

    /*
     * Establish the kernel virtual mapping.
     */
    if (vmm_map_page(
            vmm_get_pml4(),
            virtual_address,
            physical_address,
            VMM_WRITABLE
        ) != 0)
    {
        /*
         * The physical frame was acquired by this
         * operation, so release it if mapping failed.
         */
        pmm_free_frame(
            physical_address
        );

        return 0;
    }

    /*
     * Verify the software translation before
     * publishing ownership.
     */
    uint64_t translated_physical = 0;

    if (vmm_translate(
            vmm_get_pml4(),
            virtual_address,
            &translated_physical
        ) != 0 ||
        translated_physical != physical_address)
    {
        uint64_t unmapped_physical = 0;

        if (vmm_unmap_page(
                vmm_get_pml4(),
                virtual_address,
                &unmapped_physical
            ) == 0)
        {
            if (unmapped_physical ==
                physical_address)
            {
                pmm_free_frame(
                    unmapped_physical
                );
            }
        }

        return 0;
    }

    heap_dynamic_page_physical =
        physical_address;

    heap_dynamic_page_virtual =
        virtual_address;

    heap_dynamic_page_mapped =
        1;

    return virtual_address;
}

/*
 * Release the currently owned dynamic heap page.
 *
 * Only the exact virtual address returned by
 * heap_dynamic_page_acquire() may be released.
 */
int heap_dynamic_page_release(
    uint64_t virtual_address
)
{
    if (!heap_dynamic_page_mapped)
        return -1;

    if (virtual_address !=
        heap_dynamic_page_virtual)
    {
        return -1;
    }

    uint64_t unmapped_physical = 0;

    if (vmm_unmap_page(
            vmm_get_pml4(),
            virtual_address,
            &unmapped_physical
        ) != 0)
    {
        return -1;
    }

    /*
     * The returned physical frame must be exactly
     * the frame owned by this heap-page record.
     */
    if (unmapped_physical !=
        heap_dynamic_page_physical)
    {
        /*
         * Do not release an unexpected frame.
         *
         * This indicates an ownership/integrity failure.
         */
        return -1;
    }

    pmm_free_frame(
        unmapped_physical
    );

    heap_dynamic_page_physical = 0;
    heap_dynamic_page_virtual = 0;
    heap_dynamic_page_mapped = 0;

    return 0;
}
