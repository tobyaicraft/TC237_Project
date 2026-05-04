/**********************************************************************************************************************
 * \file DrvMpu9250.h
 * \brief MPU-9250 9축 IMU 센서 드라이버
 *
 * - SPI 통신으로 가속도계/자이로 데이터 읽기
 * - Complementary Filter로 Roll/Pitch/Yaw 계산
 * - UART로 자세 데이터 전송 (Python 3D 시각화용)
 *********************************************************************************************************************/
#ifndef DRVMPU9250_H
#define DRVMPU9250_H

#include "Ifx_Types.h"

void    DrvMpu9250_Init(void);
boolean DrvMpu9250_IsReady(void);
void    DrvMpu9250_ReadSensors(void);
float32 DrvMpu9250_GetRoll(void);
float32 DrvMpu9250_GetPitch(void);
float32 DrvMpu9250_GetYaw(void);
void    DrvMpu9250_SendUart(void);

#endif /* DRVMPU9250_H */
