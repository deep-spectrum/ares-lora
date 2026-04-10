/**
 * @file work_q.cpp
 *
 * @brief
 *
 * @date 4/9/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/spinlock.hpp>
#include <ares-lora-serial/work_q.hpp>
#include <atomic>
#include <mutex>
#include <stddef.h>

#define BIT(n) (1UL << (n))

enum {

    /* Bits that represent the work item states.  At least nine of the
     * combinations are distinct valid stable states.
     */
    WORK_RUNNING_BIT = 0,
    WORK_CANCELING_BIT = 1,
    WORK_QUEUED_BIT = 2,
    WORK_DELAYED_BIT = 3,
    WORK_FLUSHING_BIT = 4,

    WORK_MASK = BIT(WORK_DELAYED_BIT) | BIT(WORK_QUEUED_BIT) |
                BIT(WORK_RUNNING_BIT) | BIT(WORK_CANCELING_BIT) |
                BIT(WORK_FLUSHING_BIT),

    /* Static work flags */
    WORK_DELAYABLE_BIT = 8,
    WORK_DELAYABLE = BIT(WORK_DELAYED_BIT),

    /* Dynamic work queue flags */
    WORK_QUEUE_STARTED_BIT = 0,
    WORK_QUEUE_STARTED = BIT(WORK_QUEUE_STARTED_BIT),
    WORK_QUEUE_BUSY_BIT = 1,
    WORK_QUEUE_BUSY = BIT(WORK_QUEUE_BUSY_BIT),
    WORK_QUEUE_DRAIN_BIT = 2,
    WORK_QUEUE_DRAIN = BIT(WORK_QUEUE_DRAIN_BIT),
    WORK_QUEUE_PLUGGED_BIT = 3,
    WORK_QUEUE_PLUGGED = BIT(WORK_QUEUE_PLUGGED_BIT),
    WORK_QUEUE_STOP_BIT = 4,
    WORK_QUEUE_STOP = BIT(WORK_QUEUE_STOP_BIT),

    /* Static work queue flags */
    WORK_QUEUE_NO_YIELD_BIT = 8,
    WORK_QUEUE_NO_YIELD = BIT(WORK_QUEUE_NO_YIELD_BIT),

    /* Transient work flags */
    WORK_RUNNING = BIT(WORK_RUNNING_BIT),
    WORK_CANCELING = BIT(WORK_CANCELING_BIT),
    WORK_QUEUED = BIT(WORK_QUEUED_BIT),
    WORK_DELAYED = BIT(WORK_DELAYED_BIT),
    WORK_FLUSHING = BIT(WORK_FLUSHING_BIT),
};

static std::shared_ptr<SpinLock> lock = std::make_shared<SpinLock>();
WorkQ sys_work_q;

static void flag_clear(uint32_t *flags, uint32_t bit) { *flags &= ~BIT(bit); }

static void flag_set(uint32_t *flags, uint32_t bit) { *flags |= BIT(bit); }

static bool flag_test(const uint32_t *flags, uint32_t bit) {
    return (*flags & BIT(bit)) != 0;
}

static bool flag_test_and_clear(uint32_t *flags, uint32_t bit) {
    bool ret = flag_test(flags, bit);
    flag_clear(flags, bit);
    return ret;
}

static void flags_set(uint32_t *flags, uint32_t new_flags) {
    *flags = new_flags;
}

static uint32_t flags_get(const uint32_t *flags) { return *flags; }

Work::Work(const work_handler_t &handler) : _lock(lock), handler(handler) {}

int Work::work_busy_get() const {
    std::unique_lock lock_(*lock);
    return work_busy_get_locked();
}

bool Work::work_is_pending() const { return work_busy_get() != 0; }

bool Work::work_flush(WorkSync *sync) {
    WorkFlusher *flusher = &std::get<WorkFlusher>(sync->backend);
    std::unique_lock lock_(*lock);
    bool need_flush = work_flush_locked(flusher);
    lock_.unlock();

    if (need_flush) {
        (void)flusher->sem.get();
    }

    return need_flush;
}

int Work::work_cancel() {
    std::unique_lock lock_(*lock);
    return cancel_async_locked();
}

bool Work::work_cancel_sync(WorkSync *sync) {
    WorkCanceller *canceller = &std::get<WorkCanceller>(sync->backend);
    std::unique_lock lock_(*lock);
    bool pending = (work_busy_get_locked() != 0u);
    bool need_wait = false;

    if (pending) {
        (void)cancel_async_locked();
        need_wait = cancel_sync_locked(canceller);
    }
    lock_.unlock();

    if (need_wait) {
        canceller->sem.get();
    }

    return pending;
}

int Work::work_busy_get_locked() const { return flags_get(&flags) & WORK_MASK; }

bool Work::work_flush_locked(WorkFlusher *flusher) {
    bool need_flush =
        (flags_get(&flags) & (WORK_QUEUED_BIT | WORK_RUNNING)) != 0u;

    if (need_flush) {
        queue->flusher_locked(this, flusher);
        queue->notify_locked();
    }

    return need_flush;
}

int Work::cancel_async_locked() {
    if (!flag_test(&flags, WORK_CANCELING_BIT)) {
        queue->remove_locked(this);
    }

    int ret = work_busy_get_locked();

    if (ret != 0) {
        flag_set(&flags, WORK_CANCELING_BIT);
        ret = work_busy_get_locked();
    }

    return ret;
}

bool Work::cancel_sync_locked(WorkCanceller *canceller) {
    bool ret = flag_test(&flags, WORK_CANCELING_BIT);

    if (ret) {
        // todo
    }

    return ret;
}

WorkDelayable::WorkDelayable(const work_handler_t &handler) : work(handler) {}

WorkDelayable::WorkDelayable(Work &&work) : work(std::move(work)) {}

int WorkDelayable::work_busy_get() const {
    std::unique_lock lock_(*lock);
    return work_busy_delayable_get_locked();
}

bool WorkDelayable::work_is_pending() const { return work_busy_get() != 0u; }

bool WorkDelayable::work_flush(WorkSync *sync) {
    WorkFlusher *flusher = &std::get<WorkFlusher>(sync->backend);
    std::unique_lock lock_(*lock);

    if (work.work_busy_get_locked() == 0u) {
        return false;
    }

    if (unschedule_locked()) {
        (void)queue->submit_locked(&work, &queue);
    }

    bool need_flush = work.work_flush_locked(flusher);
    lock_.unlock();

    if (need_flush) {
        flusher->sem.get();
    }

    return need_flush;
}

int WorkDelayable::work_cancel() {
    std::unique_lock lock_(*lock);
    return work_cancel_async_locked();
}

bool WorkDelayable::work_cancel_sync(WorkSync *sync) {
    WorkCanceller *canceller = &std::get<WorkCanceller>(sync->backend);
    std::unique_lock lock_(*lock);
    bool pending = work_busy_delayable_get_locked() != 0u;
    bool need_wait = false;

    if (pending) {
        (void)work_cancel_async_locked();
        need_wait = work.cancel_sync_locked(canceller);
    }
    lock_.unlock();

    if (need_wait) {
        canceller->sem.get();
    }

    return pending;
}

int WorkDelayable::work_busy_delayable_get_locked() const {
    return flags_get(&work.flags) & WORK_MASK;
}

int WorkDelayable::work_cancel_async_locked() {
    (void)unschedule_locked();
    return work.cancel_async_locked();
}

bool WorkDelayable::unschedule_locked() {
    bool ret = false;

    if (flag_test_and_clear(&work.flags, WORK_DELAYED_BIT)) {
        // todo: abort timeout
        ret = true;
    }

    return ret;
}
