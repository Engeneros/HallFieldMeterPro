#pragma once
class Time
{
public:    
    Time(unsigned long int currentTime);
    void refreshTime (unsigned long int currentTime);
    unsigned long int inline GetTime() const {return mSec;}
    virtual ~Time();
private:
    unsigned long int mSec;
    void router();
};