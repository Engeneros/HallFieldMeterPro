// src/adc/adc_sequencer.cpp
#include "adc_sequencer.hpp"
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include "board_devs.hpp"
static struct k_mutex adc_rd;



double AdcSequencer::getVRef()
{
    return vReference;
} //in uV
void  AdcSequencer::setVRef(double uV)
{
    if(eeMem->write(BRD_ADDR_VREF, uV) == sizeof(double)) 
    {
        vReference = uV; 
    }
    calibrateStart();
}

AdcSequencer::AdcSequencer(const struct gpio_dt_spec* dR) : adDataRdy(dR),
isCalibrateDone(true), isRunning (false)
{
  //  gpio_pin_configure_dt(&ctrl->adDtRdy, GPIO_INPUT);
  //  gpio_pin_interrupt_configure_dt(&ctrl->adDtRdy, GPIO_INT_EDGE_TO_ACTIVE);
    isRunning = false; // Обязательно инициализируем!
    k_mutex_init(&adc_rd); 
    for (int chan = 0; chan < ACQ_CHAN_NUM; ++chan)
    {
        chanSet[chan] = nullptr;
        acqData[chan] = 123.456f;
    }
    eeMem  = getBrdEEprom();
    double temp;
    int res = eeMem->read(BRD_ADDR_VREF, &temp);
    if(res == sizeof(double) && (temp > MIN_REF_V) && (temp < MAX_REF_V))
        vReference = temp;
    else        
        vReference = 400000.0;
}
bool AdcSequencer::isClbDone()
{
    return isCalibrateDone;
}

void AdcSequencer::addChan(AbstractADC* chan, unsigned char chNum, uint32_t scale, int32_t sensSh, double sensX)
{
    if (chNum < ACQ_CHAN_NUM)
    {
        chanSet[chNum] = chan;
        chScaleFactor[chNum] = scale; 
        shift_inQ[chNum] = 0;
        uVperQuant[chNum] = 0.1;//
        sensorShift[chNum] = sensSh;
        sensorScale[chNum] = sensX;
    }
}

void AdcSequencer::start() 
{
    if (isRunning) return;

    // 1. Инициализируем семафор и коллбэк ОДИН раз для всего секвенсора
    k_sem_init(&drdySem, 0, 1);
    
    // Берем пин прерывания (его можно передать в секвенсор или взять из board_devices)
    //const struct gpio_dt_spec* drdyPin = &chanSet[0]->getDrdySpec(); // или любой другой способ получить спецификацию пина PB15
    gpio_pin_configure_dt(adDataRdy, GPIO_INPUT);
    gpio_init_callback(&drdyCbData, AdcSequencer::drdyGpioCallbackThunk, BIT(adDataRdy->pin));
    gpio_add_callback(adDataRdy->port, &drdyCbData);
    gpio_pin_interrupt_configure_dt(adDataRdy, GPIO_INT_EDGE_TO_ACTIVE);
 
    // 2. Запускаем поток
    // k_thread_create(&threadData,
    //             threadStack,
    //             K_KERNEL_STACK_SIZEOF(threadStack),
    //             AdcSequencer::threadEntryThunk, 
    //             this,                           
    //             nullptr, nullptr,               
    //             K_PRIO_COOP(5),                 
    //             0,
    //             K_NO_WAIT);
    //clbRequest = true;
    k_thread_create(&threadData,
                threadStack,
                K_KERNEL_STACK_SIZEOF(threadStack),
                AdcSequencer::threadEntryThunk, 
                this, nullptr, nullptr,               
                K_PRIO_PREEMPT(5), // <-- ИСПРАВЛЕНО: Теперь поток вытесняемый!
                0,
                K_NO_WAIT);



    isRunning = true;
}

// Исправлено: Оставляем extern "C" на уровне реализации
extern "C" void AdcSequencer::drdyGpioCallbackThunk(const struct device *port, 
                                                    struct gpio_callback *cb, 
                                                    uint32_t pins) 
{
    // Извлекаем наш C++ объект секвенсора из структуры коллбэка
    AdcSequencer* instance = CONTAINER_OF(cb, AdcSequencer, drdyCbData);
    
    // Безопасно отдаем семафор в контексте прерывания (ISR)
    k_sem_give(&instance->drdySem);
} 

