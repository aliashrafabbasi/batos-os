#include <stdint.h>
#include <stddef.h>

#include "limine.h"
#include "kernel/console/console.h"

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

static void framebuffer_console_write_char(char c);

void serial_init(void)
{
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}

void serial_write_char(char c)
{
    while ((inb(COM1 + 5) & 0x20) == 0)
    {
    }

    outb(COM1, (uint8_t)c);

    framebuffer_console_write_char(c);
}

void serial_write_string(const char *str)
{
    while (*str)
    {
        serial_write_char(*str++);
    }
}

void serial_write_hex(uint64_t value)
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

/* ============================================================
   FRAMEBUFFER
   ============================================================ */

static struct limine_framebuffer *framebuffer = NULL;

void console_set_framebuffer(struct limine_framebuffer *fb)
{
    framebuffer = fb;
}

/*
 * BATOS framebuffer kernel console.
 *
 * Serial output remains the primary debug transport.
 * Once the framebuffer is initialized, every character
 * is mirrored to the graphical console as well.
 */
static uint8_t framebuffer_console_initialized = 0;

static uint32_t framebuffer_cursor_x = 20;
static uint32_t framebuffer_cursor_y = 150;

static uint32_t framebuffer_console_scale = 2;

static const uint32_t framebuffer_console_color =
    0x00FFFFFF;

static const uint32_t framebuffer_console_background =
    0x00000000;

static const uint32_t framebuffer_console_left =
    20;

static const uint32_t framebuffer_console_top =
    150;

static const uint32_t framebuffer_console_line_height =
    18;

static const uint32_t framebuffer_console_char_width =
    12;

void framebuffer_clear(uint32_t color)
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

    if (scale == 0)
        return;

    const uint8_t *glyph = get_font(c);

    uint32_t *pixels =
        (uint32_t *)framebuffer->address;

    uint64_t pitch =
        framebuffer->pitch / 4;

    uint64_t width =
        framebuffer->width;

    uint64_t height =
        framebuffer->height;

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
                        uint64_t px =
                            (uint64_t)x +
                            (uint64_t)col * scale +
                            sx;

                        uint64_t py =
                            (uint64_t)y +
                            (uint64_t)row * scale +
                            sy;

                        if (px < width && py < height)
                        {
                            pixels[py * pitch + px] =
                                color;
                        }
                    }
                }
            }
        }
    }
}

void draw_text(
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

/*
 * Scroll only the kernel-log region.
 *
 * The BATOS title remains visible at the top of the
 * framebuffer while diagnostic output scrolls below it.
 */
static void framebuffer_console_scroll(void)
{
    if (!framebuffer)
        return;

    uint32_t *pixels =
        (uint32_t *)framebuffer->address;

    uint64_t pitch =
        framebuffer->pitch / 4;

    uint64_t width =
        framebuffer->width;

    uint64_t height =
        framebuffer->height;

    uint64_t top =
        framebuffer_console_top;

    uint64_t line_height =
        framebuffer_console_line_height;

    if (top >= height)
        return;

    if (line_height >= height - top)
        return;

    for (uint64_t y = top;
         y + line_height < height;
         y++)
    {
        uint64_t source_y =
            y + line_height;

        for (uint64_t x = 0;
             x < width;
             x++)
        {
            pixels[y * pitch + x] =
                pixels[source_y * pitch + x];
        }
    }

    uint64_t clear_start =
        height - line_height;

    for (uint64_t y = clear_start;
         y < height;
         y++)
    {
        for (uint64_t x = 0;
             x < width;
             x++)
        {
            pixels[y * pitch + x] =
                framebuffer_console_background;
        }
    }

    framebuffer_cursor_y =
        (uint32_t)(height - line_height);
}

static void framebuffer_console_newline(void)
{
    if (!framebuffer)
        return;

    framebuffer_cursor_x =
        framebuffer_console_left;

    framebuffer_cursor_y +=
        framebuffer_console_line_height;

    if ((uint64_t)framebuffer_cursor_y +
            framebuffer_console_line_height >
        framebuffer->height)
    {
        framebuffer_console_scroll();
    }
}

static void framebuffer_console_write_char(char c)
{
    if (!framebuffer ||
        !framebuffer_console_initialized)
        return;

    if (c == '\n')
    {
        framebuffer_console_newline();
        return;
    }

    if (c == '\r')
    {
        framebuffer_cursor_x =
            framebuffer_console_left;
        return;
    }

    if (c == '\t')
    {
        framebuffer_cursor_x +=
            framebuffer_console_char_width * 4;

        if ((uint64_t)framebuffer_cursor_x +
                framebuffer_console_char_width >
            framebuffer->width)
        {
            framebuffer_console_newline();
        }

        return;
    }

    /*
     * The current BATOS font is uppercase-only.
     * Normalize lowercase diagnostic text to uppercase
     * so it remains visible on the framebuffer.
     */
    if (c >= 'a' && c <= 'z')
        c = (char)(c - ('a' - 'A'));

    if ((uint64_t)framebuffer_cursor_x +
            framebuffer_console_char_width >
        framebuffer->width)
    {
        framebuffer_console_newline();
    }

    if ((uint64_t)framebuffer_cursor_y + 14 >
        framebuffer->height)
    {
        framebuffer_console_scroll();
    }

    draw_char(
        framebuffer_cursor_x,
        framebuffer_cursor_y,
        c,
        framebuffer_console_color,
        framebuffer_console_scale
    );

    framebuffer_cursor_x +=
        framebuffer_console_char_width;
}

void framebuffer_console_init(void)
{
    if (!framebuffer)
        return;

    framebuffer_console_scale = 2;

    framebuffer_cursor_x =
        framebuffer_console_left;

    framebuffer_cursor_y =
        framebuffer_console_top;

    framebuffer_console_initialized = 1;
}
