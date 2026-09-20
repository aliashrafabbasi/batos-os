#include "blocking_tests.h"

#include "../arch/x86_64/interrupt/irq_state.h"
#include "../arch/x86_64/sched/context.h"
#include "../arch/x86_64/sched/preempt.h"
#include "../console/console.h"
#include "../sched/runqueue.h"
#include "../sched/scheduler.h"
#include "../sched/task.h"
#include "../sched/wait_queue.h"

#include <stdint.h>

#define BLOCKING_TEST_RFLAGS_IF (1ULL << 9)
#define BLOCKING_TEST_STACK_SIZE 4096ULL

static struct task blocking_task_a;
static struct task blocking_task_b;
static struct wait_queue blocking_queue;
static struct x86_64_context blocking_harness_context;

static uint8_t blocking_stack_a[BLOCKING_TEST_STACK_SIZE]
    __attribute__((aligned(16)));

static uint8_t blocking_stack_b[BLOCKING_TEST_STACK_SIZE]
    __attribute__((aligned(16)));

static volatile int blocking_a_entered;
static volatile int blocking_a_returned;
static volatile int blocking_a_result;

static void blocking_test_fail(const char *message)
{
    serial_write_string("BLOCKING TEST FAILURE: ");
    serial_write_string(message);
    serial_write_string("\n");

    __asm__ volatile ("cli");

    for (;;)
        __asm__ volatile ("hlt");
}

static void prepare_runtime_task(
    struct task *task,
    uint64_t id,
    void (*entry)(void *)
)
{
    uint8_t *stack;

    *task = (struct task){0};

    task->id = id;
    task->state = TASK_STATE_NEW;
    task->entry = entry;
    task->argument = NULL;
    task->wait_queue = NULL;

    stack = (task == &blocking_task_b)
        ? blocking_stack_b
        : blocking_stack_a;

    task->context.rsp =
        (uint64_t)(uintptr_t)(stack + BLOCKING_TEST_STACK_SIZE);

    task->context.rip =
        (uint64_t)(uintptr_t)entry;

    task->resume_authority = TASK_RESUME_CONTEXT;
}

static void blocking_task_a_entry(void *argument)
{
    (void)argument;

    if (scheduler_get_current() != &blocking_task_a)
        blocking_test_fail("A was not current at entry");

    if (blocking_task_a.state != TASK_STATE_RUNNING)
        blocking_test_fail("A was not RUNNING at entry");

    if (!x86_64_irq_is_enabled())
        blocking_test_fail("A entered with IF=0");

    blocking_a_entered = 1;

    /*
     * Regression: a task may arrive at a cooperative blocking
     * boundary while an older interrupt continuation is still
     * present. task_block() must make the cooperative context
     * authoritative before handing execution to the scheduler.
     */
    blocking_task_a.resume_authority = TASK_RESUME_INTERRUPT;

    blocking_a_result =
        task_block(&blocking_task_a, &blocking_queue);

    blocking_a_returned = 1;

    if (blocking_a_result != 0)
        blocking_test_fail("task_block did not resume successfully");

    if (blocking_task_a.resume_authority != TASK_RESUME_CONTEXT)
        blocking_test_fail("blocking handoff did not establish context authority");

    if (scheduler_get_current() != &blocking_task_a)
        blocking_test_fail("A did not become current after wake");

    if (blocking_task_a.state != TASK_STATE_RUNNING)
        blocking_test_fail("A did not become RUNNING after wake");

    if (blocking_task_a.wait_queue != NULL)
        blocking_test_fail("A retained wait queue ownership");

    if (wait_queue_count(&blocking_queue) != 0)
        blocking_test_fail("wait queue not empty after wake");

    if (runqueue_contains(&blocking_task_a))
        blocking_test_fail("RUNNING A remained in runqueue");

    if (!x86_64_irq_is_enabled())
        blocking_test_fail("A resumed with IF=0");

    x86_64_context_switch(
        &blocking_task_a.context,
        &blocking_harness_context
    );

    blocking_test_fail("A unexpectedly returned from harness");
}

