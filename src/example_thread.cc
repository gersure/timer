#include <iostream>
#include <thread>
#include <memory>
#include "timer/timer-manager.hh"
#include "timer/thread_pool.hh"

using namespace std;
int main(int argc, char *argv[])
{
    auto pool = make_threadpool();
    timer_manager::Instance().set_thread_pool(pool);

    timer<steady_clock_type>  t3([](){cout<<"t3 ------ timeout!"<<endl;});
    t3.arm(std::chrono::microseconds(1000));

    for(int i=0; i< 10;i++){
        pool->enqueue([i]{
                timer_manager::Instance().expired_timer(std::chrono::microseconds(1), [i]{
                    cout<<"timer:"<<i<<endl;
                    });
                });
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
    cout<<"main end"<<endl;

    return 0;
}

