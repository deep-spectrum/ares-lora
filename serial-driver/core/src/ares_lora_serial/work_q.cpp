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
#include <cstddef>
#include <utility>
#include <ares/util.h>
#include <ares/util.hpp>

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
static sys_slist_t pending_cancels;

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

Work::Work(work_handler_t handler) : _lock(lock), handler(std::move(handler)) {}

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

int Work::work_busy_get_locked() const { return static_cast<int>(flags_get(&flags) & WORK_MASK); }

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
        WorkQ::init_canceller(canceller, this);
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
        (void)WorkQ::submit_locked(&work, &queue);
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
    return static_cast<int>(flags_get(&work.flags) & WORK_MASK);
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

WorkQ::WorkQ() : _lock(lock) {
    flags = 0;
}

WorkQ::~WorkQ() {
    this->queue_drain(true);
    stop();
}

int WorkQ::submit(Work *work) {
    std::unique_lock lock_(*lock);
    WorkQ *queue = this;
    return submit_locked(work, &queue);
}

void WorkQ::start(const WorkQConfig *config) {
    uint32_t flags_ = WORK_QUEUE_STARTED;

    sys_slist_init(&pending);
    notifyq.clear();
    drainq.clear();

    if (config != nullptr && config->no_yield) {
        flags_ |= WORK_QUEUE_NO_YIELD;
    }

    flags_set(&flags, flags_);

    _thread = std::thread(work_queue_main, this);
    if (config != nullptr && config->name != nullptr) {
        name = config->name;
    }
}

void WorkQ::run(const WorkQConfig *config) {
    uint32_t flags_ = WORK_QUEUE_STARTED;

    if (config != nullptr && config->no_yield) {
        flags_ |= WORK_QUEUE_NO_YIELD;
    }

    if (config != nullptr && config->name != nullptr) {
        name = config->name;
    }

    sys_slist_init(&pending);
    notifyq.clear();
    drainq.clear();
    flags_set(&flags, flags_);
    work_queue_main(this);
}

std::thread::id WorkQ::queue_thread_get() const {
    return _thread.get_id();
}

int WorkQ::queue_drain(bool plug) {
    int ret = 0;
    std::unique_lock lock_(*lock);

    if (((flags_get(&flags) & (WORK_QUEUE_BUSY | WORK_QUEUE_DRAIN)) != 0) || plug || !sys_slist_is_empty(&pending)) {
        flag_set(&flags, WORK_QUEUE_DRAIN_BIT);
        if (plug) {
            flag_set(&flags, WORK_QUEUE_PLUGGED_BIT);
        }

        notifyq.put_nonblocking(0);
        ret = drainq.get();
    }

    return ret;
}

int WorkQ::queue_unplug() {
    int ret = -EALREADY;
    std::unique_lock lock_(*lock);

    if (flag_test_and_clear(&flags, WORK_QUEUE_PLUGGED_BIT)) {
        ret = 0;
    }

    return ret;
}

int WorkQ::stop() {
    return stop(std::chrono::milliseconds::max());
}

int WorkQ::stop(std::chrono::milliseconds timeout) {
    std::unique_lock lock_(*lock);

    if (!flag_test(&flags, WORK_QUEUE_STARTED_BIT)) {
        return -EALREADY;
    }

    if (!flag_test(&flags, WORK_QUEUE_PLUGGED_BIT)) {
        return -EBUSY;
    }

    flag_set(&flags, WORK_QUEUE_STOP_BIT);
    notifyq.put_nonblocking(0);
    lock_.unlock();

    if (timeout == std::chrono::milliseconds::max()) {
        _thread.join();
        return 0;
    }

    // todo: some timeout (replace with packaged task)
    return -ETIMEDOUT;
}

int WorkQ::submit_locked(Work *work, WorkQ **queue) {
    int ret = 0;

    if (flag_test(&work->flags, WORK_CANCELING_BIT)) {
        ret = -EBUSY;
    } else if (!flag_test(&work->flags, WORK_QUEUED_BIT)) {
        ret = 1;

        if (*queue == nullptr) {
            *queue = work->queue;
        }

        if (flag_test(&work->flags, WORK_RUNNING_BIT)) {
            *queue = work->queue;
            ret = 2;
        }

        int rc = (*queue)->submit_locked(work);

        if (rc < 0) {
            ret = rc;
        } else {
            flag_set(&work->flags, WORK_QUEUED_BIT);
            work->queue = *queue;
        }
    } // else already in queue, do nothing ...

    if (ret <= 0) {
        *queue = nullptr;
    }

    return ret;
}

