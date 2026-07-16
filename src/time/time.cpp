#include "time.hpp"
#include <zephyr/kernel.h>

Time::Time(unsigned long int currentTime) 
{
    refreshTime(currentTime);
    router();
}

void Time::refreshTime (unsigned long int currentTime)
{
    mSec = currentTime;
}

void Time::router()
{
    k_msleep(1);
    ++mSec;    
}