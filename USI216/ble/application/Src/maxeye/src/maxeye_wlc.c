/**
 *****************************************************************************************
 *
 * @file maxeye_wlc.c
 *
 * @brief 
 *
 *****************************************************************************************
 */


/*
 * INCLUDE FILES
 *****************************************************************************************
 */
#include "app_log.h"
#include "app_assert.h"
#include "app_drv_error.h"
#include "app_timer.h"

#include "user_app.h"

#include "maxeye_wlc.h"
#include "maxeye_ra9520.h"

#include "maxeye_battery.h"
#include "maxeye_sensor.h"

#include "maxeye_ble.h"
#include "maxeye_sleep.h"
#include "maxeye_ble_cli.h"

#include "user_log.h"

/*
 * DEFINES
 *****************************************************************************************
 */
#ifdef  BLE_LOG_EN
#define LOG(format,...)  printf(format,##__VA_ARGS__) 
#else
#define LOG(format,...)  
#endif



/*
 * LOCAL VARIABLE DEFINITIONS
 *****************************************************************************************
 */

static uint8_t ask_pack_index = 3;

static uint8_t wlc_ask_buf[9] = {0};

static app_timer_id_t wlc_ask_task_id;
static app_timer_id_t wlc_int_event_id;
/*
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************************
 */

bool wlc_ask_busy = true;

uint8_t wlcStatus = WLC_DEV_POWER_DOWN;

/*
 * LOCAL FUNCTION DEFINITIONS
 *****************************************************************************************
 */

// 读WLC中断
static uint8_t read_wlc_int(uint8_t * int_flag)
{
    uint8_t readCnt = 3;

    while(readCnt--)
    {
        if (wlc_read_system_interruput(0, int_flag) == APP_DRV_SUCCESS)
        {
            return 0;
        }
    }
    return 1;
}

// 清WLC中断
static uint8_t clear_wlc_int(void)
{
    uint8_t retry_count = 3;

    while(retry_count--)
    {
        if (wlc_interrupt_clear() == APP_DRV_SUCCESS)
        {
            return 0;
        }
    }
    return 1;
}


// 检查TX回写数据是否正确
static uint8_t check_wlc_ack_match(void)
{
    uint8_t buf[2] = {0};

    wlc_read_prop_data_in(buf, 2);

    if (0x1E == buf[0] && 0xFF == buf[1])
    {
        return 1;
    }

    logXX("<%s ask_ack_err:0x%02X %02X>", __func__, buf[0], buf[1]);
    return 0;
}


// 生成ASK分包数据
void wlc_ask_data_generate(uint8_t * mac)
{
    uint8_t temp = 0;

    wlc_ask_buf[3] = ((mac[3] & 0x0F) << 4) |((mac[0] & 0xF0) >> 4);//5
    wlc_ask_buf[4] = ((mac[4] & 0x0F) << 4) |((mac[1] & 0xF0) >> 4);//4
    wlc_ask_buf[5] = ((mac[5] & 0x0F) << 4) |((mac[2] & 0xF0) >> 4);//3

    wlc_ask_buf[6] = ((mac[0] & 0x0F) << 4) |((mac[3] & 0xF0) >> 4);//2
    wlc_ask_buf[7] = ((mac[1] & 0x0F) << 4) |((mac[4] & 0xF0) >> 4);//1
    wlc_ask_buf[8] = ((mac[2] & 0x0F) << 4) |((mac[5] & 0xF0) >> 4);//0


    temp = wlc_ask_buf[3] ^ wlc_ask_buf[6];
    wlc_ask_buf[0] = ((temp & 0x0F) << 4) | ((temp & 0xF0) >> 4);

    temp = wlc_ask_buf[4] ^ wlc_ask_buf[7];
    wlc_ask_buf[1] = ((temp & 0x0F) << 4) | ((temp & 0xF0) >> 4);

    temp = wlc_ask_buf[5] ^ wlc_ask_buf[8];
    wlc_ask_buf[2] = ((temp & 0x0F) << 4) | ((temp & 0xF0) >> 4);
}


// 启动ASK传输任务, will loop it and stop it by BLE connected call back
static sdk_err_t wlc_ask_task_start(uint16_t wDelaymS, void* e)
{
    app_timer_stop(wlc_ask_task_id);
    return app_timer_start(wlc_ask_task_id, wDelaymS, e);
}

// 用于传递ASK数据
static uint8_t wlc_ask_squ = 0;
static uint8_t wlc_repeatOnce = 0;

