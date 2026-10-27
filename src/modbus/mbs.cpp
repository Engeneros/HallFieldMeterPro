#include <zephyr/kernel.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
//#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/console/console.h>

#include <zephyr/net/net_if.h>
#include <zephyr/net/socket.h>  
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_ip.h>

#include <zephyr/modbus/modbus.h>
#include <zephyr/logging/log.h>
#include "gpo.hpp"
#include "board_devs.hpp"
#include "mbs.hpp"
#include "adc_sequencer.hpp"
#include "uni_time.hpp"

//#include "mbs.hpp"
#define MB_THREAD_STACK_SIZE 4096
#define MB_THREAD_PRIORITY 4

//static struct k_mutex adc_data_mutex;

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);
//#define MODBUS_H_REGS_COUNT 10
//#define MODBUS_IN_REGS_COUNT 16
static uint16_t holding_regs[MODBUS_H_REGS_COUNT] = {0, 76, 12, 45, 0};// 20};//, 88, 99};
static uint16_t input_regs[MODBUS_IN_REGS_COUNT] = {0};//, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};

#define MODBUS_TCP_MAX_ADU_SIZE 260
static uint8_t mrx_buf[MODBUS_TCP_MAX_ADU_SIZE];
static uint8_t mtx_buf[MODBUS_TCP_MAX_ADU_SIZE];
/* Глобальная переменная для хранения дескриптора текущего активного TCP-клиента */
static int active_client_fd = -1;

//struct for callBack registration
// static struct net_mgmt_event_callback net_cb;

// // A callback that will be triggered when the router assigns an IP address

// static void net_event_handler(struct net_mgmt_event_callback *cb,
//                              uint32_t mgmt_event, struct net_if *iface)
// {
//     if (mgmt_event != NET_EVENT_IPV4_ADDR_ADD) {
//         return;
//     }

//     // Ищем первый попавшийся DHCP или действительный адрес на интерфейсе
//     for (int i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
//         // Используем стандартную функцию Zephyr 3.7+ для безопасного получения адреса
//         struct net_if_addr *if_addr = net_if_ipv4_get_addr(iface, i);
        
//         if (if_addr && if_addr->is_used && if_addr->addr_type == NET_ADDR_DHCP) {
//             char buf[NET_IPV4_ADDR_LEN];

//             // Переводим бинарный IP в строку
//             net_addr_ntop(AF_INET, &if_addr->address.in_addr, buf, sizeof(buf));
            
//             LOG_INF("=== DHCP SUCCESS! My Assigned IP Address: %s ===", buf);
//             break; // Адрес найден, выходим из цикла
//         }
//     }
// }

void setControlHR(uint16_t val)
{
    unsigned int key = irq_lock(); 
    holding_regs[HR_CONTROL] = val;
    irq_unlock(key);   
}
uint32_t getControlHR()
{
    uint16_t retV;
    unsigned int key = irq_lock(); 
    retV = holding_regs[HR_CONTROL];
    irq_unlock(key);   
    return retV & 0x3F;
}
void clbTimeWr(uint16_t val)
{
    // unsigned int key = irq_lock(); //==uncomment for autoclb
    // holding_regs[HR_AUTO_CLB_TIME] = val;
    // irq_unlock(key);   
}
uint32_t clbTimeRd()
{
    uint16_t retV = 100;
    // unsigned int key = irq_lock(); //==uncomment for autoclb
    // retV = holding_regs[HR_AUTO_CLB_TIME];
    // irq_unlock(key);   
    return retV;  
}

/* 1. Коллбеки чтения/записи регистров */
static int holding_reg_rd_cb(uint16_t addr, uint16_t *reg)
{
    if (addr >= MODBUS_H_REGS_COUNT) { return -EINVAL; }
    unsigned int key = irq_lock(); 
    *reg = holding_regs[addr];
    irq_unlock(key);  
    //LOG_INF("Modbus Read: Reg[%d] = %d", addr, *reg);
    return 0;
}
#pragma pack(push, 1)
struct timeWords
{
    uint8_t msw1 : 1;
    uint8_t msw0 : 1;
    uint8_t lsw1 : 1;
    uint8_t lsw0 : 1;
    uint8_t reserv : 4;
};
#pragma pack(pop)

