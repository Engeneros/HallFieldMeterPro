#include "console.hpp"
#include <zephyr/sys/printk.h>
#include <string.h>
#include <stdlib.h>
#include "board_devs.hpp"
//#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

static const uint16_t HW_VERSION = 0;
static const uint16_t FW_VERSION = 6;
static const uint16_t IFC_VERSION = 0;
static const char HW_DATE [] = "02.26";
static const char FW_DATE [] = "25.08.26";
static const char IFC_DATE [] = "10.07.26";

enum CMD_SET
{
    CMD_VERSION,
    CMD_CALIBRATION,
    CMD_CLB_DATA,
    CMD_DISABLE_AUTO,
    CMD_ENABLE_AUTO,
    CMD_SET_AUTO_PERIOD,
    CMD_GET_AUTOPERIOD,
    CMD_ADC_DATA,
    CMD_V_REF_SET,
    CMD_V_REF_GET,
    CMD_BAD_CMD,
    CMD_NUM
};

static const char* const CONSOLE_CMD[] = {
    "ver",
    "clb",
    "clbData",
    "disA",
    "enA",
    "ta=",
    "tA",
    "ADC",
    "vRef=",
    "vRef",
    "bad command"
};

void Console::printMenu()
{
    printk("Commands: \n");
    printk("ver - version \n");
    printk("clb - show calibrate data\n");
    printk("clbData - calibrate \n");
    printk("disA - disable autocalibrate \n");
    printk("enAa - enable autocalibtate \n");
    printk("tA=<SPACE><time in seconds> - set autocalibrate period \n");
    printk("tA - show autocalibrate period \n"); 
    printk("ADC - show ADCs cchanals data\n");
    printk("vRef= <SPACE><ref voltage in uVolt> - set reference voltage\n");
    printk("vRef get reference voltage\n");
    printk("-------------------------- \n"); 
}

void Console::printVersion()
{
    printk("BSP Hall Field Meter:\n");
    printk("Firmware version %d %s\n", FW_VERSION, FW_DATE);
}

void Console::calibrate()
{
    adcSys->calibrateStart();
    while(adcSys->isClbDone() == false)
    {        
    }
    printClbData();
}

void Console::printClbData()
{
    int32_t adDt[ACQ_CHAN_NUM];
    double uVpq[CLB_NULL];
    adcSys->getData(adDt);
    adcSys->getClbData(uVpq);
    for(uint8_t ch = CLB_NULL; ch < ACQ_CHAN_NUM; ++ch)
        printk("CLB[%d]=%dq \n", ch, adDt[ch]);
    for(uint8_t ch = HALL_SENS_L; ch <  CLB_NULL; ++ch)
        printk("uVpQ[%d]=%f \n", ch, uVpq[ch] );
}

void Console::disAutoClb()
{
    adcSys->autoClbDisable();
}

void Console::enAutoClb()
{
    adcSys->autoClbEnable();
}

void Console::setAutoClbTime(unsigned int argc, char **argv)
{
    if(argc == 3)
    {
        int seconds = atoi(argv[2]);
        if (seconds > 9)
            adcSys->autoClbSetT(seconds);
        else 
            printk ("OOPS: Bad time value. The minimum auto calibration period is 10 seconds.");
    }
    else
        printk ("OOPS:try: pr<SPACE>ta=<SPACE><time in seconds>");
}

void Console::getAutoClbTime()
{
    printk("autocalibrate period is %d cec", adcSys->autoClbGetT());
}

void Console::getAdcData()
{
    int32_t adDt[ACQ_CHAN_NUM];
    adcSys->getData(adDt);
    for (uint8_t ch = HALL_SENS_L; ch < HALL_CURRENT; ++ch )
        printk("UHall[%d]=%fmV\n", ch, static_cast <double>(adDt[ch]) / 1000.0);
    printk("Hall Current=%fmA\n", static_cast <double>(adDt[HALL_CURRENT]) / 1000000.0);
    printk("UPt1000=%fmV.\n", static_cast <double>(adDt[TEMPERATURE]) / 1000.0);        
}
void Console::getVRef()
{
    printk("V reference is %f\n", adcSys->getVRef());
}

void Console::setVRef(unsigned int argc, char **argv)
{
    if(argc == 3)
    {
        double vRef = atof(argv[2]);
        if ((vRef > MIN_REF_V) && (vRef < MAX_REF_V))
            adcSys->setVRef(vRef);
        else 
            printk ("OOPS: Bad reference voltage. Vref is about 500000 uV.");
    }
    else
        printk ("OOPS:try: pr<SPACE>vRef=<SPACE><time in seconds>");
}

void Console::badCmd()
{
    printk("unknown command, do not wory, try again \n");
    printMenu();
}
Console::Console()
{
    adcSys = getAdcSequencer();
}

void Console::parser(unsigned int argc, char **argv)
{
    static unsigned int cmdCount = ARRAY_SIZE(CONSOLE_CMD);
    unsigned int cmdN;
    for (cmdN = 0;    cmdN < cmdCount;     ++cmdN)
        if ( strcmp(argv[1], CONSOLE_CMD[cmdN]) == 0)
            break;
    switch(cmdN)
    {
        case CMD_VERSION : printVersion();
            break;
        case CMD_CALIBRATION : calibrate();
            break;
        case CMD_CLB_DATA : printClbData();
            break;
        case CMD_DISABLE_AUTO : disAutoClb();
            break;
        case CMD_ENABLE_AUTO :  enAutoClb();
            break;
        case CMD_SET_AUTO_PERIOD : setAutoClbTime(argc, argv);
            break;
        case CMD_GET_AUTOPERIOD :  getAutoClbTime();
            break;
        case CMD_BAD_CMD : badCmd();        
            break;
        case CMD_ADC_DATA : getAdcData();
            break;  
        case CMD_V_REF_SET : setVRef(argc, argv);
            break;
        case CMD_V_REF_GET : getVRef();
            break;                      
        default : badCmd();
            break;    
    }            
}