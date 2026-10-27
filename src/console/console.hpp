#pragma once
#include "adc_sequencer.hpp"


class Console
{
public:    
    Console();
    void printMenu();
    void badCmd(); 
    void printVersion();
    void printQ();
    void printNews();
    void parser(unsigned int argc, char **argv);
    virtual ~Console(){}
private:
    
    void calibrate();
    void printClbData();
    void enAutoClb();
    void disAutoClb();
    void getAutoClbTime();
    void setAutoClbTime(unsigned int argc, char **argv);
    void getAdcData();
    void setVRef(unsigned int argc, char **argv);
    void getVRef();
    


    AdcSequencer* adcSys;
};