union tmWBB
{
    uint8_t byte;
    timeWords b;    
};

static tmWBB rcvTime = {0};

static int holding_reg_wr_cb(uint16_t addr, uint16_t reg)
{
    // static bool timeRefresh = false;
    // static bool tRegRcvd [4] = {false};

    if (addr >= MODBUS_H_REGS_COUNT)
        return -EINVAL;
 //   if ((addr == HR_AUTO_CLB_TIME) && (reg  == 0))//==uncomment for autoclb
 //       return -EINVAL;
    if(addr == HR_CONTROL)
    {
        uint16_t temp = reg & 0x001f;
        input_regs[IR_STATUS] &= ~0x001f;
        input_regs[IR_STATUS] |= temp;
    }   
    else if(addr == HR_TIME_UTS_MSW1)
       rcvTime.b.msw1 = 1;
    else if (addr == HR_TIME_UTS_MSW0)
         rcvTime.b.msw0 = 1;             
    else if (addr == HR_TIME_UTS_LSW1)
       rcvTime.b.lsw1 = 1; 
    else if (addr == HR_TIME_UTS_LSW0)
       rcvTime.b.lsw0 = 1;
    unsigned int key = irq_lock();
    holding_regs[addr] = reg;
    irq_unlock(key); 
    if(rcvTime.byte == 0xf)
    {
        UniTime* tm = getBrdTime();
        tm->refreshTime(static_cast<uint16_t*>(&holding_regs[HR_TIME_UTS_MSW1]));
        rcvTime.byte = 0;
    } 
//    LOG_INF("Modbus Write: Reg[%d] -> %d", addr, reg);
    return 0;
}

//addr - uint32 - data[0] = input_reg[0]+input_reg[1], data[n] = input_reg[2n]+input_reg[2n+1],
// void refresh_input_regs(int32_t* data, uint16_t start_addr, uint16_t stop_addr)
// {
//     k_mutex_lock(&adc_data_mutex, K_FOREVER); 
//     memcpy(&input_regs[start_addr * 2], &data[0], (1+ stop_addr - start_addr) * 4);
//     k_mutex_unlock(&adc_data_mutex);  
// }

void timeRefreshed()
{
    rcvTime.byte = 0;
}
void refresh_input_regs(int32_t* data, uint16_t start_addr, uint16_t stop_addr)
{
    unsigned int key = irq_lock(); 
    //memcpy(&input_regs[start_addr * 2], &data[0], (1 + stop_addr - start_addr) * 4);
    uint16_t count_32bit = 1 + stop_addr - start_addr;
    
    for (uint16_t i = 0; i < count_32bit; i++)
    {
        int32_t val = data[i];
        
        // Разбиваем 32-битное число на два 16-битных слова
        uint16_t low_word  = (uint16_t)(val & 0xFFFF);       // Младшее слово (например, 0x5AC1)
        uint16_t high_word = (uint16_t)((val >> 16) & 0xFFFF); // Старшее слово (например, 0x0006)
        
        // Вычисляем базовый индекс в массиве регистров Modbus
        // Каждое 32-битное число занимает ровно ДВА 16-битных регистра
        uint16_t mb_index = (start_addr + i) * 2;
        // КРИТИЧЕСКИЙ ШАГ: записываем СТАРШЕЕ слово первым!
        input_regs[mb_index]     = high_word; // Сначала идет 0x0006
        input_regs[mb_index + 1] = low_word;  // Затем идет 0x5AC1
    }
    
    irq_unlock(key);  
}


