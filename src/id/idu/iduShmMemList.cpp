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
#include <ideMsgLog.h>
#include <iduRunTimeInfo.h>
#include <iduVLogShmMemList.h>

extern const vULong gFence;

void iduCheckMemConsistency( iduStShmMemList * aStShmMemList )
{
    return;

    vULong            sCnt = 0;
    iduStShmMemSlot * sSlot;

    //check free chunks
    iduStShmMemChunk *sCur =
        (iduStShmMemChunk*)IDU_SHM_GET_ADDR_PTR( aStShmMemList->mFreeChunk.mAddrNext );

    while(sCur != NULL)
    {
        sSlot = (iduStShmMemSlot *)IDU_SHM_GET_ADDR_PTR( sCur->mAddrTop );
        while(sSlot != NULL)
        {
            sSlot = (iduStShmMemSlot*)IDU_SHM_GET_ADDR_PTR( sSlot->mAddrNext );
        }

        IDE_ASSERT( sCur->mMaxSlotCnt  == sCur->mFreeSlotCnt );

        sCur = (iduStShmMemChunk*)IDU_SHM_GET_ADDR_PTR( sCur->mAddrNext );
        sCnt++;
        IDE_ASSERT(sCnt <= aStShmMemList->mFreeChunkCnt);
    }

    IDE_ASSERT(sCnt == aStShmMemList->mFreeChunkCnt);

    //check full chunks
    sCnt = 0;
    sCur = (iduStShmMemChunk*)IDU_SHM_GET_ADDR_PTR( aStShmMemList->mFullChunk.mAddrNext );
    while(sCur != NULL)
    {
        IDE_ASSERT( sCur->mFreeSlotCnt == 0 );
        sCur = (iduStShmMemChunk*)IDU_SHM_GET_ADDR_PTR( sCur->mAddrNext );
        sCnt++;
    }

    IDE_ASSERT(sCnt == aStShmMemList->mFullChunkCnt);

    //check partial chunks
    sCnt = 0;
    sCur = (iduStShmMemChunk*)IDU_SHM_GET_ADDR_PTR( aStShmMemList->mPartialChunk.mAddrNext );
    while(sCur != NULL)
    {
        /*
         *���� �˰��򿡼� slot�� �Ѱ��� chunk�� partial chunk list
         *�� �����Ҽ��� ����. free�� full chunk list���� �����ؾ���.
         */
        IDE_ASSERT( sCur->mMaxSlotCnt > 1 );

        //sCur->mMaxSlotCnt > 1
        IDE_ASSERT( (0  < sCur->mFreeSlotCnt) &&
                    (sCur->mFreeSlotCnt < sCur->mMaxSlotCnt) );

        sCur = (iduStShmMemChunk*)IDU_SHM_GET_ADDR_PTR( sCur->mAddrNext );
        sCnt++;
    }

    IDE_ASSERT(sCnt == aStShmMemList->mPartialChunkCnt);
}


#ifndef MEMORY_ASSERT
#define iduCheckMemConsistency( xx )
#endif

iduShmMemList::iduShmMemList()
{
}

iduShmMemList::~iduShmMemList(void)
{
}

/*---------------------------------------------------------
  iduShmMemList

  ___________ mFullChunk
  |iduShmMemList |    _____      _____
  |___________|-> |chunk| -> |chunk| ->NULL
  |  NULL<- |_____| <- |_____|
  |
  |
  |     mPartialChunk
  |          _____      _____
  |`------->|chunk| -> |chunk| ->NULL
  |   NULL<- |_____| <- |_____|
  |
  |
  |     mFreeChunk
  |          _____      _____
  `------->|chunk| -> |chunk| ->NULL
  NULL<- |_____| <- |_____|


  mFreeChunk    : ��� slot�� free�� chunk
  mPartialChunk : �Ϻ��� slot�� ������� chunk
  mFullChunk    : ��� slot�� ������� chunk

  *ó�� chunk�� �Ҵ������ mFreeChunk�� �Ŵ޸��Եǰ�
  slot�Ҵ��� �̷����� mPartialChunk�� ���ļ� mFullchunk�� �̵���.
  *mFullChunk�� �ִ� chunk�� slot�ݳ��� mPartialchunk�� ���ļ� mFreeChunk�� �̵�.
  *mFreeChunk�� �ִ� chunk���� �޸𸮰� ������ �Ѱ��Ȳ������ OS�� �ݳ��Ǿ����� ����.

  chunk:
  _________________________________
  |iduStShmMemChunk |Slot|Slot|... | Slot|
  |____________|____|____|____|_____|

  iduStShmMemChunk        : chunk ���(���� chunk�� ���� ������ ��� �ִ�.)



  Slot:
  ___________________________________
  |momory element |iduStShmMemChunk pointer|
  |_______________|___________________|

  memory element      : ����ڰ� ���� �޸𸮸� ����ϴ� ����
  �� ������ ����ڿ��� �Ҵ�Ǿ� ���� ��������, ����
  (free)slot������ ����Ű�� ������ ��Ȱ�� �Ѵ�.
  iduStShmMemChunk pointer : slot�� ���� chunk�� ���(chunk�� iduStShmMemChunk����)��
  ������ �ϰ� �ִ�.
  -----------------------------------------------------------*/
