// ======================= thread_pool.cpp =======================
#include "thread_pool.hpp"

namespace hz_mq {

thread_pool::thread_pool(size_t num_threads)
    : __stop(false)
{
    if (num_threads == 0) {
        num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) num_threads = 1;
    }

    for (size_t i = 0; i < num_threads; ++i) {
        __threads.emplace_back([this] {
            std::function<void()> task;
            while (true) {
                {
                    std::unique_lock<std::mutex> lock(this->__mtx);
                    this->__cv.wait(
                        lock,
                        [this] { return this->__stop || !this->__tasks.empty(); }
                    );
                    if (this->__stop && this->__tasks.empty()) return;
                    task = std::move(this->__tasks.front());
                    this->__tasks.pop();
                }
                task();  // 在锁外执行
            }
        });
    }
}

thread_pool::~thread_pool()
{
    {
        std::unique_lock<std::mutex> lock(__mtx);
        __stop = true;
    }
    __cv.notify_all();
    for (std::thread& t : __threads) {
        if (t.joinable()) t.join();
    }
}

void thread_pool::push(const std::function<void()>& task)
{
    {
        std::unique_lock<std::mutex> lock(__mtx);
        if (__stop) return;
        __tasks.emplace(task);
    }
    __cv.notify_one();
}

} 
