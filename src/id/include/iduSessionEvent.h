/***********************************************************************
 * Copyright 1999-2001, ALTIBase Corporation or its subsidiaries.
 * All rights reserved.
 **********************************************************************/

/***********************************************************************
 * $Id: iduSessionEvent.h 64386 2014-04-09 01:34:40Z raysiasd $
 **********************************************************************/

#ifndef _O_IDU_SESSION_EVENT_H_
# define _O_IDU_SESSION_EVENTH_ 1

#include <idv.h>

#define IDU_SESSION_EVENT_MASK            ID_ULONG(0xFFFFFFFFFFFFFFFF)
#define IDU_SESSION_EVENT_CLOSED          ID_ULONG(0x0000000100000000)
#define IDU_SESSION_EVENT_TIMEOUT         ID_ULONG(0x00000000FFFFFFFF)
#define IDU_SESSION_EVENT_BLOCK           ID_ULONG(0x0000001000000000)
#define IDU_SESSION_EVENT_CANCELED        ID_ULONG(0x0000010000000000)

#define IDU_SESSION_SET_CLOSED(a)         ((a) |= IDU_SESSION_EVENT_CLOSED)
#define IDU_SESSION_CLR_CLOSED(a)         ((a) &= (~IDU_SESSION_EVENT_CLOSED))
#define IDU_SESSION_CHK_CLOSED(a)         (((a) & IDU_SESSION_EVENT_CLOSED) > 0)

#define IDU_SESSION_SET_TIMEOUT(a, num)   ( (a)  = ((a) & (~IDU_SESSION_EVENT_TIMEOUT)) | (num))
#define IDU_SESSION_CLR_TIMEOUT(a)        ( (a) &= (~IDU_SESSION_EVENT_TIMEOUT))
#define IDU_SESSION_CHK_TIMEOUT(a, num)   (((a) & IDU_SESSION_EVENT_TIMEOUT) == (num))
#define IDU_SESSION_CHK_TIMEOUT_EMPTY(a)  (((a) & IDU_SESSION_EVENT_TIMEOUT) == 0)

#define IDU_SESSION_SET_BLOCK(a)          ((a) |= IDU_SESSION_EVENT_BLOCK)
#define IDU_SESSION_CLR_BLOCK(a)          ((a) &= (~IDU_SESSION_EVENT_BLOCK))

#define IDU_SESSION_SET_CANCELED(a)       ((a) |= IDU_SESSION_EVENT_CANCELED)
#define IDU_SESSION_CLR_CANCELED(a)       ((a) &= (~IDU_SESSION_EVENT_CANCELED))
#define IDU_SESSION_CHK_CANCELED(a)       (((a) & IDU_SESSION_EVENT_CANCELED) > 0)


/*----------------------------------------------------------------------
  Name:
      iduCheckSessionEvent() -- Session�� ���� Event �˻� �ڵ�

  Argument:
      aFlag - Event Pointer

  Return:
      IDE_RC ����(No Event), ����(Event Occurred)

  Description:
      �� �Լ��� �������� ȣ���Ͽ�, ������ Event�� �����ϴ�
      �����μ� Close, Timeout Event�� �����Ѵ�.
 *--------------------------------------------------------------------*/

IDE_RC iduSessionErrorFilter( ULong aFlag, UInt aCurrStmtID );

inline IDE_RC iduCheckSessionEvent(idvSQL *aStatistics)
{
    ULong * sFlag;
    UInt  * sCurrStmtID;

    if (aStatistics != NULL)
    {
        sFlag = IDV_SESSION_EVENT(aStatistics);

        if ((sFlag != NULL) && ((*sFlag & IDU_SESSION_EVENT_BLOCK) == 0))
        {
            if (!IDU_SESSION_CHK_CLOSED(*sFlag))
            {
                if ((aStatistics->mLink           != NULL) &&
                    (aStatistics->mLinkCheckTime  != NULL) &&
                    (*aStatistics->mLinkCheckTime != idvManager::getLinkCheckTime()))
                {
                    *aStatistics->mLinkCheckTime   = idvManager::getLinkCheckTime();

                    if (idvManager::mLinkCheckCallback(aStatistics->mLink) != IDE_SUCCESS)
                    {
                        IDU_SESSION_SET_CLOSED(*sFlag);

                        IDE_SET(ideSetErrorCode(idERR_ABORT_Session_Disconnected));

                        return IDE_FAILURE;
                    }
                }
            }

            if (*sFlag != 0)
            {
                sCurrStmtID = IDV_CURRENT_STATEMENT_ID( aStatistics );

                if ( sCurrStmtID != NULL )
                {
                    return iduSessionErrorFilter( *sFlag, *sCurrStmtID );
                }
                else
                {
                    return iduSessionErrorFilter( *sFlag, 0 );
                }
            }
        }
    }

    return IDE_SUCCESS;
}

#endif /* _O_IDU_H_ */
