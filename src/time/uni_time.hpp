#pragma once
#include <stdint.h>
#include <zephyr/kernel.h>

class UniTime
{
public:    
    UniTime(int64_t currentTime);
    void refreshTime(int64_t currentTime);
    void refreshTime(uint16_t* hRegs);
    int64_t getTime();
    virtual ~UniTime() = default;

private:
    bool start(int64_t currentTime);    
    static void threadEntryThunk(void *p1, void *p2, void *p3);
    
    static int64_t mSec64;
    static struct k_spinlock time_lock;
    static struct k_thread time_thread_data;
    
    // СТРОКУ С time_thread_stack ОТСЮДА ПОЛНОСТЬЮ УДАЛЯЕМ!
};



// #pragma once
// #include <stdint.h>
// #include <atomic>
// class UniTime
// {
// public:    
//     UniTime(int32_t currentTime);
//     void  refreshTime (int32_t currentTime);
//     int32_t  getTime();
//     virtual ~UniTime();
// private:
//     bool start();    
//     static void threadEntryThunk(void *p1, void *p2, void *p3);
//     static std::atomic<int32_t>  mSec;
//     struct k_thread time_thread_data;
//     K_KERNEL_STACK_MEMBER(time_thread_stack, 2048);
// };


// // #pragma once
// // #include <stdint.h>
// #include <atomic>
// #include <zephyr/kernel.h> // нужно для k_thread и стека

// class UniTime {
// public:
//     UniTime(int32_t currentTime);
//     void refreshTime(int32_t currentTime);
//     int32_t getTime();
//     virtual ~UniTime();

// private:
//     bool start();
//     static void threadEntryThunk(void *p1, void *p2, void *p3);
    
//     std::atomic<int32_t> mSec; // Теперь она НЕ статическая!
//     struct k_thread time_thread_data;
//     K_KERNEL_STACK_MEMBER(time_thread_stack, 2048);
// };


