#include <stdint.h>
#include <stddef.h>

#include "limine.h"
#include "kernel/arch/x86_64/gdt.h"
#include "kernel/arch/x86_64/tss.h"
#include "kernel/arch/x86_64/pmm.h"
#include "kernel/arch/x86_64/vmm.h"
#include "kernel/arch/x86_64/idt.h"

/* ============================================================
   LIMINE FRAMEBUFFER REQUEST
   ============================================================ */

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request
    limine_framebuffer_request = {
        .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
        .revision = 0,
        .response = NULL
    };

/* ============================================================
   LIMINE EXECUTABLE ADDRESS REQUEST
   ============================================================ */

__attribute__((used, section(".limine_requests")))
static volatile struct limine_executable_address_request
    limine_executable_address_request = {
        .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID,
        .revision = 0,
        .response = NULL
    };

/* ============================================================
   SERIAL PORT
   ============================================================ */

#define COM1 0x3F8

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;

    __asm__ volatile (
        "inb %1, %0"
        : "=a"(value)
        : "Nd"(port)
    );

    return value;
}

static void serial_init(void)
{
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}

static void serial_write_char(char c)
{
    while ((inb(COM1 + 5) & 0x20) == 0)
    {
    }

    outb(COM1, (uint8_t)c);
}

static void serial_write_string(const char *str)
{
    while (*str)
    {
        serial_write_char(*str++);
    }
}

static void serial_write_hex(uint64_t value)
{
    const char hex[] = "0123456789ABCDEF";

    serial_write_string("0x");

    for (int i = 15; i >= 0; i--)
    {
        serial_write_char(
            hex[(value >> (i * 4)) & 0xF]
        );
    }
}

static uint64_t read_cr2(void)
{
    uint64_t value;

    __asm__ volatile (
        "mov %%cr2, %0"
        : "=r"(value)
    );

    return value;
}

/* ============================================================
   FRAMEBUFFER
   ============================================================ */

static struct limine_framebuffer *framebuffer = NULL;

static void framebuffer_clear(uint32_t color)
{
    if (!framebuffer)
        return;

    uint32_t *pixels =
        (uint32_t *)framebuffer->address;

    uint64_t width = framebuffer->width;
    uint64_t height = framebuffer->height;
    uint64_t pitch = framebuffer->pitch / 4;

    for (uint64_t y = 0; y < height; y++)
    {
        for (uint64_t x = 0; x < width; x++)
        {
            pixels[y * pitch + x] = color;
        }
    }
}

/* ============================================================
   5x7 FONT
   ============================================================ */

static const uint8_t font_A[7] =
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11};

static const uint8_t font_B[7] =
    {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E};

static const uint8_t font_C[7] =
    {0x0F,0x10,0x10,0x10,0x10,0x10,0x0F};

static const uint8_t font_D[7] =
    {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E};

static const uint8_t font_E[7] =
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F};

static const uint8_t font_F[7] =
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10};

static const uint8_t font_G[7] =
    {0x0F,0x10,0x10,0x17,0x11,0x11,0x0F};

static const uint8_t font_H[7] =
    {0x11,0x11,0x11,0x1F,0x11,0x11,0x11};

static const uint8_t font_I[7] =
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x1F};

static const uint8_t font_J[7] =
    {0x01,0x01,0x01,0x01,0x11,0x11,0x0E};

static const uint8_t font_K[7] =
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11};

static const uint8_t font_L[7] =
    {0x10,0x10,0x10,0x10,0x10,0x10,0x1F};

static const uint8_t font_M[7] =
    {0x11,0x1B,0x15,0x15,0x11,0x11,0x11};

static const uint8_t font_N[7] =
    {0x11,0x19,0x15,0x13,0x11,0x11,0x11};

static const uint8_t font_O[7] =
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E};

static const uint8_t font_P[7] =
    {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10};

static const uint8_t font_Q[7] =
    {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D};

static const uint8_t font_R[7] =
    {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11};

static const uint8_t font_S[7] =
    {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E};

static const uint8_t font_T[7] =
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x04};

static const uint8_t font_U[7] =
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E};

