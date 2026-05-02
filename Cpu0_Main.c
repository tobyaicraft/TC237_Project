/**********************************************************************************************************************
 * \file Cpu0_Main.c
 * \brief TC237 Application Kit - 1ms STM tick 기반 협력형 스케줄러
 *********************************************************************************************************************/
#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "DrvIntc.h"
#include "DrvDio.h"
#include "DrvAdc.h"
#include "DrvPwm.h"
#include "DrvGtmTimer.h"
#include "DrvTim.h"
#include "DrvUart.h"
#include "Scheduler.h"

/*-----------------------------------------------------------------------------------------------*/
/* main                                                                                          */
/*-----------------------------------------------------------------------------------------------*/
void core0_main(void)
{
    /* Driver 초기화 */
    DrvIntc_Init();
    DrvDio_Init();
    DrvAdc_Init();
    DrvPwm_Init();
    DrvGtmTimer_Init();
    DrvTim_Init();
    DrvUart_Init();

    /* 글로벌 인터럽트 Enable — 모든 초기화 완료 후 마지막 수행 */
    IfxCpu_enableInterrupts();

    DrvUart_SendString("=== TC237 UART Echo Test ===\r\n");
    DrvUart_SendString("Send any character -> TC237 echoes it back\r\n");
    DrvUart_SendString("Ready.\r\n\r\n");

    while (1)
    {
        Scheduler_Run();
    }
}
