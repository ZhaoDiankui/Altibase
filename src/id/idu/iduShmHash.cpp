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
#include <iduProperty.h>
#include <iduShmDef.h>

#if defined(SMALL_FOOTPRINT)
#define IDU_SHM_HASH_MAX_BUCKET_COUNT (8)
#endif

// 0 => No Latch, 1 => latch
iduShmHashLatchFunc iduShmHash::mLatchVector[2] =
{
    {
        iduShmHash::findNodeNoLatch,  // NoLatch Array
        iduShmHash::insertNodeNoLatch,
        iduShmHash::deleteNodeNoLatch
    },
    {
        iduShmHash::findNodeLatch,    // Latch Array
        iduShmHash::insertNodeLatch,
        iduShmHash::deleteNodeLatch
    }
};

// �� �Լ����� ���� ����
iduShmHashGenFunc  iduShmHash::mArrHashFunction[2] = {
    genHashValueFunc, //IDU_SHMHASH_ID_SMXTRANS_GEN_HASH_VALUE_FUNC
    hashFunc,         //IDU_SHMHASH_ID_SMXTABLEINFOMGR_HASHFUNC
};

iduShmHashCompFunc  iduShmHash::mArrCompFunction[2] = {
    compareFunc,      //IDU_SHMHASH_ID_SMXTRANS_COMPARE_FUNC
    compareFunc4Hash, //IDU_SHMHASH_ID_SMXTABLEINFOMGR_COMPAREFUNC4HASH
};

//smxTrans::genHashValueFunc IDU_SHMHASH_ID_SMXTRANS_GEN_HASH_VALUE_FUNC
UInt iduShmHash::genHashValueFunc( iduStShmHashBase * aStHashInfo, void * aData )
{
    return *(UInt*)(aData) % aStHashInfo->mHashTableSize;
}

//smxTableInfoMgr::hashFunc IDU_SHMHASH_ID_SMXTABLEINFOMGR_HASHFUNC
UInt iduShmHash::hashFunc( iduStShmHashBase * aStHashInfo, void* aKeyPtr )
{
    //TODO iduShmHash::mHashTableSize ?
    return *((vULong*)aKeyPtr) % aStHashInfo->mHashTableSize;
}

//smxTrans::compareFunc IDU_SHMHASH_ID_SMXTRANS_COMPARE_FUNC
SInt iduShmHash::compareFunc( void* aLhs, void* aRhs )
{

    IDE_ASSERT( aLhs != NULL );
    IDE_ASSERT( aRhs != NULL );

    if (*((UInt*)(aLhs)) == *((UInt*)(aRhs)) )
    {
        return (0);
    }

    return (-1);
}

//smxTableInfoMgr::compareFunc4Hash IDU_SHMHASH_ID_SMXTABLEINFOMGR_COMPAREFUNC4HASH
 SInt iduShmHash::compareFunc4Hash( void* aLhs, void* aRhs )
{
    IDE_ASSERT( aLhs != NULL );
    IDE_ASSERT( aRhs != NULL );

    if( *((vULong*)(aLhs)) == *((vULong*)(aRhs)) )
    {
        return (0);
    }

    return (-1);
}

