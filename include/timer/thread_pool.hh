#pragma once

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
#include <sys/eventfd.h>

class thread_pool {
public:
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args)
        -> std::future<typename std::result_of<F(Args...)>::type>;
    void notify_monitor();

    ~thread_pool() noexcept;
    thread_pool();
protected:
private:

    void work();
    void recycle();

    // need to keep track of threads so we can join them
    std::vector< std::thread > workers;
    // the task queue
    std::queue< std::function<void()> > tasks;

    // synchronization
    std::mutex queue_mutex;
    std::condition_variable condition;
    std::atomic<bool> stop;
    unsigned waiters;

    // for recycle idle threads
    static thread_local bool working;
    std::atomic< unsigned > maxIdle;
    int         monitorfd;
    std::thread monitor;
};

thread_local bool thread_pool::working = true;

// the constructor just launches monitor thread
inline thread_pool::thread_pool()
    :   stop(false), waiters(0)
{
    maxIdle = std::max< unsigned >(1, static_cast<int>(std::thread::hardware_concurrency()));
     monitorfd = ::eventfd(1,0);
    assert(monitorfd != -1);
    //   for (int i= 0; i < (maxIdle*2+1); i++)
    //   {
    //       std::thread  t([this]() { this->work(); } );
    //       workers.push_back(std::move(t));
    //   }

    monitor = std::thread([this]() { this->recycle(); } );
}

// add new work item to the pool
template<class F, class... Args>
auto thread_pool::enqueue(F&& f, Args&&... args)
    -> std::future<typename std::result_of<F(Args...)>::type>
{
    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared< std::packaged_task<return_type()> >(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(queue_mutex);

        // don't allow enqueueing after stopping the pool
        if(stop)
            throw std::runtime_error("enqueue on stopped thread_pool");

        tasks.emplace([task](){ (*task)(); });

        // if there is no idle thread, create one, do not let this task wait
        if (waiters == 0 && workers.size() < (maxIdle*2+1))
        {
            std::thread  t([this]() { this->work(); } );
            workers.push_back(std::move(t));
        }
    }
    condition.notify_one();
    return res;
}

// the destructor joins all threads
inline thread_pool::~thread_pool() noexcept
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop.store(true, std::memory_order_relaxed);
    }
    condition.notify_all();
    for(auto& worker: workers)
        worker.join();

    notify_monitor();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    close(monitorfd);
    monitor.join();
}

inline void thread_pool::work()
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

//inline void thread_pool::recycle()
//{
//    while (!stop)
//    {
//        std::this_thread::sleep_for(std::chrono::seconds(60));
//
//        std::unique_lock<std::mutex> lock(this->queue_mutex);
//        if (stop)
//            return;
//
//        auto nw = waiters;
//        while (nw -- > maxIdle)
//        {
//            // the thread which fetch this task item will exit
//            tasks.emplace([this]() { working = false; });
//            condition.notify_one();
//        }
//    }
//}
void thread_pool::notify_monitor()
{
    eventfd_t  value = 1;
    auto ret = write(monitorfd, &value, sizeof(value));
    assert(ret == sizeof(eventfd_t));
}


inline std::shared_ptr<thread_pool>  make_threadpool()
{
    return std::make_shared<thread_pool>();
}


