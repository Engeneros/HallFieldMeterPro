// Имя файла: adc_interface.hpp
#pragma once
//#include <stdint.h>
class AbstractADC
{
public:  
    virtual ~AbstractADC() {}
    virtual void start() = 0;
    virtual void stop() = 0;
//Defines the ratio between the quanta and the value that getVal() returns
//q*factor == getVal() 
    virtual void setScale(double factor) = 0;
//Always returns the most recent measurement    
//    virtual float getVal() = 0;
 //   virtual int32_t getVal() = 0;
    virtual int32_t getQuants() = 0;
//Always returns the most recent measurement 
//*dataNum contains the ordinal number of the measurement 
//*isNewData == TRUE if the data has not been read before, *isNewDAta == FALSE, otherwise  
//*msmTime contains measurment time
 //   virtual float getVal(int* dataNum, bool* isNewData, unsigned long int* msmTime ) = 0;    
    virtual bool isReady() const = 0;
};


