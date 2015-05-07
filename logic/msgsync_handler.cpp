/*
 * msgsync_handler.cpp
 *
 *  Created on: 2015年4月6日
 *      Author: jimm
 */

#include "msgsync_handler.h"
#include "../../common/common_datetime.h"
#include "../../common/common_api.h"
#include "../../frame/frame.h"
#include "../../frame/server_helper.h"
#include "../../frame/redissession_bank.h"
#include "../../logger/logger.h"
#include "../../include/cachekey_define.h"
#include "../../include/control_head.h"
#include "../../include/typedef.h"
#include "../config/string_config.h"
#include "../server_typedef.h"
#include "../bank/redis_bank.h"

using namespace LOGGER;
using namespace FRAME;

int32_t CMsgSyncHandler::MsgSync(ICtlHead *pCtlHead, IMsgHead *pMsgHead, IMsgBody *pMsgBody, uint8_t *pBuf, int32_t nBufSize)
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
		CRedisChannel *pClientRespChannel = pRedisBank->GetRedisChannel(pControlHead->m_nGateID, CLIENT_RESP);

		return CServerHelper::KickUser(pControlHead, pMsgHeadCS, pClientRespChannel, KickReason_NotLogined);
	}

	CMsgSyncReq *pMsgSyncReq = dynamic_cast<CMsgSyncReq *>(pMsgBody);
	if(pMsgSyncReq == NULL)
	{
		return 0;
	}

	CRedisSessionBank *pRedisSessionBank = (CRedisSessionBank *)g_Frame.GetBank(BANK_REDIS_SESSION);
	RedisSession *pSession = pRedisSessionBank->CreateSession(this, static_cast<RedisReply>(&CMsgSyncHandler::OnSessionGetUnreadMsgList),
			static_cast<TimerProc>(&CMsgSyncHandler::OnRedisSessionTimeout));
	UserSession *pSessionData = new(pSession->GetSessionData()) UserSession();
	pSessionData->m_stCtlHead = *pControlHead;
	pSessionData->m_stMsgHeadCS = *pMsgHeadCS;
	pSessionData->m_stMsgSyncReq = *pMsgSyncReq;

	CRedisBank *pRedisBank = (CRedisBank *)g_Frame.GetBank(BANK_REDIS);
	UserUnreadMsgList *pUnreadMsgList = (UserUnreadMsgList *)g_Frame.GetConfig(USER_UNREADMSGLIST);
	CRedisChannel *pUnreadMsgChannel = pRedisBank->GetRedisChannel(pUnreadMsgList->string);
	pUnreadMsgChannel->ZRangeByScore(pSession, itoa(pMsgHeadCS->m_nSrcUin));

	return 0;
}

