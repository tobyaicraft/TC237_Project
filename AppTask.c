/**********************************************************************************************************************
 * \file AppTask.c
 * \brief Application task implementations
 *
 * Task_1ms   : reserved for future fast-cycle processing
 * Task_10ms  : ADC scan, PWM duty update, TIM 측정값 읽기
 * Task_100ms : LED heartbeat toggle
 *********************************************************************************************************************/
#include "AppTask.h"
#include "DrvDio.h"
#include "DrvAdc.h"
#include "DrvPwm.h"
#include "DrvTim.h"

float32 g_timDutyPercent = 0.0f;
float32 g_timPeriodSec   = 0.0f;

/******************************************************************************/
/*                           1ms Task                                         */
/******************************************************************************/
void AppTask_1ms(void)
{
    /* Reserved - fast-cycle processing */
}

/******************************************************************************/
/*                           10ms Task                                        */
/******************************************************************************/
void AppTask_10ms(void)
{
    DrvAdc_Run();
    DrvPwm_SetDuty(g_pwmDuty);

    g_timDutyPercent = DrvTim_GetDutyPercent();
    g_timPeriodSec   = DrvTim_GetPeriodSec();
}

/******************************************************************************/
/*                           100ms Task                                       */
/******************************************************************************/
void AppTask_100ms(void)
{
    DrvDio_ToggleLed0();
}
