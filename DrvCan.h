/**********************************************************************************************************************
 * \file DrvCan.h
 * \brief CAN driver (placeholder)
 *********************************************************************************************************************/
#ifndef DRVCAN_H
#define DRVCAN_H

#include "Ifx_Types.h"

/******************************************************************************/
/*                           Function Prototypes                              */
/******************************************************************************/

/** \brief Initialize CAN module */
void DrvCan_Init(void);

/** \brief CAN periodic processing. Call from scheduler task. */
void DrvCan_Run(void);

#endif /* DRVCAN_H */
