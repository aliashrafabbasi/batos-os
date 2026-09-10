#include "kernel/arch/x86_64/heap.h"

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
