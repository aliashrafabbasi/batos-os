#include "pmm.h"
#include <stddef.h>
#include "../../../limine.h"

#define PAGE_SIZE 4096ULL
#define PAGE_MASK (PAGE_SIZE - 1)

#define BITMAP_USED 1
#define BITMAP_FREE 0

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request
    limine_memmap_request = {
        .id = LIMINE_MEMMAP_REQUEST_ID,
        .revision = 0,
        .response = NULL
    };

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request
    limine_hhdm_request = {
        .id = LIMINE_HHDM_REQUEST_ID,
        .revision = 0,
        .response = NULL
    };

static uint8_t *frame_bitmap = 0;

static uint64_t hhdm_offset = 0;
static uint64_t bitmap_physical = 0;
static uint64_t bitmap_size = 0;

static uint64_t total_frames = 0;
static uint64_t free_frames = 0;

static uint64_t align_up(uint64_t value)
{
    return (value + PAGE_MASK) & ~PAGE_MASK;
}

static uint64_t align_down(uint64_t value)
{
    return value & ~PAGE_MASK;
}

static uint64_t physical_to_virtual(uint64_t physical)
{
    return physical + hhdm_offset;
}

static void bitmap_set(uint64_t frame)
{
    frame_bitmap[frame / 8] |=
        (uint8_t)(1U << (frame % 8));
}

static void bitmap_clear(uint64_t frame)
{
    frame_bitmap[frame / 8] &=
        (uint8_t)~(1U << (frame % 8));
}

static uint8_t bitmap_test(uint64_t frame)
{
    return
        (uint8_t)(
            frame_bitmap[frame / 8] &
            (1U << (frame % 8))
        );
}

static void bitmap_set_range(
    uint64_t start_frame,
    uint64_t frame_count
)
{
    for (uint64_t i = 0; i < frame_count; i++)
    {
        bitmap_set(start_frame + i);
    }
}

static void bitmap_clear_range(
    uint64_t start_frame,
    uint64_t frame_count
)
{
    for (uint64_t i = 0; i < frame_count; i++)
    {
        uint64_t frame = start_frame + i;

        if (bitmap_test(frame))
        {
            bitmap_clear(frame);
            free_frames++;
        }
    }
}

static uint64_t find_bitmap_location(
    uint64_t bitmap_required_size
)
{
    if (!limine_memmap_request.response)
        return 0;

    for (uint64_t i = 0;
         i < limine_memmap_request.response->entry_count;
         i++)
    {
        struct limine_memmap_entry *entry =
            limine_memmap_request.response->entries[i];

        if (entry->type != LIMINE_MEMMAP_USABLE)
            continue;

        uint64_t start =
            align_up(entry->base);

        uint64_t end =
            align_down(entry->base + entry->length);

        if (end <= start)
            continue;

        if (end - start >= bitmap_required_size)
            return start;
    }

    return 0;
}

