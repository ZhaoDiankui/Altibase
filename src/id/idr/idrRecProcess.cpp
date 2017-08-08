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
 
/*******************************************************************************
 * $Id$
 ******************************************************************************/
#include <ide.h>
#include <idl.h>
#include <idrLogMgr.h>
#include <idrRecProcess.h>
#include <idrRecThread.h>
#include <idw.h>

idrRecProcess::idrRecProcess()
        : idtBaseThread()
{

}

idrRecProcess::~idrRecProcess()
{

}

IDE_RC idrRecProcess::initialize( iduShmProcInfo  * aDeadProcInfo )
{
    mDeadProcInfo = aDeadProcInfo;
    return IDE_SUCCESS;
}

IDE_RC idrRecProcess::destroy()
{
    IDE_TEST( shutdown() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void idrRecProcess::run()
{
    UInt              i;
    iduShmTxInfo    * sCurDeadTxInfo;
    idrRecThread    * sArrThread;
    idShmAddr         sHeadAddr;
    idvSQL            sStatistics;
    iduShmTxInfo    * sTxInfo;
    iduShmListNode  * sThrListNode;
    UInt              sDeadThrCount;
    iduShmListNode  * sCurNode;

    idvManager::initSQL( &sStatistics, NULL /* Session */ );

    IDV_WP_SET_PROC_INFO( &sStatistics, mDeadProcInfo );
    IDV_WP_SET_THR_INFO( &sStatistics,  &mDeadProcInfo->mMainTxInfo );

    /* Main Thread�� ���ؼ� Recovery�� �Ǹ��� �����Ѵ�. �ֳ��ϸ� ProcInfo�� ����
     * Thread List ������ ���� ������ �߻��Ͽ��� �� �ֱ� �����̴�. */
    IDE_TEST( idrLogMgr::abort( &sStatistics,
                                &mDeadProcInfo->mMainTxInfo )
              != IDE_SUCCESS );

    /* ThrInfo�� ProcessInfo Latch�� ��� ���� ���, Log�� ���⶧���� ������
     * Latch�� Ǯ���ش�. */
    IDE_TEST( idwPMMgr::unlockProcess( &sStatistics, mDeadProcInfo ) != IDE_SUCCESS );

    sDeadThrCount = mDeadProcInfo->mThreadCnt;

    if( sDeadThrCount != 0 )
    {
        IDE_TEST( iduMemMgr::malloc(
                      IDU_MEM_SM_IN_MEMORY_RECOVERY,
                      ID_SIZEOF(idrRecThread) * mDeadProcInfo->mThreadCnt,
                      (void**)&sArrThread )
                  != IDE_SUCCESS );

        sThrListNode     = (iduShmListNode*)IDU_SHM_GET_ADDR_PTR(
            mDeadProcInfo->mMainTxInfo.mNode.mAddrNext );
        sCurDeadTxInfo  = (iduShmTxInfo*)iduShmList::getDataPtr( sThrListNode );

        sHeadAddr        = mDeadProcInfo->mMainTxInfo.mSelfAddr;

        /* ���� Process�� ���ؼ� Recovery Thread���� �����Ѵ�. */
        for( i = 0; i < sDeadThrCount; i++ )
        {
            IDE_ASSERT( sCurDeadTxInfo->mSelfAddr != sHeadAddr );

            new ( sArrThread + i ) idrRecThread;

            /* Process Log Buffer�� �ִ� Log�鿡 ���ؼ� Undo�� �����Ѵ�. */
            /* ��� Thread Info�� Free�Ѵ�. */

            IDE_ASSERT( sCurDeadTxInfo != &mDeadProcInfo->mMainTxInfo );

            IDE_TEST( sArrThread[i].initialize( sCurDeadTxInfo )
                      != IDE_SUCCESS );

            sThrListNode    = (iduShmListNode*)IDU_SHM_GET_ADDR_PTR(
                sCurDeadTxInfo->mNode.mAddrNext );

            sCurDeadTxInfo  = (iduShmTxInfo*)iduShmList::getDataPtr( sThrListNode );

            IDE_TEST( sArrThread[i].startThread() != IDE_SUCCESS );
        }

        sThrListNode    = (iduShmListNode*)IDU_SHM_GET_ADDR_PTR(
            mDeadProcInfo->mMainTxInfo.mNode.mAddrNext );
        sCurDeadTxInfo  = (iduShmTxInfo*)iduShmList::getDataPtr( sThrListNode );

        for( i = 0; i < sDeadThrCount; i++ )
        {
            IDV_WP_SET_THR_INFO( &sStatistics,  sCurDeadTxInfo );

            IDE_ASSERT( sCurDeadTxInfo->mSelfAddr != sHeadAddr );

            /* Process Log Buffer�� �ִ� Log�鿡 ���ؼ� Undo�� �����Ѵ�. */
            /* ��� Thread Info�� Free�Ѵ�. */
            IDE_TEST( sArrThread[i].shutdown() != IDE_SUCCESS );
            IDE_TEST( sArrThread[i].destroy() != IDE_SUCCESS );

            IDE_DASSERT( iduShmMgr::validateShmMgr( &sStatistics, &mDeadProcInfo->mMainTxInfo )
                         == IDE_SUCCESS );

            sThrListNode = (iduShmListNode*)IDU_SHM_GET_ADDR_PTR(
                sCurDeadTxInfo->mNode.mAddrNext );

            /* ThrInfo�� ProcessInfo���� �����Ѵ�. */
            IDE_TEST( idwPMMgr::freeThrInfo( &sStatistics, sCurDeadTxInfo )
                      != IDE_SUCCESS );

            sCurDeadTxInfo  = (iduShmTxInfo*)iduShmList::getDataPtr( sThrListNode );
        }
    }

    IDV_WP_SET_THR_INFO( &sStatistics, &mDeadProcInfo->mMainTxInfo );

    /* PROCINFO�� StStmtInfo�� ��ϵǾ� �ִ� ��쿡,
     * mmcStatementManager::removeStStmtInfo�� ���Ͽ� �ش� �޸𸮸�
     * �޸� ����Ʈ���� UNLINK�� �ϰ� POOL���� �����Ͽ� ���Ҵ��� �����ϵ��� �Ѵ�.
     *
     * ���� NOK �ּ� �Դϴ�.
     *     http://nok.altibase.com/display/rnd/BUG-41760+Working+Report
     */
    if (mDeadProcInfo->mLPID >= IDU_PROC_TYPE_USER)
    {
        if ((idwWatchDog::mStStmtRmFunc != NULL) &&
                    (idwWatchDog::mWatchDogStatus == ID_WATCHDOG_RUN))
        {
            (void)idwWatchDog::mStStmtRmFunc( &sStatistics,
                                              &mDeadProcInfo->mStStatementList,
                                              (void *)&idwWatchDog::mWatchDogStatus );
        }
        else
        {
            /* do nothing.*/
        }
    }
    else
    {
        /* do nothing. */
    }

    IDE_TEST( idwPMMgr::unRegisterProcByWatchDog( &sStatistics,
                                                  mDeadProcInfo )
              != IDE_SUCCESS );

    if( mDeadProcInfo->mRestartProcFunc != NULL )
    {
        IDV_WP_SET_MAIN_PROC_INFO( &sStatistics );

        IDE_TEST( idwPMMgr::allocThrInfo( &sStatistics,
                                          IDU_SHM_THR_LOGGING_ON,
                                          &sTxInfo )
                        != IDE_SUCCESS );

        IDE_TEST( mDeadProcInfo->mRestartProcFunc( &sStatistics )
                  != IDE_SUCCESS );

        IDE_TEST( idwPMMgr::freeThrInfo( &sStatistics,
                                         sTxInfo )
                  != IDE_SUCCESS );
    }

    return;

    IDE_EXCEPTION_END;

    IDE_SET( ideSetErrorCode( idERR_FATAL_UNDO_PROCESS_BY_WATCH_DOG ) );

    IDE_CALLBACK_FATAL( "Runtime InMemory Process Recovery Error " );

    return;
}
