#pragma once

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <stdint.h>

class GPO
{
public:
    GPO(const struct gpio_dt_spec dtSpc);
    bool isReady(); 
    void toggle();
    void setOn();
    void setOff();
    void set(bool isOn);       
    virtual ~GPO() {};
private:
    const struct gpio_dt_spec spec;
};
