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

#ifndef _O_IDU_SHM_HASH_H_
#define _O_IDU_SHM_HASH_H_ 1

#include <idl.h>

#include <idrLogMgr.h>
#include <iduShmList.h>
#include <iduShmMemPool.h>

//following structure will be refered from VLogShmHash
struct iduLtShmHashBase;
struct iduStShmHashBucket;
struct iduStShmHashBase;

#include <iduVLogShmHash.h>

typedef enum iduShmHashGenFuncID
{
    IDU_SHMHASH_ID_SMXTRANS_GEN_HASH_VALUE_FUNC = 0,
    IDU_SHMHASH_ID_SMXTABLEINFOMGR_HASHFUNC
} iduShmHashGenFuncID;

typedef enum iduShmHashCompFuncID
{
    IDU_SHMHASH_ID_SMXTRANS_COMPARE_FUNC = 0,
    IDU_SHMHASH_ID_SMXTABLEINFOMGR_COMPAREFUNC4HASH
} iduShmHashCompFuncID;

/*
 * Hash
 * ����ڴ� �ؽ� ��ü�� ����ϱ� ���� �Ʒ��� �Ӽ��� initialize()�� �Ѱܾ� �Ѵ�.
 *
 *  - ���� ����   : �Էµ� ����� �ִ� ������ ����� �����Ǿ�� ��.
 *  - ���ü� ���� : ���� ��ü�� �̿��ϴ� ������, �ټ� �����尡 ���ÿ� �̿��ϴ�
 *                  �������� ���� ��ġ (�� ��� �Ϲ������� CPU���� * 2)
 *  - Mutex�� ����� �������� ���� Flag
 *  - ���� Ű�� ����
 *  - Ű�κ��� ��� �ؽ� �Լ�
 *  - Ű���� �� �Լ�
 */

typedef struct iduStShmHashChain
{
    idShmAddr       mAddrSelf;
    iduShmListNode  mList;      /* for double-linked list  */
    idShmAddr       mAddrNode;  /* real Hash Target Node   */
    ULong           mKey[1];    /* ��忡 ���� Ű�� �����Ͱ� ���� : 8�� align��  */
}iduStShmHashChain;

typedef struct iduStShmHashBucket
{
    idShmAddr       mAddrSelf;
    iduShmSXLatch      mLatch;     /* bucket�� ���� ���ü� ���� */
    UInt            mCount;     /* ���� bucket�� ����� ��� ���� */
    iduShmListNode  mBaseList;  /* double-link list�� base list */
}iduStShmHashBucket;

struct iduLtShmHashBase;

typedef struct iduShmHashLatchFunc
{
    IDE_RC (*findNode)  ( idvSQL             *aStatistics,
                          iduShmTxInfo       *aShmTxInfo,
                          iduLtShmHashBase   *aBase,
                          iduStShmHashBucket *aBucket,
                          void               *aKeyPtr,
                          void              **aNode );

    IDE_RC (*insertNode)( idvSQL             *aStatistics,
                          iduShmTxInfo       *aShmTxInfo,
                          iduLtShmHashBase   *aBase,
                          iduStShmHashBucket *aBucket,
                          void               *aKeyPtr,
                          idShmAddr           aAddr4Node,
                          void               *aNode );

    IDE_RC (*deleteNode)( idvSQL             *aStatistics,
                          iduShmTxInfo       *aShmTxInfo,
                          idrSVP             *aVSavepoint,
                          iduLtShmHashBase   *aBase,
                          iduStShmHashBucket *aBucket,
                          void               *aKeyPtr,
                          void              **aNode );
} iduShmHashLatchFunc;

typedef struct iduStShmHashBase
{
    idShmAddr          mAddrSelf;
    iduShmLatch        mLatch;          /* Hash ��ü�� Latch */

    iduStShmMemPool    mShmMemPool;     /* Chain�� �޸� ������ :
                                           Struct���� Class ��� �Ұ�. so, void */
    UInt               mKeyLength;
    UInt               mBucketCount;
    idShmAddr          mAddrBucketLst;  /* Bucket List */

    /* For Traverse  */
    idBool             mOpened;         /* Traverse�� Open ����   */
    UInt               mCurBucket;      /* ��������   Bucket ��ȣ  */
    idShmAddr          mAddrCurChain;   /* ���� ����  Chain Addr  */

    /* fix BUG-21311 */
    idBool             mDidAllocChain;

    /* For logical logging */
    iduShmHashGenFuncID   mHashFuncID;
    iduShmHashCompFuncID  mCompFuncID;
    idBool                mUseLatch;
    UInt                  mHashTableSize;
} iduStShmHashBase;

