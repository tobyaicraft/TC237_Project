/**********************************************************************************************************************
 * \file DrvPwm.h
 * \brief GTM TOM0 Ch0 PWM driver
 *
 * Output : P00.9 (GTM TOM0_0 TOUT18)
 * Freq   : ~100 Hz  (FXCLK2 = 200MHz/256 = 781,250Hz, period = 7812 ticks)
 * Duty   : 0-100% via DrvPwm_SetDuty()
 *********************************************************************************************************************/
#ifndef DRVPWM_H
#define DRVPWM_H

#include "Ifx_Types.h"

extern uint8 g_pwmDuty;   /* 0 ~ 100 (%), 외부에서 직접 변경 가능 */

void DrvPwm_Init(void);
void DrvPwm_SetDuty(uint8 duty_percent);  /* 0 ~ 100 */

#endif /* DRVPWM_H */
