#include <zephyr/sys/printk.h>
#include  "mem.hpp"

Mem::Mem(const struct device* dev) noexcept : memChip(dev) 
{
    if (!device_is_ready(memChip))
        printk("Ошибка: EEPROM устройство не готово!\n");
} 

union dble_and_bytes
{
    double val;
    uint8_t bytes[sizeof(double)];
};

//return:
//if posintiv: number of readed bytes
//if negativ - error code
int Mem::read(uint32_t addr, uint32_t nByte, uint8_t* data) noexcept
{
    int ret = eeprom_read(memChip, addr, data, nByte);
    if (ret < 0)
        printk("memory read err: %d\n", ret);
    else
        ret = nByte;
    return ret;
}

int Mem::read(uint32_t addr, double* data) noexcept
{    
    dble_and_bytes temp;
    int ret = read(addr, sizeof(double), temp.bytes);
    if (ret < 0)
        return ret;
    else 
        *data = temp.val;
    return ret;
}
//return:
//if posintiv: number of writed bytes
//if negativ - error code
int Mem::write(uint32_t addr, uint32_t nByte, uint8_t* data) noexcept
{
    int ret = eeprom_write(memChip, addr, data, nByte);
    if (ret < 0)
        printk("mem Write ERR: %d\n", ret);
    else 
        ret = nByte;
    return ret;
}

int Mem::write(uint32_t addr, double data) noexcept
{
    dble_and_bytes temp;
    temp.val = data;
    int ret = write(addr, sizeof(double), temp.bytes);
    if (ret < 0)
        printk("mem Write ERR: %d\n", ret);
    else 
        ret = sizeof(double);
    return ret;
}