typedef UInt (*iduShmHashGenFunc)(iduStShmHashBase * aStHashInfo, void *);
typedef SInt (*iduShmHashCompFunc)(void *, void *);

typedef struct iduLtShmHashBase
{
    iduStShmHashBase       * mStShmHashBase;
    iduShmHashLatchFunc    * mLatchVector;
    iduStShmMemPool        * mStShmMemPool;
    iduStShmHashBucket     * mBucket;
    iduShmHashGenFunc        mHashFunc;    /* HASH �Լ� : callback */
    iduShmHashCompFunc       mCompFunc;    /* �� �Լ� : callback */
} iduLtShmHashBase;

class iduShmHash
{
public:
    static IDE_RC initialize( idvSQL               *aStatistics,
                              iduShmTxInfo         *aShmTxInfo,
                              UInt                  aConcurrentLevel,/* > 0 */
                              UInt                  aBucketCount,
                              UInt                  aKeyLength,
                              idBool                aUseLatch,
                              iduShmHashGenFuncID   aHashFuncID,
                              iduShmHashCompFuncID  aCompFuncID,
                              idShmAddr             aAddrStShmHashBase,
                              iduStShmHashBase    * aStShmHashBase,
                              iduLtShmHashBase    * aBase );

    static IDE_RC destroy( idvSQL            * aStatistics,
                           iduShmTxInfo      * aShmTxInfo,
                           iduLtShmHashBase  * aBase );

    static IDE_RC attach( idBool                aUseLatch,
                          iduStShmHashBase    * aStShmHashBase,
                          iduLtShmHashBase    * aLtHashBase );

    static IDE_RC detach( iduLtShmHashBase * aLtHashBase );

    static IDE_RC reset( idvSQL            * aStatistics,
                         iduShmTxInfo      * aShmTxInfo,
                         iduLtShmHashBase  * aBase );

    static IDE_RC findNode( idvSQL            * aStatistics,
                            iduShmTxInfo      * aShmTxInfo,
                            iduLtShmHashBase  * aBase,
                            void              * aKeyPtr,
                            void             ** aNode );

    static IDE_RC insertNode( idvSQL            * aStatistics,
                              iduShmTxInfo      * aShmTxInfo,
                              iduLtShmHashBase  * aBase,
                              void              * aKeyPtr,
                              idShmAddr           aAddrNode,
                              void              * aNode );

    static IDE_RC deleteNode( idvSQL            * aStatistics,
                              iduShmTxInfo      * aShmTxInfo,
                              idrSVP            * aVSavepoint,
                              iduLtShmHashBase  * aBase,
                              void              * aKeyPtr,
                              void             ** aNode );

    /*
     * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! ���� !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
     *
     *  lock(), unlock() �Լ��� findNode(), deleteNode(), insertNode() ����
     *  �����Լ��� �����ϱ� ������
     *  lock(), unlock() �Լ��� ����Ѵٰ� �ؼ� �̷���
     *  �����Լ��� ������ ������ �� �ִٰ� �����ؼ��� �ȵȴ�!!!!
     *  �� �Լ��� ������ �ܺο��� ��� �ǵ��� ���Ǵ� Mutex �ǹ̸� ����
     *  ���� �Լ� �̻� �ƹ� �͵� �ƴϴ�.
     */
    static void lock( idvSQL            * aStatistics,
                      iduShmTxInfo      * aShmTxInfo,
                      iduLtShmHashBase  * aBase );

    static void unlock( iduShmTxInfo *aShmTxInfo, iduLtShmHashBase  * aBase );

