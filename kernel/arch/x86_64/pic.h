#ifndef BATOS_PIC_H
#define BATOS_PIC_H

#include <stdint.h>

#define PIC_MASTER_COMMAND 0x20
#define PIC_MASTER_DATA    0x21

#define PIC_SLAVE_COMMAND  0xA0
#define PIC_SLAVE_DATA     0xA1

#define PIC_EOI             0x20

#define PIC_MASTER_OFFSET   0x20
#define PIC_SLAVE_OFFSET    0x28

void pic_init(void);
void pic_send_eoi(uint8_t irq);
void pic_set_mask(uint8_t irq);
void pic_clear_mask(uint8_t irq);

#endif
