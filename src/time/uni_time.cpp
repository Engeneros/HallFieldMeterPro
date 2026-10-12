#include <zephyr/kernel.h>
#include "uni_time.hpp"

#define TIME_THREAD_PRIORITY 10

int64_t UniTime::mSec64{0};
struct k_spinlock UniTime::time_lock;
struct k_thread UniTime::time_thread_data;

// Объявляем стек прямо в .cpp файле. 
// Слово static сделает эту переменную видимой только внутри этого файла.
static K_KERNEL_STACK_DEFINE(time_thread_stack, 2048);

bool UniTime::start(int64_t currentTime)
{
    refreshTime(currentTime);
    
    // Теперь макросы Zephyr отработают идеально, так как они видят "родную" переменную стека
    k_thread_create(&time_thread_data,
                time_thread_stack,
                K_KERNEL_STACK_SIZEOF(time_thread_stack),
                UniTime::threadEntryThunk, 
                nullptr, nullptr, nullptr,               
                K_PRIO_PREEMPT(TIME_THREAD_PRIORITY), 
                0,
                K_NO_WAIT);
    return true;
}

UniTime::UniTime(int64_t currentTime) 
{
    static bool isRun = start(currentTime);
    (void) isRun;
}

void UniTime::refreshTime(int64_t currentTime)
{
    k_spinlock_key_t key = k_spin_lock(&time_lock);
    mSec64 = currentTime;
    k_spin_unlock(&time_lock, key);
}

union regsAndTime
{
    int64_t time;
    uint16_t w[4];
};

static uint16_t swp16(uint16_t val) {
    // Сдвигаем младший байт влево, а старший вправо, и объединяем по ИЛИ
    return static_cast<uint16_t>((val << 8) | (val >> 8));
}

void UniTime::refreshTime(uint16_t* hRegs)
{
    uint16_t words[4];
    regsAndTime tmp;
    for (int cnt = 0; cnt < 4; ++cnt)
        tmp.w[3 - cnt] = words[cnt] = hRegs[cnt]; //swp16(hRegs[cnt]);
    k_spinlock_key_t key = k_spin_lock(&time_lock);
    mSec64 = tmp.time;
    k_spin_unlock(&time_lock, key);
}

int64_t UniTime::getTime()
{
    int64_t localTime;
    k_spinlock_key_t key = k_spin_lock(&time_lock);
    localTime = mSec64;
    k_spin_unlock(&time_lock, key);
    return localTime;
}

void UniTime::threadEntryThunk(void *p1, void *p2, void *p3)
{
     while (true)
     {
         k_msleep(1);
         
         k_spinlock_key_t key = k_spin_lock(&time_lock);
         ++mSec64;    
         k_spin_unlock(&time_lock, key);
     }
}



// #include <zephyr/kernel.h>
// #include "uni_time.hpp"
// #define TIME_THREAD_PRIORITY 10

// std::atomic<int32_t> UniTime::mSec{0};

// bool UniTime::start()
// {
//     refreshTime(currentTime);
//     k_thread_create(&time_thread_data,
//                 time_thread_stack,
//                 K_KERNEL_STACK_SIZEOF(time_thread_stack),
//                 UniTime::threadEntryThunk, 
//                 this, nullptr, nullptr,               
//                 K_PRIO_PREEMPT(TIME_THREAD_PRIORITY), 
//                 0,
//                 K_NO_WAIT);
//     return true;
// }

// UniTime::UniTime(int32_t currentTime) 
// {
//     static bool isRun = start();
//     (void) isRun;
// }

// void  UniTime::refreshTime (int32_t currentTime)
// {
//     mSec = currentTime;
// }

// int32_t  UniTime::getTime()
// {
//     return mSec;
// }

// void UniTime::threadEntryThunk(void *p1, void *p2, void *p3)
// {
//      while (true)
//      {
//          k_msleep(1);
//          ++mSec;    
//      }
// }

// UniTime::~UniTime()
// {

// }


// #include <zephyr/kernel.h>
// #include "uni_time.hpp"

// #define TIME_THREAD_PRIORITY 10

// bool UniTime::start() {
//     // Передаем 'this' в качестве первого аргумента p1
//     k_thread_create(&time_thread_data, time_thread_stack, 
//                     K_KERNEL_STACK_SIZEOF(time_thread_stack), 
//                     UniTime::threadEntryThunk, 
//                     this, nullptr, nullptr, 
//                     K_PRIO_PREEMPT(TIME_THREAD_PRIORITY), 0, K_NO_WAIT);
//     return true;
// }

// UniTime::UniTime(int32_t currentTime) : mSec(currentTime) {
//     start(); // Каждый объект имеет свой независимый поток времени
// }

// void UniTime::refreshTime(int32_t currentTime) {
//     mSec = currentTime;
// }

// int32_t UniTime::getTime() {
//     return mSec;
// }

// void UniTime::threadEntryThunk(void *p1, void *p2, void *p3) {
//     // Приводим указатель p1 обратно к нашему объекту
//     UniTime *instance = static_cast<UniTime*>(p1);
    
//     while (true) {
//         k_msleep(1);
//         // Обращаемся к переменной конкретного экземпляра
//         instance->mSec.fetch_add(1, std::memory_order_relaxed);
//     }
// }

// UniTime::~UniTime() {
//     // В реальном коде здесь нужно останавливать поток k_thread_abort,
//     // иначе он будет обращаться к удаленной памяти!
// }

