#include <iostream>
#include <thread>
#include <memory>
#include "timer/timer-manager.hh"

using namespace std;
int main(int argc, char *argv[])
{
    timer_manager& tmanager = timer_manager::Instance();
    timer<steady_clock_type>  t1([](){cout<<"t1 ------ timeout!"<<endl;});
    t1.arm(std::chrono::microseconds(1000));
    //tmanager.add_timer(&t1);

    timer_manager::Instance().expired_timer(std::chrono::microseconds(3000), [](){cout<<"expired --- timeout!"<<endl;});

    timer<steady_clock_type>  t3([](){cout<<"t3 ------ timeout!"<<endl;});
    t3.arm_periodic(std::chrono::microseconds(1000));

//    tmanager._signals.poll_signal();

    for(int i=0; i< 10000000;){
        i++;
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::this_thread::sleep_for(std::chrono::seconds(2));

    cout<<"main end"<<endl;

    return 0;
}
