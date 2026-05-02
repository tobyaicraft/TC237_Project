/**********************************************************************************************************************
 * \file DrvTim.c
 * \brief GTM TIM0 Ch0 PWM 입력 캡처 드라이버
 *
 * Clock chain:
 *   GTM GCLK  = 100 MHz  (DrvPwm_Init() 에서 설정 완료)
 *   CMU_CLK0  = GCLK / 1 = 100 MHz  (본 드라이버에서 추가 활성화)
 *
 * 입력 핀: P00.0 (TIN9)
 *   - P00.9(TOM PWM 출력)에서 점퍼 와이어로 P00.0 에 연결
 *   - 전용 입력 핀이므로 포트 충돌 없음, 일반 inputPinMode 사용
 *********************************************************************************************************************/
#include "DrvTim.h"
#include "Compilers.h"
#include "Gtm/Tim/In/IfxGtm_Tim_In.h"
#include "Gtm/Std/IfxGtm_Cmu.h"
#include "_Impl/IfxGtm_cfg.h"
#include "_PinMap/IfxGtm_PinMap.h"

/******************************************************************************/
/*                           Module Variables                                 */
/******************************************************************************/
static IfxGtm_Tim_In s_tim;

/******************************************************************************/
/*                           ISR                                              */
/******************************************************************************/
/* 100Hz PWM → 10ms 마다 발생 */
IFX_INTERRUPT(DrvTim_ISR, 0, DRVTIM_ISR_PRIORITY)
{
    IfxGtm_Tim_In_onIsr(&s_tim);
}

/******************************************************************************/
/*                           Functions                                        */
/******************************************************************************/
void DrvTim_Init(void)
{
    Ifx_GTM *gtm = &MODULE_GTM;

    /* CMU_CLK0 활성화 (FXCLK 는 DrvPwm_Init() 에서 이미 활성화됨)
     * CLK_EN 레지스터: 0b00 = no change, 0b01 = disable, 0b10 = enable
     * IFXGTM_CMU_CLKEN_CLK0 = 0x00000002 → EN_CLK0 = 0b10, 나머지 필드 = 0b00(변경없음) */
    float32 fMod = IfxGtm_Cmu_getModuleFrequency(gtm);       /* 100 MHz */
    IfxGtm_Cmu_setClkFrequency(gtm, IfxGtm_Cmu_Clk_0, fMod); /* CLK0 = 100 MHz, CLK_CNT=0 */
    IfxGtm_Cmu_enableClocks(gtm, IFXGTM_CMU_CLKEN_CLK0);      /* FXCLK 영향 없음 */

    /* TIM 설정 */
    IfxGtm_Tim_In_Config cfg;
    IfxGtm_Tim_In_initConfig(&cfg, gtm);

    cfg.timIndex              = IfxGtm_Tim_0;
    cfg.channelIndex          = IfxGtm_Tim_Ch_0;          /* TOM0_Ch0 와 동일 채널 번호 */
    cfg.irqMode               = IfxGtm_IrqMode_pulseNotify;
    cfg.isrProvider           = IfxSrc_Tos_cpu0;
    cfg.isrPriority           = DRVTIM_ISR_PRIORITY;      /* 5 — DrvGtmTimer(11) 와 다른 우선순위 */

    cfg.capture.clock         = IfxGtm_Cmu_Clk_0;         /* 100 MHz */
    cfg.capture.irqOnNewVal   = TRUE;
    cfg.capture.mode          = Ifx_Pwm_Mode_leftAligned;  /* Rising edge = 주기 기준 */

    /* P00.0 핀: P00.9(TOM 출력)에서 점퍼 와이어로 연결된 전용 입력 */
    cfg.filter.inputPin       = &IfxGtm_TIM0_0_TIN9_P00_0_IN;
    cfg.filter.inputPinMode   = IfxPort_InputMode_noPullDevice;

    cfg.mode                  = IfxGtm_Tim_Mode_pwmMeasurement;

    IfxGtm_Tim_In_init(&s_tim, &cfg);
}

float32 DrvTim_GetDutyPercent(void)
{
    boolean coherent;
    return IfxGtm_Tim_In_getDutyPercent(&s_tim, &coherent);
}

float32 DrvTim_GetPeriodSec(void)
{
    return IfxGtm_Tim_In_getPeriodSecond(&s_tim);
}
