/**********************************************************************************************************************
 * \file AppTask.c
 * \brief Application task implementations
 *
 * Task_1ms   : UART 명령 수신 및 파싱 (D=xx, SAVE, LOAD, CAL?)
 * Task_10ms  : ADC scan, PWM duty update, TIM 측정값 읽기, CAN 송신
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
#include "DrvFlash.h"

float32 g_timDutyPercent = 0.0f;
float32 g_timPeriodSec   = 0.0f;

/******************************************************************************/
/*                           UART 명령 파서                                   */
/******************************************************************************/
#define CMD_BUF_SIZE    32u

static uint8  s_cmdBuf[CMD_BUF_SIZE];
static uint8  s_cmdIdx = 0u;

/* 간단한 ASCII 숫자 파싱 (최대 3자리) */
static uint8 parseUint8(const uint8 *str)
{
    uint16 val = 0u;
    while (*str >= '0' && *str <= '9')
    {
        val = val * 10u + (*str - '0');
        str++;
    }
    if (val > 100u) val = 100u;
    return (uint8)val;
}

/* Duty 값을 UART로 출력: "[CAL] Duty=xx\r\n" */
static void sendDutyResponse(const char *prefix)
{
    char buf[40];
    uint8 d = g_pwmDuty;
    char *p = buf;
    const char *s;

    /* prefix 복사 */
    for (s = prefix; *s != '\0'; s++) { *p++ = *s; }

    /* "Duty=" */
    *p++ = 'D'; *p++ = 'u'; *p++ = 't'; *p++ = 'y'; *p++ = '=';

    /* 숫자 → ASCII (최대 3자리) */
    if (d >= 100u) { *p++ = '1'; *p++ = '0'; *p++ = '0'; }
    else
    {
        if (d >= 10u) { *p++ = '0' + (d / 10u); }
        *p++ = '0' + (d % 10u);
    }

    *p++ = '\r'; *p++ = '\n'; *p = '\0';
    DrvUart_SendString(buf);
}

/* 수신된 명령 라인 처리 */
static void processCommand(void)
{
    /* 줄 끝 정리 (\r, \n 제거) */
    uint8 len = s_cmdIdx;
    while (len > 0u && (s_cmdBuf[len - 1u] == '\r' || s_cmdBuf[len - 1u] == '\n'))
    {
        len--;
    }
    s_cmdBuf[len] = '\0';

    if (len == 0u) return;

    /* "D=xx" — PWM Duty 변경 */
    if (len >= 3u && s_cmdBuf[0] == 'D' && s_cmdBuf[1] == '=')
    {
        g_pwmDuty = parseUint8(&s_cmdBuf[2]);
        sendDutyResponse("[CAL] ");
    }
    /* "SAVE" — Flash 저장 */
    else if (len == 4u && s_cmdBuf[0] == 'S' && s_cmdBuf[1] == 'A'
                       && s_cmdBuf[2] == 'V' && s_cmdBuf[3] == 'E')
    {
        if (DrvFlash_SaveCalibration())
        {
            sendDutyResponse("[CAL] Saved! ");
        }
        else
        {
            DrvUart_SendString("[CAL] Save FAILED!\r\n");
        }
    }
    /* "LOAD" — Flash 로드 */
    else if (len == 4u && s_cmdBuf[0] == 'L' && s_cmdBuf[1] == 'O'
                       && s_cmdBuf[2] == 'A' && s_cmdBuf[3] == 'D')
    {
        if (DrvFlash_LoadCalibration())
        {
            sendDutyResponse("[CAL] Loaded! ");
        }
        else
        {
            DrvUart_SendString("[CAL] No valid data in Flash\r\n");
        }
    }
    /* "CAL?" — 현재 값 조회 */
    else if (len == 4u && s_cmdBuf[0] == 'C' && s_cmdBuf[1] == 'A'
                       && s_cmdBuf[2] == 'L' && s_cmdBuf[3] == '?')
    {
        sendDutyResponse("[CAL] ");
    }
    else
    {
        DrvUart_SendString("[ERR] Unknown: ");
        DrvUart_SendString((const char *)s_cmdBuf);
        DrvUart_SendString("\r\n");
    }
}

/******************************************************************************/
/*                           1ms Task                                         */
/******************************************************************************/
void AppTask_1ms(void)
{
    /* UART 수신 → 라인 버퍼에 축적, '\n' 수신 시 명령 처리 */
    uint8 rxByte;
    while (DrvUart_ReceiveByte(&rxByte))
    {
        if (rxByte == '\n' || rxByte == '\r')
        {
            if (s_cmdIdx > 0u)
            {
                processCommand();
                s_cmdIdx = 0u;
            }
        }
        else
        {
            if (s_cmdIdx < (CMD_BUF_SIZE - 1u))
            {
                s_cmdBuf[s_cmdIdx] = rxByte;
                s_cmdIdx++;
            }
        }
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

    /* MPU-9250 센서 읽기 (UART 전송은 제거 — UART를 명령 채널로 사용) */
    DrvMpu9250_ReadSensors();
}

/******************************************************************************/
/*                           100ms Task                                       */
/******************************************************************************/
void AppTask_100ms(void)
{
    DrvDio_ToggleLed0();
}