int WorkQ::submit_locked(Work *work) {
    int ret;
    bool chained = std::this_thread::get_id() == _thread.get_id();
    bool draining = flag_test(&flags, WORK_QUEUE_DRAIN_BIT);
    bool plugged = flag_test(&flags, WORK_QUEUE_PLUGGED_BIT);

    if (!flag_test(&flags, WORK_QUEUE_STARTED_BIT)) {
        ret = -ENODEV;
    } else if ((draining && !chained) || (plugged && !draining)) {
        ret = -EBUSY;
    } else {
        sys_slist_append(&pending, &work->node);
        ret = 1;
        notifyq.put_nonblocking(0);
    }

    return ret;
}

static void handle_flush(Work *work) { ARG_UNUSED(work); }

void WorkQ::init_flusher(WorkFlusher *flusher) {
    flusher->work = Work(handle_flush);
    flusher->sem.clear();
    flag_set(&flusher->work.flags, WORK_FLUSHING_BIT);
}

void WorkQ::init_canceller(WorkCanceller *canceller, Work *work) {
    canceller->sem.clear();
    canceller->work = work;
    sys_slist_append(&pending_cancels, &canceller->node);
}

void WorkQ::flusher_locked(Work *work, WorkFlusher *flusher) {
    init_flusher(flusher);

    if ((flags_get(&work->flags) & WORK_QUEUED) != 0u) {
        sys_slist_insert(&pending, &work->node, &flusher->work.node);
    } else {
        sys_slist_prepend(&pending, &flusher->work.node);
    }
}

void WorkQ::notify_locked() {
    notifyq.put_nonblocking(0);
}

void WorkQ::remove_locked(Work *work) {
    if (flag_test_and_clear(&work->flags, WORK_QUEUED_BIT)) {
        (void)sys_slist_find_and_remove(&pending, &work->node);
    }
}

void WorkQ::work_queue_main(WorkQ *queue) {
    while (true) {
        sys_snode_t *node;
        Work *work = nullptr;
        work_handler_t handler = nullptr;

        std::unique_lock lock_(*lock);

        node = sys_slist_get(&queue->pending);
        if (node != nullptr) {
            flag_set(&queue->flags, WORK_QUEUE_BUSY_BIT);
            work = container_of(node, &Work::node);
            flag_set(&work->flags, WORK_RUNNING_BIT);
            flag_clear(&work->flags, WORK_QUEUED_BIT);

            handler = work->handler;
        } else if (flag_test_and_clear(&queue->flags, WORK_QUEUE_DRAIN_BIT)) {
            queue->drainq.put_nonblocking(1);
        } else if (flag_test(&queue->flags, WORK_QUEUE_STOP_BIT)) {
            flags_set(&queue->flags, 0);
            return;
        }

        if (work == nullptr) {
            lock_.unlock();
            queue->notifyq.get();
            continue;
        }

        lock_.unlock();

        handler(work);

        lock_.lock();

        flag_clear(&work->flags, WORK_RUNNING_BIT);
        if (flag_test(&work->flags, WORK_FLUSHING_BIT)) {
            finalize_flush_locked(work);
        }
        if (flag_test(&work->flags, WORK_CANCELING_BIT)) {
            finalize_cancel_locked(work);
        }

        flag_clear(&queue->flags, WORK_CANCELING_BIT);
        bool yield = !flag_test(&queue->flags, WORK_QUEUE_NO_YIELD_BIT);
        lock_.unlock();

        if (yield) {
            std::this_thread::yield();
        }
    }
}

void WorkQ::finalize_flush_locked(Work *work) {
    WorkFlusher *flusher = container_of(work, &WorkFlusher::work);
    flag_clear(&work->flags, WORK_FLUSHING_BIT);
    flusher->sem.put_nonblocking(0);
}

void WorkQ::finalize_cancel_locked(Work *work) {
    WorkCanceller *wc, *tmp;
    sys_snode_t *prev = nullptr;

    flag_clear(&work->flags, WORK_CANCELING_BIT);

    SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&pending_cancels, wc, tmp, node) {
        if (wc->work == work) {
            sys_slist_remove(&pending_cancels, prev, &wc->node);
            wc->sem.put_nonblocking(0);
            break;
        }
        prev = &wc->node;
    }
}

int work_submit_to_queue(WorkQ *queue, Work *work) {
    return queue->submit(work);
}

int work_submit(Work *work) {
    return sys_work_q.submit(work);
}

WorkDelayable * work_delayable_from_work(Work *work) {
    return container_of(work, &WorkDelayable::work);
}