void copy_input_regs(int32_t* data, uint16_t start_addr, uint16_t stop_addr)
{
    //k_mutex_lock(&adc_data_mutex, K_FOREVER); 
    unsigned int key = irq_lock(); 
    memcpy(&data[0], &input_regs[start_addr * 2], (1 + stop_addr - start_addr) * 4);
    irq_unlock(key);  
    //k_mutex_unlock(&adc_data_mutex);
}


// static int input_reg_rd_cb(uint16_t addr, uint16_t *reg)
// {
//     if (addr >= MODBUS_IN_REGS_COUNT) { return -EINVAL; }
//     k_mutex_lock(&adc_data_mutex, K_FOREVER);
//     *reg = input_regs[addr];
//     k_mutex_unlock(&adc_data_mutex);      
// //    LOG_INF("Modbus inpReg Read: Reg[%d] = %d", addr, *reg);
//     return 0;
// }
static const uint16_t ST_REEG_CLB_DONE_BITS = 1 << CALIBRATE_DONE;
static int input_reg_rd_cb(uint16_t addr, uint16_t *reg)
{
    if (addr >= MODBUS_IN_REGS_COUNT) { return -EINVAL; }
    unsigned int key = irq_lock();   
    *reg = input_regs[addr];   
    irq_unlock(key); 
    if((input_regs[IR_STATUS] & ST_REEG_CLB_DONE_BITS) != 0)
    {
       if((addr >= IR_HALL_L_MSW) && (addr <= IR_TEMPERATURE_LSW))
            clrStatRgClbBit();
    }
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
//        LOG_ERR("Failed to send Modbus TCP response (errno %d)", errno);
        return -EIO;
    }
    return 0;
}

//void mb_thread_entry(void *arg1, void *arg2, void *arg3)
//{
//     LOG_INF("Starting Modbus TCP Application...");
//     GPO* red = getRedLED();
//     struct modbus_iface_param param = {//1.1
//         .mode = MODBUS_MODE_RAW, 
//         .server = { 
//             .user_cb = &mbs_cbs, 
//             .unit_id = 1 
//         },
//         /* Инициализируем объединение (union) через структуру rawcb */
//         .rawcb = {//1.1.13
//             .raw_tx_cb = modbus_raw_tx_callback /* Перехватчик ответа сервера */
//         }
//     };
//     int ctx_num = modbus_init_server(0, param);
//     if (ctx_num < 0) 
//     {
// //        LOG_ERR("Failed to init Modbus backend (err %d)", ctx_num);
//         return;
//     }
// ////////////////////////
//     LOG_INF("Starting Modbus TCP Application...");
    
//     // ... ваша инициализация modbus (modbus_init_server) ...

//     /* === ЗАПУСКАЕМ DHCPv4 === */
//     struct net_if *iface = net_if_get_default();
//     if (!iface) {
//         LOG_ERR("No default network interface found!");
//         return;
//     }

//     net_dhcpv4_start(iface);
//     LOG_INF("DHCPv4 client started, waiting for IP from router...");

//     /* Цикл ожидания получения IP-адреса */
//     char ip_str[NET_IPV4_ADDR_LEN] = {0};
//     while (1) {
//         // Проверяем, назначен ли на интерфейс хоть один валидный IPv4 адрес
//         struct in_addr *addr = &iface->config.ip.ipv4->unicast[0].address.in_addr;
        
//         // Если адрес не нулевой (т.е. роутер его выдал)
//         if (addr->s_addr != 0) {
//             net_addr_ntop(AF_INET, addr, ip_str, sizeof(ip_str));
//             LOG_INF("==================================================");
//             LOG_INF("=== DHCP SUCCESS! My Assigned IP Address: %s ===", ip_str);
//             LOG_INF("==================================================");
//             break; // Адрес получен, выходим из цикла ожидания
//         }
        
//         // Каждую секунду проверяем статус заново
//         k_msleep(1000);
//     }

//     /* === СОЗДАЕМ TCP СЕРВЕР НА ПОРТУ 502 === */
//    // int server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    
//     // ... далее ваш стандартный bind, listen, accept ...




