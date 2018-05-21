#include <iostream>
#include <thread>

#include "timer/timer-manager.hh"

using namespace std;
int main(int argc, char *argv[])
{
    timer_manager& tmanager = timer_manager::Instance();
    timer<steady_clock_type>  t1([](){cout<<"t1 ------ timeout!"<<endl;});
    t1.arm(std::chrono::microseconds(1000));
    //tmanager.add_timer(&t1);

    timer<steady_clock_type>  t2([](){cout<<"t2 ------ timeout!"<<endl;});
    t2.arm(std::chrono::microseconds(2000));

    timer<steady_clock_type>  t3([](){cout<<"t3 ------ timeout!"<<endl;});
    t3.arm(std::chrono::microseconds(1000));

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
