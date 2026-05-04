/**********************************************************************************************************************
 * \file DrvCan.h
 * \brief MultiCAN Node0 TX 드라이버 (ADC값 → CAN 송신)
 *
 * TX : P20.8 (IfxMultican_TXD0_P20_8_OUT) → TLE7250GVIO → IDC10 X202
 * RX : P20.7 (IfxMultican_RXD0B_P20_7_IN) ← TLE7250GVIO ← IDC10 X202
 * Baudrate : 500 kbps  |  메시지 ID : 0x100  |  DLC : 2 bytes
 *********************************************************************************************************************/
#ifndef DRVCAN_H
#define DRVCAN_H

#include "Ifx_Types.h"

#define CAN_BAUDRATE    500000u    /* 500 kbps */
#define CAN_MSG_ID      0x100u     /* ADC 데이터 표준 프레임 ID */

void DrvCan_Init(void);
void DrvCan_SendUint16(uint16 value);   /* 12-bit ADC(0~4095) → CAN ID=0x100, DLC=2 */

#endif /* DRVCAN_H */