// // Вставьте инициализацию в mb_thread_entry до вызова socket() и accept():
// //void mb_thread_entry(void *arg1, void *arg2, void *arg3)
// //{
//     // ... ваша инициализация modbus ...

//     // Регистрируем подписку на событие "Добавлен IPv4 адрес"
//     // net_mgmt_init_event_callback(&net_cb, net_event_handler, NET_EVENT_IPV4_ADDR_ADD);
//     // net_mgmt_add_event_callback(&net_cb);

//     // // Запускаем DHCP клиент на дефолтном сетевом интерфейсе
//     // struct net_if *iface = net_if_get_default();
//     // net_dhcpv4_start(iface);
    
//     // LOG_INF("DHCPv4 client started, waiting for IP from router...");

//     // // ... далее ваш стандартный bind, listen, accept ...

/////////////////////////////////////////////////////

// Системный семафор: изначально равен 0, заставит поток ждать получения IP
static K_SEM_DEFINE(ip_obtained_sem, 0, 1);
static struct net_mgmt_event_callback net_cb;

// Коллбек: вызывается системой, когда роутер успешно выдает IP-адрес
static void net_event_handler(struct net_mgmt_event_callback *cb,
                             uint32_t mgmt_event, struct net_if *iface)
{
    if (mgmt_event == NET_EVENT_IPV4_ADDR_ADD) {
        // Даем сигнал семафору, что адрес получен
        k_sem_give(&ip_obtained_sem);
    }
}
static bool isLstStart = false;
bool isListen()
{
    return isLstStart;
}
void mb_thread_entry(void *arg1, void *arg2, void *arg3)
{
    LOG_INF("Starting Modbus TCP Application...");
    GPO* red = getRedLED();
    GPO* extAmb = getExtAmbrLED();

    // === 1. ОСТАВЛЯЕМ ВАШУ ИНИЦИАЛИЗАЦИЮ MODBUS БЕЗ ИЗМЕНЕНИЙ ===
    struct modbus_iface_param param = {
        .mode = MODBUS_MODE_RAW, 
        .server = { 
            .user_cb = &mbs_cbs, 
            .unit_id = 1 
        },
        .rawcb = {
            .raw_tx_cb = modbus_raw_tx_callback
        }
    };
    
    int ctx_num = modbus_init_server(0, param);
    if (ctx_num < 0) {
        return;
    }

    // === 2. ПОДПИСЫВАЕМСЯ НА СОБЫТИЕ СЕТИ И ЗАПУСКАЕМ DHCP ===
    struct net_if *iface = net_if_get_default();
    if (!iface) {
        LOG_ERR("No default network interface found!");
        return;
    }

    //IP event subscribe registration
    net_mgmt_init_event_callback(&net_cb, net_event_handler, NET_EVENT_IPV4_ADDR_ADD);
    net_mgmt_add_event_callback(&net_cb);

    net_dhcpv4_start(iface);
    LOG_INF("DHCPv4 client started, waiting for IP from router...");
    k_sem_take(&ip_obtained_sem, K_FOREVER);
////////////////////
    k_msleep(50); 

    // === БЕЗОПАСНО ПОЛУЧАЕМ IP АДРЕС ЧЕРЕЗ МАССИВ UNICAST ===
    char ip_str[NET_IPV4_ADDR_LEN] = "0.0.0.0";
    
    // unicast — это массив. Берем его первый [0] элемент и вытаскиваем адрес
    //struct in_addr *addr = &iface->config.ip.ipv4->unicast[0].address.in_addr;
    struct in_addr *addr = &iface->config.ip.ipv4->unicast[0].ipv4.address.in_addr;
    if (addr->s_addr != 0)
    {
        net_addr_ntop(AF_INET, addr, ip_str, sizeof(ip_str));
    }

    // === ПОЛУЧАЕМ И ВЫВОДИМ MAC АДРЕС ===
    struct net_linkaddr *mac_addr = net_if_get_link_addr(iface);

    LOG_INF("==================================================");
    LOG_INF("=== NETWORK CONFIGURATION ACQUIRED SUCCESSFULLY ===");
    LOG_INF("==================================================");
    LOG_INF("Board Hostname : hm01.local");
    LOG_INF("Assigned IP    : %s", ip_str);
    
    if (mac_addr && mac_addr->len == 6) {
        LOG_INF("Board MAC Addr : %02X:%02X:%02X:%02X:%02X:%02X",
                mac_addr->addr[0], mac_addr->addr[1], mac_addr->addr[2],
                mac_addr->addr[3], mac_addr->addr[4], mac_addr->addr[5]);
    } else {
        LOG_WRN("Board MAC Addr : Failed to read MAC");
    }
    LOG_INF("==================================================");

//////////////////
     // === 3. ОТКРЫВАЕМ СЕТЕВОЙ СОКЕТ MODBUS ===
    int server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd < 0) 
    {
        LOG_INF("------OOPS #1! Server_fd < 0");
        return;
    }
    else
        LOG_INF("Server OK");
    struct sockaddr_in bind_addr;// = {
    //     .sin_family = AF_INET,
    //     .sin_port = htons(502),
    //     .sin_addr.s_addr = INADDR_ANY,
    // };            
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(502);
    bind_addr.sin_addr.s_addr = INADDR_ANY; 

    if (bind(server_fd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) 
    {
        LOG_INF("------OOPS #2! sizeof_bindAddr < 0");
        close(server_fd);
        return;
    }
    else
        LOG_INF("Bind_OK");
    if (listen(server_fd, 4) < 0)
    {
        LOG_INF("------OOPS #3! listen < 0");
        close(server_fd);
        return;
    }
    else 
    {
        isLstStart = true;
        LOG_INF("LISTEN == ok");
    }

    LOG_INF("====== ENTERING MAIN LOOP: BEFORE ACCEPT ======");
    while (1) 
    {
        struct sockaddr_in client_addr;
        // ЯВНО обнуляем структуру перед вызовом, чтобы убрать возможный мусор из памяти
        memset(&client_addr, 0, sizeof(client_addr));
        
        // Переинициализируем длину СТРОГО на каждом витке цикла!
        socklen_t client_addr_len = sizeof(struct sockaddr_in);
        
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_fd < 0)
        { 
            // КРИТИЧЕСКИ ВАЖНО: Давайте выведем реальную причину ошибки (errno)!
            LOG_ERR("------ACCEPT FAILED! client_fd: %d, errno: %d", client_fd, errno);
            k_sleep(K_MSEC(1000)); // Спим секунду, чтобы не забивать лог
            continue;
        }
        
        LOG_INF("QModMaster has connected to socket successfully, fd: %d", client_fd);
        active_client_fd = client_fd;
        
        while (1) 
/////
        {
            int rc = recv(client_fd, mrx_buf, sizeof(mrx_buf), 0);
            if (rc <= 0)
            {
               // LOG_INF("Master has disconnected");
               // ЛОГ 1: Фиксируем, ПОЧЕМУ мы вышли из цикла чтения
                if (rc == 0) 
                {
                    LOG_INF("rc=0 ");
                } else
                {
                    LOG_ERR("ERROR: %d, errno: %d ", rc, errno);
                }
                break;              
            }
            if (rc < 8) 
                continue;
            struct modbus_adu adu;            
            /* Разбираем входящий заголовок MBAP (первые 7 байт) */
            modbus_raw_get_header(&adu, mrx_buf);           
            /* Назначаем Unit ID (индекс 6) и Function Code (индекс 7) */
            adu.unit_id = mrx_buf[6]; 
            adu.fc = mrx_buf[7];
            red->toggle();   
            extAmb->toggle();        
            /* Длина PDU данных — всё, что идет после Function Code */
            adu.length = rc - 8; 
            
            /* Копируем чистые данные PDU во внутренний массив структуры adu */
            if (adu.length <= sizeof(adu.data)) 
            {
                if (adu.length > 0)
                {
                    memcpy(adu.data, &mrx_buf[8], adu.length);                    
                }
            }
            else 
                continue;
            /* ВЫЗОВ ИСПРАВЛЕН: Передаем СТРОГО 2 аргумента согласно сигнатуре Zephyr 3.7.1 */
            /* Функция вернет 0, выполнит holding_reg_rd_cb и автоматически вызовет modbus_raw_tx_callback */
            int err = modbus_raw_submit_rx(0, &adu);
//           if (err != 0) 
//                LOG_ERR("Error submitting raw ADU: %d", err);
        }     
        int close_rc = close(client_fd);
        if (close_rc == 0)
        {
            LOG_INF("close ok ");
        }
        else
        {
            LOG_ERR("close FAILED, errno: %d ", errno);
        }

        active_client_fd = -1;
        k_msleep(10);
        LOG_INF("goTo->accept ");
    }   
    close(server_fd);

}

