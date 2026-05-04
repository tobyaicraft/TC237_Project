/**********************************************************************************************************************
 * \file DrvMpu9250.c
 * \brief MPU-9250 9축 IMU 센서 드라이버
 *
 * - QSPI3(DrvSpi)를 통해 MPU-9250과 SPI 통신
 * - 가속도계: ±4g, 자이로: ±500°/s, DLPF: 41Hz, 샘플레이트: 100Hz
 * - Complementary Filter (α=0.96)로 Roll/Pitch/Yaw 계산
 * - UART로 "R:±xxx.x,P:±xxx.x,Y:±xxx.x\r\n" 포맷 전송
 *********************************************************************************************************************/
#include "DrvMpu9250.h"
#include "DrvSpi.h"
#include "DrvUart.h"
#include <math.h>

/******************************************************************************/
/*                           MPU-9250 레지스터 정의                           */
/******************************************************************************/
#define MPU_REG_SMPLRT_DIV      0x19u
#define MPU_REG_CONFIG          0x1Au
#define MPU_REG_GYRO_CONFIG     0x1Bu
#define MPU_REG_ACCEL_CONFIG    0x1Cu
#define MPU_REG_ACCEL_CONFIG2   0x1Du
#define MPU_REG_ACCEL_XOUT_H   0x3Bu   /* 가속도+온도+자이로 14바이트 연속 */
#define MPU_REG_USER_CTRL       0x6Au
#define MPU_REG_PWR_MGMT_1     0x6Bu
#define MPU_REG_PWR_MGMT_2     0x6Cu
#define MPU_REG_WHO_AM_I       0x75u

#define MPU_WHO_AM_I_MPU9250   0x71u
#define MPU_WHO_AM_I_MPU6500   0x70u

/* 센서 감도 */
#define ACCEL_SENSITIVITY      16384.0f  /* ±2g → LSB/g (AC 레지스터 기본값) */
#define GYRO_SENSITIVITY       65.5f     /* ±500°/s → LSB/(°/s) */

/* Complementary Filter */
#define FILTER_ALPHA           0.96f
#define DT                     0.01f     /* 10ms (100Hz) */

/* Yaw 드리프트 보정 */
#define YAW_DEADZONE           1.5f      /* ±1.5°/s 이하 무시 (노이즈 제거) */
#define YAW_DECAY              0.998f    /* 정지 시 Yaw 서서히 감쇠 */

/* 수학 상수 */
#define RAD_TO_DEG             57.2957795f   /* 180/π */

/******************************************************************************/
/*                           Module Variables                                 */
/******************************************************************************/
static boolean s_initialized = FALSE;

/* Raw 센서 데이터 (16-bit signed) */
static sint16  s_accelRaw[3];   /* [0]=X, [1]=Y, [2]=Z */
static sint16  s_gyroRaw[3];    /* [0]=X, [1]=Y, [2]=Z */

/* 필터링된 자세 각도 (degrees) */
static float32 s_roll  = 0.0f;
static float32 s_pitch = 0.0f;
static float32 s_yaw   = 0.0f;

/* 시작 시 오프셋 보정값 */
static float32 s_rollOffset  = 0.0f;
static float32 s_pitchOffset = 0.0f;
static float32 s_gyroOffsetRoll  = 0.0f;   /* GY 바이어스 (°/s) */
static float32 s_gyroOffsetPitch = 0.0f;   /* GX 바이어스 (°/s) */
static float32 s_gyroOffsetYaw   = 0.0f;   /* GZ 바이어스 (°/s) */
static boolean s_calibrated  = FALSE;
static uint16  s_calibCount  = 0u;

/******************************************************************************/
/*                           내부 함수                                        */
/******************************************************************************/

/*---------------------------------------------------------------------------*/
/* 간단한 딜레이 (초기화용, 정밀도 불필요)                                   */
/*---------------------------------------------------------------------------*/
static void delay_ms(uint32 ms)
{
    volatile uint32 count;
    uint32 i;
    for (i = 0u; i < ms; i++)
    {
        for (count = 0u; count < 20000u; count++)
        {
            /* 약 1ms @200MHz — 대략적인 값 */
        }
    }
}

