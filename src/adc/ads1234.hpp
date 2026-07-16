#pragma once
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include "abstract_adc.hpp"

struct ControlPins
{
    const struct gpio_dt_spec  adMx0;  
    const struct gpio_dt_spec  adMx1;
    const struct gpio_dt_spec  adGn0;
    const struct gpio_dt_spec  adGn1;
    const struct gpio_dt_spec  clbMx0;
    const struct gpio_dt_spec  clbMx1;
    const struct gpio_dt_spec  clbMxEn;
    const struct gpio_dt_spec  adSpeed;
    const struct gpio_dt_spec  adReset;
//    const struct gpio_dt_spec  adDtRdy;  
    const struct spi_dt_spec   adSPI;
};

class ADS1234 : public AbstractADC
{
public:  
    ADS1234(const struct ControlPins* pinsCtl, uint8_t adCh, uint8_t adG,
         uint8_t clbCh, uint8_t speed, float x);
    virtual ~ADS1234() {}
    void start();
    void stop();
//Defines the ratio between the quanta and the value that getVal() returns
//q*factor == getVal() 
    void setScale(double factor);
//Always returns the most recent measurement    
//    float getVal();
 //   int32_t getVal();
    int32_t getQuants();
//Always returns the most recent measurement 
//*dataNum contains the ordinal number of the measurement 
//*isNewData == TRUE if the data has not been read before, *isNewDAta == FALSE, otherwise  
//*msmTime contains measurment time
 //   virtual float getVal(int* dataNum, bool* isNewData, unsigned long int* msmTime ) = 0;    
    bool isReady() const;



private:
    bool ifcInit(); 
 
    uint32_t portMask;
    uint32_t chanCfg;
    double scale;
    const struct ControlPins* ctrl;
    // Структура для регистрации прерывания в ядре Zephyr. 
    // Она ОБЯЗАТЕЛЬНО должна лежать внутри класса physically (не по указателю)!
 //   struct gpio_callback drdyCbData;
 //   struct k_sem drdySem; // Скрытый семафор ядра Zephyr
 
    // Статическая Си-совместимая функция-переходник для EXTI
 //   static void drdyGpioCallbackThunk(const struct device *port, 
 //                                                struct gpio_callback *cb, 
 //                                                uint32_t pins);
    int32_t readRawSpiData();
    int32_t latestRawValue = 0;
 //   int32_t quantCorrector;                                                 
};

