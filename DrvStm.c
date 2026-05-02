/**********************************************************************************************************************
 * \file DrvStm.c
 * \brief STM0 1ms tick driver
 *
 * - STM0 Comparator0 → 1kHz (1ms) 주기 인터럽트
 * - ISR 에서 g_1ms_counter 만 증가 (플래그 처리는 Scheduler 가 담당)
 *********************************************************************************************************************/
#include "DrvStm.h"
#include "IfxStm.h"

/******************************************************************************/
/*                           Module Variables                                 */
/******************************************************************************/
static IfxStm_CompareConfig s_stmCfg;
volatile uint32             g_1ms_counter = 0u;

/******************************************************************************/
/*                           ISR (1ms tick)                                   */
/******************************************************************************/
IFX_INTERRUPT(stm0Tick_ISR, 0, DRVSTM_ISR_PRIORITY)
{
    IfxStm_clearCompareFlag(&MODULE_STM0, s_stmCfg.comparator);
    IfxStm_increaseCompare(&MODULE_STM0, s_stmCfg.comparator, s_stmCfg.ticks);
    g_1ms_counter++;
}

/******************************************************************************/
/*                           Functions                                        */
/******************************************************************************/
void DrvStm_Init(void)
{
    IfxStm_initCompareConfig(&s_stmCfg);
    s_stmCfg.triggerPriority = DRVSTM_ISR_PRIORITY;
    s_stmCfg.typeOfService   = IfxSrc_Tos_cpu0;
    s_stmCfg.ticks           = (uint32)IfxStm_getTicksFromMilliseconds(&MODULE_STM0, DRVSTM_TICK_MS);
    IfxStm_initCompare(&MODULE_STM0, &s_stmCfg);
}