/*---------------------------------------------------------------------------*/
/* 14바이트 센서 데이터 파싱 (ACCEL[6] + TEMP[2] + GYRO[6])                  */
/*---------------------------------------------------------------------------*/
static void parseSensorData(const uint8 *buf)
{
    /* 가속도계: Big Endian (H, L) */
    s_accelRaw[0] = (sint16)((uint16)buf[0]  << 8u) | buf[1];   /* AX */
    s_accelRaw[1] = (sint16)((uint16)buf[2]  << 8u) | buf[3];   /* AY */
    s_accelRaw[2] = (sint16)((uint16)buf[4]  << 8u) | buf[5];   /* AZ */

    /* buf[6], buf[7] = 온도 (무시) */

    /* 자이로: Big Endian (H, L) */
    s_gyroRaw[0]  = (sint16)((uint16)buf[8]  << 8u) | buf[9];   /* GX */
    s_gyroRaw[1]  = (sint16)((uint16)buf[10] << 8u) | buf[11];  /* GY */
    s_gyroRaw[2]  = (sint16)((uint16)buf[12] << 8u) | buf[13];  /* GZ */
}

/*---------------------------------------------------------------------------*/
/* Complementary Filter로 Roll/Pitch/Yaw 갱신                                */
/*---------------------------------------------------------------------------*/
static void updateAttitude(void)
{
    /* 실측 축 매핑:
     *   Roll  → 가속도: AX(raw[0]), AZ(raw[2])  / 자이로: GY(raw[1])
     *   Pitch → 가속도: AY(raw[1]), AZ(raw[2])  / 자이로: GX(raw[0])
     *   Yaw   → 자이로: GZ(raw[2])                                    */
    float32 ax = (float32)s_accelRaw[0] / ACCEL_SENSITIVITY;
    float32 ay = (float32)s_accelRaw[1] / ACCEL_SENSITIVITY;
    float32 az = (float32)s_accelRaw[2] / ACCEL_SENSITIVITY;

    float32 rollRate  = -(float32)s_gyroRaw[1] / GYRO_SENSITIVITY;  /* GY → Roll  (반전) */
    float32 pitchRate = -(float32)s_gyroRaw[0] / GYRO_SENSITIVITY;  /* GX → Pitch (반전) */
    float32 yawRate   = (float32)s_gyroRaw[2] / GYRO_SENSITIVITY;  /* GZ → Yaw   */

    /* 캘리브레이션 구간: 100회(1초) 동안 정지 상태 평균 수집 */
    if (!s_calibrated)
    {
        float32 rollAcc  = atan2f(-ax, az) * RAD_TO_DEG;
        float32 pitchAcc = atan2f(ay, sqrtf(ax * ax + az * az)) * RAD_TO_DEG;

        s_rollOffset      += rollAcc;
        s_pitchOffset     += pitchAcc;
        s_gyroOffsetRoll  += rollRate;
        s_gyroOffsetPitch += pitchRate;
        s_gyroOffsetYaw   += yawRate;
        s_calibCount++;

        if (s_calibCount >= 100u)
        {
            s_rollOffset      /= 100.0f;
            s_pitchOffset     /= 100.0f;
            s_gyroOffsetRoll  /= 100.0f;
            s_gyroOffsetPitch /= 100.0f;
            s_gyroOffsetYaw   /= 100.0f;
            s_calibrated = TRUE;
            s_roll  = 0.0f;
            s_pitch = 0.0f;
            s_yaw   = 0.0f;
        }
        return;
    }

    /* 자이로 바이어스 보정 */
    rollRate  -= s_gyroOffsetRoll;
    pitchRate -= s_gyroOffsetPitch;
    yawRate   -= s_gyroOffsetYaw;

    /* 가속도계 기반 각도 (오프셋 보정 적용) */
    float32 rollAcc  = atan2f(-ax, az) * RAD_TO_DEG - s_rollOffset;
    float32 pitchAcc = atan2f(ay, sqrtf(ax * ax + az * az)) * RAD_TO_DEG - s_pitchOffset;

    /* Complementary Filter: Roll/Pitch */
    s_roll  = FILTER_ALPHA * (s_roll  + rollRate  * DT) + (1.0f - FILTER_ALPHA) * rollAcc;
    s_pitch = FILTER_ALPHA * (s_pitch + pitchRate * DT) + (1.0f - FILTER_ALPHA) * pitchAcc;

    /* Yaw: 데드존 + 감쇠로 드리프트 억제 */
    if (yawRate > YAW_DEADZONE || yawRate < -YAW_DEADZONE)
    {
        s_yaw += yawRate * DT;       /* 실제 회전 중 → 적분 */
    }
    else
    {
        s_yaw *= YAW_DECAY;          /* 정지 중 → 서서히 0으로 복귀 */
    }
}

