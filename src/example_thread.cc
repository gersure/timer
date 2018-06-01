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

    auto ret = timer_manager::Instance().add_timer(std::chrono::seconds(2), [](){
            cout<<"timer 2s ..."<<endl;
            });

    ret = timer_manager::Instance().add_timer(std::chrono::seconds(1), [](){
            cout<<"timer 1s ..."<<endl;
            });

    for(int i=0; i< 10;i++){
        auto ret = timer_manager::Instance().add_timer(std::chrono::seconds(i+1), [i](){
                cout<<"timer :"<<i<<" timeout"<<endl;
                });
        if (i==5)
            timer_manager::Instance().del_timer(ret);
    }
    for(int i=0; i<2; i++)
        std::this_thread::sleep_for(std::chrono::seconds(10));
    cout<<"main end"<<endl;

    return 0;
}

