/**********************************************************************************************************************
 * \file Cpu0_Main.c
 * \brief TC237 Application Kit - 1ms STM tick 기반 협력형 스케줄러
 *********************************************************************************************************************/
#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "DrvIntc.h"
#include "DrvStm.h"
#include "AppTask.h"
#include "Scheduler.h"

/*-----------------------------------------------------------------------------------------------*/
/* Section 배치 실험용 변수                                                                        */
/*   Test1 : 초기값 없는 전역변수  → .bss   (dsram0)                                               */
/*   Test2 : 초기값 있는 전역변수  → .data  (dsram0)                                               */
/*   Test3 : const 읽기 전용      → .rodata (pfls0)                                               */
/*   Test4 : 함수 안의 지역변수    → stack  (dsram0)                                               */
/*-----------------------------------------------------------------------------------------------*/
int         Test1;          /* 초기값 없는 전역변수 */
int         Test2 = 2;      /* 초기값 있는 전역변수 */
const int   Test3 = 3;      /* const (읽기 전용)   */

/*-----------------------------------------------------------------------------------------------*/
/* TestFunction — "test" code section 에 배치 (링커 스크립트로 0x80001000 고정)                     */
/*-----------------------------------------------------------------------------------------------*/
static int cnt = 0;

#pragma protect on
#pragma section code "test"
void TestFunction(void)
{
    cnt++;
}
#pragma protect restore
#pragma section code restore

IfxCpu_syncEvent cpuSyncEvent = 0;

/*-----------------------------------------------------------------------------------------------*/
/* main                                                                                          */
/*-----------------------------------------------------------------------------------------------*/
void core0_main(void)
{
    int Test4 = 4;          /* 지역변수 → stack */

    /* Watchdog 비활성화 */
    DrvIntc_Init();

    /* CPU 동기화 */
    IfxCpu_emitEvent(&cpuSyncEvent);
    IfxCpu_waitEvent(&cpuSyncEvent, 1);

    /* 각 변수가 어느 섹션에 위치하는지 확인용 사용 */
    Test1++;
    Test2++;
    Test4++;
    TestFunction();

    /* 주변장치 초기화 */
    AppTask_Init();
    DrvStm_Init();

    /* 글로벌 인터럽트 Enable — 모든 초기화 완료 후 마지막 수행 */
    IfxCpu_enableInterrupts();

    while (1)
    {
        Scheduler_Run();
    }
}