static const uint8_t font_V[7] =
    {0x11,0x11,0x11,0x11,0x11,0x0A,0x04};

static const uint8_t font_W[7] =
    {0x11,0x11,0x11,0x15,0x15,0x1B,0x11};

static const uint8_t font_X[7] =
    {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11};

static const uint8_t font_Y[7] =
    {0x11,0x11,0x0A,0x04,0x04,0x04,0x04};

static const uint8_t font_Z[7] =
    {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F};

static const uint8_t font_0[7] =
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E};

static const uint8_t font_1[7] =
    {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E};

static const uint8_t font_2[7] =
    {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F};

static const uint8_t font_3[7] =
    {0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E};

static const uint8_t font_4[7] =
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02};

static const uint8_t font_5[7] =
    {0x1F,0x10,0x10,0x1E,0x01,0x01,0x1E};

static const uint8_t font_6[7] =
    {0x0E,0x10,0x10,0x1E,0x11,0x11,0x0E};

static const uint8_t font_7[7] =
    {0x1F,0x01,0x02,0x04,0x08,0x08,0x08};

static const uint8_t font_8[7] =
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E};

static const uint8_t font_9[7] =
    {0x0E,0x11,0x11,0x0F,0x01,0x01,0x0E};

static const uint8_t font_space[7] =
    {0,0,0,0,0,0,0};

static const uint8_t *get_font(char c)
{
    switch (c)
    {
        case 'A': return font_A;
        case 'B': return font_B;
        case 'C': return font_C;
        case 'D': return font_D;
        case 'E': return font_E;
        case 'F': return font_F;
        case 'G': return font_G;
        case 'H': return font_H;
        case 'I': return font_I;
        case 'J': return font_J;
        case 'K': return font_K;
        case 'L': return font_L;
        case 'M': return font_M;
        case 'N': return font_N;
        case 'O': return font_O;
        case 'P': return font_P;
        case 'Q': return font_Q;
        case 'R': return font_R;
        case 'S': return font_S;
        case 'T': return font_T;
        case 'U': return font_U;
        case 'V': return font_V;
        case 'W': return font_W;
        case 'X': return font_X;
        case 'Y': return font_Y;
        case 'Z': return font_Z;

        case '0': return font_0;
        case '1': return font_1;
        case '2': return font_2;
        case '3': return font_3;
        case '4': return font_4;
        case '5': return font_5;
        case '6': return font_6;
        case '7': return font_7;
        case '8': return font_8;
        case '9': return font_9;

        case ' ': return font_space;

        default:
            return font_space;
    }
}

static void draw_char(
    uint32_t x,
    uint32_t y,
    char c,
    uint32_t color,
    uint32_t scale
)
{
    if (!framebuffer)
        return;

    const uint8_t *glyph = get_font(c);

    uint32_t *pixels =
        (uint32_t *)framebuffer->address;

    uint64_t pitch =
        framebuffer->pitch / 4;

    for (uint32_t row = 0; row < 7; row++)
    {
        for (uint32_t col = 0; col < 5; col++)
        {
            if (glyph[row] & (1 << (4 - col)))
            {
                for (uint32_t sy = 0; sy < scale; sy++)
                {
                    for (uint32_t sx = 0; sx < scale; sx++)
                    {
                        uint32_t px =
                            x + col * scale + sx;

                        uint32_t py =
                            y + row * scale + sy;

                        pixels[py * pitch + px] = color;
                    }
                }
            }
        }
    }
}

static void draw_text(
    uint32_t x,
    uint32_t y,
    const char *text,
    uint32_t color,
    uint32_t scale
)
{
    while (*text)
    {
        draw_char(
            x,
            y,
            *text,
            color,
            scale
        );

        x += 6 * scale;
        text++;
    }
}

/* ============================================================
   EXCEPTION HANDLER
   ============================================================ */

