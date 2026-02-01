/*
 * File:        thread_pool.cpp
 * Module:      encode-orc
 * Purpose:     Thread pool implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "thread_pool.h"
#include <stdexcept>

namespace encode_orc {

ThreadPool::ThreadPool(size_t num_threads)
    : stop_(false), busy_threads_(0)
{
    if (num_threads == 0) {
        throw std::invalid_argument("ThreadPool must have at least 1 thread");
    }
    
    workers_.reserve(num_threads);
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back(&ThreadPool::worker_thread, this);
    }
}

ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        stop_ = true;
    }
    
    condition_.notify_all();
    
    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::wait_all()
{
    std::unique_lock<std::mutex> lock(queue_mutex_);
    wait_condition_.wait(lock, [this] {
        return tasks_.empty() && busy_threads_ == 0;
    });
}

void ThreadPool::worker_thread()
{
    while (true) {
        std::function<void()> task;
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            
            condition_.wait(lock, [this] {
                return stop_ || !tasks_.empty();
            });
            
            if (stop_ && tasks_.empty()) {
                return;
            }
            
            if (!tasks_.empty()) {
                task = std::move(tasks_.front());
                tasks_.pop();
                ++busy_threads_;
            }
        }
        
        if (task) {
            task();
            
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                --busy_threads_;
                wait_condition_.notify_all();
            }
        }
    }
}

} // namespace encode_orc
