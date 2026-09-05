#include <stdint.h>
#include "../limine.h"

__attribute__((used, section(".limine_requests")))
static volatile uint64_t base_revision[] =
    LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t requests_start_marker[] =
    LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t requests_end_marker[] =
    LIMINE_REQUESTS_END_MARKER;

static void hcf(void)
{
    for (;;)
    {
        __asm__ volatile ("hlt");
    }
}

void kernel_main(void)
{
    if (!LIMINE_BASE_REVISION_SUPPORTED(base_revision))
        hcf();

    hcf();
}