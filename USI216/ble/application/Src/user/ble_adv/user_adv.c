/**
 *****************************************************************************************
 *
 * @file user_adv.c
 *
 * @brief User function Implementation.
 *
 *****************************************************************************************
 */

/*
 * INCLUDE FILES
 *****************************************************************************************
 */
#include "gr55xx_sys.h"
#include "gr55xx_pwr.h"
#include "gr55xx_hal.h"
#include "utility.h"
#include "app_error.h"

#include "user_log.h"
#include "user_adv.h"
#include "maxeye_version.h"

//// using maxeye_srvc2_char4_notify(uint8_t conn_idx, uint8_t *p_data, uint16_t length);
//#include "maxeye_private_services.h"

//// using bleConnStatus
//#include "maxeye_ble.h"

/*
 * DEFINES
 *****************************************************************************************
 */
#define APP_ADV_FAST_MIN_INTERVAL          32               /**< The fast advertising min interval (in units of 0.625 ms. This value corresponds to 160 ms). */
#define APP_ADV_FAST_MAX_INTERVAL          48               /**< The fast advertising max interval (in units of 0.625 ms. This value corresponds to 1000 ms). */
#define APP_ADV_SLOW_MIN_INTERVAL          160              /**< The slow advertising min interval (in units of 0.625 ms). */
#define APP_ADV_SLOW_MAX_INTERVAL          160              /**< The slow advertising max interval (in units of 0.625 ms). */
#define APP_ADV_TIMEOUT_IN_SECONDS         0                /**< The advertising timeout in units of seconds. */


/*
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************************
 */
uint8_t s_is_adv_exc_cplt = 0;

// 0: initail, NO adv
// 1: starting adv
// 2: started cplt, advertising
// 3: stoping adv
uint8_t adv_stage = 0;  

/*
 * LOCAL VARIABLE DEFINITIONS
 *****************************************************************************************
 */
static gap_adv_param_t      s_gap_adv_param;            /**< Advertising parameters for legay advertising. */
static gap_adv_time_param_t s_gap_adv_time_param;       /**< Advertising time parameter. */

static const uint8_t s_adv_data_set[] =                 /**< Advertising data. */
{
    PENCIL_MODEL_NUM_LEN+1,   // Length of this data
    BLE_GAP_AD_TYPE_COMPLETE_NAME,
    PENCIL_MODEL_NUM_STR_S
    0x03,
    BLE_GAP_AD_TYPE_APPEARANCE,
    LO_U16(BLE_APPEARANCE_HID_DIGITAL_PEN),
    HI_U16(BLE_APPEARANCE_HID_DIGITAL_PEN),

    0x02,
    BLE_GAP_AD_TYPE_TRANSMIT_POWER,
    0,
    0,
};

static const uint8_t s_adv_rsp_data_set[] = /**< Scan responce data. */
{
    // length of this data
    PENCIL_MODEL_NUM_LEN+1,   // Length of this data
    BLE_GAP_AD_TYPE_COMPLETE_NAME,
    PENCIL_MODEL_NUM_STR_S
};
 
void adv_params_init(void)
{
    sdk_err_t   error_code;

    s_gap_adv_time_param.max_adv_evt = 0;
    s_gap_adv_param.chnl_map         = GAP_ADV_CHANNEL_37_38_39;
    s_gap_adv_param.max_tx_pwr       = 0;

    memset(&s_gap_adv_param.peer_addr, 0, sizeof(gap_bdaddr_t));
    s_gap_adv_param.disc_mode  = GAP_DISC_MODE_NON_DISCOVERABLE;
    s_gap_adv_param.adv_mode   = GAP_ADV_TYPE_ADV_IND;
    s_gap_adv_param.filter_pol = GAP_ADV_ALLOW_SCAN_ANY_CON_ANY;

    s_gap_adv_param.adv_intv_max = APP_ADV_SLOW_MAX_INTERVAL;
    s_gap_adv_param.adv_intv_min = APP_ADV_SLOW_MIN_INTERVAL;

    error_code = ble_gap_adv_data_set(0, BLE_GAP_ADV_DATA_TYPE_DATA,
                                      s_adv_data_set, sizeof(s_adv_data_set));
    APP_ERROR_CHECK(error_code);



    error_code = ble_gap_adv_param_set(0, BLE_GAP_OWN_ADDR_STATIC,
                                       &s_gap_adv_param);
    APP_ERROR_CHECK(error_code);


    error_code = ble_gap_adv_data_set(0, BLE_GAP_ADV_DATA_TYPE_SCAN_RSP,
                                      s_adv_rsp_data_set, sizeof(s_adv_rsp_data_set));
    APP_ERROR_CHECK(error_code);

}

void maxeye_ble_adv_start(uint16_t wAdvDuration)
{
    logX("<%s adv_stage:%d>", __func__, adv_stage);
    
    if(adv_stage !=0 ){
        ble_gap_adv_stop(0);
        return;
    }
    
    sdk_err_t error_code = SDK_SUCCESS;
    s_gap_adv_time_param.duration = wAdvDuration;
    error_code = ble_gap_adv_start(0, &s_gap_adv_time_param);
    logX("</%s adv_start:0x%02x>", __func__, error_code);
}

uint16_t user_adv_stop(uint8_t index){
    sdk_err_t error_code;
    error_code = ble_gap_adv_stop(index);
    
//    extern void ble_task_force_schedule(void);
//    s_is_adv_exc_cplt = 0;
//    
//    error_code = ble_gap_adv_stop(index);
//    if(error_code == SDK_SUCCESS){
//        while(!s_is_adv_exc_cplt){
//            ble_task_force_schedule();
//        }
//    }
    return error_code;
}
