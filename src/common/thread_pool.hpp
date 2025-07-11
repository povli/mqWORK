// ======================= thread_pool.hpp =======================
#pragma once

#include <functional>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>

namespace hz_mq {

class thread_pool {
public:
    using ptr = std::shared_ptr<thread_pool>;

    explicit thread_pool(size_t num_threads = 0);
    ~thread_pool();

    // 向线程池提交任务
    void push(const std::function<void()>& task);

private:
    std::vector<std::thread> __threads;
    std::queue<std::function<void()>> __tasks;
    std::mutex __mtx;
    std::condition_variable __cv;
    std::atomic<bool> __stop;
};

} 
