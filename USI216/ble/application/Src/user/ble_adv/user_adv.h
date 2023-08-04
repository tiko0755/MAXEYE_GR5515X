/**
 *****************************************************************************************
 *
 * @file user_adv.h
 *
 * @brief Header file - User Function
 *
 *****************************************************************************************
 */
#ifndef __USER_ADV_H__
#define __USER_ADV_H__

#include <stdint.h>
#include "usr_typedef.h"
 
/**
 *****************************************************************************************
 * @brief 
 *****************************************************************************************
 */

void adv_params_init(void);
/**
 *****************************************************************************************
 * @brief config for UART or BLE log output
 * [00]b both disable
 * [01]b ble disable, uart enable
 * [10]b ble enable, uart disable
 * [11]b both enable
 *****************************************************************************************
 */
void maxeye_ble_adv_start(uint16_t wAdvDuration);
uint16_t user_adv_stop(uint8_t index);


#endif

