/** 
 *  Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License, version 3,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
 
/***********************************************************************
 * $Id$
 **********************************************************************/

#include <idl.h>
#include <ideErrorMgr.h>
#include <idErrorCode.h>
#include <ideMsgLog.h>
#include <idErrorCode.h>
#include <iduShmDef.h>
#include <iduShmKeyMgr.h>

iduShmSSegment  * iduShmKeyMgr::mSSegment;

iduShmKeyMgr::iduShmKeyMgr()
{
}

iduShmKeyMgr::~iduShmKeyMgr()
{
}

/*
   ���� ����� �����޸� Key �ĺ��� ã�� �����Ѵ�.

   �ĺ��� ���� :
     �ش� Key�� �̿��Ͽ� �� �����޸� ������ ������ �� ���� ���� �ְ�
     �̹� �ش� Key�� �����޸� ������ �����Ǿ� ���� ���� �ֱ� ����.

   aShmKeyCandidate [OUT] 0 : �����޸� Key �ĺ��� ����
                          Otherwise : ����� �� �ִ� �����޸� Key �ĺ�

   PROJ-1548 Memory Tablespace
   - mSeekKey�� ���� ���ü� ��� �ϴ� ����
     - mSeekKey�� ���� Tx�� ���ÿ� ������ �� �ִ�.
     - mSeekKey�� ���� Tablespace���� ���ÿ� ������ �� �ִ�.

  - �����ϸ� ������ �ٸ� Tablespace���� ���Ǿ��� Key��
     ��Ȱ���Ͽ� ����Ѵ�.
*/
IDE_RC iduShmKeyMgr::getShmKeyCandidate( key_t * aShmKeyCandidate )
{
    IDE_ASSERT( mSSegment != NULL );
    IDE_ASSERT( aShmKeyCandidate != NULL );

    // ����� �� �ִ� Key�ĺ��� �ִ°�?
    if( mSSegment->mNxtShmKey > IDU_MIN_SHM_KEY_CANDIDATE )
    {
        *aShmKeyCandidate = --mSSegment->mNxtShmKey;
    }
    else /* Rotate shm key */
    {
        mSSegment->mNxtShmKey = IDU_MAX_SHM_KEY_CANDIDATE;
        *aShmKeyCandidate = mSSegment->mNxtShmKey;
    }

    return IDE_SUCCESS;

#if 0
    IDE_EXCEPTION( error_no_more_key );
    {
        IDE_SET( ideSetErrorCode( idERR_ABORT_No_More_Shm_Key ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
#endif
}

/*
 * shmShmKeyMgr�� static �ʱ�ȭ ����
 */
IDE_RC iduShmKeyMgr::initializeStatic( iduShmSSegment *aSSegment )
{
    mSSegment = aSSegment;

    return IDE_SUCCESS;
}

/*
 * shmShmKeyMgr�� static �ı� ����
 */
IDE_RC iduShmKeyMgr::destroyStatic()
{
    mSSegment = NULL;

    return IDE_SUCCESS;
}