K_THREAD_STACK_DEFINE(mb_thread_stack, MB_THREAD_STACK_SIZE);
static struct k_thread mb_thread_data;


// enum MB_INPUT_REG_MAP
// {
//     IR_HALL_L_MSW,//1
//     IR_HALL_L_LSW,//2
//     IR_HALL_R_MSW,//3
//     IR_HALL_R_LSW,//4
//     IR_CURRENT_R_MSW,//5
//     IR_CURRENT_R_LSW,//6
//     IR_TEMPERATURE_MSW,//7
//     IR_TEMPERATURE_LSW,//8
//     IR_TIME_UTS_MSW1,//9
//     IR_TIME_UTS_MSW0,//10
//     IR_TIME_UTS_LSW1,//11
//     IR_TIME_UTS_LSW0,//12
//     IR_STATUS,//13
//     IR_NULL,//14
//     IR_CLB_PLUS_MSW,//15
//     IR_CLB_PLUS_LSW,//16
//     IR_CLB_MINUS_MSW,//17
//     IR_CLB_MINUS_LSW,//18
//     IR_V_REF_NV_MSW,//19
//     IR_V_REF_NV_LSW,//20
//  //   MODBUS_IN_REGS_COUNT,    
//     IR_BRD_INFO,//21
//     IR_PROBE_INFO,//22
//     IR_FW_VERSION,//23
//     IR_IFC_VERSION,//24
//     MODBUS_IN_REGS_COUNT
// };

