/**********************************************************************************************************************
 * \file DrvSpi.h
 * \brief QSPI3 SPI Master 드라이버 (MPU-9250 통신용)
 *
 * 핀 배치:
 *   SCLK : P22.3 (X102 pin 26)
 *   MOSI : P22.0 (X102 pin 23)
 *   MISO : P22.1 (X102 pin 24)
 *   CS   : P22.2 (X102 pin 25, SLSO12)
 *
 * ISR 우선순위: TX=6, RX=7, ERR=8
 *********************************************************************************************************************/
#ifndef DRVSPI_H
#define DRVSPI_H

#include "Ifx_Types.h"

/* ISR 우선순위 — IFX_INTERRUPT 매크로에서 사용하므로 괄호 없이 정의 */
#define DRVSPI_TX_PRIORITY   6
#define DRVSPI_RX_PRIORITY   7
#define DRVSPI_ERR_PRIORITY  8

void  DrvSpi_Init(void);
uint8 DrvSpi_ReadReg(uint8 regAddr);
void  DrvSpi_WriteReg(uint8 regAddr, uint8 data);
void  DrvSpi_ReadBurst(uint8 regAddr, uint8 *buf, uint16 len);

#endif /* DRVSPI_H */
