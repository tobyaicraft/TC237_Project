/**********************************************************************************************************************
 * \file DrvIntc.c
 * \brief Interrupt controller driver
 *
 * - CPU Watchdog 및 Safety Watchdog 비활성화
 * - 글로벌 인터럽트 Enable 은 모든 초기화 완료 후 core0_main 에서 직접 수행
 *********************************************************************************************************************/
#include "DrvIntc.h"
#include "IfxScuWdt.h"

/******************************************************************************/
/*                           Functions                                        */
/******************************************************************************/
void DrvIntc_Init(void)
{
    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_disableSafetyWatchdog(IfxScuWdt_getSafetyWatchdogPassword());
}