    /*
     * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! ���� !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
     *
     *  �Ʒ��� �Լ��� ȣ���� ��쿡�� �ƹ��� Concurrency Control��
     *  ���� �ʱ� ������, ȣ���ڴ� �Ʒ��� cut�Լ��� ��뿡 �־
     *  �����ؾ� �Ѵ�.
     *  ����, open(), cutNode() �������� �ٸ� �����尡 Insert Ȥ�� delete��
     *  �Ѵٸ�, ����ġ ���� ��Ȳ�� �߻��� �� �����Ƿ�,
     *  ��� �Լ� lock(), unlock()�� �̿��ؼ� �ܺ� ȣ�� �������� ���������
     *  ��� ���� �Լ��� ���� ó���� �� �־�� �Ѵ�.
     */

    // ���� Traverse & Node ���� �۾�
    // Fix BUG-21311
    static inline IDE_RC open( iduShmTxInfo     * aShmTxInfo,
                               iduLtShmHashBase * aBase );
    static inline IDE_RC cutNode( idvSQL           * aStatistics,
                                  iduShmTxInfo     * aShmTxInfo,
                                  iduLtShmHashBase * aBase,
                                  void **aNode );
    static inline IDE_RC close( iduShmTxInfo     *aShmTxInfo,
                                iduLtShmHashBase *aBase );
    static IDE_RC isEmpty( iduShmTxInfo     *aShmTxInfo,
                           iduLtShmHashBase *aBase,
                           idBool           *aIsEmpty );

    static IDE_RC deleteNodeNoLatch( idvSQL              * aStatistics,
                                     iduShmTxInfo        * aShmTxInfo,
                                     idrSVP              * aVSavepoint,
                                     iduLtShmHashBase    * aBase,
                                     iduStShmHashBucket  * aBucket,
                                     void                * aKeyPtr,
                                     void               ** aNode );

    static iduShmHashGenFunc   mArrHashFunction[2];
    static iduShmHashCompFunc  mArrCompFunction[2];

    static UInt genHashValueFunc( iduStShmHashBase * aStHashInfo, void * aData );
    static UInt hashFunc( iduStShmHashBase * aStHashInfo, void* aKeyPtr );
    static SInt compareFunc( void* aLhs, void* aRhs );
    static SInt compareFunc4Hash( void* aLhs, void* aRhs );

private:
    static iduShmHashLatchFunc mLatchVector[2];
    static iduStShmHashChain* findNodeInternal( iduLtShmHashBase   * aBase,
                                                iduStShmHashBucket * aBucket,
                                                void               * aKeyPtr );

    static IDE_RC findNodeLatch( idvSQL             * aStatistics,
                                 iduShmTxInfo       * aShmTxInfo,
                                 iduLtShmHashBase   * aBase,
                                 iduStShmHashBucket * aBucket,
                                 void               * aKeyPtr,
                                 void              ** aNode );

    static IDE_RC insertNodeLatch( idvSQL             * aStatistics,
                                   iduShmTxInfo       * aShmTxInfo,
                                   iduLtShmHashBase   * aBase,
                                   iduStShmHashBucket * aBucket,
                                   void               * aKeyPtr,
                                   idShmAddr            aAddr4Node,
                                   void               * aNode );

    static IDE_RC deleteNodeLatch( idvSQL             * aStatistics,
                                   iduShmTxInfo        * aShmTxInfo,
                                   idrSVP              * aVSavepoint,
                                   iduLtShmHashBase    * aBase,
                                   iduStShmHashBucket  * aBucket,
                                   void                * aKeyPtr,
                                   void               ** aNode );

    static IDE_RC findNodeNoLatch( idvSQL              * aStatistics,
                                   iduShmTxInfo        * aShmTxInfo,
                                   iduLtShmHashBase    * aBase,
                                   iduStShmHashBucket  * aBucket,
                                   void                * aKeyPtr,
                                   void               ** aNode );

    static IDE_RC insertNodeNoLatch( idvSQL              * aStatistics,
                                     iduShmTxInfo        * aShmTxInfo,
                                     iduLtShmHashBase    * aBase,
                                     iduStShmHashBucket  * aBucket,
                                     void                * aKeyPtr,
                                     idShmAddr             aAddr4Node,
                                     void                * aNode );

