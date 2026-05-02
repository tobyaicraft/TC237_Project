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

    /* AN0 ADC 값 UART 전송 → Python 오실로스코프 */
    DrvUart_SendUint16(DrvAdc_GetResult(0));
}

/******************************************************************************/
/*                           100ms Task                                       */
/******************************************************************************/
void AppTask_100ms(void)
{
    DrvDio_ToggleLed0();
}