__attribute__((noreturn))
void exception_handler(struct exception_frame *frame)
{
    if (frame->vector == 14)
    {
        serial_write_string(
            "\n================================\n"
        );

        serial_write_string(
            "BATOS KERNEL EXCEPTION\n"
        );

        serial_write_string(
            "================================\n"
        );

        serial_write_string(
            "VECTOR: "
        );

        serial_write_hex(frame->vector);

        serial_write_string("\n");

        serial_write_string(
            "ERROR CODE: "
        );

        serial_write_hex(frame->error_code);

        serial_write_string("\n");

        serial_write_string(
            "RIP: "
        );

        serial_write_hex(frame->rip);

        serial_write_string("\n");

        serial_write_string(
            "CS: "
        );

        serial_write_hex(frame->cs);

        serial_write_string("\n");

        serial_write_string(
            "RFLAGS: "
        );

        serial_write_hex(frame->rflags);

        serial_write_string("\n");

        uint64_t cr2 = read_cr2();

        serial_write_string(
            "EXCEPTION: PAGE FAULT (#PF)\n"
        );

        serial_write_string(
            "PAGE FAULT ADDRESS: "
        );

        serial_write_hex(cr2);

        serial_write_string("\n");

        serial_write_string(
            "FIRST PAGE FAULT HANDLER: ACTIVE\n"
        );

        serial_write_string(
            "TRIGGERING NESTED PAGE FAULT...\n"
        );

        volatile uint64_t *nested_fault =
            (volatile uint64_t *)0x0000000000000000ULL;

        volatile uint64_t nested_value =
            *nested_fault;

        (void)nested_value;

        serial_write_string(
            "ERROR: NESTED PAGE FAULT DID NOT OCCUR\n"
        );
    }
    else if (frame->vector == 8)
    {
        serial_write_string(
            "\n================================\n"
        );

        serial_write_string(
            "BATOS DOUBLE FAULT\n"
        );

        serial_write_string(
            "================================\n"
        );

        serial_write_string(
            "VECTOR: "
        );

        serial_write_hex(frame->vector);

        serial_write_string("\n");

        serial_write_string(
            "ERROR CODE: "
        );

        serial_write_hex(frame->error_code);

        serial_write_string("\n");

        serial_write_string(
            "RIP: "
        );

        serial_write_hex(frame->rip);

        serial_write_string("\n");

        serial_write_string(
            "CS: "
        );

        serial_write_hex(frame->cs);

        serial_write_string("\n");

        serial_write_string(
            "RFLAGS: "
        );

        serial_write_hex(frame->rflags);

        serial_write_string("\n");

        serial_write_string(
            "EXCEPTION: DOUBLE FAULT (#DF)\n"
        );

        serial_write_string(
            "IST1 HANDLER: ACTIVE\n"
        );

        serial_write_string(
            "TSS IST1: "
        );

        serial_write_hex(
            tss_get_ist1()
        );

        serial_write_string("\n");

        serial_write_string(
            "DOUBLE FAULT HANDLING: OK\n"
        );
    }
    else
    {
        serial_write_string(
            "\nUNKNOWN CPU EXCEPTION\n"
        );

        serial_write_string(
            "VECTOR: "
        );

        serial_write_hex(frame->vector);

        serial_write_string("\n");
    }

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

/* ============================================================
   KERNEL MAIN
   ============================================================ */

void kernel_main(void)
{
    serial_init();

    serial_write_string(
        "BATOS KERNEL STARTING...\n"
    );

    /* --------------------------------------------------------
       LIMINE EXECUTABLE ADDRESS VERIFICATION
       -------------------------------------------------------- */

    serial_write_string(
        "\nEXECUTABLE ADDRESS VERIFICATION\n"
    );

    if (limine_executable_address_request.response)
    {
        uint64_t executable_physical =
            limine_executable_address_request.response->physical_base;

        uint64_t executable_virtual =
            limine_executable_address_request.response->virtual_base;

        serial_write_string(
            "EXECUTABLE PHYSICAL BASE: "
        );

        serial_write_hex(
            executable_physical
        );

        serial_write_string("\n");

        serial_write_string(
            "EXECUTABLE VIRTUAL BASE: "
        );

        serial_write_hex(
            executable_virtual
        );

        serial_write_string("\n");

        serial_write_string(
            "EXECUTABLE ADDRESS: OK\n"
        );
    }
    else
    {
        serial_write_string(
            "EXECUTABLE ADDRESS: FAILED\n"
        );
    }

    /* --------------------------------------------------------
       FRAMEBUFFER
       -------------------------------------------------------- */

    if (limine_framebuffer_request.response &&
        limine_framebuffer_request.response->framebuffer_count > 0)
    {
        framebuffer =
            limine_framebuffer_request.response->framebuffers[0];

        framebuffer_clear(0x00000000);

        draw_text(
            40,
            40,
            "BATOS OS",
            0x00FFFFFF,
            5
        );

        draw_text(
            40,
            110,
            "KERNEL RUNNING",
            0x00FFFFFF,
            3
        );

        serial_write_string(
            "FRAMEBUFFER OK\n"
        );
    }
    else
    {
        serial_write_string(
            "FRAMEBUFFER ERROR\n"
        );
    }

    /* --------------------------------------------------------
       GDT
       -------------------------------------------------------- */

    gdt_init();

    serial_write_string(
        "GDT READY\n"
    );

    serial_write_string(
        "TSS LOADED\n"
    );

    /* --------------------------------------------------------
       IDT
       -------------------------------------------------------- */

    idt_init();

    serial_write_string(
        "IDT READY\n"
    );

    /* --------------------------------------------------------
       TSS / IST VERIFICATION
       -------------------------------------------------------- */

    serial_write_string(
        "\nTSS/IST VERIFICATION\n"
    );

    serial_write_string(
        "TR: "
    );

    serial_write_hex(
        tss_get_selector()
    );

    serial_write_string("\n");

    serial_write_string(
        "IDT[8] IST: "
    );

    serial_write_hex(
        idt_get_ist(8)
    );

    serial_write_string("\n");

    serial_write_string(
        "IDT[8] SELECTOR: "
    );

    serial_write_hex(
        idt_get_selector(8)
    );

    serial_write_string("\n");

    serial_write_string(
        "IST1 STACK: "
    );

    serial_write_hex(
        tss_get_ist1()
    );

    serial_write_string("\n");

    serial_write_string(
        "TSS/IST READY\n"
    );

    /* --------------------------------------------------------
       PHYSICAL MEMORY MANAGER
       -------------------------------------------------------- */

    serial_write_string(
        "\n================================\n"
    );

    serial_write_string(
        "BATOS PMM INITIALIZING...\n"
    );

    serial_write_string(
        "================================\n"
    );

    pmm_init();

    serial_write_string(
        "PMM MEMORY MAP: OK\n"
    );

    serial_write_string(
        "HHDM OFFSET: "
    );

    serial_write_hex(
        pmm_get_hhdm_offset()
    );

    serial_write_string("\n");

    serial_write_string(
        "TOTAL FRAMES: "
    );

    serial_write_hex(
        pmm_get_total_frames()
    );

    serial_write_string("\n");

    serial_write_string(
        "FREE FRAMES: "
    );

    serial_write_hex(
        pmm_get_free_frames()
    );

    serial_write_string("\n");

    serial_write_string(
        "USED FRAMES: "
    );

    serial_write_hex(
        pmm_get_used_frames()
    );

    serial_write_string("\n");

    serial_write_string(
        "PMM BITMAP PHYSICAL: "
    );

    serial_write_hex(
        pmm_get_bitmap_physical()
    );

    serial_write_string("\n");

    serial_write_string(
        "PMM BITMAP SIZE: "
    );

    serial_write_hex(
        pmm_get_bitmap_size()
    );

    serial_write_string("\n");

    serial_write_string(
        "PMM READY\n"
    );

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
     * ACTUAL CR3 SWITCH.
     */
    vmm_write_cr3(
        batos_pml4
    );

    /*
     * Read CR3 back immediately after the switch.
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

    if (activated_pml4 == batos_pml4)
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

    /* --------------------------------------------------------
       FINAL HALT
       -------------------------------------------------------- */

    serial_write_string(
        "CPU HALTED\n"
    );

    for (;;)
    {
        __asm__ volatile (
            "hlt"
        );
    }
}