    static IDE_RC allocChain( idvSQL                * aStatistics,
                              iduShmTxInfo          * aShmTxInfo,
                              iduLtShmHashBase      * aBase,
                              iduStShmHashChain    ** aChain );

    static IDE_RC freeChain( idvSQL             * aStatistics,
                             iduShmTxInfo       * aShmTxInfo,
                             idrSVP             * aSavepoint,
                             iduLtShmHashBase   * aBase,
                             iduStShmHashChain  * aChain );

    static iduStShmHashChain *searchFirstNode( iduLtShmHashBase * aBase );
    static iduStShmHashChain *searchNextNode ( iduLtShmHashBase * aBase );

};

//fix BUG-21311 transform inline function.
inline IDE_RC iduShmHash::open( iduShmTxInfo     * /*aShmTxInfo*/,
                                iduLtShmHashBase * aBase )
{
    iduStShmHashChain   * sCurChainPtr;
    iduStShmHashBase    * sStHashBase;

    sStHashBase = aBase->mStShmHashBase;

    IDE_ASSERT( sStHashBase->mOpened == ID_FALSE );

    sStHashBase->mOpened    = ID_TRUE;
    sStHashBase->mCurBucket = 0; // before search

    if( sStHashBase->mDidAllocChain == ID_TRUE )
    {
        sCurChainPtr = searchFirstNode( aBase );

        if( sCurChainPtr != NULL )
        {
            sStHashBase->mAddrCurChain = sCurChainPtr->mAddrSelf;
        }
        else
        {
            sStHashBase->mAddrCurChain = IDU_SHM_NULL_ADDR;
        }
    }
    else
    {
        //empty
        sStHashBase->mAddrCurChain = IDU_SHM_NULL_ADDR;
    }

    return IDE_SUCCESS;
}

IDE_RC iduShmHash::cutNode( idvSQL           * aStatistics,
                            iduShmTxInfo     * aShmTxInfo,
                            iduLtShmHashBase * aBase,
                            void            ** aNode )
{
    iduStShmHashChain   * sDeleteChain;
    iduStShmHashChain   * sNxtChain;
    iduStShmHashBase    * sStHashBase;
    idrSVP                sSavepoint;

    sStHashBase = aBase->mStShmHashBase;

    idrLogMgr::setSavepoint( aShmTxInfo, &sSavepoint );

    if( sStHashBase->mAddrCurChain != IDU_SHM_NULL_ADDR )
    {
        // delete current node & return the node pointer
        sDeleteChain =
            (iduStShmHashChain*)IDU_SHM_GET_ADDR_PTR( sStHashBase->mAddrCurChain );
        *aNode       =
            (void*)IDU_SHM_GET_ADDR_PTR( sDeleteChain->mAddrNode );

        // find next
        sNxtChain = searchNextNode( aBase );

        iduVLogShmHash::writeCutNode( aShmTxInfo, sStHashBase );

        if( sNxtChain == NULL )
        {
            sStHashBase->mAddrCurChain = IDU_SHM_NULL_ADDR;
        }
        else
        {
            sStHashBase->mAddrCurChain = sNxtChain->mAddrSelf;
        }

        iduShmList::remove( aShmTxInfo, &sDeleteChain->mList );

        IDE_TEST( freeChain( aStatistics,
                             aShmTxInfo,
                             &sSavepoint,
                             aBase,
                             sDeleteChain ) != IDE_SUCCESS );
    }
    else
    {
        *aNode = NULL;
    }

    IDE_TEST( idrLogMgr::commit2Svp( aStatistics,
                                     aShmTxInfo,
                                     &sSavepoint )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_ASSERT( idrLogMgr::abort2Svp( aStatistics,
                                      aShmTxInfo,
                                      &sSavepoint ) == IDE_SUCCESS );

    return IDE_FAILURE;
}

inline IDE_RC iduShmHash::close( iduShmTxInfo     * /*aShmTxInfo*/,
                                 iduLtShmHashBase * aBase )
{
    IDE_ASSERT( aBase->mStShmHashBase->mOpened == ID_TRUE );

    aBase->mStShmHashBase->mOpened = ID_FALSE;

    return IDE_SUCCESS;
}

#endif  // _O_SCU_HASH_H_


