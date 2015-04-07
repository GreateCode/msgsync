/*
 * msgfinack_handler.h
 *
 *  Created on: 2015年4月6日
 *      Author: jimm
 */

#ifndef LOGIC_MSGFINACK_HANDLER_H_
#define LOGIC_MSGFINACK_HANDLER_H_

#include "../../common/common_object.h"
#include "../../frame/frame_impl.h"
#include "../../frame/redis_session.h"
#include "../../include/control_head.h"
#include "../../include/sync_msg.h"
#include "../../include/msg_head.h"
#include <string>

using namespace std;
using namespace FRAME;

class CMsgFinAckHandler : public CBaseObject
{
//	struct UserSession
//	{
//		UserSession()
//		{
//		}
//
//		ControlHead			m_stCtlHead;
//		MsgHeadCS			m_stMsgHeadCS;
//		CMsgFinAckReq		m_stMsgFinAckReq;
//	};

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

	int32_t MsgFinAck(ICtlHead *pCtlHead, IMsgHead *pMsgHead, IMsgBody *pMsgBody, uint8_t *pBuf, int32_t nBufSize);
};


#endif /* LOGIC_MSGFINACK_HANDLER_H_ */