// struct statInpRbits
// {
//     uint16_t hallLadcOn : 1;//0
//     uint16_t hallRadcOn : 1;//1
//     uint16_t hCurradcOn : 1;//2
//     uint16_t TadcOn : 1;//3
//     uint16_t currentOn : 1;//4
//     uint16_t clbrDOne : 1;//5
//     uint16_t ready : 1;//6
//     uint16_t probErr : 1;//7
//     uint16_t reserv : 8;//8-15
// };
void modbus_start()
{
    statusInpR ir;
    ir.word = 0x5f;
    input_regs[IR_STATUS] = ir.word;
    input_regs[IR_BRD_INFO] = 0x0001;///
    input_regs[IR_PROBE_INFO]  = 0x0110;///
    input_regs[IR_FW_VERSION]  = 0x000C;///
    input_regs[IR_IFC_VERSION]  = 0x0000;///
    holding_regs[HR_CONTROL] = 0x1F;
    
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
void setStatRgClbBit()
{
      statusInpR ir;
      ir.word =  input_regs[IR_STATUS];
      ir.b.clbrDOne = 1;  
      input_regs[IR_STATUS] = ir.word;
}
void clrStatRgClbBit()
{
      statusInpR ir;
      ir.word =  input_regs[IR_STATUS];
      ir.b.clbrDOne = 0;  
      input_regs[IR_STATUS] = ir.word;
}