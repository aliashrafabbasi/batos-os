#include <stdint.h>
#include "limine.h"
#include "arch/x86_64/idt.h"

/*
 * Limine base revision.
 */
static volatile uint64_t limine_base_revision[]
    __attribute__((used, section(".limine_requests"))) =
    LIMINE_BASE_REVISION(3);

/*
 * Framebuffer request.
 */
static volatile struct limine_framebuffer_request framebuffer_request
    __attribute__((used, section(".limine_requests"))) =
{
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};


/*
 * Simple 5x7 bitmap font.
 *
 * A-Z
 * 0-9
 */
static const uint8_t font[36][7] =
{
    /* A */
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},

    /* B */
    {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},

    /* C */
    {0x0F,0x10,0x10,0x10,0x10,0x10,0x0F},

    /* D */
    {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E},

    /* E */
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},

    /* F */
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},

    /* G */
    {0x0F,0x10,0x10,0x17,0x11,0x11,0x0F},

    /* H */
    {0x11,0x11,0x11,0x1F,0x11,0x11,0x11},

    /* I */
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x1F},

    /* J */
    {0x01,0x01,0x01,0x01,0x11,0x11,0x0E},

    /* K */
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11},

    /* L */
    {0x10,0x10,0x10,0x10,0x10,0x10,0x1F},

    /* M */
    {0x11,0x1B,0x15,0x15,0x11,0x11,0x11},

    /* N */
    {0x11,0x19,0x15,0x13,0x11,0x11,0x11},

    /* O */
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},

    /* P */
    {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},

    /* Q */
    {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},

    /* R */
    {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},

    /* S */
    {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E},

    /* T */
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x04},

    /* U */
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E},

    /* V */
    {0x11,0x11,0x11,0x11,0x11,0x0A,0x04},

    /* W */
    {0x11,0x11,0x11,0x15,0x15,0x15,0x0A},

    /* X */
    {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},

    /* Y */
    {0x11,0x11,0x0A,0x04,0x04,0x04,0x04},

    /* Z */
    {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},

    /* 0 */
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},

    /* 1 */
    {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},

    /* 2 */
    {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F},

    /* 3 */
    {0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E},

    /* 4 */
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},

    /* 5 */
    {0x1F,0x10,0x10,0x1E,0x01,0x01,0x1E},

    /* 6 */
    {0x0E,0x10,0x10,0x1E,0x11,0x11,0x0E},

    /* 7 */
    {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},

    /* 8 */
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},

    /* 9 */
    {0x0E,0x11,0x11,0x0F,0x01,0x01,0x0E}
};


/*
 * Get framebuffer.
 */
static struct limine_framebuffer *get_framebuffer(void)
{
    if (framebuffer_request.response == 0)
        return 0;

    if (framebuffer_request.response->framebuffer_count < 1)
        return 0;

    return framebuffer_request.response->framebuffers[0];
}


/*
 * Fill framebuffer.
 */
static void fill_screen(uint32_t color)
{
    struct limine_framebuffer *fb = get_framebuffer();

    if (fb == 0)
        return;

    uint32_t *pixels = (uint32_t *)fb->address;

    for (uint64_t y = 0; y < fb->height; y++)
    {
        for (uint64_t x = 0; x < fb->width; x++)
        {
            pixels[y * (fb->pitch / 4) + x] = color;
        }
    }
}


/*
 * Draw one character.
 */
static void draw_char(
    char c,
    uint64_t x,
    uint64_t y,
    uint32_t color,
    uint32_t scale
)
{
    struct limine_framebuffer *fb = get_framebuffer();

    if (fb == 0)
        return;

    int index = -1;

    if (c >= 'A' && c <= 'Z')
    {
        index = c - 'A';
    }
    else if (c >= '0' && c <= '9')
    {
        index = 26 + (c - '0');
    }

    if (index < 0)
        return;

    uint32_t *pixels = (uint32_t *)fb->address;

    for (int row = 0; row < 7; row++)
    {
        for (int col = 0; col < 5; col++)
        {
            if (font[index][row] & (1 << (4 - col)))
            {
                for (uint32_t sy = 0; sy < scale; sy++)
                {
                    for (uint32_t sx = 0; sx < scale; sx++)
                    {
                        uint64_t px =
                            x + col * scale + sx;

                        uint64_t py =
                            y + row * scale + sy;

                        if (px < fb->width &&
                            py < fb->height)
                        {
                            pixels[
                                py * (fb->pitch / 4) + px
                            ] = color;
                        }
                    }
                }
            }
        }
    }
}


