#include <zephyr/kernel.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
//#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/console/console.h>

#include <zephyr/net/net_if.h>
#include <zephyr/net/socket.h>  /* Добавляем стандартные POSIX сокеты */
#include <zephyr/modbus/modbus.h>
#include <zephyr/logging/log.h>
#include "gpo.hpp"
#include "board_devs.hpp"
#include "mbs.hpp"
#include "adc_sequencer.hpp"

//#include "mbs.hpp"
#define MB_THREAD_STACK_SIZE 2048
#define MB_THREAD_PRIORITY 4

static struct k_mutex adc_data_mutex;

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define MODBUS_H_REGS_COUNT 10
#define MODBUS_IN_REGS_COUNT 16
static uint16_t holding_regs[MODBUS_H_REGS_COUNT] = {0, 11, 22, 33, 44, 55, 66, 77, 88, 99};
static uint16_t input_regs[MODBUS_IN_REGS_COUNT] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

#define MODBUS_TCP_MAX_ADU_SIZE 260
static uint8_t mrx_buf[MODBUS_TCP_MAX_ADU_SIZE];
static uint8_t mtx_buf[MODBUS_TCP_MAX_ADU_SIZE];
/* Глобальная переменная для хранения дескриптора текущего активного TCP-клиента */
static int active_client_fd = -1;
/* 1. Коллбеки чтения/записи регистров */
static int holding_reg_rd_cb(uint16_t addr, uint16_t *reg)
{
    if (addr >= MODBUS_H_REGS_COUNT) { return -EINVAL; }
    *reg = holding_regs[addr];
    //LOG_INF("Modbus Read: Reg[%d] = %d", addr, *reg);
    return 0;
}

static int holding_reg_wr_cb(uint16_t addr, uint16_t reg)
{
    if (addr >= MODBUS_H_REGS_COUNT) { return -EINVAL; }
    holding_regs[addr] = reg;
//    LOG_INF("Modbus Write: Reg[%d] -> %d", addr, reg);
    return 0;
}

//addr - uint32 - data[0] = input_reg[0]+input_reg[1], data[n] = input_reg[2n]+input_reg[2n+1],
void refresh_input_regs(int32_t* data, uint16_t start_addr, uint16_t stop_addr)
{
    k_mutex_lock(&adc_data_mutex, K_FOREVER); 
    memcpy(&input_regs[start_addr * 2], &data[0], (1+ stop_addr - start_addr) * 4);
    k_mutex_unlock(&adc_data_mutex);  
}

void copy_input_regs(int32_t* data, uint16_t start_addr, uint16_t stop_addr)
{
    k_mutex_lock(&adc_data_mutex, K_FOREVER); 
    memcpy(&data[0], &input_regs[start_addr * 2], (1 + stop_addr - start_addr) * 4);
    k_mutex_unlock(&adc_data_mutex);
}


static int input_reg_rd_cb(uint16_t addr, uint16_t *reg)
{
    if (addr >= MODBUS_IN_REGS_COUNT) { return -EINVAL; }
    k_mutex_lock(&adc_data_mutex, K_FOREVER);
    *reg = input_regs[addr];
    k_mutex_unlock(&adc_data_mutex);      
//    LOG_INF("Modbus inpReg Read: Reg[%d] = %d", addr, *reg);
    return 0;
}


static struct modbus_user_callbacks mbs_cbs = {
    .input_reg_rd =   input_reg_rd_cb,
    .holding_reg_rd = holding_reg_rd_cb,
    .holding_reg_wr = holding_reg_wr_cb,
};

/* 2. КЛЮЧЕВОЙ КОЛЛБЕК: Сюда сервер Zephyr отдает готовый ответный пакет Modbus TCP */
static int modbus_raw_tx_callback(int iface, const struct modbus_adu *adu, void *user_data)
//static int modbus_raw_tx_callback(const int iface, const struct modbus_adu *adu)
{
    if (active_client_fd < 0) {
        return -ENOTCONN;
    }

    /* Собираем заголовок MBAP ответа прямо в буфер отправки */
    modbus_raw_put_header(adu, mtx_buf);
    
    /* Дописываем Unit ID, Function Code и данные */
    mtx_buf[6] = adu->unit_id;
    mtx_buf[7] = adu->fc;
    
    if (adu->length > 0) {
        memcpy(&mtx_buf[8], adu->data, adu->length);
    }

    /* Итоговый размер: 7 байт MBAP/UnitID + 1 байт FC + длина данных */
    size_t tx_len = 7 + 1 + adu->length;

    /* Отправляем сформированный сервером Modbus ответ обратно в TCP-сокет */
    int bytes_sent = send(active_client_fd, mtx_buf, tx_len, 0);
    if (bytes_sent < 0) {
        LOG_ERR("Failed to send Modbus TCP response (errno %d)", errno);
        return -EIO;
    }
    return 0;
}

