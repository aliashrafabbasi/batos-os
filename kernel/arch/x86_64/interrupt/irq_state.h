#ifndef BATOS_X86_64_IRQ_STATE_H
#define BATOS_X86_64_IRQ_STATE_H

#include <stdint.h>

/*
 * Save the current RFLAGS and disable maskable interrupts.
 *
 * The returned value contains the complete original RFLAGS.
 * Restoring it therefore preserves the caller's original IF state.
 */
uint64_t x86_64_irq_save(void);

/*
 * Restore a previously saved RFLAGS value.
 *
 * In particular, the original IF state is restored exactly.
 */
void x86_64_irq_restore(uint64_t flags);

/*
 * Return non-zero when maskable interrupts are currently enabled.
 */
int x86_64_irq_is_enabled(void);

#endif
