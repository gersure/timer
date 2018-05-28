#include <iostream>
#include <thread>
#include <memory>
#include "timer/timer-manager.hh"

using namespace std;
int main(int argc, char *argv[])
{

    timer_manager::Instance().set_thread_pool(make_threadpool());
    {
    timer<steady_clock_type>  t1([](){cout<<"t1 ------ timeout!"<<endl;});
    t1.rearm(std::chrono::microseconds(1000));
    }

    {
    }

    timer<steady_clock_type>  t3([](){cout<<"t3 ------ timeout!"<<endl;});
    t3.arm(std::chrono::microseconds(1000));

    for(int i=0; i< 3;i++){
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    cout<<"main end"<<endl;

    return 0;
}