void AdcSequencer::threadEntryThunk(void *p1, void *p2, void *p3)
{
    AdcSequencer* instance = static_cast<AdcSequencer*>(p1);  
    instance->threadLoop();
}

void AdcSequencer::calibrateStart()
{
    isCalibrateDone = false;
    clbRequest = true;
}
void AdcSequencer::acq()
{
//    double tempVal;
    int32_t summ;
    int err;
    int count;
    AbstractADC* currChan;
    static int cnt = 0;
    for (int ch = HALL_SENS_L; ch < CLB_NULL; ++ch)
    {
        currChan = chanSet[ch];
        if (currChan != nullptr)
        {
            currChan->start();   
            for(int i = 0; i < TRASH_SAMPLE_NUM; ++i)
            {
                // 2. Секвенсор засыпает на СВОЕМ семафоре и ждет физический спад от PB15
                err = k_sem_take(&drdySem, K_MSEC(200));
                // ПЕРЕД чтением по SPI временно удаляем наш коллбэк из обработки
                gpio_remove_callback(adDataRdy->port, &drdyCbData);
                if (err == 0)              // 3. Сигнал DRDY прилетел! Говорим каналу вычитать данные по SPI
                    summ = currChan->getQuants();
                k_sem_reset(&drdySem);    
                gpio_add_callback(adDataRdy->port, &drdyCbData);
            }
            count = 0;
            summ = 0;
            for(int i = 0; i < SAMPLES_PER_ACQ; ++i)
            {
                // 2. Секвенсор засыпает на СВОЕМ семафоре и ждет физический спад от PB15
                err = k_sem_take(&drdySem, K_MSEC(200));
                // ПЕРЕД чтением по SPI временно удаляем наш коллбэк из обработки
                gpio_remove_callback(adDataRdy->port, &drdyCbData);
                if (err == 0)
                {              // 3. Сигнал DRDY прилетел! Говорим каналу вычитать данные по SPI
                    summ += currChan->getQuants();// - shift_inQ[ch];
                    ++count;
                }
                cnvNum[ch] = count;
                cnvSumm[ch] = summ;
                k_sem_reset(&drdySem);    
                gpio_add_callback(adDataRdy->port, &drdyCbData);
            }
            // k_mutex_lock(&adc_rd, K_FOREVER); 
            // if(count > 0)
            // {
            //    tempVal = uVperQuant[ch]*(static_cast<double>(summ)/static_cast<double>(count));
            //    tempVal -= static_cast<double>(sensorShift[ch]);
            //     tempVal *= sensorScale[ch];
            //     acqData[ch] = static_cast<int32_t>(tempVal);
            // }
            // // else 
            // //     printk("OOPS!!: no valid samples\n"); 
            // k_mutex_unlock(&adc_rd);                
        }
        k_msleep(5);
    }
}

void AdcSequencer::getClbData(double* uVPerQ)
{
    for(int ch = HALL_SENS_L; ch < CLB_NULL; ++ch)
        uVPerQ[ch] = uVperQuant[ch];
}

