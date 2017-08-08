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
#include <idu.h>
#include <ideErrorMgr.h>
#include <idErrorCode.h>
#include <iduVLogShmMemPool.h>
#include <ideMsgLog.h>

/*
 * �ش� mem pool�� mem pool mgr�� ���ؼ� �����Ǿ����� �ִ°�?
 */
#define IDU_ADD_OR_DEL_MEMPOOL ( ( iduMemMgr::isServerMemType() == ID_TRUE )   && \
                                 ( mUseMutex == ID_TRUE) )

/* ------------------------------------------------

   MemPool ������ ���� ( 2005.3.30 by lons )


   ����
   mempool�� �Ź� malloc system call�� ȣ������ �ʰ�
   memory�� �Ҵ��ϱ� ���� ������ ���� Ŭ�����̴�.
   �ϳ��� mempool���� �Ҵ��� �� �� �ִ� �޸��� ũ��� �׻� �����ȴ�.
   ���� �޸� ũ�⸦ �Ҵ��ϰ��� �Ѵٸ� �ٸ� ũ���
   init�� mempool�� 2�� �̻� ������ �Ѵ�.


   �������( class )
   iduShmMemPool : mem list�� �����Ѵ�. �ϳ��� mem pool���� mem list��
   �ټ����� �� �ִ�. �̰��� system cpu ������ ��������.
   iduShmMemList : ���� memory�� �Ҵ��Ͽ� Ȯ���ϰ� �ִ� list
   memChunk�� ����Ʈ�� �����ȴ�.
   iduStShmMemChunk : scuMemSlot list�� �����Ѵ�. mempool���� �Ҵ�����
   �޸𸮰� ���ڶ������� �ϳ��� �߰��ȴ�.
   scuMemSlot�� free count, max count�� �����Ѵ�.
   scuMemSlot : ���� �Ҵ����� �޸�

   -------      -------
   | chunk | -  | chunk | - ...
   -------      -------
   |
   ---------      ---------
   | element |  - | element | - ....
   ---------      ---------

   (eg.)

   iduShmMemPool::initialize( cpu����, ID_SIZEOF(UInt), 5 )�� �����ϸ�
   �ϳ��� chunk�� 5���� element�� �����.
   element�� ������ � ��ŭ�� �޸𸮸� �̸� �Ҵ��ϴ��ĸ� �����Ѵ�.

   -------
   | chunk |
   -------
   |
   ---------      ---------
   | element |  - | element | - ...
   ---------      ---------

   iduShmMemPool::alloc�� �����ϸ�
   element �ϳ��� return �ȴ�.
   ���� element�� �����ϸ� chunk �ϳ��� element 5���� �� �Ҵ��Ѵ�.
   * ----------------------------------------------*/
/*-----------------------------------------------------------
 * Description: iduShmMemPool�� �ʱ�ȭ �Ѵ�.
 *
 * aIndex            - [IN] �� �Լ��� ȣ���� ���
 *                        (��� ȣ���ϴ��� ������ �����ϱ� ���� �ʿ�)
 * aName             - [IN] iduShmMemPool�� �ĺ� �̸�
 * aListCount
 * aElemSize         - [IN] �Ҵ��� �޸��� ����ũ��
 * aElemCount        - [IN] �� chunk���� momory element�� ����,
 *                          ��, chunk���� slot�� ����
 * aChunkLimit       - [IN] memPool���� ������ chunk������ �ִ�ġ.
 *             (�� ��� chunk�� �� �Ҵ����� ��쿡�� �� ������ ���Ե��� �ʴ´�.)
 * aUseLatch         - [IN] mutex��� ����
 * aAlignByte        - [IN] aline�Ǿ� �ִ� �޸𸮸� �Ҵ�ް� ������, ���ϴ� align
 *                          ���� �־ �Լ��� ȣ���Ѵ�.
 * aForcePooling     - [IN] property���� mem pool�� ������� �ʵ��� �����ߴ���
 *                          ������ ����.(BUG-21547)
 * aGarbageCollection - [IN] Garbage collection�� �⿩�ϵ��� �Ұ��ΰ� ����.
 *                           ID_TRUE�϶� mem pool manager�� ���ؼ� �����Ǿ���
 * ---------------------------------------------------------*/
