/**********************************************************************************************************************
 * \file DrvUart.c
 * \brief ASCLIN0 UART 드라이버 (115200 8-N-1)
 *
 * TX : P15.2 (X102 pin 31)  RX : P15.3 (X102 pin 32)
 * SW FIFO : TX 256 byte / RX 64 byte
 * ISR 우선순위 : TX=3, RX=4, ERR=2  (DrvTim=5, DrvGtmTimer=11 과 충돌 없음)
 *********************************************************************************************************************/
#include "DrvUart.h"
#include "Compilers.h"
#include "Asclin/Asc/IfxAsclin_Asc.h"
#include "_PinMap/IfxAsclin_PinMap.h"

/******************************************************************************/
/*                           Configuration                                    */
/******************************************************************************/
#define UART_TX_BUFFER_SIZE     256u
#define UART_RX_BUFFER_SIZE     64u

/******************************************************************************/
/*                           Module Variables                                 */
/******************************************************************************/
static IfxAsclin_Asc s_asc;

/* iLLD FIFO: 실제 데이터 크기 + sizeof(Ifx_Fifo) + 8 바이트 필수 */
static uint8 s_txBuffer[UART_TX_BUFFER_SIZE + sizeof(Ifx_Fifo) + 8u];
static uint8 s_rxBuffer[UART_RX_BUFFER_SIZE + sizeof(Ifx_Fifo) + 8u];

/******************************************************************************/
/*                           ISR                                              */
/******************************************************************************/
IFX_INTERRUPT(DrvUart_TxISR, 0, DRVUART_TX_PRIORITY)
{
    IfxAsclin_Asc_isrTransmit(&s_asc);
}

IFX_INTERRUPT(DrvUart_RxISR, 0, DRVUART_RX_PRIORITY)
{
    IfxAsclin_Asc_isrReceive(&s_asc);
}

IFX_INTERRUPT(DrvUart_ErrISR, 0, DRVUART_ERR_PRIORITY)
{
    IfxAsclin_Asc_isrError(&s_asc);
}

/******************************************************************************/
/*                           Functions                                        */
/******************************************************************************/
void DrvUart_Init(void)
{
    static const IfxAsclin_Asc_Pins pins = {
        NULL,                            IfxPort_InputMode_pullUp,              /* CTS 미사용 */
        &IfxAsclin0_RXB_P15_3_IN,       IfxPort_InputMode_pullUp,              /* RX: P15.3 (X102 pin 32) */
        NULL,                            IfxPort_OutputMode_pushPull,           /* RTS 미사용 */
        &IfxAsclin0_TX_P15_2_OUT,       IfxPort_OutputMode_pushPull,           /* TX: P15.2 (X102 pin 31) */
        IfxPort_PadDriver_cmosAutomotiveSpeed1
    };

    IfxAsclin_Asc_Config cfg;
    IfxAsclin_Asc_initModuleConfig(&cfg, &MODULE_ASCLIN0);

    cfg.baudrate.baudrate    = 115200.0f;
    cfg.baudrate.prescaler   = 1u;

    cfg.frame.dataLength     = IfxAsclin_DataLength_8;
    cfg.frame.parityBit      = FALSE;                   /* Parity 없음 */
    cfg.frame.stopBit        = IfxAsclin_StopBit_1;

    cfg.interrupt.txPriority = DRVUART_TX_PRIORITY;
    cfg.interrupt.rxPriority = DRVUART_RX_PRIORITY;
    cfg.interrupt.erPriority = DRVUART_ERR_PRIORITY;
    cfg.interrupt.typeOfService = IfxSrc_Tos_cpu0;

    cfg.pins         = &pins;
    cfg.txBuffer     = s_txBuffer;
    cfg.txBufferSize = UART_TX_BUFFER_SIZE;
    cfg.rxBuffer     = s_rxBuffer;
    cfg.rxBufferSize = UART_RX_BUFFER_SIZE;

    IfxAsclin_Asc_initModule(&s_asc, &cfg);
}

void DrvUart_SendByte(uint8 data)
{
    IfxAsclin_Asc_blockingWrite(&s_asc, data);
}

void DrvUart_SendString(const char *str)
{
    while (*str != '\0')
    {
        IfxAsclin_Asc_blockingWrite(&s_asc, (uint8)*str);
        str++;
    }
}

boolean DrvUart_ReceiveByte(uint8 *data)
{
    Ifx_SizeT count = 1;
    return IfxAsclin_Asc_read(&s_asc, data, &count, TIME_NULL);
}
