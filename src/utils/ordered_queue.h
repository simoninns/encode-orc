/*
 * File:        ordered_queue.h
 * Module:      encode-orc
 * Purpose:     Thread-safe queue that maintains ordering
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_ORDERED_QUEUE_H
#define ENCODE_ORC_ORDERED_QUEUE_H

#include <map>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <stdexcept>

namespace encode_orc {

/**
 * @brief Thread-safe queue that preserves ordering by index
 * 
 * Items can be pushed with arbitrary indices, but they can only be
 * popped in sequential order (0, 1, 2, ...). This is useful for
 * parallel processing where results must be consumed in order.
 * 
 * @tparam T Type of items stored in queue
 */
template<typename T>
class OrderedQueue {
public:
    /**
     * @brief Construct empty ordered queue
     */
    OrderedQueue() : next_index_(0), stopped_(false) {}
    
    /**
     * @brief Destructor
     */
    ~OrderedQueue() {
        stop();
    }
    
    // Non-copyable, non-movable
    OrderedQueue(const OrderedQueue&) = delete;
    OrderedQueue& operator=(const OrderedQueue&) = delete;
    OrderedQueue(OrderedQueue&&) = delete;
    OrderedQueue& operator=(OrderedQueue&&) = delete;
    
    /**
     * @brief Push an item with a specific index
     * @param index Index of the item
     * @param item Item to push
     * @throws std::invalid_argument if index already exists
     */
    void push(size_t index, T&& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        if (stopped_) {
            throw std::runtime_error("Cannot push to stopped OrderedQueue");
        }
        
        auto result = buffer_.emplace(index, std::move(item));
        if (!result.second) {
            throw std::invalid_argument("Index already exists in OrderedQueue");
        }
        
        cv_.notify_all();
    }
    
    /**
     * @brief Try to pop the next item in order (non-blocking)
     * @param item Output parameter to receive the item
     * @return true if item was popped, false if next item not yet available
     */
    bool try_pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        auto it = buffer_.find(next_index_);
        if (it != buffer_.end()) {
            item = std::move(it->second);
            buffer_.erase(it);
            ++next_index_;
            return true;
        }
        
        return false;
    }
    
    /**
     * @brief Wait for and pop the next item in order (blocking)
     * @param item Output parameter to receive the item
     * @return true if item was popped, false if queue was stopped
     */
    bool wait_and_pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        cv_.wait(lock, [this] {
            return stopped_ || buffer_.find(next_index_) != buffer_.end();
        });
        
        if (stopped_ && buffer_.find(next_index_) == buffer_.end()) {
            return false;
        }
        
        auto it = buffer_.find(next_index_);
        if (it != buffer_.end()) {
            item = std::move(it->second);
            buffer_.erase(it);
            ++next_index_;
            return true;
        }
        
        return false;
    }
    
    /**
     * @brief Wait for a specific index to be available and pop it
     * @param index Expected index to pop
     * @param item Output parameter to receive the item
     * @return true if item was popped, false if queue was stopped
     * @throws std::logic_error if index doesn't match next expected index
     */
    bool wait_and_pop(size_t index, T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        if (index != next_index_) {
            throw std::logic_error("Requested index doesn't match next expected index");
        }
        
        cv_.wait(lock, [this, index] {
            return stopped_ || buffer_.find(index) != buffer_.end();
        });
        
        if (stopped_ && buffer_.find(index) == buffer_.end()) {
            return false;
        }
        
        auto it = buffer_.find(index);
        if (it != buffer_.end()) {
            item = std::move(it->second);
            buffer_.erase(it);
            ++next_index_;
            return true;
        }
        
        return false;
    }
    
    /**
     * @brief Stop the queue, unblocking any waiting threads
     */
    void stop() {
        std::unique_lock<std::mutex> lock(mutex_);
        stopped_ = true;
        cv_.notify_all();
    }
    
    /**
     * @brief Check if queue is empty
     */
    bool empty() const {
        std::unique_lock<std::mutex> lock(mutex_);
        return buffer_.empty();
    }
    
    /**
     * @brief Get number of items currently in queue
     */
    size_t size() const {
        std::unique_lock<std::mutex> lock(mutex_);
        return buffer_.size();
    }
    
    /**
     * @brief Get the next expected index
     */
    size_t next_index() const {
        std::unique_lock<std::mutex> lock(mutex_);
        return next_index_;
    }
    
private:
    std::map<size_t, T> buffer_;
    size_t next_index_;
    bool stopped_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
};

} // namespace encode_orc

#endif // ENCODE_ORC_ORDERED_QUEUE_H
