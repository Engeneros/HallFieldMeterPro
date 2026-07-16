#include "gpo.hpp"

GPO::GPO(const struct gpio_dt_spec dtSpc) : spec(dtSpc)
{
    if (isReady())
       gpio_pin_configure_dt(&spec, GPIO_OUTPUT_ACTIVE);
}

void GPO::toggle()
{
    gpio_pin_toggle_dt(&spec);
}

void GPO::setOn()
{
    gpio_pin_set_dt(&spec, 1);
}

void GPO::setOff()
{
    gpio_pin_set_dt(&spec, 0);
}

void GPO::set(bool isOn)
{
    gpio_pin_set_dt(&spec, isOn? 1:0);
}

bool GPO::isReady()
{
    return gpio_is_ready_dt(&spec);
}