int32_t CMsgSyncHandler::OnSessionGetUnreadMsgList(int32_t nResult, void *pReply, void *pSession)
{
	redisReply *pRedisReply = (redisReply *)pReply;
	RedisSession *pRedisSession = (RedisSession *)pSession;
	UserSession *pUserSession = (UserSession *)pRedisSession->GetSessionData();

	CRedisSessionBank *pRedisSessionBank = (CRedisSessionBank *)g_Frame.GetBank(BANK_REDIS_SESSION);
	CStringConfig *pStringConfig = (CStringConfig *)g_Frame.GetConfig(CONFIG_STRING);

	CRedisBank *pRedisBank = (CRedisBank *)g_Frame.GetBank(BANK_REDIS);
	CRedisChannel *pMsgPushChannel = pRedisBank->GetRedisChannel(CLIENT_RESP);
	if(pMsgPushChannel == NULL)
	{
		WRITE_WARN_LOG(SERVER_NAME, "it's not found redis channel by msgid!{msgid=%d, srcuin=%u, dstuin=%u}\n", pUserSession->m_stMsgHeadCS.m_nMsgID,
				pUserSession->m_stMsgHeadCS.m_nSrcUin, pUserSession->m_stMsgHeadCS.m_nDstUin);
		pRedisSessionBank->DestroySession(pRedisSession);
		return 0;
	}

	uint8_t arrRespBuf[MAX_MSG_SIZE];

	if(pRedisReply->type == REDIS_REPLY_ARRAY)
	{
		MsgHeadCS stMsgHeadCS;
		stMsgHeadCS.m_nMsgID = MSGID_MSGPUSH_NOTI;
		stMsgHeadCS.m_nSeq = pUserSession->m_stMsgHeadCS.m_nSeq;
		stMsgHeadCS.m_nSrcUin = pUserSession->m_stMsgHeadCS.m_nDstUin;
		stMsgHeadCS.m_nDstUin = pUserSession->m_stMsgHeadCS.m_nSrcUin;

		for(size_t i = 0; i < pRedisReply->elements; ++i)
		{
			redisReply *pReplyElement = pRedisReply->element[i];
			if(pReplyElement->type != REDIS_REPLY_NIL)
			{
				CMsgPushNoti stMsgPushNoti;
				if(i == pRedisReply->elements - 1)
				{
					stMsgPushNoti.m_nSyncFlag = CMsgPushNoti::enmSyncFlag_Fin;
				}
				else
				{
					stMsgPushNoti.m_nSyncFlag = CMsgPushNoti::enmSyncFlag_Sync;
				}
				stMsgPushNoti.m_nSyncSeq = pUserSession->m_stMsgHeadCS.m_nSeq + i;
				stMsgPushNoti.m_nMsgSize = pReplyElement->len;
				memcpy(stMsgPushNoti.m_arrMsg, pReplyElement->str, pReplyElement->len);

				uint16_t nTotalSize = CServerHelper::MakeMsg(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stMsgPushNoti, arrRespBuf, sizeof(arrRespBuf));
				pMsgPushChannel->RPush(NULL, (char *)arrRespBuf, nTotalSize);

				g_Frame.Dump(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stMsgPushNoti, "send ");
			}
		}
	}while(0);

	pRedisSessionBank->DestroySession(pRedisSession);

	return 0;
}

int32_t CMsgSyncHandler::OnRedisSessionTimeout(void *pTimerData)
{
	CRedisSessionBank *pRedisSessionBank = (CRedisSessionBank *)g_Frame.GetBank(BANK_REDIS_SESSION);
	RedisSession *pRedisSession = (RedisSession *)pTimerData;
	UserSession *pUserSession = (UserSession *)pRedisSession->GetSessionData();

	CRedisBank *pRedisBank = (CRedisBank *)g_Frame.GetBank(BANK_REDIS);
//	CRedisChannel *pMsgPushChannel = pRedisBank->GetRedisChannel(pMsgDispatchConfig->GetChannelKey(MSGID_MSGPUSH_NOTI));
//	if(pMsgPushChannel == NULL)
//	{
//		WRITE_WARN_LOG(SERVER_NAME, "it's not found redis channel by msgid!{msgid=%d, srcuin=%u, dstuin=%u}\n", MSGID_MSGPUSH_NOTI,
//				pUserSession->m_stMsgHeadCS.m_nSrcUin, pUserSession->m_stMsgHeadCS.m_nDstUin);
//		pRedisSessionBank->DestroySession(pRedisSession);
//		return 0;
//	}
//
//	CStringConfig *pStringConfig = (CStringConfig *)g_Frame.GetConfig(CONFIG_STRING);
//
//	uint8_t arrRespBuf[MAX_MSG_SIZE];
//
//	MsgHeadCS stMsgHeadCS;
//	stMsgHeadCS.m_nMsgID = MSGID_MSGPUSH_NOTI;
//	stMsgHeadCS.m_nSeq = pUserSession->m_stMsgHeadCS.m_nSeq;
//	stMsgHeadCS.m_nSrcUin = pUserSession->m_stMsgHeadCS.m_nSrcUin;
//	stMsgHeadCS.m_nDstUin = pUserSession->m_stMsgHeadCS.m_nDstUin;
//
//	CChatToOneResp stChatToOneResp;
//	stChatToOneResp.m_nResult = CChatToOneResp::enmResult_Unknown;
//	stChatToOneResp.m_strTips = pStringConfig->GetString(stMsgHeadCS.m_nMsgID, stChatToOneResp.m_nResult);
//
//	uint16_t nTotalSize = CServerHelper::MakeMsg(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stChatToOneResp, arrRespBuf, sizeof(arrRespBuf));
//	pMsgPushChannel->RPush(NULL, (char *)arrRespBuf, nTotalSize);
//
//	g_Frame.Dump(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stChatToOneResp, "send ");

	pRedisSessionBank->DestroySession(pRedisSession);
	return 0;
}

