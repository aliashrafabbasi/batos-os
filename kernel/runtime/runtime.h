#ifndef BATOS_RUNTIME_H
#define BATOS_RUNTIME_H

int kernel_runtime_init(void);

/*
 * Start production scheduler execution.
 *
 * On success this function transfers CPU execution into the
 * production root Task and does not return.
 */
int kernel_runtime_start(void);

#endif
