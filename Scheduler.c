/**********************************************************************************************************************
 * \file Scheduler.c
 * \brief 1ms STM tick 기반 협력형 스케줄러
 *
 * - g_1ms_counter (DrvStm) 를 modulo 로 분주하여 주기별 태스크 호출
 * - 1ms / 10ms / 100ms 태스크 지원
 *********************************************************************************************************************/
#include "Scheduler.h"
#include "DrvStm.h"
#include "AppTask.h"

/******************************************************************************/
/*                           Functions                                        */
/******************************************************************************/
void Scheduler_Run(void)
{
    static uint32 last_counter = 0u;
    uint32        now = g_1ms_counter;

    if (now == last_counter)
    {
        return;
    }
    last_counter = now;

    if ((now %   1U) == 0U) AppTask_1ms();
    if ((now %  10U) == 0U) AppTask_10ms();
    if ((now % 100U) == 0U) AppTask_100ms();
}
