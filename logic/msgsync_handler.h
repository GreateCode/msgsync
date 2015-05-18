/*
 * msgsync_handler.h
 *
 *  Created on: 2015年4月6日
 *      Author: jimm
 */

#ifndef LOGIC_MSGSYNC_HANDLER_H_
#define LOGIC_MSGSYNC_HANDLER_H_

#include "common/common_object.h"
#include "frame/frame_impl.h"
#include "frame/redis_session.h"
#include "include/control_head.h"
#include "include/sync_msg.h"
#include "include/msg_head.h"
#include <string>

using namespace std;
using namespace FRAME;

class CMsgSyncHandler : public CBaseObject
{
	struct UserSession
	{
		UserSession()
		{
		}

		ControlHead			m_stCtlHead;
		MsgHeadCS			m_stMsgHeadCS;
		CMsgSyncReq			m_stMsgSyncReq;
	};

public:

	virtual int32_t Init()
	{
		return 0;
	}
	virtual int32_t Uninit()
	{
		return 0;
	}
	virtual int32_t GetSize()
	{
		return 0;
	}

	int32_t MsgSync(ICtlHead *pCtlHead, IMsgHead *pMsgHead, IMsgBody *pMsgBody, uint8_t *pBuf, int32_t nBufSize);

	int32_t OnSessionGetUnreadMsgList(int32_t nResult, void *pReply, void *pSession);

	int32_t OnRedisSessionTimeout(void *pTimerData);
};

#endif /* LOGIC_MSGSYNC_HANDLER_H_ */