/*
 * Draw text.
 */
static void draw_text(
    const char *text,
    uint64_t x,
    uint64_t y,
    uint32_t color,
    uint32_t scale
)
{
    while (*text)
    {
        if (*text == ' ')
        {
            x += 6 * scale;
        }
        else
        {
            draw_char(
                *text,
                x,
                y,
                color,
                scale
            );

            x += 6 * scale;
        }

        text++;
    }
}


/*
 * Write character to COM1.
 */
static void serial_write_char(char c)
{
    __asm__ volatile (
        "movb %0, %%al\n"
        "movw $0x3F8, %%dx\n"
        "outb %%al, %%dx"
        :
        : "q"(c)
        : "rax", "rdx"
    );
}


/*
 * Write string to COM1.
 */
static void serial_write_string(const char *text)
{
    while (*text)
    {
        serial_write_char(*text);
        text++;
    }
}


/*
 * Write 64-bit hexadecimal value.
 */
static void serial_write_hex(uint64_t value)
{
    static const char hex[] =
        "0123456789ABCDEF";

    serial_write_string("0x");

    for (int shift = 60; shift >= 0; shift -= 4)
    {
        serial_write_char(
            hex[(value >> shift) & 0xF]
        );
    }
}


/*
 * Kernel exception handler.
 */
__attribute__((noreturn))
void exception_handler(struct exception_frame *frame)
{
    const char *name = "UNKNOWN EXCEPTION";

    switch (frame->vector)
    {
        case 0:
            name = "DIVIDE ERROR (#DE)";
            break;

        case 1:
            name = "DEBUG (#DB)";
            break;

        case 2:
            name = "NON-MASKABLE INTERRUPT (NMI)";
            break;

        case 3:
            name = "BREAKPOINT (#BP)";
            break;

        case 4:
            name = "OVERFLOW (#OF)";
            break;

        case 5:
            name = "BOUND RANGE EXCEEDED (#BR)";
            break;

        case 6:
            name = "INVALID OPCODE (#UD)";
            break;

        case 7:
            name = "DEVICE NOT AVAILABLE (#NM)";
            break;

        case 8:
            name = "DOUBLE FAULT (#DF)";
            break;

        case 9:
            name = "COPROCESSOR SEGMENT OVERRUN";
            break;

        case 10:
            name = "INVALID TSS (#TS)";
            break;

        case 11:
            name = "SEGMENT NOT PRESENT (#NP)";
            break;

        case 12:
            name = "STACK-SEGMENT FAULT (#SS)";
            break;

        case 13:
            name = "GENERAL PROTECTION FAULT (#GP)";
            break;

        case 14:
            name = "PAGE FAULT (#PF)";
            break;

        case 15:
            name = "RESERVED";
            break;

        case 16:
            name = "X87 FLOATING-POINT (#MF)";
            break;

        case 17:
            name = "ALIGNMENT CHECK (#AC)";
            break;

        case 18:
            name = "MACHINE CHECK (#MC)";
            break;

        case 19:
            name = "SIMD FLOATING-POINT (#XM/#XF)";
            break;

        case 20:
            name = "VIRTUALIZATION EXCEPTION (#VE)";
            break;

        case 21:
            name = "CONTROL PROTECTION (#CP)";
            break;

        case 22:
        case 23:
        case 24:
        case 25:
        case 26:
        case 27:
            name = "RESERVED";
            break;

        case 28:
            name = "HYPERVISOR INJECTION (#HV)";
            break;

        case 29:
            name = "VMM COMMUNICATION (#VC)";
            break;

        case 30:
            name = "SECURITY EXCEPTION (#SX)";
            break;

        case 31:
            name = "RESERVED";
            break;
    }


    /*
     * Exception header.
     */
    serial_write_string(
        "\n\n"
        "================================\n"
        "BATOS KERNEL EXCEPTION\n"
        "================================\n"
    );

    serial_write_string("VECTOR: ");
    serial_write_hex(frame->vector);

    serial_write_string("\nEXCEPTION: ");
    serial_write_string(name);

    serial_write_string("\nERROR CODE: ");
    serial_write_hex(frame->error_code);


    /*
     * General purpose registers.
     */
    serial_write_string("\n\nREGISTERS:\n");

    serial_write_string("RAX:    ");
    serial_write_hex(frame->rax);

    serial_write_string("\nRBX:    ");
    serial_write_hex(frame->rbx);

    serial_write_string("\nRCX:    ");
    serial_write_hex(frame->rcx);

    serial_write_string("\nRDX:    ");
    serial_write_hex(frame->rdx);

    serial_write_string("\nRBP:    ");
    serial_write_hex(frame->rbp);

    serial_write_string("\nRSI:    ");
    serial_write_hex(frame->rsi);

    serial_write_string("\nRDI:    ");
    serial_write_hex(frame->rdi);

    serial_write_string("\nR8:     ");
    serial_write_hex(frame->r8);

    serial_write_string("\nR9:     ");
    serial_write_hex(frame->r9);

    serial_write_string("\nR10:    ");
    serial_write_hex(frame->r10);

    serial_write_string("\nR11:    ");
    serial_write_hex(frame->r11);

    serial_write_string("\nR12:    ");
    serial_write_hex(frame->r12);

    serial_write_string("\nR13:    ");
    serial_write_hex(frame->r13);

    serial_write_string("\nR14:    ");
    serial_write_hex(frame->r14);

    serial_write_string("\nR15:    ");
    serial_write_hex(frame->r15);


    /*
     * CPU exception state.
     */
    serial_write_string("\n\nEXCEPTION STATE:\n");

    serial_write_string("RIP:    ");
    serial_write_hex(frame->rip);

    serial_write_string("\nCS:     ");
    serial_write_hex(frame->cs);

    serial_write_string("\nRFLAGS: ");
    serial_write_hex(frame->rflags);

    serial_write_string(
        "\n\n"
        "C HANDLER: OK\n"
        "CPU HALTED\n"
        "================================\n\n"
    );


    /*
     * Kernel panic screen.
     */
    fill_screen(0x00330000);

    draw_text(
        "BATOS KERNEL PANIC",
        40,
        40,
        0x00FFFFFF,
        4
    );

    draw_text(
        "CPU EXCEPTION",
        40,
        100,
        0x00FFFFFF,
        3
    );

    draw_text(
        "CPU HALTED",
        40,
        150,
        0x00FFFFFF,
        3
    );


    /*
     * Never return.
     */
    for (;;)
    {
        __asm__ volatile ("cli; hlt");
    }
}


