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
#include "DrvCan.h"
#include "DrvSpi.h"
#include "DrvMpu9250.h"
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
    DrvCan_Init();
    DrvSpi_Init();

    /* 글로벌 인터럽트 Enable — SPI ISR이 동작해야 MPU-9250 통신 가능 */
    IfxCpu_enableInterrupts();

    /* MPU-9250 초기화 (인터럽트 활성화 후 수행) */
    DrvMpu9250_Init();

    DrvUart_SendString("=== TC237 IMU + CAN Sender ===\r\n");
    DrvUart_SendString("MPU-9250 -> UART (Roll/Pitch/Yaw)\r\n");
    DrvUart_SendString("AN0 ADC -> CAN ID=0x100, 500kbps\r\n");
    DrvUart_SendString("Ready.\r\n\r\n");

    while (1)
    {
        Scheduler_Run();
    }
}
