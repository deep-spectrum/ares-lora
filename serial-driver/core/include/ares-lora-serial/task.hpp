/**
 * @file task.hpp
 *
 * @brief
 *
 * @date 4/10/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */
 

#ifndef ARES_TASK_HPP
#define ARES_TASK_HPP

#include <thread>
#include <future>
#include <functional>
#include <chrono>
#include <string>
#include <atomic>
#include <utility>

template <typename Signature>
class Task {
public:
    explicit Task(std::function<Signature> handler) : handler(handler) {}
    ~Task();

    int set_name(std::string new_name);
    [[nodiscard]] const char *get_name() const;

    int set_essential(bool essential_task);
    [[nodiscard]] bool get_essential() const;

    template <typename... Args>
    void start(Args&&... args);

    template <typename... Args>
    void run(Args&&... args);

    int join(std::chrono::milliseconds timeout = std::chrono::milliseconds::max());
    [[nodiscard]] std::thread::id get_id() const;

private:
    std::atomic_bool _locked = false;
    bool essential = false;
    std::string name;
    std::packaged_task<void()> task;
    std::future<void> future;
    std::thread thread;
    std::function<Signature> handler;

    template <typename... Args>
    void init_task(Args&&... args);
};

template<typename Signature>
Task<Signature>::~Task() {
    this->join();
}

template<typename Signature>
int Task<Signature>::set_name(std::string new_name) {
    if (_locked) {
        return -EBUSY;
    }
    name = std::move(new_name);
    return 0;
}

template<typename Signature>
const char * Task<Signature>::get_name() const {
    return name.c_str();
}

template<typename Signature>
int Task<Signature>::set_essential(bool essential_task) {
    if (_locked) {
        return -EBUSY;
    }
    essential = essential_task;
    return 0;
}

template<typename Signature>
bool Task<Signature>::get_essential() const {
    return essential;
}

template<typename Signature>
template<typename ... Args>
void Task<Signature>::start(Args &&...args) {
    init_task(std::forward<Args>(args)...);
    thread = std::thread(std::move(task));
}

template<typename Signature>
template<typename ... Args>
void Task<Signature>::run(Args &&...args) {
    init_task(std::forward<Args>(args)...);
    task();
}

template<typename Signature>
int Task<Signature>::join(const std::chrono::milliseconds timeout) {
    if (timeout == std::chrono::milliseconds::max()) {
        thread.join();
        return 0;
    }

    auto status = future.wait_for(timeout);
    if (status == std::future_status::ready) {
        thread.join();
        return 0;
    }
    return -ETIMEDOUT;
}

template<typename Signature>
std::thread::id Task<Signature>::get_id() const {
    return thread.get_id();
}

template<typename Signature>
template<typename ... Args>
void Task<Signature>::init_task(Args &&...args) {
    _locked = true;

    auto wrapper = [this](auto&&... bound_args) mutable {
        try {
            this->handler(std::forward<decltype(bound_args)>(bound_args)...);
        } catch (const std::exception &e) {
            if (essential) {
                // todo: log
                std::abort();
            }
            throw;
        }
    };

    auto bound_task = std::bind(std::move(wrapper), std::forward<Args>(args)...);
    task = std::packaged_task<void()>(std::move(bound_task));
    future = task.get_future();
}

#endif //ARES_TASK_HPP
