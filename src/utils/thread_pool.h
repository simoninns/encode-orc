/*
 * File:        thread_pool.h
 * Module:      encode-orc
 * Purpose:     Thread pool for parallel frame encoding
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_THREAD_POOL_H
#define ENCODE_ORC_THREAD_POOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <stdexcept>

namespace encode_orc {

/**
 * @brief Thread pool for executing tasks in parallel
 * 
 * Manages a pool of worker threads that process tasks from a queue.
 * Tasks are submitted via enqueue() and executed asynchronously.
 */
class ThreadPool {
public:
    /**
     * @brief Construct thread pool with specified number of workers
     * @param num_threads Number of worker threads to create
     * @throws std::invalid_argument if num_threads is 0
     */
    explicit ThreadPool(size_t num_threads);
    
    /**
     * @brief Destructor - waits for all tasks to complete
     */
    ~ThreadPool();
    
    // Non-copyable, non-movable
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;
    
    /**
     * @brief Enqueue a task for execution
     * @param f Function to execute
     * @param args Arguments to pass to function
     * @return Future that will contain the result
     * @throws std::runtime_error if pool has been stopped
     */
    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type>;
    
    /**
     * @brief Wait for all currently queued tasks to complete
     * 
     * Blocks until the task queue is empty and all workers are idle.
     * New tasks can still be submitted after this call returns.
     */
    void wait_all();
    
    /**
     * @brief Get number of worker threads
     */
    size_t thread_count() const { return workers_.size(); }
    
private:
    // Worker thread function
    void worker_thread();
    
    // Worker threads
    std::vector<std::thread> workers_;
    
    // Task queue
    std::queue<std::function<void()>> tasks_;
    
    // Synchronization
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::condition_variable wait_condition_;
    
    // State
    bool stop_;
    size_t busy_threads_;
};

// Template implementation
template<typename F, typename... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args) 
    -> std::future<typename std::invoke_result<F, Args...>::type>
{
    using return_type = typename std::invoke_result<F, Args...>::type;
    
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
    std::future<return_type> result = task->get_future();
    
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        
        if (stop_) {
            throw std::runtime_error("Cannot enqueue on stopped ThreadPool");
        }
        
        tasks_.emplace([task]() { (*task)(); });
    }
    
    condition_.notify_one();
    return result;
}

} // namespace encode_orc

#endif // ENCODE_ORC_THREAD_POOL_H
