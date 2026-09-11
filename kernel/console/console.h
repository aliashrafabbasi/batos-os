#ifndef BATOS_CONSOLE_H
#define BATOS_CONSOLE_H

#include <stdint.h>

struct limine_framebuffer;

void console_set_framebuffer(struct limine_framebuffer *fb);

void serial_init(void);
void serial_write_char(char c);
void serial_write_string(const char *str);
void serial_write_hex(uint64_t value);

void framebuffer_clear(uint32_t color);
void draw_text(
    uint32_t x,
    uint32_t y,
    const char *text,
    uint32_t color,
    uint32_t scale
);
void framebuffer_console_init(void);

#endif
