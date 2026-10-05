#pragma once
#include <stdint.h>
#include <atomic>
class UniTime
{
public:    
    UniTime();
    UniTime(unsigned long int currentTime);
    void inline  refreshTime (int64_t currentTime);
    int64_t inline GetTime() const; //{return mSec;}
    virtual ~UniTime();
private:
    bool start();
    static void router(void *p1, void *p2, void *p3);
    static std::atomic<int64_t>  mSec;
    struct k_thread time_thread_data;
    K_KERNEL_STACK_MEMBER(time_thread_stack, 2048);
};