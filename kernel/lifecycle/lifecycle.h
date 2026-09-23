#ifndef BATOS_LIFECYCLE_H
#define BATOS_LIFECYCLE_H

#include <stdint.h>

struct task;

/*
 * Deferred task-reclamation lifecycle boundary.
 *
 * Lifecycle owns only the pending-reclamation collection and
 * coordinates release of Task-side ownership. It does not own
 * scheduler runnable state, Process address spaces, or VMM
 * resources.
 */
int lifecycle_init(void);

/*
 * Publish a managed terminated Task for deferred reclamation.
 *
 * Publication requires the Task to have explicitly acquired the
 * production lifecycle contract through execution admission.
 * Publication transfers deferred-reclamation responsibility to
 * the lifecycle subsystem. The Task remains registry-owned and,
 * when applicable, Process-owned until lifecycle_reap_one()
 * successfully completes.
 */
int lifecycle_publish_terminated_task(
    struct task *task
);

/*
 * Reap the oldest pending terminated Task.
 *
 * On success, the Task is no longer Process-owned or registry-owned
 * and its Task resources have been destroyed.
 *
 * Returns:
 *   0  success
 *  <0  validation or reclamation failure
 *   1  no Task is pending
 */
int lifecycle_reap_one(void);

uint64_t lifecycle_pending_count(void);

int lifecycle_contains_pending(
    const struct task *task
);

#endif
