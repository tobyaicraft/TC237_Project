/**********************************************************************************************************************
 * \file DrvTim.h
 * \brief GTM TIM0 Ch0 PWM 입력 캡처 드라이버
 *
 * Input  : P00.9 (TIN18) — TOM0_Ch0 출력과 동일 핀, INSEL 라우팅만 설정
 * Mode   : PWM Measurement (Left-Aligned)
 * Clock  : CMU_CLK0 = GCLK(100MHz) / 1 = 100 MHz
 *
 * 측정 대상: DrvPwm 의 100Hz PWM 신호
 *   - Period  ≈ 1,000,000 ticks @ 100MHz → 10ms (100Hz)
 *   - Duty    = g_pwmDuty (%) 에 따라 변동
 *********************************************************************************************************************/
#ifndef DRVTIM_H
#define DRVTIM_H

#include "Ifx_Types.h"

/* 주의: 괄호 없이 정의 — 인터럽트 벡터 섹션 이름 생성에 사용됨 */
#define DRVTIM_ISR_PRIORITY     5

void    DrvTim_Init(void);
float32 DrvTim_GetDutyPercent(void);   /* 0.0 ~ 100.0 % */
float32 DrvTim_GetPeriodSec(void);     /* 단위: 초 */

#endif /* DRVTIM_H */
