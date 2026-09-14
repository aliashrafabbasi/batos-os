#ifndef BATOS_IRQ_ROUTING_H
#define BATOS_IRQ_ROUTING_H

int irq_routing_init(void);

int irq_routing_set_irq0_masked(
    int masked
);

#endif
