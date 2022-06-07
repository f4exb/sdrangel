#pragma once

#include <stdexcept>
#include <list>
#include <iterator>
#include <algorithm>
#include <thread>
#include <condition_variable>
#include <mutex>

namespace mobilinkd
{

/**
 * A thread-safe queue
 */
template <typename T, size_t SIZE>
class queue
{
private:

    using mutex_type = std::mutex;
    using lock_type = std::unique_lock<mutex_type>;
    using guard_type = std::lock_guard<mutex_type>;

    enum class State {OPEN, CLOSING, CLOSED};

    std::list<T> queue_;
    size_t size_ = 0;
    State state_ = State::OPEN;
    mutable mutex_type mutex_;
    std::condition_variable full_;
    std::condition_variable empty_;
    
    queue(queue&) = delete;
    queue& operator=(const queue&) = delete;

public:

    static constexpr auto forever = std::chrono::seconds::max();

    /// The data type stored in the queue.
    using value_type = T;

    /// A reference to an element stored in the queue.
    using reference = value_type&;

    /// A const reference to an element stored in the queue.
    using const_reference = value_type const&;

    /// A pointer to an element stored in a Queue.
    using pointer = value_type*;

    /// A pointer to an element stored in a Queue.
    using const_pointer = const value_type*;
    
    queue()
    {}

    /**
     * Get the next item in the queue.
     *
     * @param[out] val is an object into which the object will be moved
     *  or copied.
     * @param[in] timeout is the duration to wait for an item to appear
     *  in the queue (default is forever, duration::max()).
     *
     * @return true if a value was returned, otherwise false.
     * 
     * @note The return value me be false if either the timeout expires
     *  or the queue is closed.
     */
    template<class Clock> 
    bool get_until(reference val, std::chrono::time_point<Clock> when)
    {
        lock_type lock(mutex_);

        while (queue_.empty())
        {
            if (State::CLOSED == state_)
            {
                return false;
            }

            if (empty_.wait_until(lock, when) == std::cv_status::timeout)
            {
                return false;
            }
        }

        val = std::move(queue_.front());
        queue_.pop_front();
        size_ -= 1;

        if (state_ == State::CLOSING && queue_.empty())
        {
            state_ == State::CLOSED;
        }
        
        full_.notify_one();

        return true;
    }

    /**
     * Get the next item in the queue.
     *
     * @param[out] val is an object into which the object will be moved
     *  or copied.
     * @param[in] timeout is the duration to wait for an item to appear
     *  in the queue (default is forever, duration::max()).
     *
     * @return true if a value was returned, otherwise false.
     * 
     * @note The return value me be false if either the timeout expires
     *  or the queue is closed.
     */
    template<class Rep = int64_t, class Period = std::ratio<1>> 
    bool get(reference val, std::chrono::duration<Rep, Period> timeout = std::chrono::duration<Rep, Period>::max())
    {
        lock_type lock(mutex_);

        while (queue_.empty())
        {
            if (State::CLOSED == state_)
            {
                return false;
            }

            if (empty_.wait_for(lock, timeout) == std::cv_status::timeout)
            {
                return false;
            }
        }

        val = std::move(queue_.front());
        queue_.pop_front();
        size_ -= 1;

        if (state_ == State::CLOSING && queue_.empty())
        {
            state_ == State::CLOSED;
        }
        
        full_.notify_one();

        return true;
    };
    
    /**
     * Put an item on the queue.
     *
     * @param[in] val is the element to be appended to the queue.
     * @param[in] timeout is the duration to wait until queue there is room
     *  for more items on the queue (default is forever -- duration::max()).
     *
     * @return true if a value was put on the queue, otherwise false.
     * 
     * @note The return value me be false if either the timeout expires
     *  or the queue is closed.
     */
    template<typename U, class Rep = int64_t, class Period = std::ratio<1>> 
    bool put(U&& val, std::chrono::duration<Rep, Period> timeout = std::chrono::duration<Rep, Period>::max())
    {
        // Get the queue mutex.
        lock_type lock(mutex_);

        if (SIZE == size_)
        {
            if (timeout.count() == 0)
            {
                return false; 
            }

            auto expiration = std::chrono::system_clock::now() + timeout;
            
            while (SIZE == size_)
            {
                if (State::OPEN != state_)
                {
                    return false;
                }

                if (full_.wait_until(lock, expiration) == std::cv_status::timeout)
                {
                    return false;
                }
            }
        }
        
        if (State::OPEN != state_)
        {
            return false;
        }

        queue_.emplace_back(std::forward<U>(val));
        size_ += 1;

        empty_.notify_one();
        
        return true;
    };

    void close()
    {
        guard_type lock(mutex_);

        state_ = (queue_.empty() ? State::CLOSED : State::CLOSING);
        
        full_.notify_all();
        empty_.notify_all();
    }
    
    bool is_open() const
    {
        return State::OPEN == state_;
    }
        
    bool is_closed() const
    {
        return State::CLOSED == state_;
    }

    /**
     *  @return the number of items in the queue.
     */
    size_t size() const
    {
        guard_type lock(mutex_);
        return size_;
    }
    
    /**
     *  @return the number of items in the queue.
     */
    bool empty() const
    {
        guard_type lock(mutex_);
        return size_ == 0;
    }

    /**
     *  @return the capacity of the queue.
     */
    static constexpr size_t capacity()
    {
        return SIZE;
    }
};

} // mobilinkd
