/**
 * @file work_q.hpp
 *
 * @brief
 *
 * @date 4/9/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_WORK_Q_HPP
#define ARES_WORK_Q_HPP

#include <ares/data-structures/queue.hpp>
#include <ares/data-structures/sys/slist.h>
#include <chrono>
#include <functional>
#include <thread>
#include <variant>
#include "spinlock.hpp"
#include "task.hpp"

using namespace std::chrono_literals;

class WorkQ;
struct Work;
struct WorkSync;
struct WorkFlusher;
struct WorkCanceller;
struct WorkDelayable;

using work_handler_t = std::function<void(Work *)>;

struct Work {
    explicit Work(work_handler_t handler);
    Work() = delete;
    friend class WorkQ;
    friend struct WorkDelayable;

    [[nodiscard]] int work_busy_get() const;
    [[nodiscard]] bool work_is_pending() const;
    bool work_flush(WorkSync *sync);
    int work_cancel();
    bool work_cancel_sync(WorkSync *sync);

  private:
    // this is so the lock doesn't get destroyed before objects of this type
    std::shared_ptr<SpinLock> _lock;

    // protected with spinlock
    uint32_t flags = 0;
    WorkQ *queue = nullptr;
    work_handler_t handler = nullptr;
    sys_snode_t node = {nullptr};

    [[nodiscard]] int work_busy_get_locked() const;
    bool work_flush_locked(WorkFlusher *flusher);
    int cancel_async_locked();
    bool cancel_sync_locked(WorkCanceller *canceller);
};

struct WorkDelayable {
    explicit WorkDelayable(const work_handler_t &handler);
    explicit WorkDelayable(Work &&work);
    WorkDelayable() = delete;

    [[nodiscard]] int work_busy_get() const;
    [[nodiscard]] bool work_is_pending() const;
    // todo: expires_get()
    // todo: remaining_get()
    bool work_flush(WorkSync *sync);
    int work_cancel();
    bool work_cancel_sync(WorkSync *sync);

    friend WorkDelayable *work_delayable_from_work(Work *work);
  private:
    Work work;
    std::chrono::milliseconds timeout{};
    WorkQ *queue = nullptr;

    [[nodiscard]] int work_busy_delayable_get_locked() const;
    int work_cancel_async_locked();
    bool unschedule_locked();
};

struct WorkQConfig {
    const char *name = "";
    bool no_yield = false;

    // unused right now
    bool essential = false;
    std::chrono::milliseconds work_timeout_ms = 0ms;
};

class WorkQ {
  public:
    WorkQ();
    ~WorkQ();

    int submit(Work *work);
    void start(const WorkQConfig *config);
    void run(const WorkQConfig *config);
    [[nodiscard]] std::thread::id queue_thread_get() const;
    int queue_drain(bool plug);
    int queue_unplug();
    int stop();
    int stop(std::chrono::milliseconds timeout);
    // int schedule(WorkDelayable *dwork, std::chrono::milliseconds delay);
    // int reschedule(WorkDelayable *dwork, std::chrono::milliseconds delay);

    friend struct Work;
    friend struct WorkDelayable;

  private:
    Task<void(WorkQ*)> _thread;
    // this is so the lock doesn't get destroyed before objects of this type
    std::shared_ptr<SpinLock> _lock;

    // protected with spinlock
    sys_slist_t pending{};
    ares::bounded_queue<uint8_t, 1, true> notifyq;
    ares::bounded_queue<uint8_t, 1, true> drainq;
    uint32_t flags = 0;

    static int submit_locked(Work *work, WorkQ **queue);
    int submit_locked(Work *work);

    static void init_flusher(WorkFlusher *flusher);
    static void init_canceller(WorkCanceller *canceller, Work *work);
    void flusher_locked(Work *work, WorkFlusher *flusher);
    void notify_locked();
    void remove_locked(Work *work);

    static void work_queue_main(WorkQ *queue);

    static void finalize_flush_locked(Work *work);
    static void finalize_cancel_locked(Work *work);
};

struct WorkFlusher {
    Work work;
    ares::bounded_queue<uint8_t> sem;
};

struct WorkCanceller {
    sys_snode_t node;
    Work *work;
    ares::bounded_queue<uint8_t> sem;
};

struct WorkSync {
    friend struct Work;
    friend struct WorkDelayable;

  private:
    std::variant<std::monostate, WorkFlusher, WorkCanceller> backend;
};

extern WorkQ sys_work_q;

int work_submit_to_queue(WorkQ *queue, Work *work);
int work_submit(Work *work);
// int work_schedule_for_queue(WorkQ *queue, WorkDelayable *dwork,
//                             std::chrono::milliseconds delay);
// int work_schedule(WorkDelayable *dwork, std::chrono::milliseconds delay);
// int work_reschedule_for_queue(WorkQ *queue, WorkDelayable *dwork,
//                               std::chrono::milliseconds delay);
// int work_reschedule(WorkDelayable *dwork, std::chrono::milliseconds delay);

WorkDelayable *work_delayable_from_work(Work *work);

#endif // ARES_WORK_Q_HPP