/*
 * BATOS kernel entry point.
 */
void kernel_main(void)
{
    /*
     * Disable interrupts during
     * early kernel initialization.
     */
    __asm__ volatile ("cli");

    /*
     * Get framebuffer.
     */
    struct limine_framebuffer *fb =
        get_framebuffer();

    /*
     * Halt if framebuffer unavailable.
     */
    if (fb == 0)
    {
        for (;;)
        {
            __asm__ volatile ("cli; hlt");
        }
    }

    /*
     * Clear screen.
     */
    fill_screen(0x00000000);

    /*
     * Kernel startup information.
     */
    draw_text(
        "BATOS OS",
        40,
        40,
        0x00FFFFFF,
        5
    );

    draw_text(
        "KERNEL RUNNING",
        40,
        120,
        0x00FFFFFF,
        3
    );

    draw_text(
        "FRAMEBUFFER OK",
        40,
        170,
        0x00FFFFFF,
        3
    );

    /*
     * Initialize IDT.
     */
    idt_init();

    /*
     * Confirm IDT.
     */
    draw_text(
        "IDT READY",
        40,
        220,
        0x00FFFFFF,
        3
    );


    /*
     * ==========================================
     * TEMPORARY EXCEPTION TEST
     * ==========================================
     *
     * This intentionally triggers:
     *
     *     Divide Error (#DE)
     *
     * CPU -> IDT -> Assembly Stub
     *     -> Register Frame -> C Handler
     */
    volatile uint64_t a = 10;
    volatile uint64_t b = 0;

    volatile uint64_t result =
        a / b;

    (void)result;


    /*
     * Should never reach here because
     * the divide error is fatal.
     */
    for (;;)
    {
        __asm__ volatile ("hlt");
    }
}
