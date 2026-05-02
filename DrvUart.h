/**********************************************************************************************************************
 * \file DrvUart.h
 * \brief ASCLIN0 UART 드라이버
 *
 * TX : P15.2 (IfxAsclin0_TX_P15_2_OUT)  → X102 pin 31
 * RX : P15.3 (IfxAsclin0_RXB_P15_3_IN)  → X102 pin 32
 * BaudRate : 115200, 8-N-1
 *********************************************************************************************************************/
#ifndef DRVUART_H
#define DRVUART_H

#include "Ifx_Types.h"

/* 주의: 괄호 없이 정의 — 인터럽트 벡터 섹션 이름 생성에 사용됨 */
#define DRVUART_TX_PRIORITY     3
#define DRVUART_RX_PRIORITY     4
#define DRVUART_ERR_PRIORITY    2

void    DrvUart_Init(void);
void    DrvUart_SendByte(uint8 data);
void    DrvUart_SendString(const char *str);
boolean DrvUart_ReceiveByte(uint8 *data);

#endif /* DRVUART_H */
