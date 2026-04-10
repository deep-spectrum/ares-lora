/**
 * @file spinlock.hpp
 *
 * @brief
 *
 * @date 4/9/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_SPINLOCK_HPP
#define ARES_SPINLOCK_HPP

#include <atomic>

class SpinLock {
    std::atomic_flag _locked = ATOMIC_FLAG_INIT;

  public:
    void lock() {
        while (_locked.test_and_set(std::memory_order_acquire))
            ;
    }

    void unlock() { _locked.clear(std::memory_order_release); }
};

#endif // ARES_SPINLOCK_HPP
