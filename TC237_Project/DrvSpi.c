/**********************************************************************************************************************
 * \file DrvSpi.c
 * \brief QSPI3 SPI Master 드라이버
 *
 * - QSPI3 모듈을 SPI Master 모드로 초기화
 * - MPU-9250 통신: CPOL=0, CPHA=0, MSB first, 1 MHz
 * - CS(P22.2)는 QSPI3 SLSO12 하드웨어 자동 제어
 *********************************************************************************************************************/
#include "DrvSpi.h"
#include "Qspi/SpiMaster/IfxQspi_SpiMaster.h"
#include "IfxPort.h"

/******************************************************************************/
/*                           Module Variables                                 */
/******************************************************************************/
static IfxQspi_SpiMaster         s_spiMaster;
static IfxQspi_SpiMaster_Channel s_spiChannel;
static boolean s_spiReady = FALSE;

/******************************************************************************/
/*                           ISR Handlers                                     */
/******************************************************************************/
IFX_INTERRUPT(DrvSpi_TxISR, 0, DRVSPI_TX_PRIORITY)
{
    IfxQspi_SpiMaster_isrTransmit(&s_spiMaster);
}

IFX_INTERRUPT(DrvSpi_RxISR, 0, DRVSPI_RX_PRIORITY)
{
    IfxQspi_SpiMaster_isrReceive(&s_spiMaster);
}

IFX_INTERRUPT(DrvSpi_ErrISR, 0, DRVSPI_ERR_PRIORITY)
{
    IfxQspi_SpiMaster_isrError(&s_spiMaster);
}

/* CS는 QSPI3 SLSO12 (P22.2) 하드웨어 자동 제어 — 수동 함수 불필요 */

/******************************************************************************/
/*                           Functions                                        */
/******************************************************************************/
void DrvSpi_Init(void)
{
    /* ── 모듈 설정 ── */
    IfxQspi_SpiMaster_Config masterCfg;
    IfxQspi_SpiMaster_initModuleConfig(&masterCfg, &MODULE_QSPI3);

    masterCfg.mode            = IfxQspi_Mode_master;
    masterCfg.maximumBaudrate = 1000000;

    masterCfg.txPriority   = DRVSPI_TX_PRIORITY;
    masterCfg.rxPriority   = DRVSPI_RX_PRIORITY;
    masterCfg.erPriority   = DRVSPI_ERR_PRIORITY;
    masterCfg.isrProvider  = IfxCpu_Irq_getTos(IfxCpu_getCoreIndex());

    /* 핀 설정: P22.3(SCLK), P22.0(MOSI), P22.1(MISO) */
    const IfxQspi_SpiMaster_Pins pins = {
        &IfxQspi3_SCLK_P22_3_OUT,  IfxPort_OutputMode_pushPull,
        &IfxQspi3_MTSR_P22_0_OUT,  IfxPort_OutputMode_pushPull,
        &IfxQspi3_MRSTE_P22_1_IN,  IfxPort_InputMode_pullDown,
        IfxPort_PadDriver_cmosAutomotiveSpeed1
    };
    masterCfg.pins = &pins;

    IfxQspi_SpiMaster_initModule(&s_spiMaster, &masterCfg);

    /* ── 채널 설정 ── */
    IfxQspi_SpiMaster_ChannelConfig chCfg;
    IfxQspi_SpiMaster_initChannelConfig(&chCfg, &s_spiMaster);

    chCfg.ch.baudrate            = 1000000;
    chCfg.ch.mode.clockPolarity  = 0;       /* CPOL=0: 클럭 Idle = Low */
    chCfg.ch.mode.shiftClock     = 1;       /* 데이터 Shift = Trailing Edge (Falling) */
    chCfg.ch.mode.dataHeading    = 1;       /* MSB first */
    chCfg.ch.mode.dataWidth      = 8;       /* 8-bit 프레임 */

    /* Leading Delay: CS Low → 첫 SCK까지 최소 8ns 보장 (1클럭 = 1000ns, 충분) */
    chCfg.ch.mode.csLeadDelay   = IfxQspi_SlsoTiming_1;
    /* Trailing Delay: 마지막 SCK → CS High까지 최소 500ns 보장 (2클럭 = 2000ns, 마진 확보) */
    chCfg.ch.mode.csTrailDelay  = IfxQspi_SlsoTiming_2;

    /* SLSO12 (P22.2) 하드웨어 자동 CS */
    const IfxQspi_SpiMaster_Output slsOutput = {
        &IfxQspi3_SLSO12_P22_2_OUT,
        IfxPort_OutputMode_pushPull,
        IfxPort_PadDriver_cmosAutomotiveSpeed1
    };
    chCfg.sls.output = slsOutput;

    IfxQspi_SpiMaster_initChannel(&s_spiChannel, &chCfg);

    s_spiReady = TRUE;
}

/*---------------------------------------------------------------------------*/
/* SPI 전송 완료 대기                                                        */
/*---------------------------------------------------------------------------*/
static void DrvSpi_WaitReady(void)
{
    volatile uint32 timeout = 1000000u;
    while (IfxQspi_SpiMaster_getStatus(&s_spiChannel) == IfxQspi_Status_busy)
    {
        if (--timeout == 0u)
        {
            break;
        }
    }
}

/*---------------------------------------------------------------------------*/
/* 단일 레지스터 읽기                                                        */
/*---------------------------------------------------------------------------*/
uint8 DrvSpi_ReadReg(uint8 regAddr)
{
    uint8 txBuf[2];
    uint8 rxBuf[2];

    txBuf[0] = 0x80u | regAddr;
    txBuf[1] = 0x00u;

    DrvSpi_WaitReady();
    IfxQspi_SpiMaster_exchange(&s_spiChannel, txBuf, rxBuf, 2);
    DrvSpi_WaitReady();

    return rxBuf[1];
}

/*---------------------------------------------------------------------------*/
/* 단일 레지스터 쓰기                                                        */
/*---------------------------------------------------------------------------*/
void DrvSpi_WriteReg(uint8 regAddr, uint8 data)
{
    uint8 txBuf[2];

    txBuf[0] = regAddr & 0x7Fu;
    txBuf[1] = data;

    DrvSpi_WaitReady();
    IfxQspi_SpiMaster_exchange(&s_spiChannel, txBuf, NULL_PTR, 2);
    DrvSpi_WaitReady();
}

/*---------------------------------------------------------------------------*/
/* 버스트 읽기                                                               */
/*---------------------------------------------------------------------------*/
void DrvSpi_ReadBurst(uint8 regAddr, uint8 *buf, uint16 len)
{
    uint8 txBuf[15];
    uint8 rxBuf[15];
    uint16 totalLen = (uint16)(1u + len);
    uint16 i;

    txBuf[0] = 0x80u | regAddr;
    for (i = 1u; i < totalLen; i++)
    {
        txBuf[i] = 0x00u;
    }

    DrvSpi_WaitReady();
    IfxQspi_SpiMaster_exchange(&s_spiChannel, txBuf, rxBuf, totalLen);
    DrvSpi_WaitReady();

    for (i = 0u; i < len; i++)
    {
        buf[i] = rxBuf[i + 1u];
    }
}