void pmm_init(void)
{
    if (!limine_memmap_request.response)
        return;

    if (!limine_hhdm_request.response)
        return;

    hhdm_offset =
        limine_hhdm_request.response->offset;

    uint64_t highest_address = 0;

    /*
       Find the highest physical address described
       by the Limine memory map.
    */
    for (uint64_t i = 0;
         i < limine_memmap_request.response->entry_count;
         i++)
    {
        struct limine_memmap_entry *entry =
            limine_memmap_request.response->entries[i];

        uint64_t end =
            entry->base + entry->length;

        if (end > highest_address)
            highest_address = end;
    }

    total_frames =
        (highest_address + PAGE_MASK) / PAGE_SIZE;

    if (total_frames == 0)
        return;

    bitmap_size =
        (total_frames + 7) / 8;

    uint64_t bitmap_allocation_size =
        align_up(bitmap_size);

    bitmap_physical =
        find_bitmap_location(
            bitmap_allocation_size
        );

    if (bitmap_physical == 0)
        return;

    frame_bitmap =
        (uint8_t *)physical_to_virtual(
            bitmap_physical
        );

    /*
       Start with every frame marked USED.
    */
    for (uint64_t i = 0;
         i < bitmap_size;
         i++)
    {
        frame_bitmap[i] = 0xFF;
    }

    free_frames = 0;

    /*
       Only LIMINE_MEMMAP_USABLE memory becomes
       available to BATOS.
    */
    for (uint64_t i = 0;
         i < limine_memmap_request.response->entry_count;
         i++)
    {
        struct limine_memmap_entry *entry =
            limine_memmap_request.response->entries[i];

        if (entry->type != LIMINE_MEMMAP_USABLE)
            continue;

        uint64_t start =
            align_up(entry->base);

        uint64_t end =
            align_down(entry->base + entry->length);

        if (end <= start)
            continue;

        uint64_t first_frame =
            start / PAGE_SIZE;

        uint64_t frame_count =
            (end - start) / PAGE_SIZE;

        bitmap_clear_range(
            first_frame,
            frame_count
        );
    }

    /*
       Reserve the physical frames occupied by
       the PMM bitmap itself.
    */
    uint64_t bitmap_first_frame =
        bitmap_physical / PAGE_SIZE;

    uint64_t bitmap_frame_count =
        bitmap_allocation_size / PAGE_SIZE;

    bitmap_set_range(
        bitmap_first_frame,
        bitmap_frame_count
    );

    /*
       Never allow physical frame 0 to be allocated.
    */
    if (total_frames > 0 &&
        !bitmap_test(0))
    {
        bitmap_set(0);

        if (free_frames > 0)
            free_frames--;
    }
}

uint64_t pmm_alloc_frame(void)
{
    if (!frame_bitmap)
        return 0;

    for (uint64_t frame = 1;
         frame < total_frames;
         frame++)
    {
        if (!bitmap_test(frame))
        {
            bitmap_set(frame);

            if (free_frames > 0)
                free_frames--;

            return frame * PAGE_SIZE;
        }
    }

    return 0;
}

void pmm_free_frame(uint64_t physical_address)
{
    if (!frame_bitmap)
        return;

    if (physical_address == 0)
        return;

    if (physical_address & PAGE_MASK)
        return;

    uint64_t frame =
        physical_address / PAGE_SIZE;

    if (frame >= total_frames)
        return;

    if (!bitmap_test(frame))
        return;

    /*
       Never free the PMM bitmap's own frames.
    */
    uint64_t bitmap_first_frame =
        bitmap_physical / PAGE_SIZE;

    uint64_t bitmap_frame_count =
        align_up(bitmap_size) / PAGE_SIZE;

    if (frame >= bitmap_first_frame &&
        frame < bitmap_first_frame + bitmap_frame_count)
    {
        return;
    }

    bitmap_clear(frame);
    free_frames++;
}

uint64_t pmm_get_total_frames(void)
{
    return total_frames;
}

uint64_t pmm_get_free_frames(void)
{
    return free_frames;
}

uint64_t pmm_get_used_frames(void)
{
    return total_frames - free_frames;
}

uint64_t pmm_get_bitmap_physical(void)
{
    return bitmap_physical;
}

uint64_t pmm_get_bitmap_size(void)
{
    return bitmap_size;
}

uint64_t pmm_get_hhdm_offset(void)
{
    return hhdm_offset;
}

int pmm_is_physical_range_valid(
    uint64_t physical,
    uint64_t length
)
{
    if (length == 0)
        return 0;

    if (!limine_memmap_request.response)
        return 0;

    /*
     * Validate physical + length without unsigned overflow.
     */
    if (physical > UINT64_MAX - length)
        return 0;

    uint64_t range_end =
        physical + length;

    for (uint64_t i = 0;
         i < limine_memmap_request.response->entry_count;
         i++)
    {
        struct limine_memmap_entry *entry =
            limine_memmap_request.response->entries[i];

        if (entry == 0)
            continue;

        if (entry->length == 0)
            continue;

        /*
         * Ignore malformed memory-map entries whose
         * base + length would overflow.
         */
        if (entry->base > UINT64_MAX - entry->length)
            continue;

        uint64_t entry_end =
            entry->base + entry->length;

        /*
         * The complete requested range must be contained
         * inside one physical memory-map entry.
         */
        if (physical >= entry->base &&
            range_end <= entry_end)
        {
            return 1;
        }
    }

    return 0;
}
