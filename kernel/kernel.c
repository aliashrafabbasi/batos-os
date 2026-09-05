#include <stdint.h>
#include "../limine.h"
#include "arch/x86_64/idt.h"

__attribute__((used, section(".limine_requests")))
static volatile uint64_t base_revision[] =
    LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0,
    .response = 0
};

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t requests_start_marker[] =
    LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t requests_end_marker[] =
    LIMINE_REQUESTS_END_MARKER;

// Global framebuffer pointer for drivers
struct limine_framebuffer *global_framebuffer = 0;

static void hcf(void)
{
    for (;;)
    {
        __asm__ volatile ("hlt");
    }
}

static void put_pixel(
    struct limine_framebuffer *framebuffer,
    uint64_t x,
    uint64_t y,
    uint32_t color)
{
    uint8_t *fb = (uint8_t *)framebuffer->address;
    uint32_t *pixel =
        (uint32_t *)(fb + y * framebuffer->pitch + x * 4);
    *pixel = color;
}

static void fill_screen(
    struct limine_framebuffer *framebuffer,
    uint32_t color)
{
    for (uint64_t y = 0; y < framebuffer->height; y++)
    {
        for (uint64_t x = 0; x < framebuffer->width; x++)
        {
            put_pixel(framebuffer, x, y, color);
        }
    }
}

static const uint8_t font[26][7] =
{
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}, /* A */
    {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}, /* B */
    {0x0F,0x10,0x10,0x10,0x10,0x10,0x0F}, /* C */
    {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E}, /* D */
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}, /* E */
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}, /* F */
    {0x0F,0x10,0x10,0x17,0x11,0x11,0x0F}, /* G */
    {0x11,0x11,0x11,0x1F,0x11,0x11,0x11}, /* H */
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x1F}, /* I */
    {0x07,0x02,0x02,0x02,0x12,0x12,0x0C}, /* J */
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11}, /* K */
    {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}, /* L */
    {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}, /* M */
    {0x11,0x19,0x15,0x13,0x11,0x11,0x11}, /* N */
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}, /* O */
    {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}, /* P */
    {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}, /* Q */
    {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}, /* R */
    {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}, /* S */
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}, /* T */
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}, /* U */
    {0x11,0x11,0x11,0x11,0x11,0x0A,0x04}, /* V */
    {0x11,0x11,0x11,0x15,0x15,0x15,0x0A}, /* W */
    {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}, /* X */
    {0x11,0x11,0x0A,0x04,0x04,0x04,0x04}, /* Y */
    {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}  /* Z */
};

void draw_char(
    struct limine_framebuffer *framebuffer,
    char character,
    uint64_t x,
    uint64_t y,
    uint64_t scale,
    uint32_t color)
{
    if (character >= 'A' && character <= 'Z')
    {
        const uint8_t *glyph = font[character - 'A'];
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
                                framebuffer,
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
}

void draw_text(
    struct limine_framebuffer *framebuffer,
    const char *text,
    uint64_t x,
    uint64_t y,
    uint64_t scale,
    uint32_t color)
{
    uint64_t cursor_x = x;
    for (uint64_t i = 0; text[i] != '\0'; i++)
    {
        if (text[i] == ' ')
        {
            cursor_x += 6 * scale;
        }
        else
        {
            draw_char(framebuffer, text[i], cursor_x, y, scale, color);
            cursor_x += 6 * scale;
        }
    }
}

void kernel_main(void)
{
    if (!LIMINE_BASE_REVISION_SUPPORTED(base_revision))
        hcf();

    if (framebuffer_request.response == 0 || framebuffer_request.response->framebuffer_count == 0)
        hcf();

    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];
    if (framebuffer->bpp != 32)
        hcf();

    global_framebuffer = framebuffer;

    fill_screen(framebuffer, 0x001B263B);

    draw_text(framebuffer, "BATOS OS", 100, 100, 8, 0x00FFFFFF);
    draw_text(framebuffer, "KERNEL RUNNING", 100, 200, 4, 0x00FFFFFF);
    draw_text(framebuffer, "FRAMEBUFFER OK", 100, 250, 4, 0x00FFFFFF);
    draw_text(framebuffer, "KEYBOARD READY", 100, 300, 4, 0x0000FF00);

    // Initialize IDT after framebuffer is ready
    idt_init();

    // Keep the kernel alive and waiting for interrupts
    for (;;)
    {
        __asm__ volatile ("hlt");
    }
}