void mb_thread_entry(void *arg1, void *arg2, void *arg3)
{
    LOG_INF("Starting Modbus TCP Application...");
    GPO* red = getRedLED();
    struct modbus_iface_param param = {
        .mode = MODBUS_MODE_RAW, 
        .server = { 
            .user_cb = &mbs_cbs, 
            .unit_id = 1 
        },
        /* Инициализируем объединение (union) через структуру rawcb */
        .rawcb = {
            .raw_tx_cb = modbus_raw_tx_callback /* Перехватчик ответа сервера */
        }
    };
    int ctx_num = modbus_init_server(0, param);
    if (ctx_num < 0) 
    {
        LOG_ERR("Failed to init Modbus backend (err %d)", ctx_num);
        return;
    }
    /* === СОЗДАЕМ TCP СЕРВЕР НА ПОРТУ 502 === */
    int server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd < 0) {
        LOG_ERR("Failed to create socket! (%d)", errno);
        return;
    }

    struct sockaddr_in bind_addr;// = {
    //     .sin_family = AF_INET,
    //     .sin_port = htons(502),
    //     .sin_addr.s_addr = INADDR_ANY,
    // };            
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(502);
    bind_addr.sin_addr.s_addr = INADDR_ANY; // Теперь это корректное C++ присвоение

    if (bind(server_fd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
        LOG_ERR("Bind failed! (%d)", errno);
        close(server_fd);
        return;
    }
    if (listen(server_fd, 1) < 0)
    {
        LOG_ERR("Listen failed! (%d)", errno);
        close(server_fd);
        return;
    }
    LOG_INF("Modbus TCP Server listen port 502...");
    while (1) 
    {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_fd < 0) {
            k_sleep(K_MSEC(10));
            continue;
        }
        //LOG_INF("QModMaster has connected to socket");     
        /* Сохраняем дескриптор сокета, чтобы коллбек отправки знал, куда слать данные */
        active_client_fd = client_fd;

        while (1) 
        {
            holding_regs[0]++; 
            int rc = recv(client_fd, mrx_buf, sizeof(mrx_buf), 0);
            if (rc <= 0)
            {
               // LOG_INF("Master has disconnected");
                break;               
            }
            if (rc < 8) 
                continue;
            holding_regs[1]++; 
            struct modbus_adu adu;            
            /* Разбираем входящий заголовок MBAP (первые 7 байт) */
            modbus_raw_get_header(&adu, mrx_buf);           
            /* Назначаем Unit ID (индекс 6) и Function Code (индекс 7) */
            adu.unit_id = mrx_buf[6]; 
            adu.fc = mrx_buf[7];
            red->toggle();           
            /* Длина PDU данных — всё, что идет после Function Code */
            adu.length = rc - 8; 
            
            /* Копируем чистые данные PDU во внутренний массив структуры adu */
            if (adu.length <= sizeof(adu.data)) 
            {
                if (adu.length > 0)
                {
                    memcpy(adu.data, &mrx_buf[8], adu.length);
                    holding_regs[2] += 2; 
                }
            } 
            else 
                continue;
            /* ВЫЗОВ ИСПРАВЛЕН: Передаем СТРОГО 2 аргумента согласно сигнатуре Zephyr 3.7.1 */
            /* Функция вернет 0, выполнит holding_reg_rd_cb и автоматически вызовет modbus_raw_tx_callback */
            int err = modbus_raw_submit_rx(0, &adu);
            if (err != 0) 
                LOG_ERR("Error submitting raw ADU: %d", err);
        }      
        close(client_fd);
        active_client_fd = -1;
    }   
    close(server_fd);
}

K_THREAD_STACK_DEFINE(mb_thread_stack, MB_THREAD_STACK_SIZE);
static struct k_thread mb_thread_data;

void modbus_start()
{
k_tid_t mb_tid = k_thread_create(&mb_thread_data,
                                        mb_thread_stack,
                                        K_THREAD_STACK_SIZEOF(mb_thread_stack),
                                        mb_thread_entry,
                                        NULL, NULL, NULL,
                                        MB_THREAD_PRIORITY,
                                        0,
                                        K_NO_WAIT); // Старт немедленно
    if (mb_tid)
        printk("ModB thread created.\n");
}    