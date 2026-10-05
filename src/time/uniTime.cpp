#include <zephyr/kernel.h>
#include "uniTime.hpp"
#define TIME_THREAD_PRIORITY 10

bool UniTime::start()
{
    k_thread_create(&time_thread_data,
                time_thread_stack,
                K_KERNEL_STACK_SIZEOF(time_thread_stack),
                UniTime::router, 
                this, nullptr, nullptr,               
                K_PRIO_PREEMPT(TIME_THREAD_PRIORITY), // <-- ИСПРАВЛЕНО: Теперь поток вытесняемый!
                0,
                K_NO_WAIT);

    return true;
}

UniTime::UniTime(unsigned long int currentTime) 
{
    refreshTime(currentTime);
    static bool isRun = start();
    (void) isRun;
}

void inline UniTime::refreshTime (int64_t currentTime)
{
    mSec = currentTime;
}

int64_t inline UniTime::GetTime() const
{
    return mSec;
}

void UniTime::router(void *p1, void *p2, void *p3)
{
    while (true)
    {
        k_msleep(1);
        ++mSec;    
    }
}