static void wlc_ask_task_handler(void* p_ctx)
{
    logXX("<%s>", __func__);
    
    uint8_t data_buf[5] = {0};
    wlc_ask_busy = true;
    uint8_t flg = *(uint8_t*)p_ctx;
        
    if (flg & 0x40)                // ldo enable
    {
        wlc_ask_squ = 1;    // restart
    }
    else if ((wlc_repeatOnce==0) && ((flg&0x10)==0))           // BIT[4] 之外的中断不处理
    {
        return;
    }
    wlc_repeatOnce = 0;
    
    logXX("<%s flg:0x%02x  wlc_ask_squ:%d >", __func__, flg, wlc_ask_squ);
    
    data_buf[0] = RX_PROP_4_BYTE;
    switch (wlc_ask_squ){
        case 0:{
            break;
        }
        case 1:{    // send first packet
            battery_charge_start();         // 建立轻负载以保证ASK数据稳定传输
            if (battMeterStatus != BATT_METER_NORMAL)       // 电量计异常 尝试重新配置
            {
                batt_meter_init_event_register();
            }
            if (gSensorStatus == G_SENSOR_ABNORMAL)         // g sensor异常 尝试重新配置
            {
                g_sensor_init_event_register();
            }
            wlcStatus = WLC_DEV_POWER_UP;
            wlc_ask_squ++;
            ask_pack_index = 0;
            data_buf[1] = 0xC1;
            break;
        }
        case 2:{    // wait first packet ACK, and send second packet
            if(check_wlc_ack_match() == 0){
                ask_pack_index = 0; // resend
                data_buf[1] = 0xC1;
            }
            else{
                ask_pack_index = 1;
                data_buf[1] = 0xB6;
                wlc_ask_squ++;
            }
            break;
        }
        case 3:{
            if(check_wlc_ack_match() == 0){
                ask_pack_index = 1;
                data_buf[1] = 0xB6;
            }
            else{
                ask_pack_index = 2;
                data_buf[1] = 0xB7;
                wlc_ask_squ++;
            }
            break;
        }
        case 4:{
            if(check_wlc_ack_match() == 0){
                ask_pack_index = 0;
                data_buf[1] = 0xB7;
            }
            else{
                ask_pack_index = 0;
                data_buf[1] = 0xC1;
                wlc_ask_squ = 2;
                if (BLE_NO_CONN_NO_ADV == bleConnStatus)
                {
                    maxeye_pencil_wakeup();
                }
            }
            break;
        }
        
        case 90:{
            break;
        }
    }

    if (bleConnStatus < BLE_CONNECTED)
    {
        logXX("<%s w_to_adv>", __func__);
        maxeye_ble_adv_start(ADV_LONG_DURATION);
    }
    else
    {
        logXX("<%s wlc_not_adv:%d>", __func__, bleConnStatus);
    }

    // will NOT response when it is completed
    if(wlc_ask_squ == 90){
        return;
    }
    
    memcpy(&data_buf[2], &wlc_ask_buf[ask_pack_index * 3], 3);
    uint16_t ret = wlc_write_prop_data_out(data_buf, 5);
    ret |= wlc_send_prop_data();

    if (APP_DRV_SUCCESS == ret)
    {
        logXX("<%s ask:0x%02X %02X %02X %02X>", __func__, data_buf[1], data_buf[2], data_buf[3], data_buf[4]);
        app_timer_start(wlc_ask_task_id, 350, NULL);
        wlc_repeatOnce = 1;
    }
    else
    {
        logXX("<%s ask_err:%d>", __func__, ret);
        uint16_t chipid = 0;
        wlc_read_chip_type(&chipid);                        // 检测wlc tx是否正常
        if (WLC_CHIP_ID == chipid)                          // 读到ID说明在充电 需要启动ASK传输
        {
            app_timer_start(wlc_ask_task_id, 10, NULL);     // 延迟并重试发数据
            ask_pack_index = 0;
        }
        else
        {
            wlc_ask_busy = false;
            wlcStatus = WLC_DEV_POWER_DOWN;                 // 充电已关闭 不再尝试重发
        }
    }
    logXX("</%s>", __func__);
}

// stop ask task
void wlc_ask_task_stop(void)
{
    wlc_ask_squ = 90;
    app_timer_stop(wlc_ask_task_id);
}


// 注册定时ASK任务
void wlc_prop_event_register(void)
{
    sdk_err_t error_code = app_timer_create(&wlc_ask_task_id, ATIMER_ONE_SHOT, wlc_ask_task_handler);
    APP_ERROR_CHECK(error_code);
}

// 启动中断响应Event
sdk_err_t wlc_int_event_start(uint16_t wDelaymS)
{
    sdk_err_t ret = SDK_SUCCESS;

    ret = app_timer_start(wlc_int_event_id, wDelaymS, NULL);
    if (SDK_SUCCESS != ret)
    {
        logXX("<%s w_i_s:%d>", __func__,ret);
    }

    return ret;
}

static uint8_t sIntFlag;
// IO中断来临时处理9520中断 初始化时检测9520中断
static void wlc_int_event_handler(void* p_ctx)
{
logXX("<%s >", __func__);
    uint8_t ret_code = read_wlc_int(&sIntFlag);
    clear_wlc_int();                        // 清中断以恢复INT电平

    if (ret_code == APP_DRV_SUCCESS)
    {
        wlc_ask_task_start(5, &sIntFlag);
    }
    else
    {
        wlcStatus = WLC_DEV_ABNORMAL;
    }
    
    logXX("</%s int_flag:0x%02x>", __func__, sIntFlag);
}


// 注册定时触发中断Event
void wlc_int_event_register(void)
{
    sdk_err_t error_code = app_timer_create(&wlc_int_event_id, ATIMER_ONE_SHOT, wlc_int_event_handler);
    APP_ERROR_CHECK(error_code);
}


// 注册WLC相关定时处理任务
void maxeye_wlc_event_register(void)
{
    wlc_prop_event_register();
    wlc_int_event_register();
}


// 初始化
void maxeye_wlc_init(void)
{
    uint16_t chipid = 0;

    wlc_read_chip_type(&chipid);            // 上电检测wlc tx是否正常

    if (WLC_CHIP_ID == chipid)              // 读到ID说明在充电 需要启动ASK传输
    {
        battery_charge_start();             // 建立轻负载以保证ASK数据稳定传输
        ask_pack_index = 0;
        wlcStatus = WLC_DEV_POWER_UP;
        uint8_t ret_code = read_wlc_int(&sIntFlag);
        clear_wlc_int();                    // 清中断以恢复INT电平
//        wlc_ask_task_start(100, &sIntFlag);            // 确保在g sensor前
    }
    else
    {
        logX("<%s wlc id:%04x>", __func__, chipid);
        battery_discharge(END_POWER_WLC_NOT_ATTEND);
        wlc_ask_busy = false;
    }
}