void AdcSequencer::clbProc()
{
    printk("calibration:");
    double tempVal;
    int32_t summ;
    int err;
    int count;
    for (int ch = CLB_NULL; ch < ACQ_CHAN_NUM; ++ch)
    {
        AbstractADC* currChan = chanSet[ch];
        if (currChan != nullptr)
        {
            currChan->start();   
            for(int i = 0; i < TRASH_SAMPLE_NUM; ++i)
            {
                // 2. Секвенсор засыпает на СВОЕМ семафоре и ждет физический спад от PB15
                err = k_sem_take(&drdySem, K_MSEC(200));
                // ПЕРЕД чтением по SPI временно удаляем наш коллбэк из обработки
                gpio_remove_callback(adDataRdy->port, &drdyCbData);
                if (err == 0)              // 3. Сигнал DRDY прилетел! Говорим каналу вычитать данные по SPI
                    summ = currChan->getQuants();
                k_sem_reset(&drdySem);    
                gpio_add_callback(adDataRdy->port, &drdyCbData);
            }
            count = 0;
            summ = 0;
            for(int i = 0; i < SAMPLES_PER_ACQ; ++i)
            {
                // 2. Секвенсор засыпает на СВОЕМ семафоре и ждет физический спад от PB15
                err = k_sem_take(&drdySem, K_MSEC(200));
                // ПЕРЕД чтением по SPI временно удаляем наш коллбэк из обработки
                gpio_remove_callback(adDataRdy->port, &drdyCbData);
                if (err == 0)
                {              // 3. Сигнал DRDY прилетел! Говорим каналу вычитать данные по SPI
                    summ += currChan->getQuants();
                    ++count;
                }
                k_sem_reset(&drdySem);    
                gpio_add_callback(adDataRdy->port, &drdyCbData);
            }
            if(count > 0)
            {
                acqData[ch] = summ/count;
//                printk("ch%d=%d - %d; c=%d", ch, acqData[ch], tempVal, count);
            }
            else 
                printk("OOPS!!: no valid samples, ch %\n", ch); 
        }
    }
    for (int ch = HALL_SENS_L; ch < CLB_NULL; ++ch)
    {
        shift_inQ[ch] = acqData[CLB_NULL]/chScaleFactor[ch];//
        //vReference = 500000uV - voltage from mux calibration chanel
        uVperQuant[ch] =  vReference * 2.0 * static_cast<double>(chScaleFactor[ch])/ static_cast<double>(acqData[CLB_PLUS] - acqData[CLB_MINUS]);
    }
    printk(" zero shift is %d\n", acqData[CLB_NULL]);
    printk("%d quants per 1V\n", acqData[CLB_PLUS] - acqData[CLB_MINUS]);
    isCalibrateDone = true;
}

void AdcSequencer::threadLoop()
{
    static const unsigned int SAMPLES_PER_CYCLE = (TRASH_SAMPLE_NUM + SAMPLES_PER_ACQ)*CLB_NULL;
    clbRequest = false;
    clbProc();
    while (true)
    {          
        for (acqCycCnt = (autoClbTsec * 100)/SAMPLES_PER_CYCLE; acqCycCnt > 0; --acqCycCnt) 
        {
            acq();
            if (clbRequest)
            {
                clbRequest = false;
                clbProc();
                break;
            }            
        }               
        if(autoClbEn && !clbRequest)            
            clbProc();
    }
 }

void AdcSequencer::getData  (int32_t* dataOut, unsigned char startCh, unsigned char endCh)
{
    double tempVal; 
       
    for (int ch = startCh; ch <= endCh; ++ch)
    {
        if(cnvNum[ch] > 0)
        {
            k_mutex_lock(&adc_rd, K_FOREVER);
            tempVal = uVperQuant[ch]*(static_cast<double>(cnvSumm[ch])/static_cast<double>(cnvNum[ch]));
            k_mutex_unlock(&adc_rd); 
            tempVal -= static_cast<double>(sensorShift[ch]);
            tempVal *= sensorScale[ch];
            k_mutex_lock(&adc_rd, K_FOREVER);
            acqData[ch] = static_cast<int32_t>(tempVal);
            k_mutex_unlock(&adc_rd);
        }
        dataOut[ch] = acqData[ch];
    }
}

void AdcSequencer::getData  (int32_t* dataOut) 
{
   getData(dataOut, 0, ACQ_CHAN_NUM - 1); 
}



void AdcSequencer::autoClbEnable()
{
    autoClbEn = true;
}

void AdcSequencer::autoClbDisable()
{
    autoClbEn = false;
}

void AdcSequencer::autoClbSetT(unsigned int sec)
{
    autoClbTsec = sec;
}

unsigned int AdcSequencer::autoClbGetT()
{
    return autoClbTsec;
}