/**********************************************************************************************************************
 * \file AppTask.h
 * \brief Application task functions called by the scheduler (1ms / 10ms / 100ms)
 *********************************************************************************************************************/
#ifndef APPTASK_H
#define APPTASK_H

#include "Ifx_Types.h"

/** \brief Initialize all application-level peripherals (called once before scheduler starts) */
void AppTask_Init(void);

/** \brief 1ms periodic task */
void AppTask_1ms(void);

/** \brief 10ms periodic task */
void AppTask_10ms(void);

/** \brief 100ms periodic task */
void AppTask_100ms(void);

#endif /* APPTASK_H */
