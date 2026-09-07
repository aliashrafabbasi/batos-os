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
     * VMM-1 only creates and tests the page-table hierarchy.
     *
     * We intentionally do NOT load this PML4 into CR3 yet.
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
    }

    /*
     * The test frame is no longer needed after the
     * page-table construction test.
     */
    pmm_free_frame(test_frame);

    serial_write_string(
        "VMM INFRASTRUCTURE: OK\n"
    );

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