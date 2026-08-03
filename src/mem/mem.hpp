#pragma once
#include <zephyr/kernel.h>
#include <zephyr/drivers/eeprom.h>
#include <zephyr/device.h>
#include "abstract_mem.hpp"

class Mem : public AbstractMem
{
public:
    Mem(const struct device* dev) noexcept;
//return:
//if posintiv: number of readed bytes
//if negativ - error code
    int read(uint32_t addr, uint32_t nByte, uint8_t* data) noexcept;
    int read(uint32_t addr, double* data) noexcept;
//return:
//if posintiv: number of writed bytes
//if negativ - error code
    int write(uint32_t addr, uint32_t nByte, uint8_t* data) noexcept;
    int write(uint32_t addr, double* data) noexcept;
private:
    const struct device* memChip;    
};