static void blocking_task_b_entry(void *argument)
{
    (void)argument;

    if (scheduler_get_current() != &blocking_task_b)
        blocking_test_fail("B was not current at entry");

    if (blocking_task_b.state != TASK_STATE_RUNNING)
        blocking_test_fail("B was not RUNNING at entry");

    if (!x86_64_irq_is_enabled())
        blocking_test_fail("B entered with IF=0");

    if (!blocking_a_entered)
        blocking_test_fail("A never reached blocking point");

    if (blocking_a_returned)
        blocking_test_fail("A returned before wake");

    if (blocking_task_a.state != TASK_STATE_BLOCKED)
        blocking_test_fail("A did not become BLOCKED");

    if (blocking_task_a.wait_queue != &blocking_queue)
        blocking_test_fail("A was not wait-queue owned");

    if (!wait_queue_contains(&blocking_queue, &blocking_task_a))
        blocking_test_fail("blocked A missing from wait queue");

    if (runqueue_contains(&blocking_task_a))
        blocking_test_fail("blocked A remained runnable");

    if (runqueue_count() != 0)
        blocking_test_fail("runqueue not empty before wake");

    if (task_wake(&blocking_task_a) != 0)
        blocking_test_fail("task_wake(A) failed");

    if (!x86_64_irq_is_enabled())
        blocking_test_fail("task_wake changed enabled IF");

    if (blocking_task_a.state != TASK_STATE_READY)
        blocking_test_fail("woken A was not READY");

    if (blocking_task_a.wait_queue != NULL)
        blocking_test_fail("wake retained queue ownership");

    if (wait_queue_count(&blocking_queue) != 0)
        blocking_test_fail("wait queue retained woken A");

    if (!runqueue_contains(&blocking_task_a))
        blocking_test_fail("woken A missing from runqueue");

    if (runqueue_contains(&blocking_task_b))
        blocking_test_fail("RUNNING B entered runqueue");

    if (scheduler_yield() != 0)
        blocking_test_fail("B scheduler_yield failed");

    blocking_test_fail("B resumed unexpectedly");
}

static void prepare_blocked_wake_task(
    struct task *task,
    struct wait_queue *queue
)
{
    *task = (struct task){0};

    task->id = 500;
    task->state = TASK_STATE_BLOCKED;
    task->wait_queue = NULL;

    if (wait_queue_init(queue) != 0)
        blocking_test_fail("wake test queue init failed");

    if (wait_queue_enqueue(queue, task) != 0)
        blocking_test_fail("wake test enqueue failed");
}

static void test_task_wake_preserves_disabled_if(void)
{
    struct task task;
    struct wait_queue queue;
    uint64_t flags;

    scheduler_init();

    prepare_blocked_wake_task(&task, &queue);

    flags = x86_64_irq_save();

    if (x86_64_irq_is_enabled())
        blocking_test_fail("IF did not become disabled");

    if (task_wake(&task) != 0)
        blocking_test_fail("wake with IF=0 failed");

    if (x86_64_irq_is_enabled())
        blocking_test_fail("task_wake changed disabled IF");

    if (task.state != TASK_STATE_READY)
        blocking_test_fail("wake with IF=0 did not make READY");

    if (task.wait_queue != NULL)
        blocking_test_fail("wake with IF=0 retained ownership");

    if (!runqueue_contains(&task))
        blocking_test_fail("wake with IF=0 did not enqueue");

    x86_64_irq_restore(flags);

    scheduler_init();

    serial_write_string(
        "BLOCKING TEST: task_wake preserves disabled IF PASS\n"
    );
}

static void test_task_block_failure_atomicity(void)
{
    struct task task = {0};
    struct wait_queue queue = {0};
    uint64_t flags;

    scheduler_init();

    task.id = 600;
    task.state = TASK_STATE_NEW;

    if (wait_queue_init(&queue) != 0)
        blocking_test_fail("failure queue init failed");

    if (task_transition(&task, TASK_STATE_READY) != 0)
        blocking_test_fail("failure READY transition failed");

    if (scheduler_add(&task) != 0)
        blocking_test_fail("failure scheduler_add failed");

    if (scheduler_start() != 0)
        blocking_test_fail("failure scheduler_start failed");

    if (scheduler_get_current() != &task)
        blocking_test_fail("failure task not current");

    flags = x86_64_irq_save();

    if (task_block(&task, &queue) == 0)
        blocking_test_fail("task_block succeeded without successor");

    if (task.state != TASK_STATE_RUNNING)
        blocking_test_fail("failed block changed state");

    if (task.wait_queue != NULL)
        blocking_test_fail("failed block changed ownership");

    if (wait_queue_count(&queue) != 0)
        blocking_test_fail("failed block changed queue");

    if (scheduler_get_current() != &task)
        blocking_test_fail("failed block changed current");

    x86_64_irq_restore(flags);

    scheduler_init();

    serial_write_string(
        "BLOCKING TEST: task_block failure atomicity PASS\n"
    );
}