/*-----------------------------------------------------------
 * task-2440 scuMemPool���� �޸𸮸� ���ö� align�� �޸� �ּҸ� ���
 *          �� �� �ְ� ����.
 *
 * chunk������ align�� �Ͼ�� ���� �����ϰڴ�.
 * ___________________________________________
 |iduStShmMemChunk |   |  Slot|  Slot|... |   Slot|
 |____________|___|______|______|____|_______|
 ^            ^   ^      ^
 1            2   3      4

 �ϴ� chunk�� �Ҵ��ϸ� 1���� ������ �ּҰ����� �����ȴ�.  �� �ּҰ���
 ����ڰ� align�Ǳ⸦ ���ϴ� �ּҰ� �ƴϱ� ������ �׳� ���д�.
 2���ּ� = 1�� �ּ� + SIZEOF(iduStShmMemChunk)
 2�� �ּҴ� ���� ���� ��������, 2���ּҸ� align up��Ű�� 3��ó�� �� ������
 ����Ű�� �ȴ�. �׷��� 3���� align�� �Ǿ��ְ� �ȴ�.
 ������ ����ڿ��� �Ҵ� �Ǵ� ������ 3�� ���� �����̴�.
 4���� align�Ǿ� �ֱ� ���ؼ��� slotSize�� align �Ǿ� �־�� �Ѵ�.
 �׷��� ������ SlotSize�� ���� size���� align up�� ���� ����Ѵ�.
 �׷��� ��� slot�� �ּҴ� align�Ǿ������� �� ���ִ�.

 ���� 2���� 3������ ������ �ִ� ũ��� alignByte�̴�.
 �׷��� ������
 chunkSize = SIZEOF(iduStShmMemChunk)+ alignByte +
 ('alignByte�� align up�� SIZEOF(slot)' * 'slot�� ����')
 �̴�.
 * ---------------------------------------------------------*/
/*-----------------------------------------------------------
 * Description: iduShmMemList�� �ʱ�ȭ �Ѵ�.
 *
 * aIndex              - [IN] �� �Լ��� ȣ���� ���
 *                            (��� ȣ���ϴ��� ������ �����ϱ� ���� �ʿ�)
 * aSeqNumber          - [IN] iduShmMemList �ĺ� ��ȣ
 * aName               - [IN] iduShmMemList�� ȣ���� scuMemPool�� �̸�.
 * aElemSize           - [IN] �ʿ�� �ϴ� �޸� ũ��( �Ҵ���� �޸��� ����ũ��)
 * aElemCnt            - [IN] �� chunk���� element�� ����, ��, chunk����
 *                            slot�� ����
 * aAutofreeChunkLimit -[IN] mFreeChunk�� ������ �� ���ں��� ū ��쿡
 *                           mFreeChunk�߿��� ���� �ٸ� ������ ������ �ʴ�
 *                           chunk�� �޸𸮿��� �����Ѵ�.
 * aUseMutex           - [IN] mutex��� ����
 * aAlignByte          - [IN] aline�Ǿ� �ִ� �޸𸮸� �Ҵ�ް� ������, ���ϴ� align
 *                            ���� �־ �Լ��� ȣ���Ѵ�.
 * ---------------------------------------------------------*/
