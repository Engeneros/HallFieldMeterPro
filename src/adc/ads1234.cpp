#include "ads1234.hpp"
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <zephyr/kernel.h>


bool ADS1234::ifcInit()
{
    gpio_pin_configure_dt(&ctrl->adGn0, GPIO_OUTPUT_ACTIVE);
    gpio_pin_configure_dt(&ctrl->adGn1, GPIO_OUTPUT_ACTIVE);
    gpio_pin_configure_dt(&ctrl->adMx0, GPIO_OUTPUT_ACTIVE); 
    gpio_pin_configure_dt(&ctrl->adMx1, GPIO_OUTPUT_ACTIVE);   
    gpio_pin_configure_dt(&ctrl->adSpeed, GPIO_OUTPUT_ACTIVE);
    gpio_pin_configure_dt(&ctrl->adReset, GPIO_OUTPUT_ACTIVE);
    gpio_pin_configure_dt(&ctrl->clbMx0, GPIO_OUTPUT_ACTIVE);
    gpio_pin_configure_dt(&ctrl->clbMx1, GPIO_OUTPUT_ACTIVE);
    gpio_pin_configure_dt(&ctrl->clbMxEn, GPIO_OUTPUT_ACTIVE);
    gpio_pin_set_dt(&ctrl->adReset,  0); 
    // 2. Проверяем, готов ли порт B к работе
    // if (!device_is_ready(drdy_spec.port)) {
    //     printk("Error: GPIO port B is not ready!\n");
    //     return;
    // }
    // 3. Конфигурируем пин строго как ВХОД (GPIO_INPUT)
    // ОС Zephyr автоматически настроит внутренние подтягивающие резисторы 
    // в соответствии со схемотехникой STM32
    // Конструкция k_sem_init(указатель, начальное_значение, максимальное_значение)
//    k_sem_init(&drdySem, 0, 1); 
//    printk("----kSemInit-----\n");
//    gpio_pin_configure_dt(&ctrl->adDtRdy, GPIO_INPUT);
    // if (err != 0) {
    //     printk("Error configuring DRDY pin: %d\n", err);
    // }

    // Включаем аппаратное EXTI прерывание по ОТРИЦАТЕЛЬНОМУ фронту (спад в LOW)
    gpio_pin_set_dt(&ctrl->adReset,  1); 
//    printk("ADS1234 Reset = 1\n");
//    gpio_pin_interrupt_configure_dt(&ctrl->adDtRdy, GPIO_INT_EDGE_TO_ACTIVE);
//    printk("EXTI DataReady interrupt work\n");
//    int32_t tmp =  readRawSpiData();
//    (void) tmp;
//    k_sem_reset(&drdySem);
    return true;
}

// extern "C" void ADS1234::drdyGpioCallbackThunk(const struct device *port, 
//                                                 struct gpio_callback *cb, 
//                                                 uint32_t pins)
// {
//     /*
//       Используем CONTAINER_OF: cb указывает на поле drdyCbData.
//       Компилятор вычитает смещение и находит адрес начала нашего класса ADS1234,
//       возвращая нам полноценный указатель instance (аналог this).
//     */
//     ADS1234* instance = CONTAINER_OF(cb, ADS1234, drdyCbData);

//     // Прыгаем в нормальный C++ метод объекта
//     instance->handleDataReady();
// }

// extern "C" void ADS1234::drdyGpioCallbackThunk(const struct device *port, 
//                                                 struct gpio_callback *cb, 
//                                                 uint32_t pins)
// {
//     ADS1234* instance = CONTAINER_OF(cb, ADS1234, drdyCbData);
    
//     // Отдаем семафор. Эта функция в Zephyr атомарна и разрешена внутри ISR
//     k_sem_give(&instance->drdySem);
// }

#include <zephyr/drivers/spi.h>

int32_t ADS1234::readRawSpiData()
{
    uint8_t rxBuffer[3] = {0}; // Буфер под 3 байта (24 бита) данных АЦП
    static int cnt = 0;
    // Настраиваем структуру буфера для Zephyr SPI API
    struct spi_buf rxBuf = {
        .buf = rxBuffer,
        .len = sizeof(rxBuffer)
    };
    struct spi_buf_set rxBufSet = {
        .buffers = &rxBuf,
        .count = 1
    };

    // Проверяем готовность SPI устройства (спецификация лежит в ctrl)
    // if (!device_is_ready(ctrl->spiSpec.port)) {
    //     printk("Error: SPI device not ready!\n");
    //     return 0;
    // }

    // Физическое вычитывание 3 байт по шине SPI
    // Zephyr автоматически прижмет пин CS, выдаст 24 тактовых импульса и отпустит CS
    int err = spi_read_dt(&ctrl->adSPI, &rxBufSet);
    if (err < 0) {
        printk("SPI read error: %d\n", err);
        return 0;
    }
    int32_t rawValue = 0;
     for (int i = 0; i < 2; ++i)
     {
        rawValue |= rxBuffer[i];
        rawValue <<= 8;
     }
    rawValue |= rxBuffer[2];
    rawValue |= ((rawValue & 0x00800000) == 0)? 0 :  0xFF000000; 
    return rawValue ;//+ (++cnt << 16) ;
}


ADS1234::ADS1234(const struct ControlPins* pinsCtl,
         uint8_t adCh, uint8_t adG, uint8_t clbCh, uint8_t speed,
        float x): ctrl(pinsCtl)
{
    static bool isIfcIni = ifcInit();
    (void) isIfcIni;
     
 
    portMask = BIT(ctrl->adMx0.pin)  | BIT(ctrl->adMx1.pin) |
               BIT(ctrl->adGn0.pin)  | BIT(ctrl->adGn1.pin) |
               BIT(ctrl->clbMx0.pin) | BIT(ctrl->clbMx1.pin) |
               BIT(ctrl->clbMxEn.pin)  | BIT(ctrl->adSpeed.pin) |
               BIT(ctrl->adReset.pin); 
    chanCfg = 0;
    chanCfg |= (adCh & 1)?  BIT(ctrl->adMx0.pin) : 0;
    chanCfg |= (adCh & 2)?  BIT(ctrl->adMx1.pin) : 0;
    chanCfg |= (adG & 1)?   BIT(ctrl->adGn0.pin) : 0;
    chanCfg |= (adG & 2)?   BIT(ctrl->adGn1.pin) : 0;
    chanCfg |= (clbCh & 1)? BIT(ctrl->clbMx0.pin) : 0;
    chanCfg |= (clbCh & 2)? BIT(ctrl->clbMx1.pin) : 0;
    chanCfg |= (clbCh & 4)? BIT(ctrl->clbMxEn.pin) : 0;
    chanCfg |= (speed & 1)? BIT(ctrl->adSpeed.pin) : 0;
    chanCfg |= BIT(ctrl->adReset.pin);  
    setScale(x);
}

void ADS1234::start()
{
     gpio_port_set_masked_raw(ctrl->adMx0.port, portMask, chanCfg); 
}

void ADS1234::stop()
{

}



void ADS1234::setScale(double factor)
{
    double  bits = (double)0x7fffff;
    scale = 1000000 * 1.25/(factor * bits);// uV per bit
}    

// int32_t  ADS1234::getVal()
// {
//     latestRawValue = readRawSpiData();
//     double temp = scale * static_cast<double>(latestRawValue);
//     return static_cast<int32_t>(temp);
// }
///--
int32_t ADS1234::getQuants()
{
    return readRawSpiData();
}

bool ADS1234::isReady() const
{
    return true;
}