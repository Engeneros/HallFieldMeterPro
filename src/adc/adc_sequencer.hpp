#pragma once
//src/adc/adc_sequencer.hpp
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include "abstract_adc.hpp"
#include "abstract_mem.hpp"


enum ACQ_CHAN
{
    HALL_SENS_L,
    HALL_SENS_R,
    HALL_CURRENT,
    TEMPERATURE,
    CLB_NULL,
    CLB_PLUS,
    CLB_MINUS,
    ACQ_CHAN_NUM
};


class AdcSequencer 
{
public:
    AdcSequencer(const struct gpio_dt_spec* dR);

//if ADC PDA factor = 2 ie scale = +-0.625V scale = 1; 
//if ADC PDA factor = 1 ie scale = +-1.25V scale = 2;
//The variable scale contains the coefficient by which
// the calibration adjustments for the channel must be multiplied.   
    void addChan(AbstractADC* chan, unsigned char chNum, uint32_t scale, int32_t sensSh, double sensX);
    void start();
    void getData(int32_t* dataOut);
    void getData(int32_t* dataOut, unsigned char startCh, unsigned char endCh);
    void getClbData(double* uVPerQ);
    double getVRef();
    void   setVRef(double uV);
    void trash();
 
    void calibrateStart();
    bool isClbDone();
    void autoClbEnable();
    void autoClbDisable();
    void autoClbSetT(unsigned int sec);
    unsigned int autoClbGetT();
private:
   void threadLoop();
   void acq();
   void clbProc();
 
   const struct gpio_dt_spec* adDataRdy;
   AbstractADC* chanSet[ACQ_CHAN_NUM];
   int32_t acqData[ACQ_CHAN_NUM];
  
   struct k_mutex adc_rd;
   struct k_thread threadData;
   K_KERNEL_STACK_MEMBER(threadStack, 2048); 
   bool isRunning;
 
   // Переносим структуры прерывания СЮДА. Они будут созданы в единственном экземпляре!
   struct gpio_callback drdyCbData;
   struct k_sem drdySem;
   // Статический Си-переходник для прерывания секвенсора
   static void drdyGpioCallbackThunk(const struct device *port, struct gpio_callback *cb, uint32_t pins);
   static void threadEntryThunk(void *p1, void *p2, void *p3);
   static const unsigned int  DEFAULT_AUTO_CLB_T = 20;
   static const unsigned int TRASH_SAMPLE_NUM = 4;
   static const unsigned int SAMPLES_PER_ACQ = 8;

    int acqCycCnt;
    volatile bool clbRequest {false};
    volatile bool autoClbEn {false};
    volatile unsigned int autoClbTsec {DEFAULT_AUTO_CLB_T};
    double uVperQuant[ACQ_CHAN_NUM];
    int32_t shift_inQ [ACQ_CHAN_NUM];
    int32_t chScaleFactor [ACQ_CHAN_NUM];
    int32_t sensorShift[ACQ_CHAN_NUM];
    double sensorScale[ACQ_CHAN_NUM];
    //vReference = 500000uV - voltage from mux calibration chanel
    double vReference;
    bool isCalibrateDone;
    AbstractMem* eeMem;
};

 

