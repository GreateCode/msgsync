/*
 * msgfinack_handler.cpp
 *
 *  Created on: 2015年4月6日
 *      Author: jimm
 */

#include "msgfinack_handler.h"
#include "common/common_datetime.h"
#include "common/common_api.h"
#include "frame/frame.h"
#include "frame/server_helper.h"
#include "frame/redissession_bank.h"
#include "frame/cachekey_define.h"
#include "logger/logger.h"
#include "include/control_head.h"
#include "include/typedef.h"
#include "config/string_config.h"
#include "server_typedef.h"
#include "bank/redis_bank.h"

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

	if(pControlHead->m_nUin != pMsgHeadCS->m_nSrcUin)
	{
		CRedisBank *pRedisBank = (CRedisBank *)g_Frame.GetBank(BANK_REDIS);
		CRedisChannel *pClientRespChannel = pRedisBank->GetRedisChannel(pControlHead->m_nGateRedisAddress, pControlHead->m_nGateRedisPort);

		return CServerHelper::KickUser(pControlHead, pMsgHeadCS, pClientRespChannel, KickReason_NotLogined);
	}

	CMsgFinAckReq *pMsgFinAckReq = dynamic_cast<CMsgFinAckReq *>(pMsgBody);
	if(pMsgFinAckReq == NULL)
	{
		return 0;
	}

	CRedisBank *pRedisBank = (CRedisBank *)g_Frame.GetBank(BANK_REDIS);
	CRedisChannel *pUnreadMsgChannel = pRedisBank->GetRedisChannel(UserUnreadMsgList::servername, pMsgHeadCS->m_nSrcUin);
	pUnreadMsgChannel->ZRemRangeByRank(NULL, CServerHelper::MakeRedisKey(UserUnreadMsgList::keyname, pMsgHeadCS->m_nSrcUin),
			0, pMsgFinAckReq->m_nSyncCount - 1);

	MsgHeadCS stMsgHeadCS;
	stMsgHeadCS.m_nMsgID = MSGID_MSGFINACK_RESP;
	stMsgHeadCS.m_nSrcUin = pMsgHeadCS->m_nDstUin;
	stMsgHeadCS.m_nDstUin = pMsgHeadCS->m_nSrcUin;

	CMsgFinAckResp stMsgFinAckResp;
	stMsgFinAckResp.m_nSyncSeq = pMsgFinAckReq->m_nSyncSeq;

	CRedisChannel *pMsgPushChannel = pRedisBank->GetRedisChannel(pControlHead->m_nGateRedisAddress, pControlHead->m_nGateRedisPort);

	uint8_t arrRespBuf[MAX_MSG_SIZE];
	uint16_t nTotalSize = CServerHelper::MakeMsg(pCtlHead, &stMsgHeadCS, &stMsgFinAckResp, arrRespBuf, sizeof(arrRespBuf));
	pMsgPushChannel->RPush(NULL, CServerHelper::MakeRedisKey(ClientResp::keyname, pControlHead->m_nGateID), (char *)arrRespBuf, nTotalSize);

	g_Frame.Dump(pCtlHead, &stMsgHeadCS, &stMsgFinAckResp, "send ");

	CRedisSessionBank *pRedisSessionBank = (CRedisSessionBank *)g_Frame.GetBank(BANK_REDIS_SESSION);
	RedisSession *pSession = pRedisSessionBank->CreateSession(this, static_cast<RedisReply>(&CMsgFinAckHandler::OnSessionGetUnreadMsgCount),
			static_cast<TimerProc>(&CMsgFinAckHandler::OnRedisSessionTimeout));
	UserSession *pSessionData = new(pSession->GetSessionData()) UserSession();
	pSessionData->m_stCtlHead = *pControlHead;
	pSessionData->m_stMsgHeadCS = *pMsgHeadCS;
	pSessionData->m_stMsgFinAckReq = *pMsgFinAckReq;

	pUnreadMsgChannel->ZCount(pSession, CServerHelper::MakeRedisKey(UserUnreadMsgList::keyname, pMsgHeadCS->m_nSrcUin));

	return 0;
}

int32_t CMsgFinAckHandler::OnSessionGetUnreadMsgCount(int32_t nResult, void *pReply, void *pSession)
{
	redisReply *pRedisReply = (redisReply *)pReply;
	RedisSession *pRedisSession = (RedisSession *)pSession;
	UserSession *pUserSession = (UserSession *)pRedisSession->GetSessionData();

	bool bIsSyncNoti = true;
	do
	{
		if(pRedisReply->type == REDIS_REPLY_ERROR)
		{
			bIsSyncNoti = false;
			break;
		}

		if(pRedisReply->type == REDIS_REPLY_INTEGER)
		{
			if(pRedisReply->integer <= 0)
			{
				bIsSyncNoti = false;
				break;
			}
		}
		else
		{
			bIsSyncNoti = false;
			break;
		}
	}while(0);

	if(bIsSyncNoti)
	{
		CRedisBank *pRedisBank = (CRedisBank *)g_Frame.GetBank(BANK_REDIS);
		CRedisChannel *pPushClientChannel = pRedisBank->GetRedisChannel(pUserSession->m_stCtlHead.m_nGateRedisAddress, pUserSession->m_stCtlHead.m_nGateRedisPort);

		CServerHelper::SendSyncNoti(pPushClientChannel, &pUserSession->m_stCtlHead, pUserSession->m_stMsgHeadCS.m_nSrcUin);
	}

	CRedisSessionBank *pRedisSessionBank = (CRedisSessionBank *)g_Frame.GetBank(BANK_REDIS_SESSION);
	pRedisSessionBank->DestroySession(pRedisSession);

	return 0;
}

int32_t CMsgFinAckHandler::OnRedisSessionTimeout(void *pTimerData)
{
	CRedisSessionBank *pRedisSessionBank = (CRedisSessionBank *)g_Frame.GetBank(BANK_REDIS_SESSION);
	RedisSession *pRedisSession = (RedisSession *)pTimerData;
	UserSession *pUserSession = (UserSession *)pRedisSession->GetSessionData();

	CRedisBank *pRedisBank = (CRedisBank *)g_Frame.GetBank(BANK_REDIS);

	pRedisSessionBank->DestroySession(pRedisSession);
	return 0;
}


