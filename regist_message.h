/*
 * regist_message.h
 *
 *  Created on: Mar 10, 2015
 *      Author: jimm
 */

#ifndef REGIST_MESSAGE_H_
#define REGIST_MESSAGE_H_

#include "frame/frame.h"
#include "include/msg_head.h"
#include "include/control_head.h"
#include "include/sync_msg.h"
#include "logic/msgsync_handler.h"
#include "logic/msgfinack_handler.h"

using namespace FRAME;

MSGMAP_BEGIN(msgmap)
ON_PROC_PCH_PMH_PMB_PU8_I32(MSGID_MSGSYNC_REQ, ControlHead, MsgHeadCS, CMsgSyncReq, CMsgSyncHandler, CMsgSyncHandler::MsgSync);
ON_PROC_PCH_PMH_PMB_PU8_I32(MSGID_MSGFINACK_REQ, ControlHead, MsgHeadCS, CMsgFinAckReq, CMsgFinAckHandler, CMsgFinAckHandler::MsgFinAck);
MSGMAP_END(msgmap)

#endif /* REGIST_MESSAGE_H_ */
