/**********************************************************************************************************************
 * \file DrvAdc.h
 * \brief VADC Group0 Scan Source driver - Ch0, Ch1, Ch2 (AN0~AN2, P40.0~P40.2)
 *********************************************************************************************************************/
#ifndef DRVADC_H
#define DRVADC_H

#include "Ifx_Types.h"

/******************************************************************************/
/*                           Configuration                                    */
/******************************************************************************/
#define DRVADC_NUM_CHANNELS     3u      /* Group0 Ch0, Ch1, Ch2 */

/******************************************************************************/
/*                           Function Prototypes                              */
/******************************************************************************/

/** \brief Initialize VADC module, Group0 (Scan), and channels 0-2 */
void DrvAdc_Init(void);

/** \brief Trigger one scan round and store results. Call every 10ms. */
void DrvAdc_Run(void);

/** \brief Get the latest 12-bit result (0~4095) for the given channel index (0,1,2) */
uint16 DrvAdc_GetResult(uint32 chIndex);

#endif /* DRVADC_H */
