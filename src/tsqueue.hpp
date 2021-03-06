#pragma once

#include <mutex>
#include <condition_variable>
#include <queue>
#include <memory>

template<typename T>
class tsqueue
{
private:
    mutable std::mutex mut;
    std::queue<T> data_queue;
    std::condition_variable data_cond;
    std::condition_variable empty_cond;
public:
    tsqueue()
    {}
    tsqueue(tsqueue const& other)
    {
        std::lock_guard<std::mutex> lk(other.mut);
        data_queue=other.data_queue;
    }

    void push(T new_value)
    {
        std::lock_guard<std::mutex> lk(mut);
        data_queue.push(new_value);
        data_cond.notify_one();
    }

    void wait_and_pop(T& value)
    {
        std::unique_lock<std::mutex> lk(mut);
        data_cond.wait(lk,[this]{return !data_queue.empty();});
        value=data_queue.front();
        data_queue.pop();
        empty_cond.notify_one();
    }

    std::shared_ptr<T> wait_and_pop()
    {
        std::unique_lock<std::mutex> lk(mut);
        data_cond.wait(lk,[this]{return !data_queue.empty();});
        std::shared_ptr<T> res(std::make_shared<T>(data_queue.front()));
        data_queue.pop();
        empty_cond.notify_one();
        return res;
    }

    void wait_empty()
    {
        std::unique_lock<std::mutex> lk(mut);
        empty_cond.wait(lk,[this]{return data_queue.empty();});
    }

    void wait_size(unsigned int sz)
    {
        std::unique_lock<std::mutex> lk(mut);
	while (data_queue.size() > sz) {
	    empty_cond.wait(lk);
	}
    }

    bool try_pop(T& value)
    {
        std::lock_guard<std::mutex> lk(mut);
        if(data_queue.empty)
            return false;
        value=data_queue.front();
        data_queue.pop();
    }

    std::shared_ptr<T> try_pop()
    {
        std::lock_guard<std::mutex> lk(mut);
        if(data_queue.empty())
            return std::shared_ptr<T>();
        std::shared_ptr<T> res(std::make_shared<T>(data_queue.front()));
        data_queue.pop();
        return res;
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> lk(mut);
        return data_queue.empty();
    }

    unsigned int size() const
    {
        std::lock_guard<std::mutex> lk(mut);
        return data_queue.size();
    }
};