IDE_RC iduShmMemList::initialize( idvSQL             * aStatistics,
                                  iduShmTxInfo       * aShmTxInfo,
                                  iduMemoryClientIndex aIndex,
                                  UInt                 aSeqNumber,
                                  SChar              * aName,
                                  vULong               aElemSize,
                                  vULong               aElemCnt,
                                  vULong               aAutofreeChunkLimit,
                                  idBool               aUseLatch,
                                  UInt                 aAlignByte,
                                  idShmAddr            aAddr4MemList,
                                  iduStShmMemList    * aMemListInfo )
{
    idrSVP sSavepoint;

    idrLogMgr::setSavepoint( aShmTxInfo, &sSavepoint );

    IDE_ASSERT( aName != NULL );
    //IDE_ASSERT( aElemCnt > 1 ); // then... why do you make THIS mem pool??

    aMemListInfo->mAddrSelf = aAddr4MemList;

    IDU_SHM_VALIDATE_ADDR_PTR( aAddr4MemList, aMemListInfo );

    aMemListInfo->mIndex    = aIndex;
    aMemListInfo->mSequence = aSeqNumber;
    aMemListInfo->mElemCnt  = aElemCnt;

    IDE_ASSERT( aElemSize <= ID_UINT_MAX );

    // ������ �Ҵ���� �ʾ��� �� Element������ iduStShmMemSlot���� ������.
    if( aElemSize < ID_SIZEOF(iduStShmMemSlot) )
    {
        aElemSize = ID_SIZEOF(iduStShmMemSlot);
    }

    /*BUG-19455 mElemSize�� align �Ǿ�� �� */
#if !defined( COMPILE_64BIT )
    aMemListInfo->mElemSize   = idlOS::align( (UInt)aElemSize, sizeof(SDouble) );
#else /* !defined(COMPILE_64BIT) */
    aMemListInfo->mElemSize   = idlOS::align( (UInt)aElemSize );
#endif /* !defined(COMPILE_64BIT) */

    aMemListInfo->mSlotSize = idlOS::align(
        aMemListInfo->mElemSize + ID_SIZEOF(idShmAddr), aAlignByte );

    aMemListInfo->mChunkSize = ID_SIZEOF(iduStShmMemChunk) +
        ( aMemListInfo->mSlotSize * aMemListInfo->mElemCnt ) + aAlignByte;
    aMemListInfo->mAlignByte = aAlignByte;

    aMemListInfo->mFreeChunk.mAddrSelf =
        IDU_SHM_GET_ADDR_SUBMEMBER( aAddr4MemList,
                                    iduStShmMemList,
                                    mFreeChunk );

    IDU_SHM_VALIDATE_ADDR_PTR( aMemListInfo->mFreeChunk.mAddrSelf,
                               &aMemListInfo->mFreeChunk );

    aMemListInfo->mFreeChunk.mAddrParent   = aAddr4MemList;
    aMemListInfo->mFreeChunk.mAddrNext     = IDU_SHM_NULL_ADDR;
    aMemListInfo->mFreeChunk.mAddrPrev     = IDU_SHM_NULL_ADDR;
    aMemListInfo->mFreeChunk.mAddrTop      = IDU_SHM_NULL_ADDR;
    aMemListInfo->mFreeChunk.mMaxSlotCnt   = 0;
    aMemListInfo->mFreeChunk.mFreeSlotCnt  = 0;

    aMemListInfo->mPartialChunk.mAddrSelf      =
        IDU_SHM_GET_ADDR_SUBMEMBER( aAddr4MemList,
                                    iduStShmMemList,
                                    mPartialChunk );

    IDU_SHM_VALIDATE_ADDR_PTR( aMemListInfo->mPartialChunk.mAddrSelf,
                               &aMemListInfo->mPartialChunk );

    aMemListInfo->mPartialChunk.mAddrParent  = aAddr4MemList;
    aMemListInfo->mPartialChunk.mAddrNext    = IDU_SHM_NULL_ADDR;
    aMemListInfo->mPartialChunk.mAddrPrev    = IDU_SHM_NULL_ADDR;
    aMemListInfo->mPartialChunk.mAddrTop     = IDU_SHM_NULL_ADDR;
    aMemListInfo->mPartialChunk.mMaxSlotCnt  = 0;
    aMemListInfo->mPartialChunk.mFreeSlotCnt = 0;

    aMemListInfo->mFullChunk.mAddrSelf      =
        IDU_SHM_GET_ADDR_SUBMEMBER( aAddr4MemList,
                                    iduStShmMemList,
                                    mFullChunk );

    IDU_SHM_VALIDATE_ADDR_PTR( aMemListInfo->mFullChunk.mAddrSelf,
                               &aMemListInfo->mFullChunk );

    aMemListInfo->mFullChunk.mAddrParent    = aAddr4MemList;
    aMemListInfo->mFullChunk.mAddrNext      = IDU_SHM_NULL_ADDR;
    aMemListInfo->mFullChunk.mAddrPrev      = IDU_SHM_NULL_ADDR;
    aMemListInfo->mFullChunk.mAddrTop       = IDU_SHM_NULL_ADDR;
    aMemListInfo->mFullChunk.mMaxSlotCnt    = 0;
    aMemListInfo->mFullChunk.mFreeSlotCnt   = 0;

    aMemListInfo->mFreeChunkCnt             = 0;
    aMemListInfo->mPartialChunkCnt          = 0;
    aMemListInfo->mFullChunkCnt             = 0;

    aMemListInfo->mAutoFreeChunkLimit       = aAutofreeChunkLimit;

    aMemListInfo->mUseLatch                 = aUseLatch;

    IDE_TEST( grow( aStatistics, aShmTxInfo, aMemListInfo ) != IDE_SUCCESS );

    if( aMemListInfo->mUseLatch == ID_TRUE )
    {
        iduShmLatchInit( IDU_SHM_GET_ADDR_SUBMEMBER( aAddr4MemList,
                                                     iduStShmMemList,
                                                     mLatch ),
                         IDU_SHM_LATCH_SPIN_COUNT_DEFAULT,
                         &aMemListInfo->mLatch );
    }

    IDE_TEST( iduVLogShmMemList::writeInitialize( aShmTxInfo,
                                                  sSavepoint.mLSN,
                                                  aMemListInfo )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_ASSERT( idrLogMgr::abort2Svp( aStatistics,
                                      aShmTxInfo,
                                      &sSavepoint )
                == IDE_SUCCESS );

    return IDE_FAILURE;
}

/* ---------------------------------------------------------------------------
 * BUG-19253
 * iduShmMemList�� mFreeChunk�� mFullChunk�� �ΰ��� chunk list�� �����Ѵ�.
 * scuMemPool�� ����ϴ� ��⿡�� scuMemPool�� destroy�ϱ� ����
 * ��� scuMemPool�� slot�� memfree���� ���� �������� destroy�� ����ȴ�.
 * ����, �ϳ��� slot�̶� free���� ���� ä destroy�� ��û�Ǹ� debug mode������
 * DASSERT()�� �װ�, release mode������ mFullChunk�� ����� chunk��ŭ��
 * memory leak�� �߻��ϰ� �ȴ�.
 * debug mode������ memory leak�� �����ϱ� ���� DASSERT���� �����ϰ�,
 * release mode������ mFullChunk�� ����� chunk�鵵 free ��Ű���� �Ѵ�.
 *
 * index build�� ���� ����� scuMemPool�� �Ϸ� �� �Ѳ����� free ��Ű�� ���
 * mFullChunk�� �߰������� free ��Ű���� �Ѵ�. �Ѳ����� free ��Ű�� ����
 * aBCheck�� ID_FALSE�� �����Ǿ�� �Ѵ�.
 * ---------------------------------------------------------------------------*/
IDE_RC iduShmMemList::destroy( idvSQL             * aStatistics,
                               iduShmTxInfo       * aShmTxInfo,
                               idrSVP             * aSavepoint,
                               iduStShmMemList    * aMemListInfo,
                               idBool               aBCheck )
{
    vULong             i;
    iduStShmMemChunk * sCur = NULL;

    idrSVP             sSVP;
    idrSVP             sInitSVP;
    vULong             sFreeChunkCnt;
    vULong             sPartialChunkCnt;
    vULong             sFullChunkCnt;

    idrLogMgr::setSavepoint( aShmTxInfo, &sInitSVP );

    if( aSavepoint == NULL )
    {
        aSavepoint = &sInitSVP;
    }

    sFreeChunkCnt = aMemListInfo->mFreeChunkCnt;

    for( i = 0; i < sFreeChunkCnt ; i++ )
    {
        sCur = (iduStShmMemChunk*)IDU_SHM_GET_ADDR_PTR( aMemListInfo->mFreeChunk.mAddrNext );
        IDE_ASSERT(sCur != NULL);

        idrLogMgr::setSavepoint( aShmTxInfo, &sSVP );

        if( aBCheck == ID_TRUE )
        {
            if( sCur->mFreeSlotCnt != sCur->mMaxSlotCnt )
            {
                ideLog::log( IDE_SERVER_0,
                             ID_TRC_MEMLIST_MISMATCH_FREE_SLOT_COUNT,
                             (UInt)sCur->mFreeSlotCnt,
                             (UInt)sCur->mMaxSlotCnt );

                IDE_DASSERT(0);
            }
        }

        unlink( aShmTxInfo, sCur );

        IDE_ASSERT( aMemListInfo->mFreeChunkCnt > 0 );

        iduVLogShmMemList::writeFreeChunkCnt( aShmTxInfo, aMemListInfo );

        aMemListInfo->mFreeChunkCnt--;


        IDE_TEST( iduShmMgr::freeMem( aStatistics,
                                      aShmTxInfo,
                                      &sSVP,
                                      sCur ) != IDE_SUCCESS );

        IDE_TEST( idrLogMgr::commit2Svp( aStatistics, aShmTxInfo, &sSVP )
                  != IDE_SUCCESS );

    }

    if( aBCheck == ID_TRUE )
    {
        if( aMemListInfo->mFreeChunkCnt != 0)
        {
            ideLog::log(IDE_SERVER_0,ID_TRC_MEMLIST_WRONG_FULL_CHUNK_COUNT,
                        (UInt)aMemListInfo->mFullChunkCnt);

            IDE_DASSERT(0);
        }
    }

    sPartialChunkCnt = aMemListInfo->mPartialChunkCnt;
    for( i = 0; i < sPartialChunkCnt ; i++ )
    {
        idrLogMgr::setSavepoint( aShmTxInfo, &sSVP );

        sCur = (iduStShmMemChunk*)IDU_SHM_GET_ADDR_PTR(
            aMemListInfo->mPartialChunk.mAddrNext );
        IDE_ASSERT( sCur != NULL );

        if( aBCheck == ID_TRUE )
        {
            if( ( sCur->mFreeSlotCnt == 0 ) ||
                ( sCur->mFreeSlotCnt == sCur->mMaxSlotCnt ) )
            {
                ideLog::log( IDE_SERVER_0,
                             ID_TRC_MEMLIST_WRONG_FREE_SLOT_COUNT,
                             (UInt)sCur->mFreeSlotCnt,
                             (UInt)sCur->mMaxSlotCnt );
                IDE_DASSERT(0);
            }
        }

        unlink( aShmTxInfo, sCur );

        IDE_ASSERT( aMemListInfo->mPartialChunkCnt > 0 );

        iduVLogShmMemList::writePartChunkCnt( aShmTxInfo, aMemListInfo );

        aMemListInfo->mPartialChunkCnt--;

        IDE_TEST( iduShmMgr::freeMem( aStatistics, aShmTxInfo, &sSVP, sCur )
                  != IDE_SUCCESS );

        IDE_TEST( idrLogMgr::commit2Svp( aStatistics, aShmTxInfo, &sSVP )
                  != IDE_SUCCESS );

    }

    // BUG-19253
    // mFullChunk list�� ����� chunk�� ������ free��Ų��.
    sFullChunkCnt = aMemListInfo->mFullChunkCnt;
    for( i = 0; i < sFullChunkCnt ; i++ )
    {
        idrLogMgr::setSavepoint( aShmTxInfo, &sSVP );

        sCur =
            (iduStShmMemChunk*)IDU_SHM_GET_ADDR_PTR( aMemListInfo->mFullChunk.mAddrNext );
        IDE_ASSERT( sCur != NULL );

        unlink( aShmTxInfo, sCur );

        IDE_ASSERT( aMemListInfo->mFullChunkCnt > 0 );

        iduVLogShmMemList::writeFullChunkCnt( aShmTxInfo, aMemListInfo );

        aMemListInfo->mFullChunkCnt--;

        IDE_TEST( iduShmMgr::freeMem( aStatistics, aShmTxInfo, &sSVP, sCur )
                  != IDE_SUCCESS );

        IDE_TEST( idrLogMgr::commit2Svp( aStatistics, aShmTxInfo, &sSVP )
                  != IDE_SUCCESS );

    }

    if( aMemListInfo->mUseLatch == ID_TRUE )
    {
        iduShmLatchDest( &aMemListInfo->mLatch );
    }

    IDE_TEST( idrLogMgr::commit2Svp( aStatistics,
                                     aShmTxInfo,
                                     aSavepoint ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_ASSERT( idrLogMgr::abort2Svp( aStatistics,
                                      aShmTxInfo,
                                      &sInitSVP )
                == IDE_SUCCESS );

    return IDE_FAILURE;
}

/*
 * iduStShmMemChunk�� �ϳ� �Ҵ��Ͽ�, �̰��� iduShmMemList->mFreeChunk�� �����Ѵ�.
 */
IDE_RC iduShmMemList::grow( idvSQL           * aStatistics,
                            iduShmTxInfo     * aShmTxInfo,
                            iduStShmMemList  * aMemListInfo )
{
    vULong             i;
    iduStShmMemChunk * sNewChunk;
    iduStShmMemSlot  * sSlot;
    iduStShmMemSlot  * sFirstSlot;
    idShmAddr          sChunkAddr;
    UInt               sOffset;
    idrSVP             sSavepoint;

    idrLogMgr::setSavepoint( aShmTxInfo, &sSavepoint );

    IDE_TEST( iduShmMgr::allocMem( aStatistics,
                                   aShmTxInfo,
                                   aMemListInfo->mIndex,
                                   aMemListInfo->mChunkSize,
                                   (void**)&sNewChunk )
              != IDE_SUCCESS );

    sChunkAddr = iduShmMgr::getAddr( sNewChunk );

    sNewChunk->mAddrSelf    = sChunkAddr;
    sNewChunk->mAddrParent  = aMemListInfo->mAddrSelf;
    sNewChunk->mMaxSlotCnt  = aMemListInfo->mElemCnt;
    sNewChunk->mFreeSlotCnt = aMemListInfo->mElemCnt;
    sNewChunk->mAddrTop     = IDU_SHM_NULL_ADDR;

    /* chunk���� �� slot�� mAddrNext�� �����Ѵ�. ���� �������� �ִ� slot��
     * sNewChunk->mAddrTop�� ����ȴ�. mAddrNext�� ����ڿ��� �Ҵ�ɶ��� ����
     * ����ϴ� �������� ���δ�.*/
    sOffset    = ID_SIZEOF( iduStShmMemChunk );
    sFirstSlot = (iduStShmMemSlot*)( (UChar*)sNewChunk + sOffset );
    sFirstSlot = (iduStShmMemSlot*)idlOS::align( sFirstSlot, aMemListInfo->mAlignByte );
    sOffset    = (SChar*)sFirstSlot - (SChar*)sNewChunk;

    for( i = 0; i < aMemListInfo->mElemCnt; i++ )
    {
        sSlot = (iduStShmMemSlot *)((UChar *)sFirstSlot + aMemListInfo->mSlotSize * i);
        sSlot->mAddrSelf = sChunkAddr + sOffset;

        IDE_DASSERT( sSlot == (iduStShmMemSlot*)IDU_SHM_GET_ADDR_PTR( sSlot->mAddrSelf ) );

        sSlot->mAddrNext = sNewChunk->mAddrTop;

        sNewChunk->mAddrTop = sSlot->mAddrSelf;

        *((idShmAddr*)((UChar *)sSlot + aMemListInfo->mElemSize)) = sNewChunk->mAddrSelf;

        sOffset += aMemListInfo->mSlotSize;
    }

    /* ------------------------------------------------
     *  mFreeChunk�� ����
     * ----------------------------------------------*/
    link( aShmTxInfo, &aMemListInfo->mFreeChunk, sNewChunk );

    iduVLogShmMemList::writeFreeChunkCnt( aShmTxInfo, aMemListInfo );

    aMemListInfo->mFreeChunkCnt++;


    // grow�� ���� memfree���� �ʾƵ� ��. �׳� free�� �޾Ƶ�
    IDE_TEST( idrLogMgr::commit2Svp( aStatistics, aShmTxInfo, &sSavepoint )
              != IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_ASSERT( idrLogMgr::abort2Svp( aStatistics, aShmTxInfo, &sSavepoint )
                == IDE_SUCCESS );

    return IDE_FAILURE;
}

/* ------------------------------------------------
 *  Allocation
 * ----------------------------------------------*/
IDE_RC iduShmMemList::alloc( idvSQL          * aStatistics,
                             iduShmTxInfo    * aShmTxInfo,
                             iduStShmMemList * aMemListInfo,
                             idShmAddr       * aAddr4Mem,
                             void           ** aMem )
{
    iduStShmMemChunk * sMyChunk     = NULL;
    idrSVP             sSavepoint;
    idBool             sIsFreeChunk = ID_TRUE; //��� chunk���� �Ҵ��ϱ�� �����ߴ°�?
                                               // 1: free  chunk   0: partial chunk

    idrLogMgr::setSavepoint( aShmTxInfo, &sSavepoint );

    iduCheckMemConsistency( aMemListInfo );

    //victim chunk�� ����.
    if( aMemListInfo->mPartialChunk.mAddrNext != IDU_SHM_NULL_ADDR )
    {
        sMyChunk = (iduStShmMemChunk*)IDU_SHM_GET_ADDR_PTR(
            aMemListInfo->mPartialChunk.mAddrNext );
        sIsFreeChunk = ID_FALSE;
    }
    else
    {
        if( aMemListInfo->mFreeChunk.mAddrNext == IDU_SHM_NULL_ADDR )
        {
            IDE_TEST( grow( aStatistics, aShmTxInfo, aMemListInfo ) != IDE_SUCCESS );
        }

        sMyChunk = (iduStShmMemChunk*)IDU_SHM_GET_ADDR_PTR(
            aMemListInfo->mFreeChunk.mAddrNext );
        sIsFreeChunk = ID_TRUE;
        IDE_ASSERT( sMyChunk != NULL );
    }

    iduCheckMemConsistency( aMemListInfo );

    IDE_ASSERT( sMyChunk->mAddrTop != IDU_SHM_NULL_ADDR );

    *aMem = IDU_SHM_GET_ADDR_PTR( sMyChunk->mAddrTop );

    // log mAddrTop , mFreeSlotCnt
    iduVLogShmMemList::writeAllocMyChunk( aShmTxInfo,
                                          sMyChunk );

    sMyChunk->mAddrTop = ((iduStShmMemSlot *)(*aMem))->mAddrNext;

    /*
     * Partial list ->Full list, Free list ->Partial list:
     chunk��  ������ 1���� �ʰ��Ҷ� (�Ϲ����ΰ��)
     * Free list -> Full list  :chunk��  ������ 1�� �ִ� ���.
     */
    if( ( --sMyChunk->mFreeSlotCnt ) == 0 )
    {
        unlink( aShmTxInfo, sMyChunk );

        if( sIsFreeChunk == ID_TRUE ) //Free list -> Full list
        {
            IDE_ASSERT( aMemListInfo->mFreeChunkCnt > 0 );

            iduVLogShmMemList::writeFreeChunkCnt( aShmTxInfo, aMemListInfo );

            aMemListInfo->mFreeChunkCnt--;
        }
        else  //Partial list -> Full list
        {
            IDE_ASSERT( aMemListInfo->mPartialChunkCnt > 0 );

            iduVLogShmMemList::writePartChunkCnt( aShmTxInfo, aMemListInfo );

            aMemListInfo->mPartialChunkCnt--;
        }

        link( aShmTxInfo, &aMemListInfo->mFullChunk, sMyChunk );

        iduVLogShmMemList::writeFullChunkCnt( aShmTxInfo, aMemListInfo );

        aMemListInfo->mFullChunkCnt++;
    }
    else
    {
        // Free list ->Partial list
        if( (sMyChunk->mMaxSlotCnt - sMyChunk->mFreeSlotCnt) == 1 )
        {
            unlink( aShmTxInfo, sMyChunk );

            IDE_ASSERT( aMemListInfo->mFreeChunkCnt > 0 );

            iduVLogShmMemList::writeFreeChunkCnt( aShmTxInfo, aMemListInfo );

            aMemListInfo->mFreeChunkCnt--;

            link( aShmTxInfo, &aMemListInfo->mPartialChunk, sMyChunk );

            iduVLogShmMemList::writePartChunkCnt( aShmTxInfo, aMemListInfo );

            aMemListInfo->mPartialChunkCnt++;
        }
    }

    iduCheckMemConsistency( aMemListInfo );

    IDU_SHM_VALIDATE_ADDR_PTR( ((iduStShmMemSlot *)(*aMem))->mAddrSelf, *aMem );

    *aAddr4Mem = ((iduStShmMemSlot *)(*aMem))->mAddrSelf;


    IDE_TEST( iduVLogShmMemList::writeNTAAlloc( aShmTxInfo,
                                                sSavepoint.mLSN,
                                                aMemListInfo,
                                                *aAddr4Mem )
              != IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_ASSERT( idrLogMgr::abort2Svp( aStatistics, aShmTxInfo, &sSavepoint )
                == IDE_SUCCESS );

    return IDE_FAILURE;
}

/* ------------------------------------------------
 *  Free for one
 * ----------------------------------------------*/
IDE_RC iduShmMemList::memfree( idvSQL          * aStatistics,
                               iduShmTxInfo    * aShmTxInfo,
                               idrSVP          * aSavepoint,
                               iduStShmMemList * aMemListInfo,
                               idShmAddr         aAddr4Mem,
                               void            * aFreeElem )
{
    iduStShmMemSlot  * sFreeElem;
    iduStShmMemChunk * sCurChunk;
    idShmAddr          sAddr4Shm;
    idrSVP             sSavepoint;

    IDE_ASSERT( aFreeElem != NULL );

    idrLogMgr::setSavepoint( aShmTxInfo, &sSavepoint );

    if( aSavepoint == NULL )
    {
        aSavepoint = &sSavepoint;
    }

    sAddr4Shm = *((idShmAddr*)((UChar *)aFreeElem + aMemListInfo->mElemSize));
    sCurChunk = (iduStShmMemChunk*)IDU_SHM_GET_ADDR_PTR_CHECK( sAddr4Shm );

    IDE_ASSERT( aMemListInfo->mAddrSelf == sCurChunk->mAddrParent );

    sFreeElem = (iduStShmMemSlot*)aFreeElem;

    iduVLogShmMemList::writeMemFree( aShmTxInfo,
                                     sCurChunk,
                                     sFreeElem);

    sFreeElem->mAddrSelf = aAddr4Mem;
    sFreeElem->mAddrNext = sCurChunk->mAddrTop;
    sCurChunk->mAddrTop  = sFreeElem->mAddrSelf;

    sCurChunk->mFreeSlotCnt++;
    IDE_ASSERT( sCurChunk->mFreeSlotCnt <= sCurChunk->mMaxSlotCnt );


    //slot cnt�� �ٲ������ ���� ����Ʈ�̵��� �Ͼ�� �ʾ����Ƿ� check�ϸ� �ȵ�!
    //iduCheckMemConsistency(this);

    /*
     * Partial List -> Free List, Full list -> Partial list :�Ϲ����� ���.
     * Full list -> Free list : chunk�� slot�� �Ѱ��϶�
     */
    if( sCurChunk->mFreeSlotCnt == sCurChunk->mMaxSlotCnt )
    {
        unlink( aShmTxInfo, sCurChunk );

        if( sCurChunk->mMaxSlotCnt == 1 ) //Full list -> Free list
        {
            IDE_ASSERT( aMemListInfo->mFullChunkCnt > 0 );

            iduVLogShmMemList::writeFullChunkCnt( aShmTxInfo, aMemListInfo );

            aMemListInfo->mFullChunkCnt--;

        }
        else   //Partial List -> Free List
        {
            IDE_ASSERT( aMemListInfo->mPartialChunkCnt > 0 );

            iduVLogShmMemList::writePartChunkCnt( aShmTxInfo, aMemListInfo );

            aMemListInfo->mPartialChunkCnt--;

        }

        //Limit�� �Ѿ�� ��� autofree
        if( aMemListInfo->mFreeChunkCnt <= aMemListInfo->mAutoFreeChunkLimit )
        {
            link( aShmTxInfo, &aMemListInfo->mFreeChunk, sCurChunk );

            iduVLogShmMemList::writeFreeChunkCnt( aShmTxInfo, aMemListInfo );

            aMemListInfo->mFreeChunkCnt++;


            iduCheckMemConsistency( aMemListInfo );
        }
        else
        {
            iduCheckMemConsistency( aMemListInfo );

            IDE_TEST( iduShmMgr::freeMem( aStatistics,
                                          aShmTxInfo,
                                          aSavepoint,
                                          sCurChunk )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // Full list -> Partial list
        if( sCurChunk->mFreeSlotCnt == 1 )
        {
            unlink( aShmTxInfo, sCurChunk );
            IDE_ASSERT( aMemListInfo->mFullChunkCnt > 0 );

            iduVLogShmMemList::writeFullChunkCnt( aShmTxInfo, aMemListInfo );

            aMemListInfo->mFullChunkCnt--;


            link( aShmTxInfo, &aMemListInfo->mPartialChunk, sCurChunk );

            iduVLogShmMemList::writePartChunkCnt( aShmTxInfo, aMemListInfo );

            aMemListInfo->mPartialChunkCnt++;

        }

        iduCheckMemConsistency( aMemListInfo );
    }

    IDE_TEST( idrLogMgr::commit2Svp( aStatistics, aShmTxInfo, aSavepoint )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_ASSERT( idrLogMgr::abort2Svp( aStatistics, aShmTxInfo, &sSavepoint )
                == IDE_SUCCESS );

    return IDE_FAILURE;
}

IDE_RC iduShmMemList::cralloc( idvSQL          * aStatistics,
                               iduShmTxInfo    * aShmTxInfo,
                               iduStShmMemList * aMemListInfo,
                               idShmAddr       * aAddr4Mem,
                               void           ** aMem )
{
    IDE_TEST( alloc( aStatistics,
                     aShmTxInfo,
                     aMemListInfo,
                     aAddr4Mem,
                     aMem ) != IDE_SUCCESS );

    idlOS::memset( *aMem, 0, aMemListInfo->mElemSize );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC iduShmMemList::shrink( idvSQL          * aStatistics,
                              iduShmTxInfo    * aShmTxInfo,
                              iduStShmMemList * aMemListInfo,
                              UInt            * aSize )
{
    iduStShmMemChunk * sCur = NULL;
    iduStShmMemChunk * sNxt = NULL;
    UInt               sFreeSizeDone =
        aMemListInfo->mFreeChunkCnt * aMemListInfo->mChunkSize;

    idrSVP      sSVP;
    idrSVP      sInitSVP;

    IDE_ASSERT( aSize != NULL );

    *aSize = 0;

    idrLogMgr::setSavepoint( aShmTxInfo, &sInitSVP );

    sCur = (iduStShmMemChunk*)IDU_SHM_GET_ADDR_PTR( aMemListInfo->mFreeChunk.mAddrNext );

    while( sCur != NULL )
    {
        idrLogMgr::setSavepoint( aShmTxInfo, &sSVP );

        sNxt = (iduStShmMemChunk*)IDU_SHM_GET_ADDR_PTR( sCur->mAddrNext );
        unlink( aShmTxInfo, sCur );

        iduVLogShmMemList::writeFreeChunkCnt( aShmTxInfo, aMemListInfo );

        aMemListInfo->mFreeChunkCnt--;

        IDE_TEST( iduShmMgr::freeMem( aStatistics, aShmTxInfo, &sSVP, sCur )
                  != IDE_SUCCESS );
        sCur = sNxt;
    }

    IDE_ASSERT( aMemListInfo->mFreeChunkCnt == 0 ); //�翬.

    *aSize = sFreeSizeDone;

    IDE_TEST( idrLogMgr::commit2Svp( aStatistics, aShmTxInfo, &sInitSVP )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_ASSERT( idrLogMgr::abort2Svp( aStatistics, aShmTxInfo, &sInitSVP )
                == IDE_SUCCESS );

    return IDE_FAILURE;
}

UInt iduShmMemList::getUsedMemory( iduStShmMemList * aMemListInfo )
{
    UInt sSize;

    sSize = aMemListInfo->mElemSize * aMemListInfo->mFreeChunkCnt * aMemListInfo->mElemCnt;
    sSize += (aMemListInfo->mElemSize * aMemListInfo->mFullChunkCnt * aMemListInfo->mElemCnt);

    return sSize;
}

void iduShmMemList::status( iduStShmMemList * aMemListInfo )
{
    SChar sBuffer[IDE_BUFFER_SIZE];

    idlOS::snprintf( sBuffer,
                     ID_SIZEOF(sBuffer),
                     "    Memory Usage: %"ID_UINT32_FMT" KB\n",
                     (UInt)(getUsedMemory( aMemListInfo ) / 1024));
    IDE_CALLBACK_SEND_SYM_NOLOG(sBuffer);
}

/*
 * X$MEMPOOL�� ���ؼ� �ʿ��� ������ ä��.
 */
void iduShmMemList::fillMemPoolInfo( iduStShmMemList     * aMemListInfo,
                                     iduShmMemPoolState  * aInfo )
{
    IDE_ASSERT( aInfo != NULL );

    aInfo->mFreeCnt    = aMemListInfo->mFreeChunkCnt;
    aInfo->mFullCnt    = aMemListInfo->mFullChunkCnt;
    aInfo->mPartialCnt = aMemListInfo->mPartialChunkCnt;
    aInfo->mChunkLimit = aMemListInfo->mAutoFreeChunkLimit;
    aInfo->mChunkSize  = aMemListInfo->mChunkSize;
    aInfo->mElemCnt    = aMemListInfo->mElemCnt;
    aInfo->mElemSize   = aMemListInfo->mElemSize;
}

void iduShmMemList::dumpPartialFreeChunk4TSM( iduStShmMemList * aMemListInfo )
{
    iduStShmMemChunk * sMyChunk = NULL;
    UInt               sID = 0;

    sMyChunk = (iduStShmMemChunk*)IDU_SHM_GET_ADDR_PTR(
        aMemListInfo->mPartialChunk.mAddrNext );

    while(1)
    {
        if( sMyChunk == NULL )
        {
            break;
        }

        idlOS::printf( "##[%d] Free Count:%"ID_UINT32_FMT"\n",
                       sID,
                       sMyChunk->mFreeSlotCnt );

        sID++;

        sMyChunk =
            (iduStShmMemChunk*)IDU_SHM_GET_ADDR_PTR( sMyChunk->mAddrNext );
    }
}

