#pragma once
#include <stdint.h>
enum MB_HOLDING_REG_MAP
{
    HR_TIME_UTS_MSW1,
    HR_TIME_UTS_MSW0,
    HR_TIME_UTS_LSW1,
    HR_TIME_UTS_LSW0,
    HR_CONTROL,
    HR_AUTO_CLB_TIME,
    MODBUS_H_REGS_COUNT
};

enum MB_INPUT_REG_MAP
{
    IR_HALL_L_MSW,
    IR_HALL_L_LSW,
    IR_HALL_R_MSW,
    IR_HALL_R_LSW,
    IR_CURRENT_R_MSW,
    IR_CURRENT_R_LSW,
    IR_TEMPERATURE_MSW,
    IR_TEMPERATURE_LSW,
    IR_NULL,
    IR_CLB_PLUS_MSW,
    IR_CLB_MINUS_MSW,
    IR_V_REF_NV,
    IR_TIME_UTS_MSW1,
    IR_TIME_UTS_MSW0,
    IR_TIME_UTS_LSW1,
    IR_TIME_UTS_LSW0,
    IR_DEV_NUM,
    IR_HW_VERSION,
    IR_FW_VERSION,
    IR_IFC_VERSION,
    MODBUS_IN_REGS_COUNT
};

struct ctlHRbits
{
    uint16_t clbAutoEn : 1;
    uint16_t startClb : 1;
    uint16_t reservCtl : 14;
};

union ctlHR
{
    uint16_t word;
    ctlHRbits bits;
};

void modbus_start();
void refresh_input_regs(int32_t* data, uint16_t start_addr, uint16_t stop_addr);
void copy_input_regs(int32_t* data, uint16_t start_addr, uint16_t stop_addr);
void setControlHR(uint16_t val);
uint32_t getControlHR();
void clbTimeWr(uint16_t val);
uint32_t clbTimeRd();


//void mb_thread_entry(void *arg1, void *arg2, void *arg3);


//#include <zephyr/kernel.h>
//#include <zephyr/device.h>
//#include <zephyr/drivers/spi.h>
//#include <zephyr/drivers/gpio.h>
//#include <zephyr/sys/printk.h>
//#include <zephyr/console/console.h>

//#include <zephyr/net/net_if.h>
//#include <zephyr/net/socket.h>  /* Добавляем стандартные POSIX сокеты */
//#include <zephyr/modbus/modbus.h>
//#include <zephyr/logging/log.h>




// K_THREAD_STACK_DEFINE(mb_thread_stack, MB_THREAD_STACK_SIZE);
// static struct k_thread mb_thread_data;
//#include "leds.h"
//#include "board_devs.hpp"
//#include "gpo.hpp"
//#include "ads1234.hpp"
//#include "adc_sequencer.hpp"

//#define MB_THREAD_STACK_SIZE 2048
//#define MB_THREAD_PRIORITY 1

//static struct k_mutex adc_data_mutex;

//LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

//#define MODBUS_H_REGS_COUNT 10
//#define MODBUS_IN_REGS_COUNT 16
///static uint16_t holding_regs[MODBUS_H_REGS_COUNT] = {0, 11, 22, 33, 44, 55, 66, 77, 88, 99};
//static uint16_t input_regs[MODBUS_IN_REGS_COUNT] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

// #define MODBUS_TCP_MAX_ADU_SIZE 260
// static uint8_t mrx_buf[MODBUS_TCP_MAX_ADU_SIZE];
// static uint8_t mtx_buf[MODBUS_TCP_MAX_ADU_SIZE];
// /* Глобальная переменная для хранения дескриптора текущего активного TCP-клиента */
// static int active_client_fd = -1;
/* 1. Коллбеки чтения/записи регистров */
// static int holding_reg_rd_cb(uint16_t addr, uint16_t *reg)
// {
//     if (addr >= MODBUS_H_REGS_COUNT) { return -EINVAL; }
//     *reg = holding_regs[addr];
//     LOG_INF("Modbus Read: Reg[%d] = %d", addr, *reg);
//     return 0;
// }

// static int holding_reg_wr_cb(uint16_t addr, uint16_t reg)
// {
//     if (addr >= MODBUS_H_REGS_COUNT) { return -EINVAL; }
//     holding_regs[addr] = reg;
//     LOG_INF("Modbus Write: Reg[%d] -> %d", addr, reg);
//     return 0;
// }

// static int input_reg_rd_cb(uint16_t addr, uint16_t *reg)
// {
//     if (addr >= MODBUS_IN_REGS_COUNT) { return -EINVAL; }
//     k_mutex_lock(&adc_data_mutex, K_FOREVER);
//     *reg = input_regs[addr];
//     k_mutex_unlock(&adc_data_mutex);      
//     LOG_INF("Modbus inpReg Read: Reg[%d] = %d", addr, *reg);
//     return 0;
// }

// static struct modbus_user_callbacks mbs_cbs = {
//     .input_reg_rd =   input_reg_rd_cb,
//     .holding_reg_rd = holding_reg_rd_cb,
//     .holding_reg_wr = holding_reg_wr_cb,
// };

// /* 2. КЛЮЧЕВОЙ КОЛЛБЕК: Сюда сервер Zephyr отдает готовый ответный пакет Modbus TCP */
// static int modbus_raw_tx_callback(int iface, const struct modbus_adu *adu, void *user_data)
// //static int modbus_raw_tx_callback(const int iface, const struct modbus_adu *adu)
// {
//     if (active_client_fd < 0) {
//         return -ENOTCONN;
//     }

//     /* Собираем заголовок MBAP ответа прямо в буфер отправки */
//     modbus_raw_put_header(adu, mtx_buf);
    
//     /* Дописываем Unit ID, Function Code и данные */
//     mtx_buf[6] = adu->unit_id;
//     mtx_buf[7] = adu->fc;
    
//     if (adu->length > 0) {
//         memcpy(&mtx_buf[8], adu->data, adu->length);
//     }

//     /* Итоговый размер: 7 байт MBAP/UnitID + 1 байт FC + длина данных */
//     size_t tx_len = 7 + 1 + adu->length;

//     /* Отправляем сформированный сервером Modbus ответ обратно в TCP-сокет */
//     int bytes_sent = send(active_client_fd, mtx_buf, tx_len, 0);
//     if (bytes_sent < 0) {
//         LOG_ERR("Failed to send Modbus TCP response (errno %d)", errno);
//         return -EIO;
//     }
//     return 0;
// }

