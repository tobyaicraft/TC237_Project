/**********************************************************************************************************************
 * \file DrvFlash.h
 * \brief DFLASH 캘리브레이션 데이터 저장/로드 드라이버
 *
 * - DFLASH Sector 0 (0xAF000000) 사용
 * - 8바이트 페이지 단위로 캘리브레이션 데이터 Read/Write
 * - UART "SAVE" 명령으로 RAM → Flash 저장, 부팅 시 Flash → RAM 복원
 *********************************************************************************************************************/
#ifndef DRVFLASH_H
#define DRVFLASH_H

#include "Ifx_Types.h"

/******************************************************************************/
/*                           캘리브레이션 데이터 구조체                       */
/******************************************************************************/
/* DFLASH 페이지 크기 = 8바이트이므로 8바이트 이내로 구성 */
typedef struct
{
    uint8  pwmDuty;         /* PWM Duty 0~100 [%]           (1 byte) */
    uint8  reserved1;       /* 예약 (확장용)                (1 byte) */
    uint16 reserved2;       /* 예약 (확장용)                (2 bytes) */
    uint32 magic;           /* 유효성 검증 매직넘버          (4 bytes) */
} DrvFlash_CalData;         /* 총 8 bytes = 1 DFLASH page */

#define DRVFLASH_CAL_MAGIC  0xCAFE1234u   /* Flash에 유효한 데이터가 있는지 판별 */

/******************************************************************************/
/*                           Public Functions                                 */
/******************************************************************************/

/** \brief 캘리브레이션 데이터를 Flash에서 읽어 RAM에 복원
 *  \return TRUE = 유효한 데이터 로드 성공, FALSE = 매직넘버 불일치 (기본값 사용) */
boolean DrvFlash_LoadCalibration(void);

/** \brief 현재 RAM 캘리브레이션 값을 DFLASH에 저장 (Erase → Write)
 *  \return TRUE = 저장 성공, FALSE = 실패 */
boolean DrvFlash_SaveCalibration(void);

#endif /* DRVFLASH_H */
