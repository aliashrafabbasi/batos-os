#include "wait_queue_tests.h"

#include "../console/console.h"
#include "../sched/task.h"
#include "../sched/wait_queue.h"

#include <stdint.h>

static void wait_queue_test_fail(const char *message)
{
    serial_write_string("WAIT QUEUE TEST FAILURE: ");
    serial_write_string(message);
    serial_write_string("\n");

    __asm__ volatile ("cli");

    for (;;)
        __asm__ volatile ("hlt");
}

static void prepare_blocked_task(
    struct task *task,
    uint64_t id
)
{
    *task = (struct task){0};

    task->id = id;
    task->state = TASK_STATE_BLOCKED;
    task->wait_queue = NULL;
}

static void test_init_and_empty(void)
{
    struct wait_queue queue = {0};

    if (wait_queue_init(NULL) == 0)
        wait_queue_test_fail("NULL init accepted");

    if (wait_queue_init(&queue) != 0)
        wait_queue_test_fail("queue initialization failed");

    if (!queue.initialized)
        wait_queue_test_fail("queue not marked initialized");

    if (wait_queue_count(&queue) != 0)
        wait_queue_test_fail("new queue not empty");

    if (wait_queue_peek(&queue) != NULL)
        wait_queue_test_fail("empty queue returned element");

    if (wait_queue_dequeue(&queue) != NULL)
        wait_queue_test_fail("empty dequeue returned element");

    serial_write_string("WAIT QUEUE TEST: init/empty PASS\n");
}

static void test_enqueue_fifo(void)
{
    struct wait_queue queue = {0};
    struct task a;
    struct task b;
    struct task c;

    prepare_blocked_task(&a, 1);
    prepare_blocked_task(&b, 2);
    prepare_blocked_task(&c, 3);

    if (wait_queue_init(&queue) != 0)
        wait_queue_test_fail("FIFO init failed");

    if (wait_queue_enqueue(&queue, &a) != 0 ||
        wait_queue_enqueue(&queue, &b) != 0 ||
        wait_queue_enqueue(&queue, &c) != 0)
    {
        wait_queue_test_fail("FIFO enqueue failed");
    }

    if (wait_queue_count(&queue) != 3)
        wait_queue_test_fail("FIFO count incorrect");

    if (wait_queue_peek(&queue) != &a)
        wait_queue_test_fail("FIFO peek incorrect");

    if (wait_queue_dequeue(&queue) != &a ||
        wait_queue_dequeue(&queue) != &b ||
        wait_queue_dequeue(&queue) != &c)
    {
        wait_queue_test_fail("FIFO dequeue order incorrect");
    }

    if (a.wait_queue != NULL ||
        b.wait_queue != NULL ||
        c.wait_queue != NULL)
    {
        wait_queue_test_fail("dequeue did not release ownership");
    }

    if (wait_queue_count(&queue) != 0)
        wait_queue_test_fail("FIFO queue not empty");

    serial_write_string("WAIT QUEUE TEST: FIFO PASS\n");
}

static void test_duplicate_and_ownership(void)
{
    struct wait_queue queue_a = {0};
    struct wait_queue queue_b = {0};
    struct task task;

    prepare_blocked_task(&task, 10);

    if (wait_queue_init(&queue_a) != 0 ||
        wait_queue_init(&queue_b) != 0)
    {
        wait_queue_test_fail("ownership init failed");
    }

    if (wait_queue_enqueue(&queue_a, &task) != 0)
        wait_queue_test_fail("initial enqueue failed");

    if (wait_queue_enqueue(&queue_a, &task) == 0)
        wait_queue_test_fail("duplicate enqueue accepted");

    if (wait_queue_enqueue(&queue_b, &task) == 0)
        wait_queue_test_fail("multi-queue ownership accepted");

    if (task.wait_queue != &queue_a)
        wait_queue_test_fail("task ownership changed unexpectedly");

    if (!wait_queue_contains(&queue_a, &task))
        wait_queue_test_fail("owner queue lost task");

    if (wait_queue_contains(&queue_b, &task))
        wait_queue_test_fail("non-owner queue contains task");

    if (wait_queue_dequeue(&queue_a) != &task)
        wait_queue_test_fail("owner dequeue failed");

    if (task.wait_queue != NULL)
        wait_queue_test_fail("ownership not released");

    serial_write_string("WAIT QUEUE TEST: duplicate/ownership PASS\n");
}

static void test_state_validation(void)
{
    struct wait_queue queue = {0};
    struct task task = {0};

    task.id = 20;
    task.state = TASK_STATE_READY;

    if (wait_queue_init(&queue) != 0)
        wait_queue_test_fail("state validation init failed");

    if (wait_queue_enqueue(&queue, &task) == 0)
        wait_queue_test_fail("non-BLOCKED task accepted");

    if (wait_queue_count(&queue) != 0)
        wait_queue_test_fail("invalid enqueue changed queue");

    serial_write_string("WAIT QUEUE TEST: state validation PASS\n");
}

