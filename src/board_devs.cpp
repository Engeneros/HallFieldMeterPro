#include "board_devs.hpp"
#include "gpo.hpp"

// Спецификация пина скрыта внутри .cpp файла
static const struct gpio_dt_spec red_spec = GPIO_DT_SPEC_GET(DT_NODELABEL(led_red), gpios);
static const struct gpio_dt_spec yellow_spec = GPIO_DT_SPEC_GET(DT_NODELABEL(led_blue), gpios);
static const struct gpio_dt_spec green_spec = GPIO_DT_SPEC_GET(DT_NODELABEL(led_green), gpios);

GPO* getRedLED()
{
    // Тот самый Meyers Singleton. Создается один раз при первом вызове.
    static GPO led(red_spec); 
    return &led;
}

GPO* getYellowLED()
{
    // Тот самый Meyers Singleton. Создается один раз при первом вызове.
    static GPO led(yellow_spec); 
    return &led;
}

GPO* getGreenLED()
{
    // Тот самый Meyers Singleton. Создается один раз при первом вызове.
    static GPO led(green_spec); 
    return &led;
}

// Реализация Си-интерфейса
extern "C" void red_led_toggle(void)
{
    GPO* led = getRedLED();
    led->toggle();
}

extern "C" void yellow_led_toggle(void)
{
    GPO* led = getYellowLED();
    led->toggle();
}

// Реализация Си-интерфейса
extern "C" void green_led_toggle(void)
{
    GPO* led = getGreenLED();
    led->toggle();
}

#include "ads1234.hpp"
#include "adc_sequencer.hpp"


static const struct gpio_dt_spec ad_mx0_spec = GPIO_DT_SPEC_GET(DT_NODELABEL(ad_mx_0), gpios);
static const struct gpio_dt_spec ad_mx1_spec = GPIO_DT_SPEC_GET(DT_NODELABEL(ad_mx_1), gpios);
static const struct gpio_dt_spec ad_g0_spec = GPIO_DT_SPEC_GET(DT_NODELABEL(ad_g_0), gpios);
static const struct gpio_dt_spec ad_g1_spec = GPIO_DT_SPEC_GET(DT_NODELABEL(ad_g_1), gpios);
static const struct gpio_dt_spec cb_mx0_spec = GPIO_DT_SPEC_GET(DT_NODELABEL(cb_mx_0), gpios);
static const struct gpio_dt_spec cb_mx1_spec = GPIO_DT_SPEC_GET(DT_NODELABEL(cb_mx_1), gpios);
static const struct gpio_dt_spec cb_en_spec = GPIO_DT_SPEC_GET(DT_NODELABEL(cb_mx_en), gpios);
static const struct gpio_dt_spec ad_speed_spec = GPIO_DT_SPEC_GET(DT_NODELABEL(ad_speed), gpios);
static const struct gpio_dt_spec ad_rst_n_spec = GPIO_DT_SPEC_GET(DT_NODELABEL(ad_rst_n), gpios);
// 1. Извлекаем спецификацию пина из Devicetree по его ноде ad_drdy
static const struct gpio_dt_spec drdy_spec = GPIO_DT_SPEC_GET(DT_NODELABEL(ad_drdy), gpios);

static const struct spi_dt_spec ad_spi_spec = SPI_DT_SPEC_GET(
    DT_NODELABEL(ads1234_spi), 
    SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_MODE_CPHA, // Режим работы
    0
);

