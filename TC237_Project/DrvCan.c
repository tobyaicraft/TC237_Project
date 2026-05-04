/**********************************************************************************************************************
 * \file DrvCan.c
 * \brief MultiCAN Node0 TX 드라이버
 *
 * Message Object 0 : TX, ID=0x100, DLC=2, 인터럽트 없음
 * AppTask_10ms 에서 DrvCan_SendUint16(DrvAdc_GetResult(0)) 호출 → 100 Hz 송신
 *********************************************************************************************************************/
#include "DrvCan.h"
#include "Multican/Can/IfxMultican_Can.h"
#include "_PinMap/IfxMultican_PinMap.h"

/******************************************************************************/
/*                           Module Variables                                 */
/******************************************************************************/
static IfxMultican_Can        s_can;
static IfxMultican_Can_Node   s_node;
static IfxMultican_Can_MsgObj s_txMsgObj;

/******************************************************************************/
/*                           Functions                                        */
/******************************************************************************/
void DrvCan_Init(void)
{
    /* ── CAN 모듈 초기화 ── */
    IfxMultican_Can_Config canCfg;
    IfxMultican_Can_initModuleConfig(&canCfg, &MODULE_CAN);
    IfxMultican_Can_initModule(&s_can, &canCfg);

    /* ── Node 0: P20.7 RX, P20.8 TX, 500 kbps ── */
    IfxMultican_Can_NodeConfig nodeCfg;
    IfxMultican_Can_Node_initConfig(&nodeCfg, &s_can);
    nodeCfg.nodeId    = IfxMultican_NodeId_0;
    nodeCfg.baudrate  = CAN_BAUDRATE;
    nodeCfg.rxPin     = &IfxMultican_RXD0B_P20_7_IN;
    nodeCfg.rxPinMode = IfxPort_InputMode_pullUp;
    nodeCfg.txPin     = &IfxMultican_TXD0_P20_8_OUT;
    nodeCfg.txPinMode = IfxPort_OutputMode_pushPull;
    IfxMultican_Can_Node_init(&s_node, &nodeCfg);

    /* ── TX Message Object 0 ── */
    IfxMultican_Can_MsgObjConfig txCfg;
    IfxMultican_Can_MsgObj_initConfig(&txCfg, &s_node);
    txCfg.msgObjId           = 0u;
    txCfg.messageId          = CAN_MSG_ID;
    txCfg.frame              = IfxMultican_Frame_transmit;
    txCfg.control.messageLen = IfxMultican_DataLengthCode_2;
    IfxMultican_Can_MsgObj_init(&s_txMsgObj, &txCfg);
}

void DrvCan_SendUint16(uint16 value)
{
    IfxMultican_Message txMsg;
    IfxMultican_Message_init(&txMsg, CAN_MSG_ID,
                             (uint32)value, 0u,
                             IfxMultican_DataLengthCode_2);
    /* 수신 보드 없거나 버스 오류 시 무한 대기 방지: 최대 10,000회 재시도 */
    uint32 retry = 0u;
    while ((IfxMultican_Can_MsgObj_sendMessage(&s_txMsgObj, &txMsg)
            == IfxMultican_Status_notSentBusy) && (++retry < 10000u)) {}
}