static void test_remove_preserves_fifo(void)
{
    struct wait_queue queue = {0};
    struct task a;
    struct task b;
    struct task c;

    prepare_blocked_task(&a, 30);
    prepare_blocked_task(&b, 31);
    prepare_blocked_task(&c, 32);

    if (wait_queue_init(&queue) != 0)
        wait_queue_test_fail("remove init failed");

    if (wait_queue_enqueue(&queue, &a) != 0 ||
        wait_queue_enqueue(&queue, &b) != 0 ||
        wait_queue_enqueue(&queue, &c) != 0)
    {
        wait_queue_test_fail("remove enqueue failed");
    }

    if (wait_queue_remove(&queue, &b) != 0)
        wait_queue_test_fail("middle removal failed");

    if (b.wait_queue != NULL)
        wait_queue_test_fail("removed task ownership retained");

    if (wait_queue_count(&queue) != 2)
        wait_queue_test_fail("remove count incorrect");

    if (wait_queue_dequeue(&queue) != &a ||
        wait_queue_dequeue(&queue) != &c)
    {
        wait_queue_test_fail("FIFO corrupted after remove");
    }

    if (wait_queue_count(&queue) != 0)
        wait_queue_test_fail("queue not empty after remove");

    serial_write_string("WAIT QUEUE TEST: remove/FIFO PASS\n");
}

static void test_remove_with_wraparound(void)
{
    struct wait_queue queue = {0};
    struct task cycle_task;
    struct task a;
    struct task b;
    struct task c;

    prepare_blocked_task(&cycle_task, 40);
    prepare_blocked_task(&a, 41);
    prepare_blocked_task(&b, 42);
    prepare_blocked_task(&c, 43);

    if (wait_queue_init(&queue) != 0)
        wait_queue_test_fail("wrap init failed");

    for (uint64_t index = 0;
         index < WAIT_QUEUE_MAX_TASKS - 2;
         index++)
    {
        if (wait_queue_enqueue(&queue, &cycle_task) != 0)
            wait_queue_test_fail("wrap enqueue failed");

        if (wait_queue_dequeue(&queue) != &cycle_task)
            wait_queue_test_fail("wrap dequeue failed");
    }

    if (wait_queue_enqueue(&queue, &a) != 0 ||
        wait_queue_enqueue(&queue, &b) != 0 ||
        wait_queue_enqueue(&queue, &c) != 0)
    {
        wait_queue_test_fail("wrapped enqueue failed");
    }

    if (wait_queue_remove(&queue, &b) != 0)
        wait_queue_test_fail("wrapped removal failed");

    if (wait_queue_dequeue(&queue) != &a ||
        wait_queue_dequeue(&queue) != &c)
    {
        wait_queue_test_fail("wrapped FIFO order incorrect");
    }

    if (wait_queue_count(&queue) != 0)
        wait_queue_test_fail("wrapped queue not empty");

    serial_write_string("WAIT QUEUE TEST: wraparound/remove PASS\n");
}

static void test_capacity(void)
{
    static struct task tasks[WAIT_QUEUE_MAX_TASKS];
    struct wait_queue queue = {0};
    struct task extra;

    if (wait_queue_init(&queue) != 0)
        wait_queue_test_fail("capacity init failed");

    for (uint64_t index = 0;
         index < WAIT_QUEUE_MAX_TASKS;
         index++)
    {
        prepare_blocked_task(&tasks[index], 100 + index);

        if (wait_queue_enqueue(&queue, &tasks[index]) != 0)
            wait_queue_test_fail("capacity enqueue failed");
    }

    if (wait_queue_count(&queue) != WAIT_QUEUE_MAX_TASKS)
        wait_queue_test_fail("capacity count incorrect");

    prepare_blocked_task(&extra, 1000);

    if (wait_queue_enqueue(&queue, &extra) == 0)
        wait_queue_test_fail("queue exceeded capacity");

    if (wait_queue_count(&queue) != WAIT_QUEUE_MAX_TASKS)
        wait_queue_test_fail("failed capacity enqueue changed count");

    for (uint64_t index = 0;
         index < WAIT_QUEUE_MAX_TASKS;
         index++)
    {
        if (wait_queue_dequeue(&queue) != &tasks[index])
            wait_queue_test_fail("capacity FIFO order incorrect");
    }

    if (wait_queue_count(&queue) != 0)
        wait_queue_test_fail("capacity queue not empty");

    serial_write_string("WAIT QUEUE TEST: capacity PASS\n");
}

void wait_queue_tests_run(void)
{
    serial_write_string("\n=== WAIT QUEUE TESTS ===\n");

    test_init_and_empty();
    test_enqueue_fifo();
    test_duplicate_and_ownership();
    test_state_validation();
    test_remove_preserves_fifo();
    test_remove_with_wraparound();
    test_capacity();

    serial_write_string("WAIT QUEUE TESTS: ALL PASS\n");
}
