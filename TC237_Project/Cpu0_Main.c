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
#include "DrvFlash.h"
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

    /* TODO: Flash 캘리브레이션 — SPI/IMU 테스트 중 임시 비활성화 */
#if 0
    if (DrvFlash_LoadCalibration())
    {
        DrvUart_SendString("[CAL] Flash data loaded.\r\n");
    }
    else
    {
        DrvUart_SendString("[CAL] No saved data, using defaults.\r\n");
    }
#endif

    DrvUart_SendString("=== TC237 IMU 3D Viewer ===\r\n");

    while (1)
    {
        Scheduler_Run();
    }
}