/*---------------------------------------------------------------------------*/
/* float를 부호+정수3자리.소수1자리 문자열로 변환 (예: "+012.3")             */
/* buf는 최소 7바이트 필요                                                    */
/*---------------------------------------------------------------------------*/
static void floatToStr(float32 val, char *buf)
{
    sint32 intPart;
    uint32 absInt;
    uint32 fracPart;

    if (val < 0.0f)
    {
        buf[0] = '-';
        val = -val;
    }
    else
    {
        buf[0] = '+';
    }

    intPart  = (sint32)val;
    fracPart = (uint32)((val - (float32)intPart) * 10.0f + 0.5f);

    if (fracPart >= 10u)
    {
        fracPart = 0u;
        intPart++;
    }

    absInt = (uint32)intPart;
    if (absInt > 999u) absInt = 999u;

    buf[1] = (char)('0' + (absInt / 100u));
    buf[2] = (char)('0' + ((absInt / 10u) % 10u));
    buf[3] = (char)('0' + (absInt % 10u));
    buf[4] = '.';
    buf[5] = (char)('0' + fracPart);
    buf[6] = '\0';
}

/*---------------------------------------------------------------------------*/
/* sint16를 부호 포함 문자열로 변환 (예: "-1234", "+0056")                    */
/* buf는 최소 7바이트 필요                                                    */
/*---------------------------------------------------------------------------*/
static void sint16ToStr(sint16 val, char *buf)
{
    uint16 absVal;
    uint8 idx = 0u;
    char tmp[6];
    uint8 len = 0u;
    uint8 i;

    if (val < 0)
    {
        buf[idx++] = '-';
        absVal = (uint16)(-(sint32)val);
    }
    else
    {
        buf[idx++] = '+';
        absVal = (uint16)val;
    }

    /* 숫자를 역순으로 tmp에 저장 */
    if (absVal == 0u)
    {
        tmp[len++] = '0';
    }
    else
    {
        while (absVal > 0u)
        {
            tmp[len++] = (char)('0' + (absVal % 10u));
            absVal /= 10u;
        }
    }

    /* 역순으로 buf에 복사 */
    for (i = 0u; i < len; i++)
    {
        buf[idx++] = tmp[len - 1u - i];
    }
    buf[idx] = '\0';
}

/******************************************************************************/
/*                           공개 함수                                        */
/*****************************************************************************/

/*---------------------------------------------------------------------------*/
/* 레지스터 쓰기 + 읽기 검증 (최대 5회 재시도)                               */
/*---------------------------------------------------------------------------*/
static boolean writeRegVerify(uint8 regAddr, uint8 data)
{
    uint8 retry;
    uint8 readBack;

    for (retry = 0u; retry < 5u; retry++)
    {
        DrvSpi_WriteReg(regAddr, data);
        delay_ms(5);
        readBack = DrvSpi_ReadReg(regAddr);
        if (readBack == data)
        {
            return TRUE;
        }
    }
    return FALSE;
}

