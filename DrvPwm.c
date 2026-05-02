/**********************************************************************************************************************
 * \file DrvPwm.c
 * \brief GTM TOM0 Ch0 PWM driver
 *
 * Clock chain:
 *   GTM GCLK = 200 MHz (module clock)
 *   FXCLK2   = GCLK / 256 = 781,250 Hz
 *   period   = 7812 ticks  =>  f_PWM = 781,250 / 7812 = 100.006 Hz
 *
 * Output pin : P00.9  (IfxGtm_TOM0_0_TOUT18_P00_9_OUT)
 * Duty update: shadow register + TGC trigger (glitch-free, takes effect next period)
 *********************************************************************************************************************/
#include "DrvPwm.h"
#include "Gtm/Tom/Pwm/IfxGtm_Tom_Pwm.h"

/******************************************************************************/
/*                           Configuration                                    */
/******************************************************************************/
/* FXCLK2 = 100 MHz / 256 = 390,625 Hz.  390,625 / 3906 = 100.006 Hz */
#define PWM_PERIOD_TICKS    3906u
#define PWM_INITIAL_DUTY    50u     /* percent */

/******************************************************************************/
/*                           Module Variables                                 */
/******************************************************************************/
uint8 g_pwmDuty = PWM_INITIAL_DUTY;

static IfxGtm_Tom_Pwm_Driver s_pwm;

/******************************************************************************/
/*                           Functions                                        */
/******************************************************************************/

void DrvPwm_Init(void)
{
    Ifx_GTM *gtm  = &MODULE_GTM;
    float32  fMod = IfxGtm_Cmu_getModuleFrequency(gtm);

    IfxGtm_enable(gtm);
    IfxGtm_Cmu_setGclkFrequency(gtm, fMod);
    IfxGtm_Cmu_enableClocks(gtm, IFXGTM_CMU_CLKEN_FXCLK);

    IfxGtm_Tom_Pwm_Config cfg;
    IfxGtm_Tom_Pwm_initConfig(&cfg, gtm);

    cfg.tom                      = IfxGtm_Tom_0;
    cfg.tomChannel               = IfxGtm_Tom_Ch_0;
    cfg.clock                    = IfxGtm_Tom_Ch_ClkSrc_cmuFxclk2;  /* div 256 */
    cfg.period                   = PWM_PERIOD_TICKS;
    cfg.dutyCycle                = (uint16)(PWM_INITIAL_DUTY * PWM_PERIOD_TICKS / 100u);
    cfg.synchronousUpdateEnabled = TRUE;
    cfg.pin.outputPin            = &IfxGtm_TOM0_0_TOUT18_P00_9_OUT;
    cfg.pin.outputMode           = IfxPort_OutputMode_pushPull;
    cfg.pin.padDriver            = IfxPort_PadDriver_cmosAutomotiveSpeed1;

    IfxGtm_Tom_Pwm_init(&s_pwm, &cfg);
}

void DrvPwm_SetDuty(uint8 duty_percent)
{
    if (duty_percent > 100u)
    {
        duty_percent = 100u;
    }

    uint16 duty_ticks = (uint16)((uint32)duty_percent * PWM_PERIOD_TICKS / 100u);

    /* Write to shadow register; transferred to active register at next period boundary */
    IfxGtm_Tom_Ch_setCompareOneShadow(s_pwm.tom, s_pwm.tomChannel, duty_ticks);
    IfxGtm_Tom_Tgc_trigger(s_pwm.tgc[0]);
}
