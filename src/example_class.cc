#include <iostream>
#include <thread>
#include <chrono>
#include "timer/timer-manager.hh"

class demo{
    mtimer_t _testTimer;
    void test_fun() {std::cout<<"test func..."<<std::endl;}
public:
    demo():_testTimer(std::bind(&demo::test_fun, this)) {_testTimer.arm(std::chrono::milliseconds(1000));}
};

int main(int argc, char *argv[])
{
    demo d;

    for(int i=0; i< 3; i++)
        std::this_thread::sleep_for(std::chrono::seconds(1));
    return 0;
}
