#pragma once

enum BRD_EEPROM_MAP
{
    BRD_ADDR_VREF = 0,
    BRD_ADDR_DESCRIPTION = 8
}; 
static const double MIN_REF_V = 400000;//uV
static const double MAX_REF_V = 600000;//uV

#ifdef __cplusplus
// Этот класс виден только в C++ коде
    class GPO; 
    GPO* getRedLED();
    GPO* getYellowLED();
    GPO* getGreenLED();

    class AbstractADC;
    class AdcSequencer;
    class Console;
    AbstractADC* getFieldMeterL();
    AbstractADC* getFieldMeterR();
    AbstractADC* getCurrentMeter();
    AbstractADC* getTemperatureMeter();
    AdcSequencer* getAdcSequencer();
    Console* getConsole();
    // AbstractADC* getFieldMeterR();
    // AbstractADC* getCurrentMeter();
    // AbstractADC* getThermoMeter();
    AbstractADC* getZeroMeter();
    AbstractADC* getRefPlusMeter();
    AbstractADC* getRefMinusMeter();
    class AbstractMem;
    AbstractMem* getBrdEEprom();
#endif

#ifdef __cplusplus
extern "C"
{
#endif
    // Си-обертка, доступная везде
    void red_led_toggle(void);
    void yellow_led_toggle(void);
    void green_led_toggle(void);
#ifdef __cplusplus
}
#endif
