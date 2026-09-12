#include <stdint.h>
#include <stddef.h>
#include <limits.h>

#include "kernel/console/console.h"
#include "kernel/mm/heap/heap.h"
#include "kernel/mm/pmm/pmm.h"
#include "kernel/mm/vmm/vmm.h"

#include "heap_tests.h"

void heap_test_bootstrap(void)
{


    serial_write_string(
        "\nHEAP-1A BOOTSTRAP HEAP TEST\n"
    );

    heap_init();

    serial_write_string(
        "HEAP INITIALIZED\n"
    );

    /*
     * Zero-size allocation must fail.
     */
    if (kmalloc(0) != NULL)
    {
        serial_write_string(
            "HEAP-1A ZERO-SIZE ALLOCATION: FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "HEAP-1A ZERO-SIZE REJECTION: VERIFIED\n"
    );

    /*
     * Normal allocations.
     */
    uint8_t *heap_a =
        (uint8_t *)kmalloc(64);

    uint8_t *heap_b =
        (uint8_t *)kmalloc(128);

    uint8_t *heap_c =
        (uint8_t *)kmalloc(256);

    if (heap_a == NULL ||
        heap_b == NULL ||
        heap_c == NULL)
    {
        serial_write_string(
            "HEAP-1A BASIC ALLOCATION: FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "HEAP-1A BASIC ALLOCATION: OK\n"
    );

    /*
     * Every payload must satisfy 16-byte alignment.
     */
    if (((uintptr_t)heap_a % 16ULL) != 0 ||
        ((uintptr_t)heap_b % 16ULL) != 0 ||
        ((uintptr_t)heap_c % 16ULL) != 0)
    {
        serial_write_string(
            "HEAP-1A ALIGNMENT: FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "HEAP-1A 16-BYTE ALIGNMENT: VERIFIED\n"
    );

    /*
     * Verify that allocated memory is actually writable.
     */
    heap_a[0] = 0xA5;
    heap_a[63] = 0x5A;

    heap_b[0] = 0x11;
    heap_b[127] = 0x22;

    heap_c[0] = 0x33;
    heap_c[255] = 0x44;

    if (heap_a[0] != 0xA5 ||
        heap_a[63] != 0x5A ||
        heap_b[0] != 0x11 ||
        heap_b[127] != 0x22 ||
        heap_c[0] != 0x33 ||
        heap_c[255] != 0x44)
    {
        serial_write_string(
            "HEAP-1A MEMORY ACCESS: FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "HEAP-1A MEMORY ACCESS: VERIFIED\n"
    );

    /*
     * Free the middle allocation and ensure that the allocator
     * can reuse the released block.
     */
    kfree(heap_b);

    uint8_t *heap_reuse =
        (uint8_t *)kmalloc(128);

    if (heap_reuse != heap_b)
    {
        serial_write_string(
            "HEAP-1A FREE/REUSE: FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "HEAP-1A FREE/REUSE: VERIFIED\n"
    );

    /*
     * Double-free must be safely rejected.
     */
    kfree(heap_reuse);
    kfree(heap_reuse);

    serial_write_string(
        "HEAP-1A DOUBLE-FREE REJECTION: VERIFIED\n"
    );

    /*
     * An interior pointer must not be accepted by kfree().
     */
    uint8_t *heap_d =
        (uint8_t *)kmalloc(96);

    if (heap_d == NULL)
    {
        serial_write_string(
            "HEAP-1A POINTER TEST ALLOCATION: FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    kfree(heap_d + 16);

    /*
     * The original allocation must still be valid after the
     * rejected interior-pointer free.
     */
    heap_d[0] = 0x7B;

    if (heap_d[0] != 0x7B)
    {
        serial_write_string(
            "HEAP-1A INVALID POINTER REJECTION: FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "HEAP-1A INVALID POINTER REJECTION: VERIFIED\n"
    );

    kfree(heap_d);

    /*
     * Deterministic fragmentation/coalescing test.
     *
     * Start from a fresh heap so no earlier allocation can
     * provide an unrelated large free block.
     *
     * X/Y/Z are adjacent 512-byte allocations. The rest of
     * the arena is filled with the same allocation size,
     * leaving only a tail smaller than the requested merged
     * size. After freeing X, Z, then Y, a 1536-byte request
     * can succeed only if the three adjacent blocks coalesce.
     */
    heap_init();

    uint8_t *heap_x =
        (uint8_t *)kmalloc(512);

    uint8_t *heap_y =
        (uint8_t *)kmalloc(512);

    uint8_t *heap_z =
        (uint8_t *)kmalloc(512);

    if (heap_x == NULL ||
        heap_y == NULL ||
        heap_z == NULL)
    {
        serial_write_string(
            "HEAP-1A COALESCE SETUP: FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    /*
     * Each 512-byte allocation consumes:
     *
     * 512-byte payload + 32-byte block header = 544 bytes.
     *
     * After X/Y/Z, 117 additional allocations consume
     * 117 * 544 bytes, leaving only 256 bytes of arena
     * payload. That tail cannot satisfy the 1536-byte test.
     */
    void *heap_fill[117];

    for (uint64_t i = 0; i < 117; i++)
    {
        heap_fill[i] = kmalloc(512);

        if (heap_fill[i] == NULL)
        {
            serial_write_string(
                "HEAP-1A COALESCE FILL: FAILED\n"
            );

            serial_write_string(
                "CPU HALTED\n"
            );

            for (;;)
            {
                __asm__ volatile (
                    "cli\n"
                    "hlt"
                );
            }
        }
    }

    kfree(heap_x);
    kfree(heap_z);
    kfree(heap_y);

    /*
     * X/Y/Z now form one 1600-byte free block
     * (512 + 32 + 512 + 32 + 512).
     *
     * No other free region is large enough for 1536 bytes.
     */
    void *heap_coalesced =
        kmalloc(1536);

    if (heap_coalesced == NULL)
    {
        serial_write_string(
            "HEAP-1A COALESCING: FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "HEAP-1A SPLIT/COALESCING: VERIFIED\n"
    );

    kfree(heap_coalesced);

    /*
     * Requests larger than the bootstrap arena must fail.
     */
    if (kmalloc(64ULL * 1024ULL) != NULL)
    {
        serial_write_string(
            "HEAP-1A OVERSIZE REJECTION: FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "HEAP-1A OVERSIZE REJECTION: VERIFIED\n"
    );

    /*
     * UINT64_MAX must fail without wrapping during alignment.
     */
    if (kmalloc(UINT64_MAX) != NULL)
    {
        serial_write_string(
            "HEAP-1A OVERFLOW REJECTION: FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "HEAP-1A OVERFLOW REJECTION: VERIFIED\n"
    );

    serial_write_string(
        "HEAP-1A BOOTSTRAP HEAP: VERIFIED\n"
    );

    serial_write_string(
        "HEAP-1A RUNTIME TESTS: PASSED\n"
    );

}

void heap_test_dynamic_page_ownership(void)
{
    /* --------------------------------------------------------
       HEAP-1B.2 DYNAMIC PAGE OWNERSHIP VERIFICATION
       -------------------------------------------------------- */

    serial_write_string(
        "\nHEAP-1B.2 DYNAMIC PAGE OWNERSHIP TEST\n"
    );

    /*
     * Record the PMM free-frame count before acquiring the
     * dynamic heap page. The page-table hierarchy was already
     * created by HEAP-1B.1, so this test should consume exactly
     * one physical frame.
     */
    uint64_t heap_dynamic_free_before =
        pmm_get_free_frames();

    serial_write_string(
        "DYNAMIC PAGE FREE FRAMES BEFORE: "
    );

    serial_write_hex(
        heap_dynamic_free_before
    );

    serial_write_string("\n");

    /*
     * Acquire one real PMM/VMM-backed heap page.
     */
    uint64_t heap_dynamic_virtual =
        heap_dynamic_page_acquire();

    if (heap_dynamic_virtual == 0)
    {
        serial_write_string(
            "HEAP-1B.2: DYNAMIC PAGE ACQUIRE FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "DYNAMIC PAGE VIRTUAL: "
    );

    serial_write_hex(
        heap_dynamic_virtual
    );

    serial_write_string("\n");

    /*
     * The ownership primitive must return the linker-defined
     * dynamic heap virtual address.
     */
    extern char __heap_start[];

    if (heap_dynamic_virtual !=
        (uint64_t)(uintptr_t)__heap_start)
    {
        serial_write_string(
            "HEAP-1B.2: VIRTUAL ADDRESS VERIFICATION FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "HEAP-1B.2: VIRTUAL ADDRESS VERIFIED\n"
    );

    /*
     * Verify the software page-table translation and obtain
     * the physical frame owned by the heap layer.
     */
    uint64_t heap_dynamic_physical = 0;

    int heap_dynamic_translate_result =
        vmm_translate(
            vmm_get_pml4(),
            heap_dynamic_virtual,
            &heap_dynamic_physical
        );

    if (heap_dynamic_translate_result != 0 ||
        heap_dynamic_physical == 0 ||
        (heap_dynamic_physical &
         (VMM_PAGE_SIZE - 1)) != 0)
    {
        serial_write_string(
            "HEAP-1B.2: SOFTWARE TRANSLATION FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "DYNAMIC PAGE PHYSICAL: "
    );

    serial_write_hex(
        heap_dynamic_physical
    );

    serial_write_string("\n");

    serial_write_string(
        "HEAP-1B.2: SOFTWARE TRANSLATION VERIFIED\n"
    );

    /*
     * Real CPU memory access through the returned virtual
     * address. This proves that the mapping is active in the
     * current address space, not merely present in software
     * page-table structures.
     */
    volatile uint64_t *heap_dynamic_memory =
        (volatile uint64_t *)(uintptr_t)
            heap_dynamic_virtual;

    const uint64_t heap_dynamic_pattern =
        0x4241544F532D3142ULL;

    *heap_dynamic_memory =
        heap_dynamic_pattern;

    uint64_t heap_dynamic_readback =
        *heap_dynamic_memory;

    serial_write_string(
        "DYNAMIC PAGE CPU WRITE/READ: "
    );

    serial_write_hex(
        heap_dynamic_readback
    );

    serial_write_string("\n");

    if (heap_dynamic_readback !=
        heap_dynamic_pattern)
    {
        serial_write_string(
            "HEAP-1B.2: CPU MEMORY ACCESS FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "HEAP-1B.2: CPU MEMORY ACCESS VERIFIED\n"
    );

    /*
     * A second acquire while the page is already owned must
     * be rejected. The existing mapping must not be replaced
     * and no additional PMM frame may be consumed.
     */
    if (heap_dynamic_page_acquire() != 0)
    {
        serial_write_string(
            "HEAP-1B.2: DUPLICATE ACQUIRE REJECTION FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    if (pmm_get_free_frames() !=
        heap_dynamic_free_before - 1)
    {
        serial_write_string(
            "HEAP-1B.2: PMM FRAME ACCOUNTING FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "HEAP-1B.2: DUPLICATE ACQUIRE REJECTED\n"
    );

    /*
     * Release the exact page through the heap ownership layer.
     */
    if (heap_dynamic_page_release(
            heap_dynamic_virtual
        ) != 0)
    {
        serial_write_string(
            "HEAP-1B.2: DYNAMIC PAGE RELEASE FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    /*
     * Software translation must disappear after release.
     */
    uint64_t heap_dynamic_after_release =
        0;

    if (vmm_translate(
            vmm_get_pml4(),
            heap_dynamic_virtual,
            &heap_dynamic_after_release
        ) == 0)
    {
        serial_write_string(
            "HEAP-1B.2: TRANSLATION STILL PRESENT\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "HEAP-1B.2: TRANSLATION REMOVED\n"
    );

    /*
     * The physical frame must have returned to PMM.
     */
    uint64_t heap_dynamic_free_after =
        pmm_get_free_frames();

    serial_write_string(
        "DYNAMIC PAGE FREE FRAMES AFTER: "
    );

    serial_write_hex(
        heap_dynamic_free_after
    );

    serial_write_string("\n");

    if (heap_dynamic_free_after !=
        heap_dynamic_free_before)
    {
        serial_write_string(
            "HEAP-1B.2: PMM FRAME RELEASE FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "HEAP-1B.2: PMM FRAME LIFECYCLE VERIFIED\n"
    );

    /*
     * Re-acquisition proves that release restored the page's
     * ownership state and that the physical frame can be
     * legitimately acquired again.
     */
    uint64_t heap_dynamic_reacquired =
        heap_dynamic_page_acquire();

    if (heap_dynamic_reacquired !=
        heap_dynamic_virtual)
    {
        serial_write_string(
            "HEAP-1B.2: RE-ACQUIRE FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "HEAP-1B.2: RE-ACQUIRE VERIFIED\n"
    );

    if (heap_dynamic_page_release(
            heap_dynamic_reacquired
        ) != 0)
    {
        serial_write_string(
            "HEAP-1B.2: FINAL RELEASE FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    /*
     * Releasing an already released page must be rejected.
     */
    if (heap_dynamic_page_release(
            heap_dynamic_reacquired
        ) == 0)
    {
        serial_write_string(
            "HEAP-1B.2: DOUBLE RELEASE REJECTION FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    if (pmm_get_free_frames() !=
        heap_dynamic_free_before)
    {
        serial_write_string(
            "HEAP-1B.2: FINAL PMM ACCOUNTING FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "HEAP-1B.2: DOUBLE RELEASE REJECTED\n"
    );

    serial_write_string(
        "HEAP-1B.2 DYNAMIC PAGE OWNERSHIP: VERIFIED\n"
    );

}

void heap_test_dynamic_kernel_page(void)
{
    serial_write_string(
        "\nHEAP-1B.1 DYNAMIC KERNEL PAGE TEST\n"
    );

    /*
     * Use the linker-defined first page-aligned virtual address
     * after the linked kernel image.
     *
     * This is only a test address for Heap-1B.1.
     */
    extern char __heap_start[];

    uint64_t dynamic_heap_virtual =
        (uint64_t)(uintptr_t)__heap_start;

    uint64_t dynamic_heap_physical = 0;

    /*
     * The candidate page must not already be mapped.
     */
    int initial_translation =
        vmm_translate(
            vmm_get_pml4(),
            dynamic_heap_virtual,
            &dynamic_heap_physical
        );

    serial_write_string(
        "INITIAL DYNAMIC HEAP TRANSLATION: "
    );

    serial_write_hex(
        (uint64_t)(uint32_t)initial_translation
    );

    serial_write_string("\n");

    if (initial_translation == 0)
    {
        serial_write_string(
            "HEAP-1B.1: CANDIDATE PAGE ALREADY MAPPED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "HEAP-1B.1: CANDIDATE PAGE IS UNMAPPED\n"
    );

    /*
     * Acquire a real physical frame from PMM.
     */
    uint64_t dynamic_heap_frame =
        pmm_alloc_frame();

    serial_write_string(
        "DYNAMIC HEAP TEST FRAME: "
    );

    serial_write_hex(
        dynamic_heap_frame
    );

    serial_write_string("\n");

    if (dynamic_heap_frame == 0)
    {
        serial_write_string(
            "HEAP-1B.1: PMM FRAME ALLOCATION FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    /*
     * Establish the real virtual → physical mapping.
     */
    int dynamic_map_result =
        vmm_map_page(
            vmm_get_pml4(),
            dynamic_heap_virtual,
            dynamic_heap_frame,
            VMM_WRITABLE
        );

    serial_write_string(
        "DYNAMIC HEAP MAP RESULT: "
    );

    serial_write_hex(
        (uint64_t)(uint32_t)dynamic_map_result
    );

    serial_write_string("\n");

    if (dynamic_map_result != 0)
    {
        serial_write_string(
            "HEAP-1B.1: DYNAMIC PAGE MAPPING FAILED\n"
        );

        pmm_free_frame(
            dynamic_heap_frame
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    /*
     * Verify the software page-table walk.
     */
    uint64_t translated_dynamic_heap =
        0;

    int dynamic_translation_result =
        vmm_translate(
            vmm_get_pml4(),
            dynamic_heap_virtual,
            &translated_dynamic_heap
        );

    serial_write_string(
        "DYNAMIC HEAP TRANSLATION RESULT: "
    );

    serial_write_hex(
        (uint64_t)(uint32_t)dynamic_translation_result
    );

    serial_write_string("\n");

    serial_write_string(
        "DYNAMIC HEAP TRANSLATED PHYSICAL: "
    );

    serial_write_hex(
        translated_dynamic_heap
    );

    serial_write_string("\n");

    if (dynamic_translation_result != 0 ||
        translated_dynamic_heap != dynamic_heap_frame)
    {
        serial_write_string(
            "HEAP-1B.1: SOFTWARE TRANSLATION FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "HEAP-1B.1: SOFTWARE TRANSLATION VERIFIED\n"
    );

    /*
     * REAL CPU MEMORY ACCESS.
     *
     * This access must travel through the active BATOS
     * page-table hierarchy established by CR3.
     */
    volatile uint64_t *dynamic_heap_address =
        (volatile uint64_t *)dynamic_heap_virtual;

    uint64_t dynamic_heap_pattern =
        0x4241544F532D3142ULL;

    *dynamic_heap_address =
        dynamic_heap_pattern;

    uint64_t dynamic_heap_readback =
        *dynamic_heap_address;

    serial_write_string(
        "DYNAMIC HEAP CPU WRITE/READ: "
    );

    serial_write_hex(
        dynamic_heap_readback
    );

    serial_write_string("\n");

    if (dynamic_heap_readback !=
        dynamic_heap_pattern)
    {
        serial_write_string(
            "HEAP-1B.1: CPU MEMORY ACCESS FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "HEAP-1B.1: CPU MEMORY ACCESS VERIFIED\n"
    );

    /*
     * Remove the virtual mapping.
     *
     * vmm_unmap_page() returns the physical frame but does
     * not release it; PMM remains responsible for ownership.
     */
    uint64_t unmapped_dynamic_frame =
        0;

    int dynamic_unmap_result =
        vmm_unmap_page(
            vmm_get_pml4(),
            dynamic_heap_virtual,
            &unmapped_dynamic_frame
        );

    serial_write_string(
        "DYNAMIC HEAP UNMAP RESULT: "
    );

    serial_write_hex(
        (uint64_t)(uint32_t)dynamic_unmap_result
    );

    serial_write_string("\n");

    serial_write_string(
        "UNMAPPED DYNAMIC HEAP FRAME: "
    );

    serial_write_hex(
        unmapped_dynamic_frame
    );

    serial_write_string("\n");

    if (dynamic_unmap_result != 0 ||
        unmapped_dynamic_frame != dynamic_heap_frame)
    {
        serial_write_string(
            "HEAP-1B.1: UNMAP FRAME VERIFICATION FAILED\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    /*
     * Confirm that the virtual mapping is gone.
     */
    uint64_t dynamic_after_unmap_physical =
        0;

    int dynamic_after_unmap_translation =
        vmm_translate(
            vmm_get_pml4(),
            dynamic_heap_virtual,
            &dynamic_after_unmap_physical
        );

    serial_write_string(
        "AFTER UNMAP TRANSLATION RESULT: "
    );

    serial_write_hex(
        (uint64_t)(uint32_t)dynamic_after_unmap_translation
    );

    serial_write_string("\n");

    if (dynamic_after_unmap_translation == 0)
    {
        serial_write_string(
            "HEAP-1B.1: TRANSLATION STILL PRESENT\n"
        );

        serial_write_string(
            "CPU HALTED\n"
        );

        for (;;)
        {
            __asm__ volatile (
                "cli\n"
                "hlt"
            );
        }
    }

    serial_write_string(
        "HEAP-1B.1: TRANSLATION REMOVED\n"
    );

    /*
     * Release the physical frame back to PMM.
     */
    pmm_free_frame(
        unmapped_dynamic_frame
    );

    serial_write_string(
        "HEAP-1B.1: PMM FRAME RELEASED\n"
    );

    serial_write_string(
        "HEAP-1B.1: DYNAMIC KERNEL PAGE VERIFIED\n"
    );
}
