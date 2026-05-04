/**********************************************************************************************************************
 * \file DrvDio.h
 * \brief Digital I/O driver - LED and general-purpose GPIO control
 *********************************************************************************************************************/
#ifndef DRVDIO_H
#define DRVDIO_H

#include "Ifx_Types.h"

/******************************************************************************/
/*                           Function Prototypes                              */
/******************************************************************************/

/** \brief Initialize all digital I/O pins (LED, etc.) */
void DrvDio_Init(void);

/** \brief Toggle LED0 (P13.0, active-low) */
void DrvDio_ToggleLed0(void);

/** \brief Set LED0 ON */
void DrvDio_SetLed0On(void);

/** \brief Set LED0 OFF */
void DrvDio_SetLed0Off(void);

#endif /* DRVDIO_H */
