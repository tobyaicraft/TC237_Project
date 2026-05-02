/**********************************************************************************************************************
 * \file DrvDio.c
 * \brief Digital I/O driver
 *
 * LED0 : P13.0 (active-low, Application Kit TC2X7)
 *********************************************************************************************************************/
#include "DrvDio.h"
#include "IfxPort.h"

/******************************************************************************/
/*                           Macros                                           */
/******************************************************************************/
#define LED0_PORT   &MODULE_P13
#define LED0_PIN    0u

/******************************************************************************/
/*                           Functions                                        */
/******************************************************************************/

void DrvDio_Init(void)
{
    IfxPort_setPinMode(LED0_PORT, LED0_PIN, IfxPort_Mode_outputPushPullGeneral);
    IfxPort_setPinHigh(LED0_PORT, LED0_PIN);    /* LED OFF (active-low) */
}

void DrvDio_ToggleLed0(void)
{
    IfxPort_togglePin(LED0_PORT, LED0_PIN);
}

void DrvDio_SetLed0On(void)
{
    IfxPort_setPinLow(LED0_PORT, LED0_PIN);     /* active-low */
}

void DrvDio_SetLed0Off(void)
{
    IfxPort_setPinHigh(LED0_PORT, LED0_PIN);    /* active-low */
}
