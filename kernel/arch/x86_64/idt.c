#include "idt.h"
#include "../../../limine.h"

struct idt_entry idt[256];
struct idt_ptr idtp;

extern void interrupt_stub(void);
extern void keyboard_handler_stub(void);

extern struct limine_framebuffer *global_framebuffer;

static uint32_t cursor_x = 50;
static uint32_t cursor_y = 280;

// Standard US QWERTY Scancode Set 1 to ASCII lookup table
static unsigned char keyboard_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8',	/* 9 */
    '9', '0', '-', '=', '\b',	/* Backspace */
    '\t',			/* Tab */
    'q', 'w', 'e', 'r',	/* 19 */
    't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',	/* Enter key */
    0,			/* 29 - Control */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	/* 39 */
    '\'', '`', 0,		/* Left shift */
    '\\', 'z', 'x', 'c', 'v', 'b', 'n',			/* 49 */
    'm', ',', '.', '/', 0,					/* Right shift */
    '*',
    0,	/* Alt */
    ' ',	/* Space bar */
    0,	/* Caps lock */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* F1 to F10 keys */
    0,	/* Num lock */
    0,	/* Scroll Lock */
    0,	/* Home key */
    0,	/* Up arrow */
    0,	/* Page Up */
    '-',
    0,	/* Left arrow */
    0,
    0,	/* Right arrow */
    '+',
    0,	/* End key */
    0,	/* Down arrow */
    0,	/* Page Down */
    0,	/* Insert key */
    0,	/* Delete key */
    0, 0, 0,
    0,	/* F11 Key */
    0,	/* F12 Key */
    0,	/* All other keys undefined */
};

// 8x8 Font Bitmap
static const unsigned char font8x8[128][8] = {
    ['a'] = {0x00, 0x00, 0x1E, 0x20, 0x3E, 0x22, 0x3C, 0x00},
    ['b'] = {0x10, 0x10, 0x1C, 0x22, 0x22, 0x22, 0x1C, 0x00},
    ['c'] = {0x00, 0x00, 0x1C, 0x22, 0x02, 0x22, 0x1C, 0x00},
    ['d'] = {0x04, 0x04, 0x1C, 0x22, 0x22, 0x22, 0x1C, 0x00},
    ['e'] = {0x00, 0x00, 0x1C, 0x22, 0x3E, 0x02, 0x1C, 0x00},
    ['f'] = {0x0C, 0x12, 0x02, 0x3E, 0x02, 0x02, 0x02, 0x00},
    ['g'] = {0x00, 0x00, 0x3C, 0x22, 0x22, 0x3C, 0x20, 0x1C},
    ['h'] = {0x10, 0x10, 0x1C, 0x22, 0x22, 0x22, 0x22, 0x00},
    ['i'] = {0x04, 0x00, 0x0C, 0x04, 0x04, 0x04, 0x0E, 0x00},
    ['j'] = {0x02, 0x00, 0x06, 0x02, 0x02, 0x22, 0x1C, 0x00},
    ['k'] = {0x10, 0x10, 0x12, 0x14, 0x18, 0x14, 0x12, 0x00},
    ['l'] = {0x0C, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E, 0x00},
    ['m'] = {0x00, 0x00, 0x36, 0x49, 0x49, 0x49, 0x49, 0x00},
    ['n'] = {0x00, 0x00, 0x1C, 0x22, 0x22, 0x22, 0x22, 0x00},
    ['o'] = {0x00, 0x00, 0x1C, 0x22, 0x22, 0x22, 0x1C, 0x00},
    ['p'] = {0x00, 0x00, 0x3C, 0x22, 0x22, 0x3C, 0x20, 0x20},
    ['q'] = {0x00, 0x00, 0x22, 0x22, 0x22, 0x3E, 0x02, 0x02},
    ['r'] = {0x00, 0x00, 0x2C, 0x32, 0x20, 0x20, 0x20, 0x00},
    ['s'] = {0x00, 0x00, 0x1C, 0x02, 0x1C, 0x20, 0x1C, 0x00},
    ['t'] = {0x08, 0x08, 0x3E, 0x08, 0x08, 0x08, 0x06, 0x00},
    ['u'] = {0x00, 0x00, 0x22, 0x22, 0x22, 0x22, 0x1C, 0x00},
    ['v'] = {0x00, 0x00, 0x22, 0x22, 0x22, 0x14, 0x08, 0x00},
    ['w'] = {0x00, 0x00, 0x22, 0x22, 0x2A, 0x2A, 0x14, 0x00},
    ['x'] = {0x00, 0x00, 0x22, 0x14, 0x08, 0x14, 0x22, 0x00},
    ['y'] = {0x00, 0x00, 0x22, 0x22, 0x22, 0x1E, 0x02, 0x1C},
    ['z'] = {0x00, 0x00, 0x3E, 0x04, 0x08, 0x10, 0x3E, 0x00},
    [' '] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    ['1'] = {0x08, 0x18, 0x08, 0x08, 0x08, 0x08, 0x1C, 0x00},
    ['2'] = {0x1C, 0x22, 0x04, 0x08, 0x10, 0x20, 0x3E, 0x00},
    ['3'] = {0x1C, 0x22, 0x02, 0x0C, 0x02, 0x22, 0x1C, 0x00},
    ['4'] = {0x04, 0x0C, 0x14, 0x24, 0x3E, 0x04, 0x04, 0x00},
    ['5'] = {0x3E, 0x20, 0x3C, 0x02, 0x02, 0x22, 0x1C, 0x00},
    ['6'] = {0x1C, 0x20, 0x3C, 0x22, 0x22, 0x22, 0x1C, 0x00},
    ['7'] = {0x3E, 0x02, 0x04, 0x08, 0x10, 0x10, 0x10, 0x00},
    ['8'] = {0x1C, 0x22, 0x22, 0x1C, 0x22, 0x22, 0x1C, 0x00},
    ['9'] = {0x1C, 0x22, 0x22, 0x1E, 0x02, 0x20, 0x1C, 0x00},
    ['0'] = {0x1C, 0x22, 0x26, 0x2A, 0x32, 0x22, 0x1C, 0x00},
    ['>'] = {0x08, 0x10, 0x20, 0x40, 0x20, 0x10, 0x08, 0x00},
    ['-'] = {0x00, 0x00, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x00},
    [':'] = {0x00, 0x08, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00},
};