static const ControlPins adPinsCtl = {
    .adMx0   = ad_mx0_spec,
    .adMx1   = ad_mx1_spec,
    .adGn0   = ad_g0_spec,
    .adGn1   = ad_g1_spec,
    .clbMx0  = cb_mx0_spec,
    .clbMx1  = cb_mx1_spec,
    .clbMxEn = cb_en_spec, 
    .adSpeed = ad_speed_spec,
    .adReset = ad_rst_n_spec,
 //   .adDtRdy = drdy_spec,
    .adSPI = ad_spi_spec
};
// value for ad  speed pin (SPEED)
enum AD_SPEED
{
    AD_SLOW,
    AD_FAST
};
// value for ad  PGA GAIN pins (GAIN0, GAIN1)
enum AD_PGA
{
    PGA_X1,
    PGA_X2,
    PGA_X64,
    PGA_128
};
// value for ad mux pins (A0, A1)
enum AD_CHAN
{
    AD_CH_HALL_LEFT,
    AD_CH_HALL_RIGHT,
    AD_CH_HALL_CURRENT,
    AD_CH_EXT_MUX
};
//// value for MUX adress & enable pins (A0, A1, EN)
enum EXT_MUX_CAHNS
{
//enable pin = 0
    EXT_MUX_OFF = 0,
//Enable pin = 1    
    EXT_MUX_TEMP = 4,
    EXT_MUX_ZERO = 5,
    EXT_MUX_MINUS = 6,
    EXT_MUX_PLUS = 7
};

AbstractADC* getFieldMeterL()
{
    static ADS1234 ad = ADS1234(&adPinsCtl, AD_CH_HALL_LEFT, PGA_X2, EXT_MUX_TEMP, AD_FAST, 2.0 );
    return (AbstractADC*) &ad;
}

AbstractADC* getFieldMeterR()
{
    static ADS1234 ad = ADS1234(&adPinsCtl, AD_CH_HALL_RIGHT, PGA_X2, EXT_MUX_TEMP, AD_FAST, 2.0 );
    return (AbstractADC*) &ad;
}

AbstractADC* getCurrentMeter()
{
    static ADS1234 ad = ADS1234(&adPinsCtl, AD_CH_HALL_CURRENT , PGA_X2, EXT_MUX_TEMP, AD_FAST, 2.0 );
    return (AbstractADC*) &ad;
}

AbstractADC* getTemperatureMeter()
{
    static ADS1234 ad = ADS1234(&adPinsCtl, AD_CH_EXT_MUX, PGA_X1, EXT_MUX_TEMP, AD_FAST, 1.0 );
    return (AbstractADC*) &ad;
}

AbstractADC* getZeroMeter()
{
    static ADS1234 ad = ADS1234(&adPinsCtl, AD_CH_EXT_MUX, PGA_X2, EXT_MUX_ZERO, AD_FAST, 2.0 );
    return (AbstractADC*) &ad;
}
AbstractADC* getRefPlusMeter()
{
    static ADS1234 ad = ADS1234(&adPinsCtl, AD_CH_EXT_MUX, PGA_X2, EXT_MUX_PLUS, AD_FAST, 2.0 );
    return (AbstractADC*) &ad;
}

AbstractADC* getRefMinusMeter()
{
    static ADS1234 ad = ADS1234(&adPinsCtl, AD_CH_EXT_MUX, PGA_X2, EXT_MUX_MINUS, AD_FAST, 2.0 );
    return (AbstractADC*) &ad;
}

AdcSequencer* getAdcSequencer()
{
    static bool isCreated = false;
    static AdcSequencer adcSqr = AdcSequencer(&drdy_spec);
    if(isCreated == false)
    {
        isCreated = true;
        printk("--------Sequencer Created----------\n");      
        adcSqr.addChan( getFieldMeterL(), HALL_SENS_L, 1, 0, 1.0);
        adcSqr.addChan( getFieldMeterR(), HALL_SENS_R, 1, 0, 1.0);
        adcSqr.addChan( getCurrentMeter(), HALL_CURRENT, 1, 0, 5.0);
        adcSqr.addChan( getTemperatureMeter(), TEMPERATURE, 2, 0, 1.0);
        adcSqr.addChan(getZeroMeter(),CLB_NULL, 1, 0, 1.0);
        adcSqr.addChan(getRefPlusMeter(), CLB_PLUS, 1, 0, 1.0);
        adcSqr.addChan(getRefMinusMeter(), CLB_MINUS, 1, 0, 1.0);
    }
    else
        printk("======Sequencer ready======= \n");
    return &adcSqr;
}
// AbstractADC* getFieldMeterR();
// AbstractADC* getCurrentMeter();
// AbstractADC* getThermoMeter();
#include "console.hpp"

Console* getConsole()
{
 static Console cli = Console();
    return &cli;
}
