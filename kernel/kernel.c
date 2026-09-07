#include <stdint.h>

#include "../limine.h"
#include "arch/x86_64/idt.h"

/* =========================================================
 * Limine Requests
 * ========================================================= */

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[3] =
    LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0,
    .response = 0
};

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t requests_start_marker[4] =
    LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t requests_end_marker[2] =
    LIMINE_REQUESTS_END_MARKER;

/* =========================================================
 * Global Framebuffer
 * ========================================================= */

static struct limine_framebuffer *framebuffer;

/* =========================================================
 * Halt CPU
 * ========================================================= */

static void hcf(void)
{
    for (;;)
    {
        __asm__ volatile ("cli; hlt");
    }
}

/* =========================================================
 * Pixel Operations
 * ========================================================= */

static void put_pixel(uint64_t x, uint64_t y, uint32_t color)
{
    if (framebuffer == 0)
        return;

    if (x >= framebuffer->width || y >= framebuffer->height)
        return;

    uint8_t *address =
        (uint8_t *)framebuffer->address +
        y * framebuffer->pitch +
        x * 4;

    *(uint32_t *)address = color;
}

static void fill_screen(uint32_t color)
{
    if (framebuffer == 0)
        return;

    for (uint64_t y = 0; y < framebuffer->height; y++)
    {
        for (uint64_t x = 0; x < framebuffer->width; x++)
        {
            put_pixel(x, y, color);
        }
    }
}

/* =========================================================
 * 5x7 Bitmap Font
 * ========================================================= */

static const uint8_t font[26][7] =
{
    /* A */
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},

    /* B */
    {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E},

    /* C */
    {0x0F, 0x10, 0x10, 0x10, 0x10, 0x10, 0x0F},

    /* D */
    {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E},

    /* E */
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F},

    /* F */
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10},

    /* G */
    {0x0F, 0x10, 0x10, 0x17, 0x11, 0x11, 0x0F},

    /* H */
    {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},

    /* I */
    {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F},

    /* J */
    {0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0E},

    /* K */
    {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11},

    /* L */
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F},

    /* M */
    {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11},

    /* N */
    {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11},

    /* O */
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},

    /* P */
    {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10},

    /* Q */
    {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D},

    /* R */
    {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11},

    /* S */
    {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E},

    /* T */
    {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},

    /* U */
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},

    /* V */
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04},

    /* W */
    {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A},

    /* X */
    {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11},

    /* Y */
    {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04},

    /* Z */
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}
};

/* =========================================================
 * Character Rendering
 * ========================================================= */

static void draw_char(
    char c,
    uint64_t x,
    uint64_t y,
    uint32_t color,
    uint64_t scale
)
{
    if (c < 'A' || c > 'Z')
        return;

    const uint8_t *glyph = font[c - 'A'];

    for (uint64_t row = 0; row < 7; row++)
    {
        for (uint64_t col = 0; col < 5; col++)
        {
            if (glyph[row] & (1 << (4 - col)))
            {
                for (uint64_t sy = 0; sy < scale; sy++)
                {
                    for (uint64_t sx = 0; sx < scale; sx++)
                    {
                        put_pixel(
                            x + col * scale + sx,
                            y + row * scale + sy,
                            color
                        );
                    }
                }
            }
        }
    }
}

/* =========================================================
 * Text Rendering
 * ========================================================= */

static void draw_text(
    const char *text,
    uint64_t x,
    uint64_t y,
    uint32_t color,
    uint64_t scale
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
            draw_char(*text, x, y, color, scale);
            x += 6 * scale;
        }

        text++;
    }
}

/* =========================================================
 * Kernel Main
 * ========================================================= */

void kernel_main(void)
{
    /* -----------------------------------------------------
     * Validate framebuffer
     * ----------------------------------------------------- */

    if (framebuffer_request.response == 0)
        hcf();

    if (framebuffer_request.response->framebuffer_count == 0)
        hcf();

    framebuffer =
        framebuffer_request.response->framebuffers[0];

    if (framebuffer == 0)
        hcf();

    if (framebuffer->address == 0)
        hcf();

    if (framebuffer->bpp != 32)
        hcf();

    /* -----------------------------------------------------
     * Initialize framebuffer
     * ----------------------------------------------------- */

    fill_screen(0x001B263B);

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
        100,
        0x00FFFFFF,
        3
    );

    draw_text(
        "FRAMEBUFFER OK",
        40,
        140,
        0x00FFFFFF,
        3
    );

    /* -----------------------------------------------------
     * Initialize IDT
     * ----------------------------------------------------- */

    idt_init();

    draw_text(
        "IDT READY",
        40,
        180,
        0x00FFFFFF,
        3
    );

    /* -----------------------------------------------------
     * Kernel idle loop
     *
     * Interrupts remain disabled until proper IRQ
     * controller and interrupt infrastructure exists.
     * ----------------------------------------------------- */

    for (;;)
    {
        __asm__ volatile ("hlt");
    }
}