IDE_RC iduShmMemPool::initialize( idvSQL              * aStatistics,
                                  iduShmTxInfo        * aShmTxInfo,
                                  iduMemoryClientIndex  aIndex,
                                  SChar               * aName,
                                  UInt                  aListCount,
                                  vULong                aElemSize,
                                  vULong                aElemCount,
                                  vULong                aChunkLimit,
                                  idBool                aUseLatch,
                                  UInt                  aAlignByte,
                                  idBool                aGarbageCollection,
                                  idShmAddr             aAddr4MemPool,
                                  iduStShmMemPool     * aMemPoolInfo )
{
    UInt              i = 0;
    idShmAddr         sAddr4MemLst;
    iduStShmMemList * sArrMemListInfo;
    idrSVP            sSavepoint;

    IDE_ASSERT( aName != NULL );

    idrLogMgr::setSavepoint( aShmTxInfo, &sSavepoint );

    IDE_TEST_RAISE( aElemCount == 0 , err_invalid_chunksize );

    aMemPoolInfo->mAddrSelf = aAddr4MemPool;

    idlOS::snprintf( aMemPoolInfo->mName,
                     IDU_SHM_MEM_POOL_NAME_LEN,
                     "%s",
                     aName );

    IDU_SHM_VALIDATE_ADDR_PTR( aAddr4MemPool, aMemPoolInfo );

    iduShmLatchInit( IDU_SHM_GET_ADDR_SUBMEMBER( aAddr4MemPool,
                                                 iduStShmMemPool,
                                                 mLatch),
                     IDU_SHM_LATCH_SPIN_COUNT_DEFAULT,
                     &aMemPoolInfo->mLatch );

    IDU_SHM_VALIDATE_ADDR_PTR( aMemPoolInfo->mLatch.mAddrSelf,
                               &aMemPoolInfo->mLatch );

    aMemPoolInfo->mUseMutex  = aUseLatch;
    aMemPoolInfo->mListCount = aListCount;
    aMemPoolInfo->mIndex     = aIndex;
    aMemPoolInfo->mAllocIdx  = 0;

    aMemPoolInfo->mGarbageCollection = aGarbageCollection;
    aMemPoolInfo->mAddrNxtPool       = IDU_SHM_NULL_ADDR;

    IDE_TEST( iduShmMgr::allocMem( aStatistics,
                                   aShmTxInfo,
                                   aIndex,
                                   aMemPoolInfo->mListCount * ID_SIZEOF(iduStShmMemList),
                                   (void**)&sArrMemListInfo )
              != IDE_SUCCESS );


    aMemPoolInfo->mAddrArrMemList = iduShmMgr::getAddr( (void*)sArrMemListInfo );

    sAddr4MemLst = aMemPoolInfo->mAddrArrMemList;

    for( i = 0; i < aMemPoolInfo->mListCount; i++ )
    {
        IDE_TEST( iduShmMemList::initialize(
                      aStatistics,
                      aShmTxInfo,
                      aMemPoolInfo->mIndex,
                      i,
                      aName,
                      aElemSize,
                      aElemCount,
                      aChunkLimit,
                      aUseLatch,
                      aAlignByte,
                      sAddr4MemLst + i * ID_SIZEOF(iduStShmMemList),
                      sArrMemListInfo + i )
                  != IDE_SUCCESS );
    }

    IDE_TEST( iduVLogShmMemPool::writeInitialize( aShmTxInfo,
                                                  sSavepoint.mLSN,
                                                  aMemPoolInfo )
              != IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION( err_invalid_chunksize );
    {
        IDE_SET( ideSetErrorCode( idERR_ABORT_idaNullValue ) );
    }
    IDE_EXCEPTION_END;

    IDE_ASSERT( idrLogMgr::abort2Svp( aStatistics, aShmTxInfo, &sSavepoint )
                == IDE_SUCCESS );

    return IDE_FAILURE;
}

IDE_RC iduShmMemPool::destroy( idvSQL          * aStatistics,
                               iduShmTxInfo    * aShmTxInfo,
                               idrSVP          * aSavepoint,
                               iduStShmMemPool * aMemPoolInfo,
                               idBool            aCheck )
{
    iduStShmMemList * sArrMemListInfo;
    idrSVP            sSavepoint;

    idrLogMgr::setSavepoint( aShmTxInfo, &sSavepoint );

    if( aSavepoint == NULL )
    {
        aSavepoint = &sSavepoint;
    }

    sArrMemListInfo =
        (iduStShmMemList*)IDU_SHM_GET_ADDR_PTR_CHECK( aMemPoolInfo->mAddrArrMemList );

    while( aMemPoolInfo->mListCount != 0 )
    {
        IDE_TEST( iduVLogShmMemPool::writeSetListCnt( aShmTxInfo,
                                                      aMemPoolInfo )
                  != IDE_SUCCESS );

        aMemPoolInfo->mListCount--;


        IDE_TEST( iduShmMemList::destroy( aStatistics,
                                          aShmTxInfo,
                                          aSavepoint,
                                          sArrMemListInfo + aMemPoolInfo->mListCount,
                                          aCheck )
                  != IDE_SUCCESS );
    }

    if( aMemPoolInfo->mAddrArrMemList != IDU_SHM_NULL_ADDR )
    {
        IDE_TEST( iduVLogShmMemPool::writeSetArrMemList( aShmTxInfo,
                                                         aMemPoolInfo )
                  != IDE_SUCCESS );

        aMemPoolInfo->mAddrArrMemList = IDU_SHM_NULL_ADDR;


        IDE_TEST( iduShmMgr::freeMem( aStatistics,
                                      aShmTxInfo,
                                      aSavepoint,
                                      sArrMemListInfo ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_ASSERT( idrLogMgr::abort2Svp( aStatistics,
                                      aShmTxInfo,
                                      &sSavepoint ) == IDE_SUCCESS );

    return IDE_FAILURE;
}

IDE_RC iduShmMemPool::cralloc( idvSQL          * aStatistics,
                               iduShmTxInfo    * aShmTxInfo,
                               iduStShmMemPool * aMemPoolInfo,
                               idShmAddr       * aAddr4Mem,
                               void           ** aMem )
{
    SInt              i = 0;
    iduStShmMemList * sCurMemListInfo;
    idrSVP            sSavepoint;

    idrLogMgr::setSavepoint( aShmTxInfo, &sSavepoint );

    i  = idCore::acpAtomicInc32( &aMemPoolInfo->mAllocIdx )
         % aMemPoolInfo->mListCount;

    sCurMemListInfo = getArrMemList( aMemPoolInfo ) + i;

    if( aMemPoolInfo->mUseMutex == ID_TRUE )
    {
        iduShmMemList::lock( aStatistics, aShmTxInfo, sCurMemListInfo );

        IDE_TEST( iduShmMemList::cralloc( aStatistics,
                                          aShmTxInfo,
                                          sCurMemListInfo,
                                          aAddr4Mem,
                                          aMem )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( iduShmMemList::cralloc( aStatistics,
                                          aShmTxInfo,
                                          sCurMemListInfo,
                                          aAddr4Mem,
                                          aMem )
                  != IDE_SUCCESS );
    }

    IDE_TEST( idrLogMgr::releaseLatch2Svp( aStatistics,
                                           aShmTxInfo,
                                           &sSavepoint )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_ASSERT( idrLogMgr::abort2Svp( aStatistics,
                                      aShmTxInfo,
                                      &sSavepoint )
                == IDE_SUCCESS );

    return IDE_FAILURE;
}

IDE_RC iduShmMemPool::alloc( idvSQL          * aStatistics,
                             iduShmTxInfo    * aShmTxInfo,
                             iduStShmMemPool * aMemPoolInfo,
                             idShmAddr       * aAddr4Mem,
                             void           ** aMem )
{
    SInt              i;
    iduStShmMemList * sMemList;
    idrSVP            sSavepoint;

    idrLogMgr::setSavepoint( aShmTxInfo, &sSavepoint );

    // fix BUG-21547
    //i  = idlOS::getParallelIndex() % aMemPoolInfo->mListCount;
    i  = idCore::acpAtomicInc32( &aMemPoolInfo->mAllocIdx ) 
         % aMemPoolInfo->mListCount;

    sMemList = getArrMemList( aMemPoolInfo ) + i;

    if( aMemPoolInfo->mUseMutex == ID_TRUE )
    {
        iduShmMemList::lock( aStatistics, aShmTxInfo, sMemList );

        IDE_TEST( iduShmMemList::alloc( aStatistics, aShmTxInfo, sMemList, aAddr4Mem, aMem )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( iduShmMemList::alloc( aStatistics, aShmTxInfo, sMemList, aAddr4Mem, aMem )
                  != IDE_SUCCESS );
    }


    IDE_TEST( idrLogMgr::releaseLatch2Svp( aStatistics, aShmTxInfo, &sSavepoint )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_ASSERT( idrLogMgr::abort2Svp( aStatistics, aShmTxInfo, &sSavepoint )
                == IDE_SUCCESS );

    return IDE_FAILURE;
}

IDE_RC iduShmMemPool::memfree( idvSQL          * aStatistics,
                               iduShmTxInfo    * aShmTxInfo,
                               idrSVP          * aSavepoint,
                               iduStShmMemPool * aMemPoolInfo,
                               idShmAddr         aAddr4Mem,
                               void            * aMem )
{
    iduStShmMemList    * sCurListInfo = NULL;
    iduStShmMemChunk   * sCurChunk    = NULL;
    idrSVP               sSavepoint;

    idrLogMgr::setSavepoint( aShmTxInfo, &sSavepoint );

    if( aSavepoint == NULL )
    {
        aSavepoint = &sSavepoint;
    }

    sCurListInfo = getArrMemList( aMemPoolInfo );

    // fix BUG-21547
    // Memory Check Option������ �� ������ �����ؾ� ��.
    sCurChunk = (iduStShmMemChunk*)IDU_SHM_GET_ADDR_PTR_CHECK(
        *((idShmAddr *)((UChar *)aMem + sCurListInfo->mElemSize)) );

    sCurListInfo = (iduStShmMemList*)
        IDU_SHM_GET_ADDR_PTR_CHECK( sCurChunk->mAddrParent );

    if( aMemPoolInfo->mUseMutex == ID_TRUE )
    {
        iduShmMemList::lock( aStatistics, aShmTxInfo, sCurListInfo );

        IDE_TEST( iduShmMemList::memfree( aStatistics,
                                          aShmTxInfo,
                                          aSavepoint,
                                          sCurListInfo,
                                          aAddr4Mem,
                                          aMem )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( iduShmMemList::memfree( aStatistics,
                                          aShmTxInfo,
                                          aSavepoint,
                                          sCurListInfo,
                                          aAddr4Mem,
                                          aMem )
                  != IDE_SUCCESS );
    }

    IDE_TEST( idrLogMgr::commit2Svp( aStatistics, aShmTxInfo, aSavepoint )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_ASSERT( idrLogMgr::abort2Svp( aStatistics, aShmTxInfo, &sSavepoint )
                == IDE_SUCCESS );

    return IDE_FAILURE;
}

/*
 * PROJ-2065 Garbage collection
 *
 * ������� �ʴ� free chunk���� OS�� �ݳ��Ͽ� �޸𸮰����� Ȯ���Ѵ�.
 */
IDE_RC iduShmMemPool::shrink( idvSQL          * aStatistics,
                              iduShmTxInfo    * aShmTxInfo,
                              iduStShmMemPool * aMemPoolInfo,
                              UInt            * aSize )
{
    UInt               i;
    UInt               sSize  = 0;
    iduStShmMemList  * sArrListInfo = NULL;
    iduStShmMemList  * sCurListInfo = NULL;
    idrSVP             sSavepoint;

    *aSize = 0;

    idrLogMgr::setSavepoint( aShmTxInfo, &sSavepoint );

    sArrListInfo = getArrMemList( aMemPoolInfo );

    if( ( aMemPoolInfo->mGarbageCollection == ID_TRUE ) &&
        ( iduMemMgr::isServerMemType() == ID_TRUE) )
    {
        for (i = 0; i < aMemPoolInfo->mListCount; i++)
        {
            sCurListInfo = &sArrListInfo[i];

            IDE_ASSERT( sCurListInfo != NULL );

            if( aMemPoolInfo->mUseMutex == ID_TRUE )
            {
                iduShmMemList::lock( aStatistics, aShmTxInfo, sCurListInfo );
            }

            IDE_TEST( iduShmMemList::shrink( aStatistics,
                                             aShmTxInfo,
                                             sCurListInfo,
                                             &sSize )
                      != IDE_SUCCESS );

            *aSize += sSize;
        }
    }

    IDE_TEST( idrLogMgr::commit2Svp( aStatistics, aShmTxInfo, &sSavepoint )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_ASSERT( idrLogMgr::abort2Svp( aStatistics, aShmTxInfo, &sSavepoint )
                == IDE_SUCCESS );

    return IDE_FAILURE;
}

void iduShmMemPool::dumpState( idvSQL          * aStatistics,
                               iduShmTxInfo    * aShmTxInfo,
                               iduStShmMemPool * aMemPoolInfo,
                               SChar           * aBuffer,
                               UInt              aBufferSize )
{
    UInt               sSize = 0;
    UInt               i;
    iduStShmMemList  * sArrMemListInfo = NULL;
    idrSVP             sVSavepoint;

    idrLogMgr::setSavepoint( aShmTxInfo, &sVSavepoint );

    sArrMemListInfo = getArrMemList( aMemPoolInfo );

    for( i = 0; i < aMemPoolInfo->mListCount; i++ )
    {
        iduShmMemList::lock( aStatistics, aShmTxInfo, sArrMemListInfo + i );

        sSize += iduShmMemList::getUsedMemory( sArrMemListInfo + i );


        IDE_ASSERT( idrLogMgr::commit2Svp( aStatistics,
                                           aShmTxInfo,
                                           &sVSavepoint )
                    == IDE_SUCCESS );
    }

    idlOS::snprintf( aBuffer,
                     aBufferSize,
                     "MemPool Used Memory:%"ID_UINT32_FMT"\n",
                     sSize );
}

void iduShmMemPool::status( iduStShmMemPool * aMemPoolInfo )
{
    UInt  i;
    SChar sBuffer[IDE_BUFFER_SIZE];

    iduStShmMemList  * sArrMemListInfo = NULL;

    sArrMemListInfo = getArrMemList( aMemPoolInfo );

    // fix BUG-21547
    idlOS::snprintf(sBuffer,
                    ID_SIZEOF(sBuffer),
                    "  - Memory List Count:%5"ID_UINT32_FMT"\n",
                    (UInt)aMemPoolInfo->mListCount);
    IDE_CALLBACK_SEND_SYM_NOLOG(sBuffer);

    idlOS::snprintf(sBuffer,
                    ID_SIZEOF(sBuffer),
                    "  - Slot   Size:%5"ID_UINT32_FMT"\n",
                    (UInt)(sArrMemListInfo[0].mSlotSize));
    IDE_CALLBACK_SEND_SYM_NOLOG(sBuffer);

    for(i = 0; i < aMemPoolInfo->mListCount; i++)
    {
        idlOS::snprintf(sBuffer,
                        ID_SIZEOF(sBuffer),
                        "  - Memory List *** [%"ID_UINT32_FMT"]\n",
                        (UInt)(i + 1));
        IDE_CALLBACK_SEND_SYM_NOLOG(sBuffer);

        iduShmMemList::status( sArrMemListInfo + i );
    }
}

/* added by mskim for check memory statement */
ULong iduShmMemPool::getMemorySize( iduStShmMemPool * aMemPoolInfo )
{
    UInt               i;
    UInt               sMemSize        = 0;
    iduStShmMemList  * sArrMemListInfo = NULL;

    sArrMemListInfo = getArrMemList( aMemPoolInfo );

    // fix BUG-21547
    for( i = 0; i < aMemPoolInfo->mListCount; i++ )
    {
        sMemSize += iduShmMemList::getUsedMemory( sArrMemListInfo + i );
    }

    return sMemSize;
}

void iduShmMemPool::fillMemPoolInfo( iduStShmMemPool     * aMemPoolInfo,
                                     iduShmMemPoolState  * aArrMemPoolInfo )
{
    UInt i;

    iduStShmMemList  * sArrMemListInfo = NULL;

    sArrMemListInfo = getArrMemList( aMemPoolInfo );

    for(i = 0; i < aMemPoolInfo->mListCount; i++)
    {
        iduShmMemList::fillMemPoolInfo( sArrMemListInfo + i,
                                        aArrMemPoolInfo + i );
    }
}

void iduShmMemPool::dumpFreeChunk4TSM( iduStShmMemPool * aMemPoolInfo )
{
    UInt               i;
    iduStShmMemList  * sArrMemListInfo = NULL;

    sArrMemListInfo = getArrMemList( aMemPoolInfo );

    for( i = 0; i < aMemPoolInfo->mListCount; i++ )
    {
        idlOS::printf( "##[%"ID_INT32_FMT"] Chunk Dump Begin\n", i );
        iduShmMemList::dumpPartialFreeChunk4TSM( sArrMemListInfo + i );
        idlOS::printf( "##[%"ID_INT32_FMT"] Chunk Dump End\n", i );
    }
}

