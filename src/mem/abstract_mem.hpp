#pragma once
class AbstractMem
{
public:
//return:
//if posintiv: number of readed bytes
//if negativ - error code
    virtual int read(uint32_t addr, uint32_t nByte, uint8_t* data) noexcept = 0;
    virtual int read(uint32_t addr, double* data) noexcept = 0;
//return:
//if posintiv: number of writed bytes
//if negativ - error code
    virtual int write(uint32_t addr, uint32_t nByte, uint8_t* data) noexcept = 0;
    virtual int write(uint32_t addr, double* data) noexcept = 0;
};