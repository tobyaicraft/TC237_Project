/**********************************************************************************************************************
 * \file DrvGtmTimer.c
 * \brief GTM TOM0 Ch1 기반 1ms 스케줄러 tick 드라이버
 *
 * Clock chain:
 *   GTM GCLK  = 100 MHz (fSOURCE 200MHz / GTMDIV 2)
 *   FXCLK1    = GCLK / 16 = 6,250,000 Hz
 *   period    = 6,250 ticks  →  1ms (오차 없음)
 *
 * GTM hard-suspend 설정으로 디버거 break 시 카운터 자동 정지
 *********************************************************************************************************************/
#include "DrvGtmTimer.h"
#include "Gtm/Tom/Pwm/IfxGtm_Tom_Pwm.h"

/******************************************************************************/
/*                           Configuration                                    */
/******************************************************************************/
#define TIMER_PERIOD_TICKS  6250u    /* FXCLK1(6.25MHz) / 6250 = 1ms */

/******************************************************************************/
/*                           Module Variables                                 */
/******************************************************************************/
volatile uint32 g_1ms_counter = 0u;

static IfxGtm_Tom_Pwm_Driver s_timer;

/******************************************************************************/
/*                           ISR (1ms tick)                                   */
/******************************************************************************/
IFX_INTERRUPT(gtmTimer_ISR, 0, DRVGTMTIMER_ISR_PRIORITY)
{
    IfxGtm_Tom_Ch_clearOneNotification(&MODULE_GTM.TOM[IfxGtm_Tom_0], IfxGtm_Tom_Ch_1);
    g_1ms_counter++;
}

/******************************************************************************/
/*                           Functions                                        */
/******************************************************************************/
void DrvGtmTimer_Init(void)
{
    /* GTM 모듈 enable/GCLK 설정은 DrvPwm_Init()에서 완료됨 — 중복 호출 금지 */
    Ifx_GTM *gtm = &MODULE_GTM;

    IfxGtm_Tom_Pwm_Config cfg;
    IfxGtm_Tom_Pwm_initConfig(&cfg, gtm);

    cfg.tom                      = IfxGtm_Tom_0;
    cfg.tomChannel               = IfxGtm_Tom_Ch_1;                   /* Ch0 = PWM 출력 */
    cfg.clock                    = IfxGtm_Tom_Ch_ClkSrc_cmuFxclk1;   /* 200MHz / 16 = 12.5MHz */
    cfg.period                   = TIMER_PERIOD_TICKS;
    cfg.dutyCycle                = TIMER_PERIOD_TICKS / 2u;
    cfg.synchronousUpdateEnabled = TRUE;   /* shadow 레지스터 사용 — FUPD 덮어쓰기 방지 */
    cfg.pin.outputPin            = NULL_PTR;
    cfg.interrupt.ccu0Enabled    = TRUE;
    cfg.interrupt.isrProvider    = IfxSrc_Tos_cpu0;
    cfg.interrupt.isrPriority    = DRVGTMTIMER_ISR_PRIORITY;

    IfxGtm_Tom_Pwm_init(&s_timer, &cfg);

    /* 디버거 break 시 GTM 전체(PWM 포함) 정지 */
    //IfxGtm_setSuspendMode(gtm, IfxGtm_SuspendMode_hard);
}
