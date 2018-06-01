#include "thread-pool.hh"

thread_local bool thread_pool::working = true;


// the destructor joins all threads
thread_pool::~thread_pool() noexcept
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop.store(true, std::memory_order_relaxed);
    }
    {
        std::unique_lock<std::mutex> lk(exit_mutex);
        while(!exits.empty()){
            auto& fun = exits.front();
            fun();
            exits.pop();
        }
    }
    condition.notify_all();
    for(auto& worker: workers)
        worker.join();

    if (monitor.joinable())
        monitor.join();
}

void thread_pool::work()
{
    working = true;

    while (working)
    {
        std::function<void()> task;

        {
            std::unique_lock<std::mutex> lock(this->queue_mutex);

            ++ waiters; // incr waiter count
            this->condition.wait(lock,
                [this]{ return this->stop || !this->tasks.empty(); });
            --  waiters; // decr waiter count

            if(this->stop && this->tasks.empty())
                return;
            task = std::move(this->tasks.front());
            this->tasks.pop();
        }

        task();
    }

    // if reach here, this thread is recycled by monitor thread
}

