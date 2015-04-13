#ifndef QUEUE_H_
#define QUEUE_H_

#include <atomic> 
#include <mutex>
#include <memory> 
#include <thread> 
#include <list> 
#include "pi.h"

namespace util {

template <typename _Tp>
class Queue {
    typedef std::list<_Tp> _ContainerT;
    typedef typename _ContainerT::value_type value_type;
        
    _ContainerT queue_;
    std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    std::atomic<int> max_;
public:
    value_type pop() {
        std::unique_lock<std::mutex> mlock(mutex_);
        while (queue_.empty()) {
            not_empty_.wait(mlock);
        }
        value_type result = queue_.front();
        queue_.pop_front();
        mlock.unlock();
        not_full_.notify_one();
        return result;
    }

    void push(const value_type& item) {
        std::unique_lock<std::mutex> mlock(mutex_);
        while (size() >= static_cast<int>(max_)) {
            not_full_.wait(mlock);
        }
        queue_.push_back(item);
        mlock.unlock();
        not_empty_.notify_one();
    }

    int size() const {
        return queue_.size();
    }

    Queue(int max = 10) : max_(max) {}
    Queue(const Queue<_Tp>& q) : queue_(q.queue_) {}
    Queue& operator=(const Queue<_Tp>& q) {
        queue_ = q.queue_;
        return *this;
    }

    pi::Process operator()(pi::Var<value_type>& var)       { return pi::Process(In<value_type>(&Queue<value_type>::pop, *this, var)); }
    pi::Process operator[](const pi::Var<value_type>& var) { return pi::Process(Out<value_type>(&Queue<value_type>::push, *this, var)); }
};

}

#endif