static void test_task_block_requires_enabled_if(void)
{
    struct task task = {0};
    struct wait_queue queue = {0};
    uint64_t flags;

    scheduler_init();

    task.id = 601;
    task.state = TASK_STATE_NEW;

    if (wait_queue_init(&queue) != 0)
        blocking_test_fail("IF queue init failed");

    if (task_transition(&task, TASK_STATE_READY) != 0)
        blocking_test_fail("IF READY transition failed");

    if (scheduler_add(&task) != 0)
        blocking_test_fail("IF scheduler_add failed");

    if (scheduler_start() != 0)
        blocking_test_fail("IF scheduler_start failed");

    flags = x86_64_irq_save();

    if (task_block(&task, &queue) == 0)
        blocking_test_fail("task_block accepted IF=0");

    if (task.state != TASK_STATE_RUNNING ||
        task.wait_queue != NULL ||
        wait_queue_count(&queue) != 0)
    {
        blocking_test_fail("IF rejection changed state/ownership");
    }

    x86_64_irq_restore(flags);

    scheduler_init();

    serial_write_string(
        "BLOCKING TEST: task_block IF precondition PASS\n"
    );
}

static void test_task_block_wake_runtime(void)
{
    uint64_t saved_irq_flags;
    int preempt_was_enabled;

    scheduler_init();

    blocking_queue = (struct wait_queue){0};
    blocking_task_a = (struct task){0};
    blocking_task_b = (struct task){0};
    blocking_harness_context = (struct x86_64_context){0};

    blocking_a_entered = 0;
    blocking_a_returned = 0;
    blocking_a_result = -1;

    if (wait_queue_init(&blocking_queue) != 0)
        blocking_test_fail("runtime queue init failed");

    prepare_runtime_task(
        &blocking_task_a,
        700,
        blocking_task_a_entry
    );

    prepare_runtime_task(
        &blocking_task_b,
        701,
        blocking_task_b_entry
    );

    if (task_transition(
            &blocking_task_a,
            TASK_STATE_READY
        ) != 0)
    {
        blocking_test_fail("A READY transition failed");
    }

    if (task_transition(
            &blocking_task_b,
            TASK_STATE_READY
        ) != 0)
    {
        blocking_test_fail("B READY transition failed");
    }

    if (scheduler_add(&blocking_task_a) != 0 ||
        scheduler_add(&blocking_task_b) != 0)
    {
        blocking_test_fail("runtime scheduler admission failed");
    }

    if (scheduler_start() != 0)
        blocking_test_fail("runtime scheduler start failed");

    if (scheduler_get_current() != &blocking_task_a)
        blocking_test_fail("runtime scheduler did not select A");

    saved_irq_flags = x86_64_irq_save();

    preempt_was_enabled = x86_64_preempt_is_enabled();
    x86_64_preempt_disable();

    x86_64_irq_restore(
        saved_irq_flags | BLOCKING_TEST_RFLAGS_IF
    );

    x86_64_context_switch(
        &blocking_harness_context,
        &blocking_task_a.context
    );

    if (!blocking_a_entered)
        blocking_test_fail("A did not execute");

    if (!blocking_a_returned)
        blocking_test_fail("A did not resume after wake");

    if (blocking_a_result != 0)
        blocking_test_fail("A recorded failed block");

    if (scheduler_get_current() != &blocking_task_a)
        blocking_test_fail("A not current after resume");

    if (blocking_task_a.state != TASK_STATE_RUNNING)
        blocking_test_fail("A not RUNNING after resume");

    if (blocking_task_b.state != TASK_STATE_READY)
        blocking_test_fail("B not READY after yield");

    if (blocking_task_a.wait_queue != NULL ||
        wait_queue_count(&blocking_queue) != 0)
    {
        blocking_test_fail("queue ownership remained after resume");
    }

    if (runqueue_contains(&blocking_task_a))
        blocking_test_fail("RUNNING A remained in runqueue");

    if (!runqueue_contains(&blocking_task_b))
        blocking_test_fail("READY B missing from runqueue");

    if (!x86_64_irq_is_enabled())
        blocking_test_fail("harness resumed with IF=0");

    scheduler_init();

    if (preempt_was_enabled)
        x86_64_preempt_enable();
    else
        x86_64_preempt_disable();

    x86_64_irq_restore(saved_irq_flags);

    serial_write_string(
        "BLOCKING TEST: task_block -> wake -> resume PASS\n"
    );
}

void blocking_tests_run(void)
{
    serial_write_string("\n=== BLOCKING / WAKE TESTS ===\n");

    test_task_wake_preserves_disabled_if();
    test_task_block_failure_atomicity();
    test_task_block_requires_enabled_if();
    test_task_block_wake_runtime();

    serial_write_string("BLOCKING / WAKE TESTS: ALL PASS\n");
}
