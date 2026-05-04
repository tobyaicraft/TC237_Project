/**********************************************************************************************************************
 * \file AppTask.h
 * \brief Application task functions called by the scheduler (1ms / 10ms / 100ms)
 *********************************************************************************************************************/
#ifndef APPTASK_H
#define APPTASK_H

#include "Ifx_Types.h"

extern float32 g_timDutyPercent;   /* TIM 측정 Duty [%] */
extern float32 g_timPeriodSec;     /* TIM 측정 주기 [s] */

/** \brief 1ms periodic task */
void AppTask_1ms(void);

/** \brief 10ms periodic task */
void AppTask_10ms(void);

/** \brief 100ms periodic task */
void AppTask_100ms(void);

#endif /* APPTASK_H */
