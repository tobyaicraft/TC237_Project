/**********************************************************************************************************************
 * \file Cpu0_Main.c
 * \brief 1 ms STM tick 기반 간단 스케줄러 (ISR 은 counter 증가만)
 *********************************************************************************************************************/
#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"
#include "IfxPort.h"
#include "IfxStm.h"

#define LED0                &MODULE_P13, 0      /* Application Kit TC2x7 LED (active-low) */
#define ISR_PRIORITY_STM0   (10U)
#define SCHED_TICK_MS       (1U)

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

IfxCpu_syncEvent            cpuSyncEvent = 0;

static IfxStm_CompareConfig g_stmCfg;
static volatile uint32      g_1ms_counter = 0;  /* 1 ms 마다 ISR 에서 ++ */

/*-----------------------------------------------------------------------------------------------*/
/* 1 ms periodic ISR — counter 만 증가                                                            */
/*-----------------------------------------------------------------------------------------------*/
IFX_INTERRUPT(stm0Tick_ISR, 0, ISR_PRIORITY_STM0)
{
    IfxStm_clearCompareFlag(&MODULE_STM0, g_stmCfg.comparator);
    IfxStm_increaseCompare(&MODULE_STM0, g_stmCfg.comparator, g_stmCfg.ticks);
    g_1ms_counter++;
}

static void initSystemTimer(void)
{
    IfxStm_initCompareConfig(&g_stmCfg);
    g_stmCfg.triggerPriority = ISR_PRIORITY_STM0;
    g_stmCfg.typeOfService   = IfxSrc_Tos_cpu0;
    g_stmCfg.ticks           = (uint32)IfxStm_getTicksFromMilliseconds(&MODULE_STM0, SCHED_TICK_MS);
    IfxStm_initCompare(&MODULE_STM0, &g_stmCfg);
}

/*-----------------------------------------------------------------------------------------------*/
/* Application tasks                                                                             */
/*-----------------------------------------------------------------------------------------------*/
static void initLed(void)
{
    IfxPort_setPinMode(LED0, IfxPort_Mode_outputPushPullGeneral);
    IfxPort_setPinHigh(LED0);
}

static void Task_1ms(void)
{
    /* 1 ms 마다 실행할 로직 */
}

static void Task_10ms(void)
{
    /* 10 ms 마다 실행할 로직 */
}

static void Task_100ms(void)
{
    /* 100 ms 마다 실행할 로직 */
}

static void Task_500ms(void)
{
    IfxPort_togglePin(LED0);    /* 500 ms 마다 LED 토글 → 1 Hz blink */
}

/*-----------------------------------------------------------------------------------------------*/
/* Scheduler — 1 ms counter 를 modulo 로 분주                                                     */
/*-----------------------------------------------------------------------------------------------*/
static void Scheduler(void)
{
    static uint32 last_counter = 0;
    uint32        now = g_1ms_counter;

    if (now == last_counter)
    {
        return;
    }
    last_counter = now;

    if ((now %   1U) == 0U) Task_1ms();
    if ((now %  10U) == 0U) Task_10ms();
    if ((now % 100U) == 0U) Task_100ms();
    if ((now % 500U) == 0U) Task_500ms();
}

/*-----------------------------------------------------------------------------------------------*/
/* main                                                                                          */
/*-----------------------------------------------------------------------------------------------*/
void core0_main(void)
{
    int Test4 = 4;          /* 지역변수 → stack */

    IfxCpu_enableInterrupts();

    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_disableSafetyWatchdog(IfxScuWdt_getSafetyWatchdogPassword());

    IfxCpu_emitEvent(&cpuSyncEvent);
    IfxCpu_waitEvent(&cpuSyncEvent, 1);

    /* 각 변수가 어느 섹션에 위치하는지 확인용 사용 */
    Test1++;
    Test2++;
    Test4++;
    TestFunction();

    initLed();
    initSystemTimer();

    while (1)
    {
        Scheduler();
    }
}
