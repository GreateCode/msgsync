/*
 * msgfinack_handler.cpp
 *
 *  Created on: 2015年4月6日
 *      Author: jimm
 */

#include "msgfinack_handler.h"
#include "../../common/common_datetime.h"
#include "../../common/common_api.h"
#include "../../frame/frame.h"
#include "../../frame/server_helper.h"
#include "../../frame/redissession_bank.h"
#include "../../logger/logger.h"
#include "../../include/cachekey_define.h"
#include "../../include/control_head.h"
#include "../../include/typedef.h"
#include "../config/msgdispatch_config.h"
#include "../config/string_config.h"
#include "../server_typedef.h"
#include "../bank/redis_bank.h"

using namespace LOGGER;
using namespace FRAME;

int32_t CMsgFinAckHandler::MsgFinAck(ICtlHead *pCtlHead, IMsgHead *pMsgHead, IMsgBody *pMsgBody, uint8_t *pBuf, int32_t nBufSize)
{
	ControlHead *pControlHead = dynamic_cast<ControlHead *>(pCtlHead);
	if(pControlHead == NULL)
	{
		return 0;
	}

	MsgHeadCS *pMsgHeadCS = dynamic_cast<MsgHeadCS *>(pMsgHead);
	if(pMsgHeadCS == NULL)
	{
		return 0;
	}

	CMsgFinAckReq *pMsgFinAckReq = dynamic_cast<CMsgFinAckReq *>(pMsgBody);
	if(pMsgFinAckReq == NULL)
	{
		return 0;
	}

	CRedisBank *pRedisBank = (CRedisBank *)g_Frame.GetBank(BANK_REDIS);
	UserUnreadMsgList *pUnreadMsgList = (UserUnreadMsgList *)g_Frame.GetConfig(USER_UNREADMSGLIST);
	CRedisChannel *pUnreadMsgChannel = pRedisBank->GetRedisChannel(pUnreadMsgList->string);
	pUnreadMsgChannel->ZRemRangeByRank(NULL, itoa(pMsgHeadCS->m_nSrcUin), 0, pMsgFinAckReq->m_nSyncCount - 1);

	MsgHeadCS stMsgHeadCS;
	stMsgHeadCS.m_nMsgID = MSGID_MSGFINACK_RESP;
	stMsgHeadCS.m_nSrcUin = pMsgHeadCS->m_nSrcUin;
	stMsgHeadCS.m_nDstUin = pMsgHeadCS->m_nDstUin;

	CMsgFinAckResp stMsgFinAckResp;
	stMsgFinAckResp.m_nSyncSeq = pMsgFinAckReq->m_nSyncSeq;

	CMsgDispatchConfig *pMsgDispatchConfig = (CMsgDispatchConfig *)g_Frame.GetConfig(CONFIG_MSGDISPATCH);
	CRedisChannel *pMsgPushChannel = pRedisBank->GetRedisChannel(pMsgDispatchConfig->GetChannelKey(MSGID_MSGFINACK_RESP));

	uint8_t arrRespBuf[MAX_MSG_SIZE];
	uint16_t nTotalSize = CServerHelper::MakeMsg(pCtlHead, &stMsgHeadCS, &stMsgFinAckResp, arrRespBuf, sizeof(arrRespBuf));
	pMsgPushChannel->RPush(NULL, (char *)arrRespBuf, nTotalSize);

	g_Frame.Dump(pCtlHead, &stMsgHeadCS, &stMsgFinAckResp, "send ");

	return 0;
}

