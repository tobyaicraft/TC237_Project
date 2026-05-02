/**********************************************************************************************************************
 * \file DrvAdc.c
 * \brief VADC Group0 Scan Source driver
 *
 * - Group0, Ch0(P40.0), Ch1(P40.1), Ch2(P40.2)
 * - Scan source, autoscan OFF (software triggered per 10ms call)
 * - 12-bit resolution, dedicated result register per channel
 *********************************************************************************************************************/
#include "DrvAdc.h"
#include "Vadc/Adc/IfxVadc_Adc.h"

/******************************************************************************/
/*                           Module Variables                                 */
/******************************************************************************/
static IfxVadc_Adc         s_vadc;
static IfxVadc_Adc_Group   s_adcGroup0;
static IfxVadc_Adc_Channel s_adcChannel[DRVADC_NUM_CHANNELS];
static uint16              s_adcResult[DRVADC_NUM_CHANNELS];

/******************************************************************************/
/*                           Functions                                        */
/******************************************************************************/

void DrvAdc_Init(void)
{
    uint32 chnIx;

    /* ---- Module ---- */
    IfxVadc_Adc_Config adcConfig;
    IfxVadc_Adc_initModuleConfig(&adcConfig, &MODULE_VADC);
    IfxVadc_Adc_initModule(&s_vadc, &adcConfig);

    /* ---- Group0 (Scan source) ---- */
    IfxVadc_Adc_GroupConfig grpConfig;
    IfxVadc_Adc_initGroupConfig(&grpConfig, &s_vadc);

    grpConfig.groupId = IfxVadc_GroupId_0;
    grpConfig.master  = IfxVadc_GroupId_0;

    grpConfig.arbiter.requestSlotScanEnabled        = TRUE;
    grpConfig.scanRequest.triggerConfig.gatingMode   = IfxVadc_GatingMode_always;
    grpConfig.scanRequest.autoscanEnabled            = FALSE;   /* manual trigger */

    IfxVadc_Adc_initGroup(&s_adcGroup0, &grpConfig);

    /* ---- Channels Ch0, Ch1, Ch2 ---- */
    uint32 scanMask = 0u;

    for (chnIx = 0u; chnIx < DRVADC_NUM_CHANNELS; chnIx++)
    {
        IfxVadc_Adc_ChannelConfig chConfig;
        IfxVadc_Adc_initChannelConfig(&chConfig, &s_adcGroup0);

        chConfig.channelId      = (IfxVadc_ChannelId)chnIx;
        chConfig.resultRegister = (IfxVadc_ChannelResult)chnIx;

        IfxVadc_Adc_initChannel(&s_adcChannel[chnIx], &chConfig);

        scanMask |= (1u << chnIx);
    }

    IfxVadc_Adc_setScan(&s_adcGroup0, scanMask, scanMask);

    /* Clear result buffer */
    for (chnIx = 0u; chnIx < DRVADC_NUM_CHANNELS; chnIx++)
    {
        s_adcResult[chnIx] = 0u;
    }
}

void DrvAdc_Run(void)
{
    uint32 chnIx;
    Ifx_VADC_RES convResult;

    /* Trigger one scan round */
    IfxVadc_Adc_startScan(&s_adcGroup0);

    /* Wait for each channel's valid result and store */
    for (chnIx = 0u; chnIx < DRVADC_NUM_CHANNELS; chnIx++)
    {
        do {
            convResult = IfxVadc_Adc_getResult(&s_adcChannel[chnIx]);
        } while (!convResult.B.VF);

        s_adcResult[chnIx] = (uint16)convResult.B.RESULT;
    }
}

uint16 DrvAdc_GetResult(uint32 chIndex)
{
    if (chIndex < DRVADC_NUM_CHANNELS)
    {
        return s_adcResult[chIndex];
    }
    return 0u;
}
