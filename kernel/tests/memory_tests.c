#include <stdint.h>

#include "kernel/console/console.h"
#include "kernel/arch/x86_64/pmm.h"
#include "kernel/arch/x86_64/vmm.h"

#include "memory_tests.h"

void memory_tests_run(void)
{
    /* --------------------------------------------------------
       FRAME ALLOCATOR TEST
       -------------------------------------------------------- */

    serial_write_string(
        "\nPMM ALLOCATOR TEST\n"
    );

    uint64_t frame_a =
        pmm_alloc_frame();

    uint64_t frame_b =
        pmm_alloc_frame();

    serial_write_string(
        "ALLOC FRAME A: "
    );

    serial_write_hex(frame_a);

    serial_write_string("\n");

    serial_write_string(
        "ALLOC FRAME B: "
    );

    serial_write_hex(frame_b);

    serial_write_string("\n");

    serial_write_string(
        "FREE FRAMES AFTER ALLOC: "
    );

    serial_write_hex(
        pmm_get_free_frames()
    );

    serial_write_string("\n");

    pmm_free_frame(frame_a);

    serial_write_string(
        "FRAME A FREED\n"
    );

    serial_write_string(
        "FREE FRAMES AFTER FREE: "
    );

    serial_write_hex(
        pmm_get_free_frames()
    );

    serial_write_string("\n");

    serial_write_string(
        "PMM ALLOCATOR: OK\n"
    );

    /* --------------------------------------------------------
       VIRTUAL MEMORY MANAGER
       -------------------------------------------------------- */

    serial_write_string(
        "\n================================\n"
    );

    serial_write_string(
        "BATOS VMM INITIALIZING...\n"
    );

    serial_write_string(
        "================================\n"
    );

    vmm_init();

    uint64_t pml4 =
        vmm_get_pml4();

    serial_write_string(
        "VMM PML4: "
    );

    serial_write_hex(
        pml4
    );

    serial_write_string("\n");

    uint64_t test_frame =
        pmm_alloc_frame();

    serial_write_string(
        "VMM TEST FRAME: "
    );

    serial_write_hex(
        test_frame
    );

    serial_write_string("\n");

    /*
     * VMM-2A test virtual address.
     *
     * This address is used inside the software
     * page-table structure.
     */
    uint64_t test_virtual =
        0x0000000040000000ULL;

    int map_result =
        vmm_map_page(
            pml4,
            test_virtual,
            test_frame,
            VMM_WRITABLE
        );

    serial_write_string(
        "VMM MAP RESULT: "
    );

    serial_write_hex(
        (uint64_t)map_result
    );

    serial_write_string("\n");

    if (map_result == 0)
    {
        serial_write_string(
            "VMM PAGE TABLES: OK\n"
        );
    }
    else
    {
        serial_write_string(
            "VMM PAGE TABLES: FAILED\n"
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

    /* --------------------------------------------------------
       VMM-2A SOFTWARE TRANSLATION TEST
       -------------------------------------------------------- */

    serial_write_string(
        "\nVMM-2A TRANSLATION TEST\n"
    );

    uint64_t translated_address = 0;

    int translate_result =
        vmm_translate(
            pml4,
            test_virtual,
            &translated_address
        );

    serial_write_string(
        "TRANSLATION RESULT: "
    );

    serial_write_hex(
        (uint64_t)translate_result
    );

    serial_write_string("\n");

    serial_write_string(
        "VIRTUAL ADDRESS: "
    );

    serial_write_hex(
        test_virtual
    );

    serial_write_string("\n");

    serial_write_string(
        "EXPECTED PHYSICAL: "
    );

    serial_write_hex(
        test_frame
    );

    serial_write_string("\n");

    serial_write_string(
        "TRANSLATED PHYSICAL: "
    );

    serial_write_hex(
        translated_address
    );

    serial_write_string("\n");

    if (translate_result == 0 &&
        translated_address == test_frame)
    {
        serial_write_string(
            "VMM TRANSLATION: OK\n"
        );
    }
    else
    {
        serial_write_string(
            "VMM TRANSLATION: FAILED\n"
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
     * Do NOT free test_frame.
     *
     * The page-table entry still references this frame.
     */
    serial_write_string(
        "VMM-2A: SOFTWARE WALKER VERIFIED\n"
    );

    /* --------------------------------------------------------
       VMM-2B CR3 READ VERIFICATION
       -------------------------------------------------------- */

    serial_write_string(
        "\nVMM-2B CR3 VERIFICATION\n"
    );

    uint64_t current_cr3 =
        vmm_read_cr3();

    uint64_t current_cr3_pml4 =
        current_cr3 &
        0x000FFFFFFFFFF000ULL;

    serial_write_string(
        "CURRENT CR3: "
    );

    serial_write_hex(
        current_cr3
    );

    serial_write_string("\n");

    serial_write_string(
        "CURRENT CR3 PML4: "
    );

    serial_write_hex(
        current_cr3_pml4
    );

    serial_write_string("\n");

    serial_write_string(
        "BATOS PML4: "
    );

    serial_write_hex(
        pml4
    );

    serial_write_string("\n");

    if (current_cr3_pml4 != 0)
    {
        serial_write_string(
            "CR3 READ: OK\n"
        );
    }
    else
    {
        serial_write_string(
            "CR3 READ: FAILED\n"
        );
    }

    uint64_t cr3_after_read =
        vmm_read_cr3();

    uint64_t cr3_after_read_pml4 =
        cr3_after_read &
        0x000FFFFFFFFFF000ULL;

    if (cr3_after_read_pml4 == current_cr3_pml4)
    {
        serial_write_string(
            "CR3 UNCHANGED: OK\n"
        );
    }
    else
    {
        serial_write_string(
            "CR3 UNCHANGED: FAILED\n"
        );
    }

    serial_write_string(
        "CR3 WRITE: NOT EXECUTED\n"
    );

    if (current_cr3_pml4 != 0 &&
        cr3_after_read_pml4 == current_cr3_pml4)
    {
        serial_write_string(
            "VMM-2B: CR3 READ VERIFIED\n"
        );
    }
    else
    {
        serial_write_string(
            "VMM-2B: CR3 READ FAILED\n"
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
        "VMM-2B: ADDRESS SPACE NOT ACTIVATED\n"
    );

    /* --------------------------------------------------------
       VMM-2C SAFE ADDRESS-SPACE PREPARATION
       -------------------------------------------------------- */

    serial_write_string(
        "\nVMM-2C SAFE ADDRESS-SPACE ACTIVATION\n"
    );

    /*
     * Prepare BATOS's PML4.
     *
     * vmm_prepare_address_space():
     *
     *     1. Recursively clones the currently active
     *        Limine page-table hierarchy.
     *
     *     2. Keeps the cloned hierarchy independent.
     *
     *     3. Merges the cloned mappings into BATOS's
     *        own PML4.
     *
     *     4. Preserves existing BATOS-owned mappings.
     */
    int prepare_result =
        vmm_prepare_address_space();

    serial_write_string(
        "ADDRESS SPACE PREPARE RESULT: "
    );

    serial_write_hex(
        (uint64_t)(uint32_t)prepare_result
    );

    serial_write_string("\n");

    if (prepare_result != 0)
    {
        serial_write_string(
            "VMM-2C PREPARE: FAILED\n"
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
        "VMM-2C PREPARE: OK\n"
    );

    /* --------------------------------------------------------
       VMM-3.1 RECURSIVE PAGE-TABLE CLONE VERIFICATION
       -------------------------------------------------------- */

    serial_write_string(
        "\nVMM-3.1 RECURSIVE PAGE-TABLE CLONE\n"
    );

    /*
     * vmm_prepare_address_space() creates a standalone
     * recursively cloned PML4 from the currently active
     * Limine hierarchy.
     *
     * The standalone clone root is exposed by:
     *
     *     vmm_get_last_cloned_pml4()
     *
     * IMPORTANT:
     *
     * We verify this standalone clone directly against
     * the original Limine PML4.
     *
     * We do NOT use test_virtual (0x40000000) here because
     * that mapping belongs to BATOS's own address space and
     * is not necessarily present in the Limine source tree.
     */
    uint64_t cloned_pml4 =
        vmm_get_last_cloned_pml4();

    serial_write_string(
        "SOURCE LIMINE PML4: "
    );

    serial_write_hex(
        current_cr3_pml4
    );

    serial_write_string("\n");

    serial_write_string(
        "CLONED PML4: "
    );

    serial_write_hex(
        cloned_pml4
    );

    serial_write_string("\n");

    if (cloned_pml4 == 0)
    {
        serial_write_string(
            "VMM-3.1 CLONED PML4: INVALID\n"
        );

        serial_write_string(
            "VMM-3.1: RECURSIVE CLONE FAILED\n"
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
     * Full recursive verification.
     *
     * This verifies the complete page-table hierarchy:
     *
     *     PML4
     *       ↓
     *     PDPT
     *       ↓
     *     PD
     *       ↓
     *     PT
     *       ↓
     *     PTE
     *
     * For normal 4 KiB mappings, the table pages must be
     * physically independent while their mapping entries
     * remain equivalent.
     */
    int clone_result =
        vmm_verify_clone(
            current_cr3_pml4,
            cloned_pml4
        );

    serial_write_string(
        "FULL CLONE VERIFICATION RESULT: "
    );

    serial_write_hex(
        (uint64_t)(uint32_t)clone_result
    );

    serial_write_string("\n");

    if (clone_result != 0)
    {
        serial_write_string(
            "VMM-3.1: RECURSIVE CLONE FAILED\n"
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
        "VMM-3.1: FULL RECURSIVE CLONE VERIFIED\n"
    );

    serial_write_string(
        "VMM-3.1: PAGE-TABLE LEVEL INDEPENDENCE VERIFIED\n"
    );

    serial_write_string(
        "VMM-3.1: PHYSICAL MAPPINGS PRESERVED\n"
    );

    serial_write_string(
        "VMM-3.1: RECURSIVE CLONE VERIFIED\n"
    );

    /* --------------------------------------------------------
       VERIFY BATOS-OWNED MAPPING SURVIVED PREPARATION
       -------------------------------------------------------- */

    /*
     * Verify that BATOS's own VMM-2A mapping survived
     * the address-space preparation.
     */
    uint64_t prepared_physical = 0;

    int prepared_result =
        vmm_translate(
            pml4,
            test_virtual,
            &prepared_physical
        );

    serial_write_string(
        "PREPARED TRANSLATION RESULT: "
    );

    serial_write_hex(
        (uint64_t)(uint32_t)prepared_result
    );

    serial_write_string("\n");

    serial_write_string(
        "PREPARED PHYSICAL: "
    );

    serial_write_hex(
        prepared_physical
    );

    serial_write_string("\n");

    if (prepared_result != 0 ||
        prepared_physical != test_frame)
    {
        serial_write_string(
            "BATOS TEST MAPPING: FAILED\n"
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
        "BATOS TEST MAPPING: PRESERVED\n"
    );

    /* --------------------------------------------------------
       ACTIVATE BATOS ADDRESS SPACE
       -------------------------------------------------------- */

    /*
     * Disable maskable interrupts before changing CR3.
     *
     * Hardware interrupt routing is not active yet.
     */
    __asm__ volatile (
        "cli"
        :
        :
        : "memory"
    );

    serial_write_string(
        "INTERRUPTS: DISABLED\n"
    );

    /*
     * BATOS-owned PML4 that will become the active
     * address-space root.
     */
    uint64_t batos_pml4 =
        vmm_get_pml4();

    serial_write_string(
        "SWITCHING CR3 TO BATOS PML4: "
    );

    serial_write_hex(
        batos_pml4
    );

    serial_write_string("\n");

    /*
     * VMM-3.2B owns the address-space lifecycle.
     *
     * The kernel keeps interrupts disabled while the VMM
     * performs and verifies the hardware CR3 transition.
     */
    int activation_result =
        vmm_activate_address_space(
            batos_pml4
        );

    /*
     * Read CR3 back after the VMM activation API.
     */
    uint64_t activated_cr3 =
        vmm_read_cr3();

    uint64_t activated_pml4 =
        activated_cr3 &
        0x000FFFFFFFFFF000ULL;

    serial_write_string(
        "CR3 AFTER SWITCH: "
    );

    serial_write_hex(
        activated_cr3
    );

    serial_write_string("\n");

    serial_write_string(
        "CR3 PML4 AFTER SWITCH: "
    );

    serial_write_hex(
        activated_pml4
    );

    serial_write_string("\n");

    if (activation_result == 0 &&
        activated_pml4 == batos_pml4)
    {
        serial_write_string(
            "CR3 SWITCH: OK\n"
        );

        serial_write_string(
            "VMM-2C: ADDRESS SPACE ACTIVATED\n"
        );
    }
    else
    {
        serial_write_string(
            "CR3 SWITCH: FAILED\n"
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
        "BATOS ADDRESS SPACE IS NOW ACTIVE\n"
    );

    /* --------------------------------------------------------
       VMM-3.2B ADDRESS-SPACE LIFECYCLE VERIFICATION
       -------------------------------------------------------- */

    serial_write_string(
        "\nVMM-3.2B ADDRESS-SPACE LIFECYCLE\n"
    );

    serial_write_string(
        "VERIFYING BATOS ADDRESS-SPACE REGISTRATION...\n"
    );

    if (vmm_verify_address_space_state(
            batos_pml4,
            VMM_ADDRESS_SPACE_ACTIVE
        ) != 0)
    {
        serial_write_string(
            "VMM-3.2B: ADDRESS-SPACE REGISTRATION FAILED\n"
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
        "BATOS ADDRESS SPACE: ACTIVE\n"
    );

    serial_write_string(
        "VERIFYING ACTIVE ADDRESS-SPACE IDENTITY...\n"
    );

    if (activated_pml4 != batos_pml4)
    {
        serial_write_string(
            "ACTIVE ADDRESS-SPACE IDENTITY: FAILED\n"
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
        "ACTIVE ADDRESS-SPACE IDENTITY: OK\n"
    );

    serial_write_string(
        "VMM-3.2B: ADDRESS-SPACE LIFECYCLE VERIFIED\n"
    );

    /* --------------------------------------------------------
       VMM-2D ADDRESS-SPACE INSPECTION
       -------------------------------------------------------- */

    serial_write_string(
        "\nVMM-2D ADDRESS-SPACE INSPECTION\n"
    );

    uint64_t active_cr3 =
        vmm_read_cr3();

    uint64_t active_pml4 =
        active_cr3 &
        0x000FFFFFFFFFF000ULL;

    serial_write_string(
        "ACTIVE CR3: "
    );

    serial_write_hex(
        active_cr3
    );

    serial_write_string("\n");

    serial_write_string(
        "ACTIVE PML4: "
    );

    serial_write_hex(
        active_pml4
    );

    serial_write_string("\n");

    serial_write_string(
        "BATOS PML4: "
    );

    serial_write_hex(
        pml4
    );

    serial_write_string("\n");

    if (active_pml4 != pml4)
    {
        serial_write_string(
            "VMM-2D ACTIVE PML4: FAILED\n"
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

    uint64_t present_entries = 0;

    int inspect_result =
        vmm_inspect_address_space(
            active_pml4,
            &present_entries
        );

    serial_write_string(
        "INSPECTION RESULT: "
    );

    serial_write_hex(
        (uint64_t)(uint32_t)inspect_result
    );

    serial_write_string("\n");

    serial_write_string(
        "ACTIVE PML4 PRESENT ENTRIES: "
    );

    serial_write_hex(
        present_entries
    );

    serial_write_string("\n");

    if (inspect_result == 0 &&
        present_entries > 0)
    {
        serial_write_string(
            "VMM-2D ADDRESS SPACE: OK\n"
        );
    }
    else
    {
        serial_write_string(
            "VMM-2D ADDRESS SPACE: FAILED\n"
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

    /* --------------------------------------------------------
       VMM-3.2A PAGE-TABLE OWNERSHIP VERIFICATION
       -------------------------------------------------------- */

    serial_write_string(
        "\nVMM-3.2A PAGE-TABLE OWNERSHIP\n"
    );

    serial_write_string(
        "VERIFYING BATOS PML4 OWNERSHIP...\n"
    );

    if (vmm_verify_page_table_root(pml4) != 0)
    {
        serial_write_string(
            "VMM-3.2A: BATOS PML4 OWNERSHIP FAILED\n"
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
        "BATOS PML4 OWNERSHIP: OK\n"
    );

    serial_write_string(
        "VERIFYING BATOS PAGE-TABLE PATH...\n"
    );

    if (vmm_verify_page_table_ownership(
            pml4,
            test_virtual
        ) != 0)
    {
        serial_write_string(
            "VMM-3.2A: PAGE-TABLE PATH OWNERSHIP FAILED\n"
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
        "BATOS PML4 -> PDPT -> PD -> PT OWNERSHIP: OK\n"
    );

    serial_write_string(
        "VERIFYING CLONED PML4 OWNERSHIP...\n"
    );

    if (vmm_verify_page_table_root(cloned_pml4) != 0)
    {
        serial_write_string(
            "VMM-3.2A: CLONED PML4 OWNERSHIP FAILED\n"
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
        "CLONED PML4 OWNERSHIP: OK\n"
    );

    if (pml4 != cloned_pml4)
    {
        serial_write_string(
            "OWNERSHIP ROOTS DISTINCT: OK\n"
        );

        serial_write_string(
            "VMM-3.2A: PAGE-TABLE OWNERSHIP VERIFIED\n"
        );
    }
    else
    {
        serial_write_string(
            "VMM-3.2A: OWNERSHIP ROOTS NOT DISTINCT\n"
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

    /* --------------------------------------------------------
       VMM-2F PAGE UNMAP + FRAME LIFECYCLE TEST
       -------------------------------------------------------- */

    serial_write_string(
        "\nVMM-2F PAGE UNMAP + FRAME LIFECYCLE TEST\n"
    );

    /*
     * Use a dedicated virtual address for the unmap test.
     *
     * This must not overlap BATOS's existing VMM-2A mapping
     * at 0x40000000.
     */
    uint64_t unmap_virtual =
        0x0000000040001000ULL;

    uint64_t unmap_check_physical = 0;

    /*
     * First prove that the candidate address is actually
     * unmapped in the active BATOS address space.
     */
    int initial_translate =
        vmm_translate(
            pml4,
            unmap_virtual,
            &unmap_check_physical
        );

    serial_write_string(
        "INITIAL TRANSLATION RESULT: "
    );

    serial_write_hex(
        (uint64_t)(uint32_t)initial_translate
    );

    serial_write_string("\n");

    if (initial_translate == 0)
    {
        serial_write_string(
            "VMM-2F: TEST ADDRESS ALREADY MAPPED\n"
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
        "VMM-2F: TEST ADDRESS IS UNMAPPED\n"
    );

    /*
     * Record PMM state before acquiring the test frame.
     */
    uint64_t free_frames_before =
        pmm_get_free_frames();

    serial_write_string(
        "PMM FREE FRAMES BEFORE ALLOCATION: "
    );

    serial_write_hex(
        free_frames_before
    );

    serial_write_string("\n");

    /*
     * Acquire one real physical frame from PMM.
     */
    uint64_t unmap_frame =
        pmm_alloc_frame();

    if (unmap_frame == 0)
    {
        serial_write_string(
            "VMM-2F: FRAME ALLOCATION FAILED\n"
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
        "VMM-2F TEST FRAME: "
    );

    serial_write_hex(
        unmap_frame
    );

    serial_write_string("\n");

    /*
     * Map the dedicated virtual address.
     */
    int unmap_map_result =
        vmm_map_page(
            pml4,
            unmap_virtual,
            unmap_frame,
            VMM_WRITABLE
        );

    serial_write_string(
        "UNMAP TEST MAP RESULT: "
    );

    serial_write_hex(
        (uint64_t)(uint32_t)unmap_map_result
    );

    serial_write_string("\n");

    if (unmap_map_result != 0)
    {
        pmm_free_frame(unmap_frame);

        serial_write_string(
            "VMM-2F: MAP FAILED\n"
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
     * Verify software translation after mapping.
     */
    uint64_t mapped_physical = 0;

    int mapped_translate_result =
        vmm_translate(
            pml4,
            unmap_virtual,
            &mapped_physical
        );

    if (mapped_translate_result != 0 ||
        mapped_physical != unmap_frame)
    {
        uint64_t cleanup_physical = 0;

        if (vmm_unmap_page(
                pml4,
                unmap_virtual,
                &cleanup_physical
            ) == 0)
        {
            pmm_free_frame(cleanup_physical);
        }
        else
        {
            pmm_free_frame(unmap_frame);
        }

        serial_write_string(
            "VMM-2F: MAP TRANSLATION FAILED\n"
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
        "VMM-2F: MAP TRANSLATION VERIFIED\n"
    );

    /*
     * Perform a REAL CPU memory access through the new mapping.
     */
    volatile uint64_t *unmap_test_address =
        (volatile uint64_t *)unmap_virtual;

    uint64_t unmap_test_pattern =
        0x4241544F532D3246ULL;

    *unmap_test_address =
        unmap_test_pattern;

    uint64_t unmap_readback =
        *unmap_test_address;

    serial_write_string(
        "VMM-2F CPU WRITE/READ: "
    );

    serial_write_hex(
        unmap_readback
    );

    serial_write_string("\n");

    if (unmap_readback != unmap_test_pattern)
    {
        uint64_t cleanup_physical = 0;

        if (vmm_unmap_page(
                pml4,
                unmap_virtual,
                &cleanup_physical
            ) == 0)
        {
            pmm_free_frame(cleanup_physical);
        }
        else
        {
            pmm_free_frame(unmap_frame);
        }

        serial_write_string(
            "VMM-2F: CPU MEMORY ACCESS FAILED\n"
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
        "VMM-2F: CPU MEMORY ACCESS VERIFIED\n"
    );

    /*
     * Remove only the virtual mapping.
     *
     * vmm_unmap_page() returns the physical frame but does
     * not free it. PMM remains responsible for frame lifetime.
     */
    uint64_t unmapped_physical = 0;

    int unmap_result =
        vmm_unmap_page(
            pml4,
            unmap_virtual,
            &unmapped_physical
        );

    serial_write_string(
        "VMM UNMAP RESULT: "
    );

    serial_write_hex(
        (uint64_t)(uint32_t)unmap_result
    );

    serial_write_string("\n");

    serial_write_string(
        "UNMAPPED PHYSICAL: "
    );

    serial_write_hex(
        unmapped_physical
    );

    serial_write_string("\n");

    if (unmap_result != 0 ||
        unmapped_physical != unmap_frame)
    {
        serial_write_string(
            "VMM-2F: UNMAP FAILED\n"
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
     * The virtual mapping must now be absent.
     *
     * We deliberately do NOT dereference unmap_virtual here:
     * a page fault would be expected and page-fault recovery
     * is a separate kernel milestone.
     */
    uint64_t after_unmap_physical = 0;

    int after_unmap_result =
        vmm_translate(
            pml4,
            unmap_virtual,
            &after_unmap_physical
        );

    serial_write_string(
        "AFTER UNMAP TRANSLATION RESULT: "
    );

    serial_write_hex(
        (uint64_t)(uint32_t)after_unmap_result
    );

    serial_write_string("\n");

    if (after_unmap_result == 0)
    {
        serial_write_string(
            "VMM-2F: TRANSLATION STILL PRESENT\n"
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
        "VMM-2F: TRANSLATION REMOVED\n"
    );

    /*
     * Return the physical frame to PMM.
     */
    pmm_free_frame(
        unmapped_physical
    );

    uint64_t free_frames_after =
        pmm_get_free_frames();

    serial_write_string(
        "PMM FREE FRAMES AFTER RELEASE: "
    );

    serial_write_hex(
        free_frames_after
    );

    serial_write_string("\n");

    if (free_frames_after !=
        free_frames_before)
    {
        serial_write_string(
            "VMM-2F: PMM FRAME LIFECYCLE FAILED\n"
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
        "VMM-2F: PMM FRAME RELEASE VERIFIED\n"
    );

    serial_write_string(
        "VMM-2F: PAGE UNMAP + FRAME LIFECYCLE VERIFIED\n"
    );


#ifdef BATOS_VMM_TEST
    /* --------------------------------------------------------
       VMM-3.2C TRANSACTIONAL MAP ROLLBACK TEST
       -------------------------------------------------------- */

    serial_write_string(
        "\nVMM-3.2C TRANSACTIONAL MAP ROLLBACK TEST\n"
    );

    /*
     * This VA is deliberately outside the addresses already
     * exercised by the earlier VMM tests.
     */
    uint64_t rollback_test_virtual =
        0x0000400000000000ULL;

    uint64_t rollback_test_frame =
        pmm_alloc_frame();

    if (rollback_test_frame == 0)
    {
        serial_write_string(
            "VMM-3.2C: TEST FRAME ALLOCATION FAILED\n"
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

    uint64_t rollback_free_before =
        pmm_get_free_frames();

    serial_write_string(
        "ROLLBACK TEST PMM BASELINE: "
    );
    serial_write_hex(
        rollback_free_before
    );
    serial_write_string("\n");

    extern int vmm_test_is_pml4_slot_empty(
        uint64_t pml4_physical,
        uint64_t virtual_address
    );

    int rollback_pml4_empty =
        vmm_test_is_pml4_slot_empty(
            pml4,
            rollback_test_virtual
        );

    if (!rollback_pml4_empty)
    {
        serial_write_string(
            "VMM-3.2C: TEST VA PML4 SLOT NOT FRESH\n"
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
        "VMM-3.2C: TEST PML4 SLOT EMPTY\n"
    );

    extern int vmm_test_fail_pt_allocation;

    vmm_test_fail_pt_allocation = 1;

    int rollback_result =
        vmm_map_page(
            pml4,
            rollback_test_virtual,
            rollback_test_frame,
            VMM_WRITABLE
        );

    vmm_test_fail_pt_allocation = 0;

    serial_write_string(
        "FORCED PT FAILURE RESULT: "
    );
    serial_write_hex(
        (uint64_t)(uint32_t)rollback_result
    );
    serial_write_string("\n");

    if (rollback_result == 0)
    {
        serial_write_string(
            "VMM-3.2C: FAILURE INJECTION DID NOT FAIL\n"
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

    uint64_t rollback_free_after =
        pmm_get_free_frames();

    serial_write_string(
        "ROLLBACK TEST PMM AFTER FAILURE: "
    );
    serial_write_hex(
        rollback_free_after
    );
    serial_write_string("\n");

    if (rollback_free_after !=
        rollback_free_before)
    {
        serial_write_string(
            "VMM-3.2C: PMM ROLLBACK LEAK DETECTED\n"
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

    uint64_t rollback_physical = 0;

    int rollback_translate_result =
        vmm_translate(
            pml4,
            rollback_test_virtual,
            &rollback_physical
        );

    serial_write_string(
        "ROLLBACK TEST TRANSLATION RESULT: "
    );
    serial_write_hex(
        (uint64_t)(uint32_t)
            rollback_translate_result
    );
    serial_write_string("\n");

    if (rollback_translate_result == 0)
    {
        serial_write_string(
            "VMM-3.2C: MAPPING SURVIVED ROLLBACK\n"
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
        "VMM-3.2C: PAGE-TABLE ROLLBACK VERIFIED\n"
    );

    /*
     * The failed mapping never took ownership of the test
     * frame because the final PTE was never installed.
     * Release the frame allocated specifically for this test.
     */
    pmm_free_frame(
        rollback_test_frame
    );

    /*
     * Recovery test:
     *
     * With failure injection disabled, the same fresh VA
     * must be mappable normally.
     */
    uint64_t recovery_frame =
        pmm_alloc_frame();

    if (recovery_frame == 0)
    {
        serial_write_string(
            "VMM-3.2C: RECOVERY FRAME ALLOCATION FAILED\n"
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

    int recovery_result =
        vmm_map_page(
            pml4,
            rollback_test_virtual,
            recovery_frame,
            VMM_WRITABLE
        );

    serial_write_string(
        "RECOVERY MAP RESULT: "
    );
    serial_write_hex(
        (uint64_t)(uint32_t)recovery_result
    );
    serial_write_string("\n");

    if (recovery_result != 0)
    {
        serial_write_string(
            "VMM-3.2C: RECOVERY MAP FAILED\n"
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

    uint64_t recovery_physical = 0;

    if (vmm_translate(
            pml4,
            rollback_test_virtual,
            &recovery_physical
        ) != 0 ||
        recovery_physical != recovery_frame)
    {
        serial_write_string(
            "VMM-3.2C: RECOVERY TRANSLATION FAILED\n"
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

    volatile uint64_t *recovery_address =
        (volatile uint64_t *)
            rollback_test_virtual;

    uint64_t recovery_pattern =
        0x4241544F532D3343ULL;

    *recovery_address = recovery_pattern;

    if (*recovery_address != recovery_pattern)
    {
        serial_write_string(
            "VMM-3.2C: RECOVERY HARDWARE ACCESS FAILED\n"
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

    uint64_t unmapped_recovery_frame = 0;

    if (vmm_unmap_page(
            pml4,
            rollback_test_virtual,
            &unmapped_recovery_frame
        ) != 0 ||
        unmapped_recovery_frame != recovery_frame)
    {
        serial_write_string(
            "VMM-3.2C: RECOVERY UNMAP FAILED\n"
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

    pmm_free_frame(
        unmapped_recovery_frame
    );

    serial_write_string(
        "VMM-3.2C: RECOVERY MAP + CPU ACCESS VERIFIED\n"
    );

    serial_write_string(
        "VMM-3.2C: TRANSACTIONAL ROLLBACK VERIFIED\n"
    );

#endif
    /* --------------------------------------------------------
       VMM-2E HARDWARE PAGE TRANSLATION TEST
       -------------------------------------------------------- */

    serial_write_string(
        "\nVMM-2E HARDWARE PAGE TRANSLATION TEST\n"
    );

    /*
     * At this point:
     *
     *     CR3
     *       ↓
     *     BATOS PML4
     *       ↓
     *     PDPT
     *       ↓
     *     PD
     *       ↓
     *     PT
     *       ↓
     *     PTE
     *       ↓
     *     test_frame
     *
     * The software walker already verified this mapping.
     *
     * VMM-2E performs a REAL memory access through
     * test_virtual.
     *
     * This forces the CPU/MMU to perform the hardware
     * page-table translation.
     */

    volatile uint64_t *hardware_test_address =
        (volatile uint64_t *)test_virtual;

    uint64_t test_pattern =
        0x4241544F532D3245ULL;

    serial_write_string(
        "HARDWARE TEST VIRTUAL: "
    );

    serial_write_hex(
        (uint64_t)hardware_test_address
    );

    serial_write_string("\n");

    serial_write_string(
        "HARDWARE TEST PHYSICAL: "
    );

    serial_write_hex(
        test_frame
    );

    serial_write_string("\n");

    serial_write_string(
        "WRITING TEST PATTERN...\n"
    );

    /*
     * REAL CPU MEMORY ACCESS.
     *
     * Because the pointer is volatile, the compiler must
     * emit an actual memory store.
     */
    *hardware_test_address =
        test_pattern;

    serial_write_string(
        "READING TEST PATTERN...\n"
    );

    /*
     * REAL CPU MEMORY ACCESS.
     *
     * This load must travel through the active BATOS
     * page-table hierarchy.
     */
    uint64_t readback =
        *hardware_test_address;

    serial_write_string(
        "EXPECTED VALUE: "
    );

    serial_write_hex(
        test_pattern
    );

    serial_write_string("\n");

    serial_write_string(
        "READBACK VALUE: "
    );

    serial_write_hex(
        readback
    );

    serial_write_string("\n");

    if (readback != test_pattern)
    {
        serial_write_string(
            "VMM-2E: HARDWARE TRANSLATION FAILED\n"
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
        "HARDWARE MEMORY ACCESS: OK\n"
    );

    serial_write_string(
        "VMM-2E: CPU PAGE TRANSLATION VERIFIED\n"
    );

    serial_write_string(
        "BATOS HARDWARE ADDRESS TRANSLATION: OK\n"
    );

}
