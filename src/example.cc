#include <iostream>
#include <thread>
#include <memory>
#include "timer/timer-manager.hh"

using namespace std;
int main(int argc, char *argv[])
{

    timer_manager& tm = timer_manager::Instance();
    tm.set_thread_pool(make_threadpool());
    auto ret = tm.add_timer(std::chrono::microseconds(10000),[](){cout<<"t1 ------ timeout!"<<endl;});

    std::this_thread::sleep_for(std::chrono::seconds(2));
    cout<<"main end"<<endl;

    return 0;
}
