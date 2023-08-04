#ifndef __MAXEYE_WLC_H__
#define __MAXEYE_WLC_H__

/*******************************************************************************
 * Include files
 ******************************************************************************/
#include <string.h>
#include <stdbool.h>
#include "stdint.h"
#include "app_error.h"

/**@brief Firmware information define*/

typedef enum
{
    WLC_DEV_ABNORMAL,
    WLC_DEV_POWER_DOWN,
    WLC_DEV_POWER_UP,
} wlc_dev_status_t;


extern bool wlc_ask_busy;
extern uint8_t wlcStatus;
/*
 * GLOBAL FUNCTION DECLARATION
 *****************************************************************************************
 */

void wlc_ask_data_generate(uint8_t *pData);
sdk_err_t wlc_int_event_start(uint16_t wDelaymS);
void maxeye_wlc_event_register(void);
void maxeye_wlc_init(void);

// stop ask task
void wlc_ask_task_stop(void);
#endif

