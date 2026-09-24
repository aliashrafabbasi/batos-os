#ifndef BATOS_RUNTIME_TEST_H
#define BATOS_RUNTIME_TEST_H

/*
 * Test-only runtime failure seam.
 *
 * Arms a single initialization attempt to fail after the
 * production root execution unit has been published and before
 * the lifecycle reaper is published.
 */
void kernel_runtime_test_fail_reaper_once(void);

#endif
