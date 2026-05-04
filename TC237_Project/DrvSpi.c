/**********************************************************************************************************************
 * \file DrvSpi.c
 * \brief QSPI3 SPI Master 드라이버
 *
 * - QSPI3 모듈을 SPI Master 모드로 초기화
 * - MPU-9250 통신: CPOL=0, CPHA=0, MSB first, 1 MHz
 * - CS(P22.2)는 GPIO 수동 제어
 *********************************************************************************************************************/
#include "DrvSpi.h"
#include "Qspi/SpiMaster/IfxQspi_SpiMaster.h"
#include "IfxPort.h"

/* CS 핀: P22.2 (GPIO 수동 제어) */
#define CS_PORT  &MODULE_P22
#define CS_PIN   2u

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

/******************************************************************************/
/*                           CS 제어                                          */
/******************************************************************************/
static void DrvSpi_CsLow(void)
{
    IfxPort_setPinLow(CS_PORT, CS_PIN);
}

static void DrvSpi_CsHigh(void)
{
    IfxPort_setPinHigh(CS_PORT, CS_PIN);
}

/******************************************************************************/
/*                           Functions                                        */
/******************************************************************************/
void DrvSpi_Init(void)
{
    /* ── CS 핀을 GPIO 출력으로 설정 (High = 비활성) ── */
    IfxPort_setPinModeOutput(CS_PORT, CS_PIN, IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_general);
    DrvSpi_CsHigh();

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
    chCfg.ch.mode.clockPolarity  = 0;
    chCfg.ch.mode.shiftClock     = 1;
    chCfg.ch.mode.dataHeading    = 1;
    chCfg.ch.mode.dataWidth      = 8;
    chCfg.ch.mode.autoCS         = 0;

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

    DrvSpi_CsLow();
    DrvSpi_WaitReady();
    IfxQspi_SpiMaster_exchange(&s_spiChannel, txBuf, rxBuf, 2);
    DrvSpi_WaitReady();
    DrvSpi_CsHigh();

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

    DrvSpi_CsLow();
    DrvSpi_WaitReady();
    IfxQspi_SpiMaster_exchange(&s_spiChannel, txBuf, NULL_PTR, 2);
    DrvSpi_WaitReady();
    DrvSpi_CsHigh();
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

    DrvSpi_CsLow();
    DrvSpi_WaitReady();
    IfxQspi_SpiMaster_exchange(&s_spiChannel, txBuf, rxBuf, totalLen);
    DrvSpi_WaitReady();
    DrvSpi_CsHigh();

    for (i = 0u; i < len; i++)
    {
        buf[i] = rxBuf[i + 1u];
    }
}