static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void ps2_keyboard_init(void) {
    while (inb(0x64) & 2);
    outb(0x64, 0xAE);
}

static void pic_remap(void) {
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0xF9);   
    outb(0xA1, 0xFF);
}

static void idt_set_gate(unsigned char num, unsigned long base, unsigned short sel, unsigned char flags) {
    idt[num].base_low = (base & 0xFFFF);
    idt[num].base_middle = (base >> 16) & 0xFFFF;
    idt[num].base_high = (base >> 32) & 0xFFFFFFFF;
    idt[num].sel = sel;
    idt[num].ist = 0;
    idt[num].flags = flags;
    idt[num].zero = 0;
}

static void draw_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!global_framebuffer) return;
    volatile uint32_t *fb_ptr = (volatile uint32_t *)global_framebuffer->address;
    uint64_t pitch = global_framebuffer->pitch / 4;
    fb_ptr[y * pitch + x] = color;
}

static void draw_char(uint32_t x, uint32_t y, char c, uint32_t color) {
    if (c < 0 || c >= 128) return;
    const unsigned char *glyph = font8x8[(unsigned char)c];
    for (int i = 0; i < 8; i++) {
        unsigned char row = glyph[i];
        for (int j = 0; j < 8; j++) {
            if (row & (1 << (7 - j))) {
                draw_pixel(x + j, y + i, color);
            }
        }
    }
}

static void serial_putchar(char c) {
    while (!(inb(0x3F8 + 5) & 0x20));
    outb(0x3F8, (unsigned char)c);
}

void keyboard_handler(void) {
    unsigned char scancode = inb(0x60);
    
    if (!(scancode & 0x80)) {
        char ascii = keyboard_map[scancode];
        if (ascii) {
            serial_putchar(ascii);

            if (global_framebuffer) {
                if (ascii == '\n') {
                    cursor_x = 50;
                    cursor_y += 18;
                    // Print a cool prompt on the new line
                    draw_char(cursor_x, cursor_y, 'b', 0x00FFFF);
                    draw_char(cursor_x + 8, cursor_y, 'a', 0x00FFFF);
                    draw_char(cursor_x + 16, cursor_y, 't', 0x00FFFF);
                    draw_char(cursor_x + 24, cursor_y, 'o', 0x00FFFF);
                    draw_char(cursor_x + 32, cursor_y, 's', 0x00FFFF);
                    draw_char(cursor_x + 40, cursor_y, '>', 0x00FFFF);
                    cursor_x += 52;
                } else if (ascii == '\b') {
                    if (cursor_x >= 60) cursor_x -= 10;
                } else {
                    // Bright Cyan/Green typing effect
                    draw_char(cursor_x, cursor_y, ascii, 0x00FF88); 
                    cursor_x += 10;
                    if (cursor_x > 700) {
                        cursor_x = 50;
                        cursor_y += 18;
                    }
                }
            }
        }
    }

    outb(0x20, 0x20);
}

void idt_init(void) {
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base = (unsigned long)&idt;

    uint16_t code_segment;
    __asm__ volatile ("mov %%cs, %0" : "=r"(code_segment));

    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, (unsigned long)interrupt_stub, code_segment, 0x8E);
    }

    idt_set_gate(33, (unsigned long)keyboard_handler_stub, code_segment, 0x8E);

    pic_rem_ap: // standard layout
    pic_remap();
    ps2_keyboard_init();

    __asm__ volatile ("lidt %0" : : "m"(idtp));
    __asm__ volatile ("sti");
}