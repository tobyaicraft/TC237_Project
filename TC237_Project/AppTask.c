/**********************************************************************************************************************
 * \file AppTask.c
 * \brief Application task implementations
 *
 * Task_1ms   : UART echo (수신 바이트를 그대로 반환)
 * Task_10ms  : ADC scan, PWM duty update, TIM 측정값 읽기
 * Task_100ms : LED 토글
 *********************************************************************************************************************/
#include "AppTask.h"
#include "DrvDio.h"
#include "DrvAdc.h"
#include "DrvPwm.h"
#include "DrvTim.h"
#include "DrvUart.h"
#include "DrvCan.h"
#include "DrvMpu9250.h"
#include "DrvSpi.h"

float32 g_timDutyPercent = 0.0f;
float32 g_timPeriodSec   = 0.0f;

/******************************************************************************/
/*                           1ms Task                                         */
/******************************************************************************/
void AppTask_1ms(void)
{
    /* UART echo: PC → TC237 수신 → PC 재전송
     * 예) PC에서 'a' 전송 → TC237이 'a' 회신 */
    uint8 rxByte;
    while (DrvUart_ReceiveByte(&rxByte))
    {
        DrvUart_SendByte(rxByte);
    }
}

/******************************************************************************/
/*                           10ms Task                                        */
/******************************************************************************/
void AppTask_10ms(void)
{
    DrvAdc_Run();
    DrvPwm_SetDuty(g_pwmDuty);

    g_timDutyPercent = DrvTim_GetDutyPercent();
    g_timPeriodSec   = DrvTim_GetPeriodSec();

    /* AN0 ADC 값 CAN 송신 → TC237_Project_CAN 보드 (100 Hz) */
    DrvCan_SendUint16(DrvAdc_GetResult(0));

    /* MPU-9250 센서 읽기 + Roll/Pitch/Yaw UART 전송 (100 Hz) */
    DrvMpu9250_ReadSensors();
    DrvMpu9250_SendUart();
}

/******************************************************************************/
/*                           100ms Task                                       */
/******************************************************************************/
void AppTask_100ms(void)
{
    DrvDio_ToggleLed0();

    /* Logic Analyzer 캡처용: WHO_AM_I(0x75) 반복 읽기 (100ms 주기)
     * 캡처 완료 후 이 코드를 삭제할 것 */
    (void)DrvSpi_ReadReg(0x75u);
}
