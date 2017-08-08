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
 * $Id:
 ******************************************************************************/
#include <idl.h>
#include <ideErrorMgr.h>
#include <ideMsgLog.h>
#include <idErrorCode.h>
#include <iduSessionEvent.h>
#include <idrLogMgr.h>
#include <iduVLogShmLatch.h>
#include <iduProperty.h>
#include <iduShmLatch.h>
#include <iduFitManager.h>

IDE_RC iduShmLatchTryAcquire( idvSQL       * /*aStatistics*/,
                              iduShmTxInfo * aShmTxInfo,
                              iduShmLatch  * aLatch,
                              idBool         aPushLatchOP2Stack,
                              idBool       * aLocked )
{
    SInt              sLatchValue;
    UInt              sLstLatchStackIdx;

    sLatchValue = ( aShmTxInfo->mThrID << 1 | 0x00000001 );

    if( aPushLatchOP2Stack == ID_TRUE )
    {
        IDE_TEST( idrLogMgr::pushLatchOp( aShmTxInfo,
                                          aLatch,
                                          IDU_SHM_LATCH_MODE_EXCLUSIVE,
                                          NULL /* aPushLatchOP2Stack */ )
                  != IDE_SUCCESS );
    }

    if( iduShmLatchTryLock( aLatch, sLatchValue ) == ID_TRUE )
    {
        *aLocked = ID_TRUE;
    }
    else
    {
        if( aPushLatchOP2Stack == ID_TRUE )
        {
            sLstLatchStackIdx = idrLogMgr::getLatchCntInStack( aShmTxInfo );

            idrLogMgr::clearLatchStack( aShmTxInfo,
                                        sLstLatchStackIdx,
                                        ID_FALSE );
        }

        *aLocked = ID_FALSE;
    }

    IDU_FIT_POINT( "iduShmLatch::iduShmLatchAcquire::return" );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * locks a spinlock
 * @param aLock the spinlock object
 */
IDE_RC iduShmLatchAcquire( idvSQL       * aStatistics,
                           iduShmTxInfo * aShmTxInfo,
                           iduShmLatch  * aLock,
                           idBool         aCheckSession,
                           idBool         aPushLatchOP2Stack )
{
    SInt              sSpinLoop;
    SInt              sSpinCount;
    idBool            sIsMyLock;
    SInt              sLatchValue;
    iduShmProcInfo  * sProcInfo;
    UInt              sSleepCnt = 0;
    UInt              i;
    PDL_Time_Value    sSleepDuration;

    /* set up conditions */
    sIsMyLock      = ID_FALSE;

    sProcInfo    = (iduShmProcInfo*)IDV_WP_GET_PROC_INFO( aStatistics );

    static iduShmTxInfo * sMainThrInfo = &sProcInfo->mMainTxInfo;

    sLatchValue  = ( aShmTxInfo->mThrID << 1 | 0x00000001 );

    IDE_ASSERT( aLock->mSpinCount >= 0 );

    if( aLock->mSpinCount == 0 )
    {
        sSpinCount = IDU_SPIN_WAIT_DEFAULT_SPIN_COUNT;
    }
    else
    {
        sSpinCount = aLock->mSpinCount;
    }

    sSleepDuration.set( 0, iduProperty::getShmLatchSleepDuration() );

    /* MainThrInfo�� ���ÿ� �������� Thread�� ������ �� �ֱ� ������
     * MainThrInfo�� Latch Stack�� ������� �ʴ´�. �̶����� Latch��
     * ��� ���� ��� Ǯ���� ����� ����. ������ MainThrInfo�� ���
     * Latch�� ThreadInfo Alloc�Ҷ� ��� Latch * �� �����̴�.
     * ������ Process Undo�� ���� MainThrInfo�� LogBuffer�ִ� Log�� Undo�ϰ�
     * Thread Info Alloc Latch�� Ǫ�� ������ ������ �õ��Ѵ�.
     * ���� ��� �׾����� Latch�� Ǯ����, �����ʾ��� ��쿡�� LatchRelease
     * ���� �ƹ��͵� ���� �ʵ��� �����Ǿ� �ִ�.
     * */
    if( ( aLock              != &sProcInfo->mLatch ) &&
        ( aPushLatchOP2Stack == ID_TRUE ) )
    {
        IDE_TEST( idrLogMgr::pushLatchOp( aShmTxInfo,
                                          aLock,
                                          IDU_SHM_LATCH_MODE_EXCLUSIVE,
                                          NULL /* aNewLatchInfo */ )
                  != IDE_SUCCESS );
    }

    while(1)
    {
        /* Only read lock variable from hardware-cache or memory
         * to reduce system-bus locking.
         */
        for( sSpinLoop = 0;
             sSpinLoop <= sSpinCount; /* sSpinCount can be 0 in UP! */
             sSpinLoop++ )
        {
            /* for cache hit, more frequency case is first */
            if( iduShmLatchIsLocked( aLock ) == ID_FALSE )
            {
                if( iduShmLatchTryLock( aLock, sLatchValue ) == ID_TRUE )
                {
                    /* success to lock */
                    /* return; is better than goto for recucing jump */
                    sIsMyLock = ID_TRUE;

                    break;
                }
                else
                {
                    /* Other process takes the lock.
                     * The process takes time in the critical section,
                     * therefore I better to sleep, not try lock.
                     * */
                    break;
                }
            }
            else
            {
                // ���� Thread�� AllocShmTx�� ���ÿ� ��û�ϰ� �Ǹ�
                // ���ü� ��� ���� �� �����Ƿ� MainShmTx�� Latch��
                // �ߺ��ؼ� ���� �� ����.
                if( sMainThrInfo != aShmTxInfo )
                {
                    if( aLock->mLock == sLatchValue )
                    {
                        /* try acquire���� ����������, �ڽ��� �̹�
                         * ��� �ִ� lock�̹Ƿ� recursive lock��
                         * ȹ�� */
                        sIsMyLock = ID_TRUE;
                        aLock->mRecursive++;
                        break;
                    }
                }
            }
        }

        if( sIsMyLock == ID_TRUE )
        {
            /* I win the lock */
            break;
        }
        else
        {
            if( aCheckSession == ID_TRUE )
            {
                IDE_TEST( iduCheckSessionEvent( aStatistics )
                          != IDE_SUCCESS );
            }

            /* sleep and try again to yield chance for others.
             * This can avoid excessive racing condition.
             */
            sSleepCnt++;
            
            if( sSleepCnt < iduProperty::getShmLatchYieldCount() )
            {
                idlOS::thr_yield();
            }
            else
            {
                idlOS::sleep(sSleepDuration);
                sSleepCnt = 0;
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * locks a spinlock
 * @param aLock the spinlock object
 * @param aLockWaitMicroSec lock timeout
 */
IDE_RC iduShmLatchAcquire( idvSQL            * aStatistics,
                           iduShmTxInfo      * aShmTxInfo,
                           iduShmLatch       * aLatch,
                           idBool              aCheckSession,
                           idBool              aPushLatchOP2Stack,
                           ULong               aLockWaitMicroSec,
                           ULong             * aWaitedMicroSec )
{
    PDL_Time_Value   sMinTime;
    idBool           sLatchAcquired     = ID_FALSE;
    idBool           sLatchStackClear   = ID_FALSE;
    UInt             sLstLatchStackIdx;
    iduShmProcInfo * sProcInfo;
    iduShmTxInfo   * sMainThrInfo;
    SInt             sLatchValue;
    ULong            sSleepTime;

    if( aLockWaitMicroSec == IDU_SHM_SX_LATCH_INFINITE_WAIT )
    {
        /* lock wait time�� infinite�� ��쿡��
         * ������ blocking acquire�Լ��� ȣ���ϵ��� �Ѵ�. */
        IDE_TEST( iduShmLatchAcquire( aStatistics,
                                      aShmTxInfo,
                                      aLatch,
                                      aCheckSession,
                                      aPushLatchOP2Stack )
                  != IDE_SUCCESS );
    }
    else
    {
        sProcInfo = (iduShmProcInfo*)IDV_WP_GET_PROC_INFO( aStatistics );

        sMainThrInfo = &sProcInfo->mMainTxInfo;

        sLatchValue  = ( aShmTxInfo->mThrID << 1 | 0x00000001 );

        sMinTime.set( 0, IDU_SPIN_WAIT_SLEEP_TIME_MAX );

        sSleepTime = 0;

        if( aPushLatchOP2Stack == ID_TRUE )
        {
            IDE_TEST( idrLogMgr::pushLatchOp( aShmTxInfo,
                                              aLatch,
                                              IDU_SHM_LATCH_MODE_EXCLUSIVE,
                                              NULL )
                      != IDE_SUCCESS );

            sLatchStackClear = ID_TRUE;
        }

        do
        {
            if( iduShmLatchTryLock( aLatch, sLatchValue ) == ID_TRUE )
            {
                sLatchAcquired = ID_TRUE;
                sLatchStackClear = ID_FALSE;
                break;
            }
            else
            {
                if( sMainThrInfo != aShmTxInfo )
                {
                    if( aLatch->mLock == sLatchValue )
                    {
                        /* try acquire���� ����������, �ڽ��� �̹�
                         * ��� �ִ� lock�̹Ƿ� recursive lock��
                         * ȹ�� */
                        sLatchAcquired = ID_TRUE;
                        aLatch->mRecursive++;
                        break;
                    }
                }

                idlOS::sleep( sMinTime );

                sSleepTime += IDU_SPIN_WAIT_SLEEP_TIME_MAX;
            }
        } while( sSleepTime < aLockWaitMicroSec );

        if( aWaitedMicroSec != NULL )
        {
            *aWaitedMicroSec = sSleepTime;
        }

        IDE_TEST_RAISE( sLatchAcquired == ID_FALSE, err_lock_timeout );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_lock_timeout );
    {
        IDE_SET( ideSetErrorCode( idERR_ABORT_LATCH_TIMEOUT ) );
    }
    IDE_EXCEPTION_END;

    if( sLatchStackClear == ID_TRUE )
    {
        sLstLatchStackIdx = idrLogMgr::getLatchCntInStack( aShmTxInfo );

        idrLogMgr::clearLatchStack( aShmTxInfo,
                                    sLstLatchStackIdx,
                                    ID_FALSE );
    }

    return IDE_FAILURE;
}

/**
 * Unlocks a spinlock object.  iduShmLatch supports recursive locking.
 * This means if a thread locks a latch multiple times, it has to
 * unlock the latch the same number of times before the latch is
 * finally released.
 *
 * @param aLock the spinlock object
 */
IDE_RC iduShmLatchReleaseByUndo( UInt          aThrID,
                                 iduShmLatch * aLatch )
{
    SInt sLatchValue;

    ACP_MEM_BARRIER();

    sLatchValue = ( aThrID << 1 | 0x00000001 );

    if ( ( aLatch->mRecursive > 0 ) && ( aLatch->mLock == sLatchValue ) )
    {
        aLatch->mRecursive--;
    }
    else
    {
        (void)idCore::acpAtomicCas32( &aLatch->mLock, 0, sLatchValue );
    }

    return IDE_SUCCESS;
}

/*******************************************************************************
 * [!] �� �Լ��� �ٷ� ȣ���ؼ� �ȵȴ�.
 *     �ݵ�� idrLogMgr::xxx2Svp�Լ��� ���� ȣ���ؾ� �Ѵ�.
 *     ����(2014.02.28) ������ ������ �����
 *     Latch�� release ���� ShmRecovery�� Hang�� �ɸ� �� �ִ�.
 *******************************************************************************/
IDE_RC iduShmLatchRelease( iduShmTxInfo * aShmTxInfo,
                           iduShmLatch  * aLatch )
{
    iduShmLatchReleaseByUndo( aShmTxInfo->mThrID, aLatch );

    return IDE_SUCCESS;
}
