/**********************************************************************************************************************
 * \file DrvFlash.c
 * \brief DFLASH 캘리브레이션 데이터 저장/로드 드라이버
 *
 * 메모리 맵:
 *   DFLASH Sector 0 : 0xAF000000 ~ 0xAF001FFF (8 KB)
 *   캘리브레이션 데이터 : 0xAF000000 (첫 번째 페이지, 8 bytes)
 *
 * 동작 순서 (Write):
 *   1. Endinit 해제
 *   2. Sector 0 Erase
 *   3. Wait Unbusy
 *   4. Enter Page Mode
 *   5. Load Page (8 bytes)
 *   6. Write Page
 *   7. Wait Unbusy
 *   8. Endinit 복원
 *
 * 동작 순서 (Read):
 *   DFLASH 주소를 직접 포인터로 읽기 (메모리 맵 I/O)
 *********************************************************************************************************************/
#include "DrvFlash.h"
#include "DrvPwm.h"
#include "Flash/Std/IfxFlash.h"
#include "IfxScuWdt.h"

/******************************************************************************/
/*                           Configuration                                    */
/******************************************************************************/
#define CAL_FLASH_ADDR      IFXFLASH_DFLASH_START   /* 0xAF000000 */

/******************************************************************************/
/*                           Functions                                        */
/******************************************************************************/

boolean DrvFlash_LoadCalibration(void)
{
    /* DFLASH 주소를 직접 읽기 (메모리 맵) */
    const DrvFlash_CalData *pFlash = (const DrvFlash_CalData *)CAL_FLASH_ADDR;

    /* 매직넘버로 유효성 검증 */
    if (pFlash->magic != DRVFLASH_CAL_MAGIC)
    {
        /* Flash에 유효한 데이터 없음 → 기본값 유지 */
        return FALSE;
    }

    /* Flash → RAM 복원 */
    if (pFlash->pwmDuty <= 100u)
    {
        g_pwmDuty = pFlash->pwmDuty;
    }

    return TRUE;
}


boolean DrvFlash_SaveCalibration(void)
{
    /* 저장할 데이터 준비 */
    DrvFlash_CalData cal;
    cal.pwmDuty   = g_pwmDuty;
    cal.reserved1 = 0u;
    cal.reserved2 = 0u;
    cal.magic     = DRVFLASH_CAL_MAGIC;

    /* 8바이트 데이터를 두 개의 32-bit 워드로 분리 */
    uint32 wordL = *((uint32 *)&cal);          /* [pwmDuty, reserved1, reserved2] */
    uint32 wordU = *((uint32 *)&cal + 1);      /* [magic] */

    /* Endinit 해제 (Flash 명령 실행에 필요) */
    uint16 endinitPw = IfxScuWdt_getCpuWatchdogPassword();
    IfxScuWdt_clearCpuEndinit(endinitPw);

    /* 1) 상태 클리어 */
    IfxFlash_clearStatus(0);

    /* 2) Sector 0 Erase */
    IfxFlash_eraseSector(CAL_FLASH_ADDR);
    IfxFlash_waitUnbusy(0, IfxFlash_FlashType_D0);

    /* 3) Page Mode 진입 */
    IfxFlash_enterPageMode(CAL_FLASH_ADDR);

    /* 4) 8바이트 데이터 로드 (DFLASH 페이지 = 8 bytes) */
    IfxFlash_loadPage2X32(CAL_FLASH_ADDR, wordL, wordU);

    /* 5) Page Write */
    IfxFlash_writePage(CAL_FLASH_ADDR);
    IfxFlash_waitUnbusy(0, IfxFlash_FlashType_D0);

    /* Endinit 복원 */
    IfxScuWdt_setCpuEndinit(endinitPw);

    /* 6) 검증: 기록된 값 다시 읽어서 매직넘버 확인 */
    const DrvFlash_CalData *pVerify = (const DrvFlash_CalData *)CAL_FLASH_ADDR;
    if (pVerify->magic != DRVFLASH_CAL_MAGIC)
    {
        return FALSE;
    }

    return TRUE;
}