/*---------------------------------------------------------------------------*/
/* MPU-9250 초기화                                                           */
/*---------------------------------------------------------------------------*/
void DrvMpu9250_Init(void)
{
    /* 1. 디바이스 리셋 */
    DrvSpi_WriteReg(MPU_REG_PWR_MGMT_1, 0x80u);
    delay_ms(200);

    /* 2. Sleep 해제, 클럭 소스 = Auto PLL */
    writeRegVerify(MPU_REG_PWR_MGMT_1, 0x01u);
    delay_ms(50);

    /* 3. SPI 전용 모드 */
    writeRegVerify(MPU_REG_USER_CTRL, 0x10u);
    delay_ms(10);

    /* 4. 모든 축 활성화 (먼저 설정) */
    writeRegVerify(MPU_REG_PWR_MGMT_2, 0x00u);
    delay_ms(10);

    /* 5. 자이로 설정: ±500°/s */
    writeRegVerify(MPU_REG_GYRO_CONFIG, 0x08u);

    /* 6. 가속도계 설정: ±4g */
    writeRegVerify(MPU_REG_ACCEL_CONFIG, 0x08u);

    /* 7. DLPF: 자이로 41Hz, 가속도계 41Hz */
    writeRegVerify(MPU_REG_CONFIG, 0x03u);
    writeRegVerify(MPU_REG_ACCEL_CONFIG2, 0x03u);

    /* 8. 샘플레이트: 100Hz */
    writeRegVerify(MPU_REG_SMPLRT_DIV, 0x09u);

    delay_ms(50);

    /* 9. WHO_AM_I 확인 */
    s_initialized = DrvMpu9250_IsReady();
    if (s_initialized)
    {
        DrvUart_SendString("MPU-9250 OK!\r\n");
    }
    else
    {
        DrvUart_SendString("MPU-9250 FAIL!\r\n");
    }

    /* 10. 레지스터 확인 (디버그) */
    {
        char numBuf[8];
        uint8 reg;

        reg = DrvSpi_ReadReg(MPU_REG_WHO_AM_I);
        DrvUart_SendString("WHO:");
        sint16ToStr((sint16)reg, numBuf);
        DrvUart_SendString(numBuf);

        reg = DrvSpi_ReadReg(MPU_REG_PWR_MGMT_1);
        DrvUart_SendString(",PM1:");
        sint16ToStr((sint16)reg, numBuf);
        DrvUart_SendString(numBuf);

        reg = DrvSpi_ReadReg(MPU_REG_PWR_MGMT_2);
        DrvUart_SendString(",PM2:");
        sint16ToStr((sint16)reg, numBuf);
        DrvUart_SendString(numBuf);

        reg = DrvSpi_ReadReg(MPU_REG_GYRO_CONFIG);
        DrvUart_SendString(",GC:");
        sint16ToStr((sint16)reg, numBuf);
        DrvUart_SendString(numBuf);

        reg = DrvSpi_ReadReg(MPU_REG_ACCEL_CONFIG);
        DrvUart_SendString(",AC:");
        sint16ToStr((sint16)reg, numBuf);
        DrvUart_SendString(numBuf);

        DrvUart_SendString("\r\n");
    }
}

/*---------------------------------------------------------------------------*/
/* WHO_AM_I 레지스터 확인 → 0x71이면 TRUE                                    */
/*---------------------------------------------------------------------------*/
boolean DrvMpu9250_IsReady(void)
{
    uint8 id = DrvSpi_ReadReg(MPU_REG_WHO_AM_I);
    return (id == MPU_WHO_AM_I_MPU9250 || id == MPU_WHO_AM_I_MPU6500) ? TRUE : FALSE;
}

/*---------------------------------------------------------------------------*/
/* 센서 데이터 읽기 + 자세 계산 (10ms 주기로 호출)                           */
/*---------------------------------------------------------------------------*/
void DrvMpu9250_ReadSensors(void)
{
    uint8 buf[14];

    if (!s_initialized)
    {
        return;
    }

    /* 각 축 2바이트씩 개별 읽기 (QSPI3 FIFO 한계 대응) */
    DrvSpi_ReadBurst(0x3Bu, &buf[0],  2u);  /* AX H,L */
    DrvSpi_ReadBurst(0x3Du, &buf[2],  2u);  /* AY H,L */
    DrvSpi_ReadBurst(0x3Fu, &buf[4],  2u);  /* AZ H,L */
    buf[6] = 0u; buf[7] = 0u;               /* TEMP 생략 */
    DrvSpi_ReadBurst(0x43u, &buf[8],  2u);  /* GX H,L */
    DrvSpi_ReadBurst(0x45u, &buf[10], 2u);  /* GY H,L */
    DrvSpi_ReadBurst(0x47u, &buf[12], 2u);  /* GZ H,L */

    parseSensorData(buf);
    updateAttitude();
}

/*---------------------------------------------------------------------------*/
/* 자세 각도 Getter                                                          */
/*---------------------------------------------------------------------------*/
float32 DrvMpu9250_GetRoll(void)  { return s_roll;  }
float32 DrvMpu9250_GetPitch(void) { return s_pitch; }
float32 DrvMpu9250_GetYaw(void)   { return s_yaw;   }

/*---------------------------------------------------------------------------*/
/* UART로 자세 데이터 전송                                                   */
/* 포맷: "R:+012.3,P:-005.7,Y:+045.2\r\n"                                   */
/*---------------------------------------------------------------------------*/
void DrvMpu9250_SendUart(void)
{
    char str[7];

    if (!s_initialized)
    {
        return;
    }

    DrvUart_SendString("R:");
    floatToStr(s_roll, str);
    DrvUart_SendString(str);

    DrvUart_SendString(",P:");
    floatToStr(s_pitch, str);
    DrvUart_SendString(str);

    DrvUart_SendString(",Y:");
    floatToStr(s_yaw, str);
    DrvUart_SendString(str);

    DrvUart_SendString("\r\n");
}