IDE_RC iduShmHash::allocChain( idvSQL               * aStatistics,
                               iduShmTxInfo         * aShmTxInfo,
                               iduLtShmHashBase     * aBase,
                               iduStShmHashChain   ** aChain )
{
    idShmAddr sAddr4Obj;

    /*
     *  Chain ��ü��  mempool�� ���� �Ҵ�޾� �ǵ�����.
     *  aBase ��ü�� ��ϵ� MemoryMgr�� ���� �Ҵ�޴´�.
     */
    IDE_TEST( iduShmMemPool::alloc( aStatistics,
                                    aShmTxInfo,
                                    aBase->mStShmMemPool,
                                    &sAddr4Obj,
                                    (void **)aChain )
              != IDE_SUCCESS );

    (*aChain)->mAddrSelf = sAddr4Obj;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC iduShmHash::freeChain( idvSQL            * aStatistics,
                              iduShmTxInfo      * aShmTxInfo,
                              idrSVP            * aSavepoint,
                              iduLtShmHashBase  * aBase,
                              iduStShmHashChain * aChain )
{
    idrSVP sSavepoint;

    idrLogMgr::setSavepoint( aShmTxInfo, &sSavepoint );

    if( aSavepoint == NULL )
    {
        aSavepoint = &sSavepoint;
    }

    /*
     *  Chain ��ü��  mempool�� �����Ѵ�.
     */
    IDE_TEST( iduShmMemPool::memfree( aStatistics,
                                      aShmTxInfo,
                                      aSavepoint,
                                      aBase->mStShmMemPool,
                                      aChain->mAddrSelf,
                                      (void *)aChain )
              != IDE_SUCCESS );

    IDE_TEST( idrLogMgr::commit2Svp( aStatistics, aShmTxInfo, aSavepoint )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_ASSERT( idrLogMgr::abort2Svp( aStatistics, aShmTxInfo, &sSavepoint )
                == IDE_SUCCESS );

    return IDE_FAILURE;
}

IDE_RC iduShmHash::initialize( idvSQL              * aStatistics,
                               iduShmTxInfo        * aShmTxInfo,
                               UInt                  aConcurrentLevel, /* > 0 */
                               UInt                  aBucketCount,
                               UInt                  aKeyLength,
                               idBool                aUseLatch,
                               iduShmHashGenFuncID   aHashFuncID,
                               iduShmHashCompFuncID  aCompFuncID,
                               idShmAddr             aAddrStShmHashBase,
                               iduStShmHashBase    * aStShmHashBase,
                               iduLtShmHashBase    * aLtHashBase )
{
/*
 *  ȣ���ڴ� ��ü iduStShmHashBase�� �̸� �Ҵ�޾� ȣ���Ѵ�.
 *  aConcurrencyLevel�� ���� mempool �ʱ�ȭ�� ����ؼ� ���ڷ� �ѱ��.
 *  aBucketCount ��ŭ calloc�ؼ� aBase�� �Ŵܴ�.
 *  aKeyLength�� �̿��Ͽ�, iduStShmHashChain�� ���� align�� ũ�⸦ ����ϰ�,
 *  [ (ID_SIZEOF(iduStShmHashChain) + aKeyLength)�� 8�� align ]
 *  �� ���� mempool �ʱ�ȭ�ÿ� ���ڷ� �ѱ��.
 *  Hash, Comp Callback �Լ��� �����Ѵ�.
 *  aUseLatch�� �̿��Ͽ�, �Լ� ������ Array vector�� �����Ѵ�.
 */
    UInt                   i = 0;
    UInt                   sChainSize;
    iduStShmHashBucket   * sHashBucketArray;
    idShmAddr              sAddr4Latch;
    idShmAddr              sAddr4MemPool;
    idShmAddr              sAddr4Bucket;
    idrSVP                 sSavepoint;

    IDE_ASSERT( aKeyLength <= ID_SIZEOF(ULong ) );

#if defined(SMALL_FOOTPRINT)
    if( aBucketCount > IDU_SHM_HASH_MAX_BUCKET_COUNT )
    {
        aBucketCount = IDU_SHM_HASH_MAX_BUCKET_COUNT;
    }
#endif

    IDE_ASSERT( aConcurrentLevel > 0 );

    idrLogMgr::setSavepoint( aShmTxInfo, &sSavepoint );

    aStShmHashBase->mAddrSelf = aAddrStShmHashBase;

    //fix BUG-21311
    aStShmHashBase->mDidAllocChain = ID_FALSE;

    sAddr4Latch = IDU_SHM_GET_ADDR_SUBMEMBER( aStShmHashBase->mAddrSelf,
                                              iduStShmHashBase,
                                              mLatch );

    iduShmLatchInit( sAddr4Latch,
                     IDU_SHM_LATCH_SPIN_COUNT_DEFAULT,
                     &aStShmHashBase->mLatch );

    sChainSize = ID_SIZEOF(iduStShmHashChain) + aKeyLength;
    sChainSize = idlOS::align8(sChainSize); // align to 8

    /* TC/Server/LimitEnv/sm/smu/iduShmHash_initialize_malloc1.sql */
    IDU_LIMITPOINT_RAISE(
        "iduShmHash::initialize::malloc1",
        err_insufficient_memory );

    sAddr4MemPool = IDU_SHM_GET_ADDR_SUBMEMBER( aStShmHashBase->mAddrSelf,
                                                iduStShmHashBase,
                                                mShmMemPool );

    /* Allocate & Init MemoryPool for keyChain */
    IDE_TEST( iduShmMemPool::initialize( aStatistics,
                                         aShmTxInfo,
                                         IDU_MEM_SM_SMU,
                                         (SChar*)"SMUHASH_OBJECT",
                                         aConcurrentLevel,
                                         sChainSize,
                                         64,
                                         IDU_AUTOFREE_CHUNK_LIMIT,
                                         ID_TRUE, /* aUseMutex */
                                         IDU_SHM_MEM_POOL_DEFAULT_ALIGN_SIZE,
                                         ID_TRUE, /* aGarbageCollection */
                                         sAddr4MemPool,
                                         &aStShmHashBase->mShmMemPool )
              != IDE_SUCCESS );


    /* TC/Server/LimitEnv/sm/smu/iduShmHash_initialize_malloc2.sql */
    IDU_LIMITPOINT_RAISE( "iduShmHash::initialize::malloc2",
                          err_insufficient_memory );

    /* Allocate  Bucket */
    IDE_TEST_RAISE( iduShmMgr::allocMem( aStatistics,
                                         aShmTxInfo,
                                         IDU_MEM_SM_SMU,
                                         ID_SIZEOF(iduStShmHashBucket) * aBucketCount,
                                         (void**)&sHashBucketArray )
                    != IDE_SUCCESS, err_insufficient_memory );


    /* Init Bucket */
    aStShmHashBase->mAddrBucketLst = iduShmMgr::getAddr( sHashBucketArray );
    aStShmHashBase->mBucketCount   = aBucketCount;
    aStShmHashBase->mKeyLength     = aKeyLength;

    sAddr4Bucket = aStShmHashBase->mAddrBucketLst;

    for( i = 0; i < aBucketCount; i++ )
    {
        iduStShmHashBucket *sBucket = sHashBucketArray + i;

        IDU_SHM_VALIDATE_ADDR_PTR( sAddr4Bucket, sBucket );

        sBucket->mAddrSelf = sAddr4Bucket;

        iduShmSXLatchInit( IDU_SHM_GET_ADDR_SUBMEMBER( sAddr4Bucket,
                                                       iduStShmHashBucket,
                                                       mLatch ),
                           ID_FALSE, /* aCheckAcquired */
                           iduProperty::getNativeMutexSpinCount(),
                           &sBucket->mLatch );

        sBucket->mCount = 0;
        iduShmList::initBase( &sBucket->mBaseList,
                              IDU_SHM_GET_ADDR_SUBMEMBER( sAddr4Bucket,
                                                          iduStShmHashBucket,
                                                          mBaseList ),
                              IDU_SHM_NULL_ADDR /* aData */ );

        IDU_SHM_VALIDATE_ADDR_PTR( sBucket->mAddrSelf, sBucket );

        sAddr4Bucket += ID_SIZEOF(iduStShmHashBucket);
    }

    aLtHashBase->mStShmHashBase = aStShmHashBase;
    aLtHashBase->mStShmMemPool  = &aStShmHashBase->mShmMemPool;
    aLtHashBase->mBucket        = sHashBucketArray;
    aLtHashBase->mLatchVector   = &mLatchVector[ aUseLatch == ID_FALSE ? 0 : 1 ];

    /* - Init Function Callback Pointers */
    aLtHashBase->mHashFunc     = mArrHashFunction[aHashFuncID];
    aLtHashBase->mCompFunc     = mArrCompFunction[aCompFuncID];

    aStShmHashBase->mHashFuncID = aHashFuncID;
    aStShmHashBase->mCompFuncID = aCompFuncID;

    aStShmHashBase->mUseLatch      = aUseLatch;
    aStShmHashBase->mHashTableSize = aBucketCount;

    aStShmHashBase->mOpened = ID_FALSE ;
    aStShmHashBase->mCurBucket = 0;;
    aStShmHashBase->mAddrCurChain = IDU_SHM_NULL_ADDR; 

    IDE_TEST( idrLogMgr::commit2Svp( aStatistics, aShmTxInfo, &sSavepoint )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_already_created));
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();

    IDE_ASSERT( idrLogMgr::abort2Svp( aStatistics, aShmTxInfo, &sSavepoint )
                == IDE_SUCCESS );

    IDE_POP();

    return IDE_FAILURE;
}

IDE_RC iduShmHash::attach( idBool                aUseLatch,
                           iduStShmHashBase    * aStShmHashBase,
                           iduLtShmHashBase    * aLtHashBase )
{
    IDU_SHM_VALIDATE_ADDR_PTR( aStShmHashBase->mAddrSelf, aStShmHashBase );

    aLtHashBase->mStShmHashBase = aStShmHashBase;
    aLtHashBase->mStShmMemPool  = &aStShmHashBase->mShmMemPool;

    aLtHashBase->mBucket        =
        (iduStShmHashBucket*)IDU_SHM_GET_ADDR_PTR( aStShmHashBase->mAddrBucketLst );

    aLtHashBase->mLatchVector   = &mLatchVector[ aUseLatch == ID_FALSE ? 0 : 1 ];

    /* - Init Function Callback Pointers */
    aLtHashBase->mHashFunc     = mArrHashFunction[aStShmHashBase->mHashFuncID];
    aLtHashBase->mCompFunc     = mArrCompFunction[aStShmHashBase->mCompFuncID];

    return IDE_SUCCESS;
}

IDE_RC iduShmHash::detach( iduLtShmHashBase * aLtHashBase )
{

    aLtHashBase->mStShmHashBase = NULL;
    aLtHashBase->mStShmMemPool  = NULL;
    aLtHashBase->mBucket        = NULL;
    aLtHashBase->mLatchVector   = NULL;

    aLtHashBase->mHashFunc     = NULL;
    aLtHashBase->mCompFunc     = NULL;

    return IDE_SUCCESS;
}

IDE_RC iduShmHash::reset( idvSQL            * aStatistics,
                          iduShmTxInfo      * aShmTxInfo,
                          iduLtShmHashBase  * aLtHashBase )
{
    iduStShmHashChain  *sCurChain;
    idrSVP              sSavepoint;

    idrLogMgr::setSavepoint( aShmTxInfo, &sSavepoint );

    sCurChain = NULL;
    IDE_TEST( open( aShmTxInfo, aLtHashBase ) != IDE_SUCCESS );

    while (1)
    {
        IDE_TEST( cutNode( aStatistics, aShmTxInfo, aLtHashBase, (void**)&sCurChain )
                  != IDE_SUCCESS );

        if (sCurChain == NULL)
        {
            break;
        }
    }

    IDE_TEST( close( aShmTxInfo, aLtHashBase ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_ASSERT( idrLogMgr::abort2Svp( aStatistics, aShmTxInfo, &sSavepoint )
                == IDE_SUCCESS );

    return IDE_FAILURE;
}

IDE_RC iduShmHash::destroy( idvSQL           * aStatistics,
                            iduShmTxInfo     * aShmTxInfo,
                            iduLtShmHashBase * aLtHashBase )
{
    UInt                    i;
    iduStShmHashChain     * sCurChain;
    iduStShmHashBase      * sStHashBase;
    iduStShmHashBucket    * sBucket;
    idrSVP                  sSavepoint;

    /*  mempool�� �����Ѵ�.
     *  ��, ���� �Ŵ޸� chain�� 0�̾�� �Ѵ�. �ƴ� ��� ASSERT!!*/
    sCurChain = NULL;

    idrLogMgr::setSavepoint( aShmTxInfo, &sSavepoint );

    sStHashBase = aLtHashBase->mStShmHashBase;

    IDE_TEST( open( aShmTxInfo, aLtHashBase ) != IDE_SUCCESS );

    while (1) /* bucket traverse and free chain */
    {
        IDE_TEST( cutNode( aStatistics, aShmTxInfo, aLtHashBase, (void**)&sCurChain )
                  != IDE_SUCCESS );

        if( sCurChain == NULL )
        {
            break;
        }
    }

    IDE_TEST( close( aShmTxInfo, aLtHashBase ) != IDE_SUCCESS );

    /* Bucket Destroy */
    for( i = 0; i < sStHashBase->mBucketCount; i++ )
    {
        sBucket = aLtHashBase->mBucket + i;

        IDE_ASSERT( sBucket->mCount == 0 );

        iduShmSXLatchDest( &sBucket->mLatch );
    }

    if( sStHashBase->mAddrBucketLst != IDU_SHM_NULL_ADDR )
    {
        IDE_TEST( iduVLogShmHash::writeUpdateAddrBucketList( aShmTxInfo,
                                                             sStHashBase )
                  != IDE_SUCCESS );

        /* Bucket Memory Free */
        sStHashBase->mAddrBucketLst = IDU_SHM_NULL_ADDR;

        IDE_TEST( iduShmMgr::freeMem( aStatistics,
                                      aShmTxInfo,
                                      &sSavepoint,
                                      (void *)aLtHashBase->mBucket )
                  != IDE_SUCCESS );
        aLtHashBase->mBucket = NULL;
    }

    if( aLtHashBase->mStShmMemPool != NULL )
    {
        /* KeyChain mempool destroy */
        IDE_TEST( iduShmMemPool::destroy( aStatistics,
                                          aShmTxInfo,
                                          &sSavepoint,
                                          aLtHashBase->mStShmMemPool,
                                          ID_TRUE/* Check */ )
                  != IDE_SUCCESS );

        aLtHashBase->mStShmMemPool = NULL;
    }

    iduShmLatchDest( &sStHashBase->mLatch );

    IDE_TEST( idrLogMgr::commit2Svp( aStatistics, aShmTxInfo, &sSavepoint )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_ASSERT( idrLogMgr::abort2Svp( aStatistics, aShmTxInfo, &sSavepoint )
                == IDE_SUCCESS );

    return IDE_FAILURE;
}

IDE_RC iduShmHash::findNode( idvSQL            * aStatistics,
                             iduShmTxInfo      * aShmTxInfo,
                             iduLtShmHashBase  * aBase,
                             void              * aKeyPtr,
                             void             ** aNode )
{
    UInt aHashValue;

    // Vector ȣ��
    aHashValue = aBase->mHashFunc( aBase->mStShmHashBase, aKeyPtr );
    return (*aBase->mLatchVector->findNode)( aStatistics,
                                             aShmTxInfo,
                                             aBase,
                                             &( aBase->mBucket[ aHashValue ] ),
                                             aKeyPtr,
                                             aNode );
}

IDE_RC iduShmHash::insertNode( idvSQL            * aStatistics,
                               iduShmTxInfo      * aShmTxInfo,
                               iduLtShmHashBase  * aBase,
                               void              * aKeyPtr,
                               idShmAddr           aAddr4Node,
                               void              * aNode )
{
    UInt aHashValue;

    // Vector ȣ��
    aHashValue = aBase->mHashFunc( aBase->mStShmHashBase, aKeyPtr );
    IDE_TEST( (*aBase->mLatchVector->insertNode)( aStatistics,
                                                  aShmTxInfo,
                                                  aBase,
                                                  &(aBase->mBucket[aHashValue]),
                                                  aKeyPtr,
                                                  aAddr4Node,
                                                  aNode )
              != IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC iduShmHash::deleteNode( idvSQL            * aStatistics,
                               iduShmTxInfo      * aShmTxInfo,
                               idrSVP            * aVSavepoint,
                               iduLtShmHashBase  * aBase,
                               void              * aKeyPtr,
                               void             ** aNode )
{
    UInt aHashValue;

    // Vector ȣ��
    aHashValue = aBase->mHashFunc( aBase->mStShmHashBase, aKeyPtr );
    return (*aBase->mLatchVector->deleteNode)( aStatistics,
                                               aShmTxInfo,
                                               aVSavepoint,
                                               aBase,
                                               &(aBase->mBucket[aHashValue]),
                                               aKeyPtr,
                                               aNode );
}

// =============================================================================
//  No Latch
// =============================================================================
iduStShmHashChain* iduShmHash::findNodeInternal( iduLtShmHashBase     * aBase,
                                                 iduStShmHashBucket   * aBucket,
                                                 void                 * aKeyPtr )
{
    iduShmListNode          * sList;
    iduShmListNode          * sBaseList;
    iduStShmHashChain  * sChainPtr;

    sBaseList = &( aBucket->mBaseList );

    for( sList  = iduShmList::getNxtPtr( sBaseList );
         sList != sBaseList;
         sList  = iduShmList::getNxtPtr( sList ) )
    {
        sChainPtr = (iduStShmHashChain *)iduShmList::getDataPtr(sList);

        if( aBase->mCompFunc( aKeyPtr, sChainPtr->mKey ) == 0 )
        {
            return sChainPtr;
        }
    }

    return NULL;
}

IDE_RC iduShmHash::findNodeNoLatch( idvSQL               * /*aStatistics*/,
                                    iduShmTxInfo         * /*aShmTxInfo*/,
                                    iduLtShmHashBase     * aBase,
                                    iduStShmHashBucket   * aBucket,
                                    void                 * aKeyPtr,
                                    void                ** aNode )
{
    /*
     *  Chain�� ���󰡸鼭, ������ key ���� �����ϴ��� �˻��Ѵ�.
     *  �ִٸ� �ش� ������, ������ NULL�� �����Ѵ�.
     */
    iduStShmHashChain * sChain;

    sChain = iduShmHash::findNodeInternal( aBase, aBucket, aKeyPtr );

    *aNode = ( sChain == NULL ?
               NULL : (void*)IDU_SHM_GET_ADDR_PTR( sChain->mAddrNode ) );

    return IDE_SUCCESS;
}

//#define USE_CLASSIC_MEMORY
IDE_RC iduShmHash::insertNodeNoLatch( idvSQL              * aStatistics,
                                      iduShmTxInfo        * aShmTxInfo,
                                      iduLtShmHashBase    * aBase,
                                      iduStShmHashBucket  * aBucket,
                                      void                * aKeyPtr,
                                      idShmAddr             aAddr4Node,
                                      void                * aNode )
{
    /*
     *  HashCahin�� mempool�� ���� �Ҵ�ް�, Ű�� �����ϰ�,
     *  ��� �����͸� assign�Ѵ�.
     *  �� Chain�� �ش� Bucket ����Ʈ ����� �����Ѵ�.
     */
    iduStShmHashChain *sChain;
    idShmAddr          sAddrObj;
    idrSVP             sSavepoint;

    idrLogMgr::setSavepoint( aShmTxInfo, &sSavepoint );

    // fix BUG-21311
    aBase->mStShmHashBase->mDidAllocChain = ID_TRUE;

    // alloc Chain & Init
    IDE_TEST( allocChain( aStatistics, aShmTxInfo, aBase, &sChain )
              != IDE_SUCCESS );

    IDU_SHM_VALIDATE_ADDR_PTR( aAddr4Node, aNode );

    sChain->mAddrNode = aAddr4Node;
    idlOS::memcpy( sChain->mKey,
                   aKeyPtr,
                   aBase->mStShmHashBase->mKeyLength ); // copy of the key

    sAddrObj = IDU_SHM_GET_ADDR_SUBMEMBER( sChain->mAddrSelf,
                                           iduStShmHashChain,
                                           mList );
    // Add to List
    iduShmList::initNode( &sChain->mList, sAddrObj, sChain->mAddrSelf );
    iduShmList::addFirst( aShmTxInfo, &aBucket->mBaseList, &sChain->mList );

    iduVLogShmHash::writeNTAInsertNodeNoLatch( aShmTxInfo,
                                               sSavepoint.mLSN,
                                               aBase,
                                               aBucket,
                                               aKeyPtr,
                                               aBase->mStShmHashBase->mKeyLength );



    //aBucket->mCount++;
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_ASSERT( idrLogMgr::abort2Svp( aStatistics, aShmTxInfo, &sSavepoint )
                == IDE_SUCCESS );

    return IDE_FAILURE;
}

IDE_RC iduShmHash::deleteNodeNoLatch( idvSQL               * aStatistics,
                                      iduShmTxInfo         * aShmTxInfo,
                                      idrSVP               * aVSavepoint,
                                      iduLtShmHashBase     * aBase,
                                      iduStShmHashBucket   * aBucket,
                                      void                 * aKeyPtr,
                                      void                ** aNode )
{
    /*
     *  Chain�� ���󰡸鼭, ������ key ���� �����ϴ��� �˻��Ѵ�.
     *  ã��  Chain�� �ش� Bucket ����Ʈ���� �����Ѵ�.
     *  aNode�� �ش� Node���� �����Ѵ�.
     *  HashChain�� mempool�� �����Ѵ�.
     */
    iduStShmHashChain *sChain;
    idrSVP             sSavepoint;

    idrLogMgr::setSavepoint( aShmTxInfo, &sSavepoint );

    if( aVSavepoint == NULL )
    {
        aVSavepoint = &sSavepoint;
    }

    sChain = iduShmHash::findNodeInternal( aBase, aBucket, aKeyPtr );

    /* Runtime Recovery�����ؼ� �ι� Delete�Ǿ� Hash���� �̹� �����Ǿ��� ��
     * �ִ�. */
    if( sChain != NULL )
    {
        iduShmList::remove( aShmTxInfo, &sChain->mList );

        *aNode = (void*)IDU_SHM_GET_ADDR_PTR( sChain->mAddrNode );

        IDE_TEST( freeChain( aStatistics, aShmTxInfo, &sSavepoint, aBase, sChain )
                  != IDE_SUCCESS );
    }
    else
    {
        *aNode = NULL;
    }


    IDE_TEST( idrLogMgr::commit2Svp( aStatistics, aShmTxInfo, aVSavepoint )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_ASSERT( idrLogMgr::abort2Svp( aStatistics, aShmTxInfo, &sSavepoint )
                == IDE_SUCCESS );

    return IDE_FAILURE;
}

// =============================================================================
//  Use Latch
// =============================================================================
IDE_RC iduShmHash::findNodeLatch( idvSQL               * aStatistics,
                                  iduShmTxInfo         * aShmTxInfo,
                                  iduLtShmHashBase     * aBase,
                                  iduStShmHashBucket   * aBucket,
                                  void                 * aKeyPtr,
                                  void                ** aNode )
{
    idrSVP  sSavepoint;

    idrLogMgr::setSavepoint( aShmTxInfo, &sSavepoint );

    /* Lock�� ��´�.
     * findNodeNoLatch() ȣ��
     * UnLock();
     */
    IDE_TEST( iduShmSXLatchAcquire( aStatistics,
                                    aShmTxInfo,
                                    &aBucket->mLatch,
                                    IDU_SX_LATCH_MODE_SHARED,
                                    IDU_SHM_SX_LATCH_INFINITE_WAIT )
              != IDE_SUCCESS );

    IDE_TEST( iduShmHash::findNodeNoLatch( aStatistics,
                                           aShmTxInfo,
                                           aBase,
                                           aBucket,
                                           aKeyPtr,
                                           aNode )
              != IDE_SUCCESS );

    IDE_TEST( idrLogMgr::commit2Svp( aStatistics, aShmTxInfo, &sSavepoint )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_ASSERT( idrLogMgr::abort2Svp( aStatistics, aShmTxInfo, &sSavepoint )
                == IDE_SUCCESS );

    return IDE_FAILURE;
}

IDE_RC iduShmHash::insertNodeLatch( idvSQL             * aStatistics,
                                    iduShmTxInfo       * aShmTxInfo,
                                    iduLtShmHashBase   * aBase,
                                    iduStShmHashBucket * aBucket,
                                    void               * aKeyPtr,
                                    idShmAddr            aAddr4Node,
                                    void               * aNode )
{
    /* Lock�� ��´�.
     * insertNodeNoLatch() ȣ��
     * UnLock();
     */
    idrSVP  sSavepoint;

    idrLogMgr::setSavepoint( aShmTxInfo, &sSavepoint );

    IDE_TEST( iduShmSXLatchAcquire( aStatistics,
                                    aShmTxInfo,
                                    &aBucket->mLatch,
                                    IDU_SX_LATCH_MODE_EXCLUSIVE,
                                    IDU_SHM_SX_LATCH_INFINITE_WAIT )
              != IDE_SUCCESS );

    IDE_TEST( iduShmHash::insertNodeNoLatch( aStatistics,
                                             aShmTxInfo,
                                             aBase,
                                             aBucket,
                                             aKeyPtr,
                                             aAddr4Node,
                                             aNode ) != IDE_SUCCESS );

    IDE_TEST( idrLogMgr::commit2Svp( aStatistics, aShmTxInfo, &sSavepoint )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_ASSERT( idrLogMgr::abort2Svp( aStatistics, aShmTxInfo, &sSavepoint )
                == IDE_SUCCESS );

    return IDE_FAILURE;
}

IDE_RC iduShmHash::deleteNodeLatch( idvSQL             * aStatistics,
                                    iduShmTxInfo       * aShmTxInfo,
                                    idrSVP             * aVSavepoint,
                                    iduLtShmHashBase   * aBase,
                                    iduStShmHashBucket * aBucket,
                                    void               * aKeyPtr,
                                    void              ** aNode )
{
    idrSVP  sSavepoint;

    idrLogMgr::setSavepoint( aShmTxInfo, &sSavepoint );

    IDE_TEST( iduShmSXLatchAcquire( aStatistics,
                                    aShmTxInfo,
                                    &aBucket->mLatch,
                                    IDU_SX_LATCH_MODE_EXCLUSIVE,
                                    IDU_SHM_SX_LATCH_INFINITE_WAIT )
              != IDE_SUCCESS );

    IDE_TEST( iduShmHash::deleteNodeNoLatch( aStatistics,
                                             aShmTxInfo,
                                             aVSavepoint,
                                             aBase,
                                             aBucket,
                                             aKeyPtr,
                                             aNode )
              != IDE_SUCCESS );


    IDE_TEST( idrLogMgr::commit2Svp( aStatistics, aShmTxInfo, &sSavepoint )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_ASSERT( idrLogMgr::abort2Svp( aStatistics, aShmTxInfo, &sSavepoint )
                == IDE_SUCCESS );

    return IDE_FAILURE;
}

void iduShmHash::lock( idvSQL            * aStatistics,
                       iduShmTxInfo      * aShmTxInfo,
                       iduLtShmHashBase  * aBase )
{
    /*
     * Hash �Ŵ����� ���� mutex�� ��´�.
     */
    iduShmLatchAcquire( aStatistics,
                        aShmTxInfo,
                        &aBase->mStShmHashBase->mLatch );
}

// =============================================================================
//  Use Latch
// =============================================================================
/*
 *  Open�ÿ� ȣ��Ǹ�,  ù��° HashNode�� ã�� ���´�.
 */
iduStShmHashChain *iduShmHash::searchFirstNode( iduLtShmHashBase *aLtHashBase )
{
    UInt                  i;
    iduShmListNode      * sList;
    iduShmListNode      * sBaseList;
    iduStShmHashBucket  * sBucket;
    iduStShmHashBase    * sStHashBase;

    sStHashBase = aLtHashBase->mStShmHashBase;

    IDE_ASSERT( sStHashBase->mCurBucket < sStHashBase->mBucketCount );
    IDE_ASSERT( sStHashBase->mOpened == ID_TRUE );

    for( i = 0; i < sStHashBase->mBucketCount; i++ )
    {
        sBucket   = aLtHashBase->mBucket + i;
        sBaseList = &sBucket->mBaseList;

        sStHashBase->mCurBucket = i;

        for( sList  = iduShmList::getNxtPtr( sBaseList );
             sList != sBaseList;
             sList  = iduShmList::getNxtPtr( sList ) )
        {
            return (iduStShmHashChain *)iduShmList::getDataPtr( sList );
        }
    }

    return NULL;
}

iduStShmHashChain *iduShmHash::searchNextNode( iduLtShmHashBase *aLtHashBase )
{
    idBool                   sFirst = ID_TRUE;
    UInt                     i;
    iduShmListNode         * sStartList;
    iduShmListNode         * sCurList;
    iduShmListNode         * sBaseList;
    iduStShmHashBucket     * sBucket;
    iduStShmHashBase       * sStHashBase;
    iduStShmHashChain      * sCurChainPtr;

    sStHashBase = aLtHashBase->mStShmHashBase;

    IDE_ASSERT( sStHashBase->mCurBucket < sStHashBase->mBucketCount );
    IDE_ASSERT( sStHashBase->mOpened == ID_TRUE );

    for( i = sStHashBase->mCurBucket;
         i < sStHashBase->mBucketCount;
         i++ )
    {
        sStHashBase->mCurBucket = i;

        sBucket   = aLtHashBase->mBucket + i;
        sBaseList = &sBucket->mBaseList;

        if( sFirst == ID_TRUE )
        {
            sCurChainPtr
                = (iduStShmHashChain *)IDU_SHM_GET_ADDR_PTR( sStHashBase->mAddrCurChain );

            sStartList = &sCurChainPtr->mList;
            sFirst = ID_FALSE;
        }
        else
        {
            sStartList = sBaseList;
        }

        for (sCurList = iduShmList::getNxtPtr( sStartList );
             sCurList != sBaseList;
             sCurList = iduShmList::getNxtPtr( sCurList ) )
        {
            sStHashBase->mAddrCurChain = iduShmList::getData( sCurList );
            sCurChainPtr
                = (iduStShmHashChain *)iduShmList::getDataPtr( sCurList );

            return sCurChainPtr;
        }
    }

    sStHashBase->mAddrCurChain = IDU_SHM_NULL_ADDR;

    return NULL;
}

IDE_RC iduShmHash::isEmpty( iduShmTxInfo      * aShmTxInfo,
                            iduLtShmHashBase  * aLtHashBase,
                            idBool            * aIsEmpty )
{
    iduStShmHashBase * sStHashBase;

    sStHashBase = aLtHashBase->mStShmHashBase;

    IDE_ASSERT( aLtHashBase != NULL );
    IDE_ASSERT( aIsEmpty != NULL );

    IDE_TEST( open( aShmTxInfo, aLtHashBase ) != IDE_SUCCESS );

    if( sStHashBase->mAddrCurChain == IDU_SHM_NULL_ADDR )
    {
        *aIsEmpty = ID_TRUE;
    }
    else
    {
        *aIsEmpty = ID_FALSE;
    }

    IDE_TEST( close( aShmTxInfo, aLtHashBase ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
