/***********************************************************************
 * Copyright 1999-2000, RTBase Corporation or its subsidiaries.
 * All rights reserved.
 **********************************************************************/

/***********************************************************************
 * $Id: iduMemPool.cpp 76892 2016-08-29 07:16:44Z djin $
 **********************************************************************/

#include <idl.h>
#include <idu.h>
#include <ideErrorMgr.h>
#include <idErrorCode.h>
#include <iduMemList.h>
#include <iduMemPool.h>
#include <iduMemPoolMgr.h>


#undef IDU_ALIGN
#define IDU_ALIGN(x)          idlOS::align(x,mAlignByte)

extern const vULong gFence;

/*
 * �ش� mem pool�� mem pool mgr�� ���ؼ� �����Ǿ����� �ִ°�?
 */
#define IDU_ADD_OR_DEL_MEMPOOL ( (iduMemMgr::isServerMemType() == ID_TRUE) &&\
                                 (mFlag & IDU_MEMPOOL_GARBAGE_COLLECT )    &&\
                                 (mFlag & IDU_MEMPOOL_USE_MUTEX )          &&\
                                 (mFlag & IDU_MEMPOOL_USE_POOLING )  )  


#define IDU_MAX_PAGE_SIZE_IN_BYTES       (4096*2)     //8k

#define IDU_INIT_MEMPOOL_CHUNK_PAGE_CNT  (4)

//chunk�� �ּ� ���԰���
#define IDU_SLOT_CNT_MIN                 (5)

//���� object���� ���� �ִ� ����ũ��
#define IDU_SMALL_SLOT_MAX_IN_BYTES           (1024)

//���� object���� ���� chunk�� �ִ�ũ��
#define IDU_MAX_MEMPOOL_CHUNK_IN_BYTES   (1024*1024*64)  //64M

#define IDU_MEMPOOL_MIN_SIZE_FOR_HW_CACHE_ALIGN    (1024*4)  

/* ------------------------------------------------

   MemPool ������ ���� ( 2005.3.30 by lons )


   ����
   mempool�� �Ź� malloc system call�� ȣ������ �ʰ�
   memory�� �Ҵ��ϱ� ���� ������ ���� Ŭ�����̴�.
   �ϳ��� mempool���� �Ҵ��� �� �� �ִ� �޸��� ũ��� �׻� �����ȴ�.
   ���� �޸� ũ�⸦ �Ҵ��ϰ��� �Ѵٸ� �ٸ� ũ���
   init�� mempool�� 2�� �̻� ������ �Ѵ�.


   �������( class )
   iduMemPool : mem list�� �����Ѵ�. �ϳ��� mem pool���� mem list��
                �ټ����� �� �ִ�. �̰��� system cpu ������ ��������.
   iduMemList : ���� memory�� �Ҵ��Ͽ� Ȯ���ϰ� �ִ� list
                memChunk�� ����Ʈ�� �����ȴ�.
   iduMemChunk : iduMemSlot list�� �����Ѵ�. mempool���� �Ҵ�����
                 �޸𸮰� ���ڶ������� �ϳ��� �߰��ȴ�.
                 iduMemSlot�� free count, max count�� �����Ѵ�.
   iduMemSlot : ���� �Ҵ����� �޸�

    -------      -------
   | chunk | -  | chunk | - ...
    -------      -------
      |
    ---------      ---------
   | element |  - | element | - ....
    ---------      ---------

    (eg.)

    iduMemPool::initialize( cpu����, ID_SIZEOF(UInt), 5 )�� �����ϸ�
    �ϳ��� chunk�� 5���� element�� �����.
    element�� ������ � ��ŭ�� �޸𸮸� �̸� �Ҵ��ϴ��ĸ� �����Ѵ�.

    -------
   | chunk |
    -------
      |
    ---------      ---------
   | element |  - | element | - ...
    ---------      ---------

    iduMemPool::alloc�� �����ϸ�
    element �ϳ��� return �ȴ�.
    ���� element�� �����ϸ� chunk �ϳ��� element 5���� �� �Ҵ��Ѵ�.
 * ----------------------------------------------*/
/*-----------------------------------------------------------
 * Description: iduMemPool�� �ʱ�ȭ �Ѵ�.
 *
 * aIndex            - [IN] �� �Լ��� ȣ���� ���
 *                        (��� ȣ���ϴ��� ������ �����ϱ� ���� �ʿ�)
 * aName             - [IN] iduMemPool�� �ĺ� �̸�
 * aListCount
 * aElemSize         - [IN] �Ҵ��� �޸��� ����ũ��
 * aElemCount        -  not used.
 * aChunkLimit       - [IN] memPool���� ������ chunk������ �ִ�ġ.
 *             (�� ��� chunk�� �� �Ҵ����� ��쿡�� �� ������ ���Ե��� �ʴ´�.)
 * aUseMutex         - [IN] mutex��� ����
 * aAlignByte        -  not used.
 *                          ���� �־ �Լ��� ȣ���Ѵ�.
 * aForcePooling     - [IN] property���� mem pool�� ������� �ʵ��� �����ߴ��� 
 *                          ������ ����.(BUG-21547)
 * aGarbageCollection - [IN] Garbage collection�� �⿩�ϵ��� �Ұ��ΰ� ����.
 *                           ID_TRUE�϶� mem pool manager�� ���ؼ� �����Ǿ���
 * aHWCacheLIne       - [IN] H/W cache line align �� ����Ұ��ΰ�
 * ---------------------------------------------------------*/
IDE_RC iduMemPool::initialize(iduMemoryClientIndex aIndex,
                              SChar*               aName,
                              UInt                 aListCount,
                              vULong               aElemSize,
                              vULong               /* aElemCount */, //not used..
                              vULong               aChunkLimit,
                              idBool               aUseMutex,
                              UInt                 aAlignByte,
                              idBool               aForcePooling,
                              idBool               aGarbageCollection,
                              idBool               aHWCacheLine )
{
    UInt  i;
    ULong sPageSize;
    ULong sCurChunkSize=0;
    ULong sCnt=0;
    ULong sMinSlotCnt = iduProperty::getMinMemPoolSlotCount();

    IDE_ASSERT( aName != NULL );

    // mFlag setting. ////////////////////////////////////
    mFlag = 0;

    //aUseMutex
    if( aUseMutex == ID_TRUE )
    {
        mFlag |=  IDU_MEMPOOL_USE_MUTEX;
    } 

    //aForcePooling
    if (IDU_USE_MEMORY_POOL == 1)
    {
        mFlag |=  IDU_MEMPOOL_USE_POOLING;
    }
    else
    {
        if (aForcePooling == ID_TRUE)
        {
            mFlag |=  IDU_MEMPOOL_USE_POOLING;
        }
        else
        {
            mFlag &=  ~IDU_MEMPOOL_USE_POOLING;
        }
    }

    //aGarbageCollection
    if( aGarbageCollection == ID_TRUE )
    {
        mFlag |=  IDU_MEMPOOL_GARBAGE_COLLECT;
    } 

    //aHWCacheLine
    if( aHWCacheLine == ID_TRUE )
    {
        mFlag |=  IDU_MEMPOOL_CACHE_LINE_IDU_ALIGN;
    } 
    ///////////////////////////////////////////////////// 


    mAutoFreeChunkLimit = aChunkLimit;

    if( (mFlag & IDU_MEMPOOL_CACHE_LINE_IDU_ALIGN) &&
        (aElemSize <= IDU_MEMPOOL_MIN_SIZE_FOR_HW_CACHE_ALIGN) )
    {
        mAlignByte = IDL_CACHE_LINE;
    }
    else
    {
        mAlignByte =  aAlignByte;
    }

    sPageSize = idlOS::getpagesize();

    if( sPageSize > IDU_MAX_PAGE_SIZE_IN_BYTES )
    {
        sPageSize = IDU_MAX_PAGE_SIZE_IN_BYTES;
    }

    mElemSize   =  IDU_ALIGN( aElemSize );

#ifdef MEMORY_ASSERT
    mSlotSize = IDU_ALIGN( mElemSize +
                           ID_SIZEOF(gFence)       +
                           ID_SIZEOF(iduMemSlot *) +
                           ID_SIZEOF(gFence));
#else
    mSlotSize   = IDU_ALIGN(mElemSize + ID_SIZEOF(iduMemSlot *));
#endif

    // default values.
    sCurChunkSize = roundUp2n(CHUNK_HDR_SIZE + mSlotSize* IDU_SLOT_CNT_MIN); 
    sCnt = IDU_SLOT_CNT_MIN;

    /*
     *  get sCnt (element count)  
     */
    if( mSlotSize <= IDU_SMALL_SLOT_MAX_IN_BYTES )
    {
        while ( sCurChunkSize <= IDU_MAX_MEMPOOL_CHUNK_IN_BYTES )
        {
            sCnt      = (sCurChunkSize  - CHUNK_HDR_SIZE ) / mSlotSize;
            
            // BUG-40492 Slot Count�� ������ Chunk Size�� �����Ѵ�.
            if( sCnt >= sMinSlotCnt )
            {
                break;
            }
            sCurChunkSize *=2;
        }
    }
    else
    {
        //used default values above.
    }

    mElemCnt     = sCnt;

    //right now. if it's not, a problem occurs.but it's never going to happen.
    IDE_ASSERT( mElemCnt > 1 ); 

    mChunkSize   = sCurChunkSize;

    IDE_ASSERT( roundUp2n(CHUNK_HDR_SIZE + mSlotSize* IDU_SLOT_CNT_MIN ) <= 
                mChunkSize ); //just for sure


    if (aName == NULL)
    {
        aName = (SChar *)"noname";
    }

    idlOS::snprintf( mName, 
                     IDU_MEM_POOL_NAME_LEN, 
                     "%s",  
                     aName );

    mListCount   = aListCount;
    mIndex       = aIndex;

    mNext = NULL;

    if( mFlag & IDU_MEMPOOL_USE_POOLING )
    {
        IDE_TEST(iduMemMgr::calloc(mIndex,
                                   mListCount,
                                   ID_SIZEOF(iduMemList *),
                                   (void**)&mMemList)
                 != IDE_SUCCESS);

        for (i = 0; i < mListCount; i++)
        {
            mMemList[i] = NULL;

            IDE_TEST(iduMemMgr::malloc(mIndex,
                                       ID_SIZEOF(iduMemList),
                                       (void**)&(mMemList[i]))
                     != IDE_SUCCESS);

#ifdef __CSURF__
            IDE_ASSERT( mMemList[i] != NULL );
            
            new (mMemList[i]) iduMemList();
#else
            mMemList[i] = new (mMemList[i]) iduMemList();
#endif

            IDE_TEST(  mMemList[i]->initialize( i, this) != IDE_SUCCESS);
        }
    }
    else
    {
        /* pass through */
    }

    if( IDU_ADD_OR_DEL_MEMPOOL == ID_TRUE )
    {
       IDE_TEST( iduMemPoolMgr::addMemPool(this)  !=  IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    {
        if( mFlag & IDU_MEMPOOL_USE_POOLING )
        {
            for (i = 0; i < mListCount; i++)
            {
                if (mMemList[i] != NULL)
                {
                    IDE_ASSERT(mMemList[i]->destroy() == IDE_SUCCESS);

                    IDE_ASSERT(iduMemMgr::free(mMemList[i]) == IDE_SUCCESS);

                    mMemList[i] = NULL;
                }
            }
        }
        else
        {
            /* fall through */
        }
    }

    return IDE_FAILURE;
}

IDE_RC iduMemPool::destroy(idBool aCheck)
{
    UInt    i;

#if defined(ALTIBASE_PRODUCT_XDB)
    if( iduGetShmProcType() == IDU_PROC_TYPE_USER )
    {
        aCheck = ID_FALSE;
    }
#endif

    if( mFlag & IDU_MEMPOOL_USE_POOLING )
    {
        if( IDU_ADD_OR_DEL_MEMPOOL == ID_TRUE )
        {
           IDE_TEST( iduMemPoolMgr::delMemPool(this)  !=  IDE_SUCCESS);
        }

        for (i = 0; i < mListCount; i++)
        {
            IDE_TEST( mMemList[i]->destroy(aCheck)!= IDE_SUCCESS);

            IDE_TEST(iduMemMgr::free(mMemList[i])
                     != IDE_SUCCESS);
        }

        IDE_TEST(iduMemMgr::free(mMemList)
                 != IDE_SUCCESS);
    }
    else
    {
        /* fall through */
    }

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC iduMemPool::cralloc(void **aMem)
{
    SInt i = 0;
    SInt sState = 0;
    
    if( mFlag & IDU_MEMPOOL_USE_POOLING )
    {
        i  = (idlOS::getParallelIndex() % mListCount);

        if( mFlag &  IDU_MEMPOOL_USE_MUTEX )
        {
            IDE_TEST(mMemList[i]->mMutex.lock(NULL /* idvSQL* */) != IDE_SUCCESS);
            sState = 1;
    
            IDE_TEST(mMemList[i]->cralloc(aMem) != IDE_SUCCESS);

            sState = 0;
            IDE_TEST(mMemList[i]->mMutex.unlock() != IDE_SUCCESS);
        }
        else
        {
            IDE_TEST(mMemList[i]->cralloc(aMem) != IDE_SUCCESS);
        }
    }
    else
    {
        IDE_TEST(iduMemMgr::calloc(mIndex, 1, mElemSize, aMem) != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( mFlag & IDU_MEMPOOL_USE_POOLING )
    {
        if(sState != 0)
        {
            (void)mMemList[i]->mMutex.unlock();
        }
    }

    return IDE_FAILURE;
}

IDE_RC iduMemPool::alloc(void **aMem)
{
    SInt i = 0;
    SInt sState = 0;

    if( mFlag & IDU_MEMPOOL_USE_POOLING )
    {
        i  = (idlOS::getParallelIndex() % mListCount);

        if( mFlag &  IDU_MEMPOOL_USE_MUTEX )
        {
            IDE_TEST(mMemList[i]->mMutex.lock(NULL /* idvSQL* */) != IDE_SUCCESS);
            sState = 1;

            IDE_TEST(mMemList[i]->alloc(aMem) != IDE_SUCCESS);

            sState = 0;
            IDE_TEST(mMemList[i]->mMutex.unlock() != IDE_SUCCESS);
        }
        else
        {
            IDE_TEST(mMemList[i]->alloc(aMem) != IDE_SUCCESS);
        }
    }
    else
    {
        IDE_TEST(iduMemMgr::malloc(mIndex, mElemSize, aMem) != IDE_SUCCESS);
    }

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;

    if( mFlag & IDU_MEMPOOL_USE_POOLING )
    {
        if(sState != 0)
        {
            (void)mMemList[i]->mMutex.unlock();
        }
    }

    return IDE_FAILURE;
}

IDE_RC iduMemPool::memfree(void *aMem)
{
    iduMemList  *sCurList  = NULL;
    iduMemChunk *sCurChunk = NULL;
    SInt         sState    = 0;

    if( mFlag & IDU_MEMPOOL_USE_POOLING )
    {
#if defined(ALTIBASE_MEMORY_CHECK)

        sCurList = mMemList[0];

#else

        // Memory Check Option������ �� ������ �����ؾ� ��.
# ifdef MEMORY_ASSERT
        sCurChunk         = *((iduMemChunk **)((UChar *)aMem + mElemSize +
                                           ID_SIZEOF(vULong)));
# else
        sCurChunk         = *((iduMemChunk **)((UChar *)aMem + mElemSize));
# endif

        sCurList = (iduMemList*)sCurChunk->mParent;

#endif

        if( mFlag &  IDU_MEMPOOL_USE_MUTEX )
        {
            IDE_TEST(sCurList->mMutex.lock(NULL) != IDE_SUCCESS);
            sState = 1;

            IDE_TEST(sCurList->memfree(aMem) != IDE_SUCCESS);

            sState = 0;
            IDE_TEST(sCurList->mMutex.unlock() != IDE_SUCCESS);
        }
        else
        {
            IDE_TEST(sCurList->memfree(aMem) != IDE_SUCCESS);
        }
    }
    else
    {
        IDE_TEST(iduMemMgr::free(aMem) != IDE_SUCCESS);
    }

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;

    if( mFlag & IDU_MEMPOOL_USE_POOLING )
    {
        if(sState != 0)
        {
            (void)sCurList->mMutex.unlock();
        }
    }

    return IDE_FAILURE;
}

/*
 * PROJ-2065 Garbage collection 
 *
 * ������� �ʴ� free chunk���� OS�� �ݳ��Ͽ� �޸𸮰����� Ȯ���Ѵ�.
 */
IDE_RC iduMemPool::shrink(UInt *aSize )
{
    UInt         i;
    UInt         sState=0;
    UInt         sSize=0;
    iduMemList  *sCurList=NULL;

    *aSize=0; 

    if ( (mFlag & IDU_MEMPOOL_USE_POOLING) &&
         (mFlag & IDU_MEMPOOL_GARBAGE_COLLECT ) &&
         (iduMemMgr::isServerMemType() == ID_TRUE) )
    {
        for (i = 0; i < mListCount; i++)
        {
            sCurList = mMemList[i];

            IDE_ASSERT( sCurList != NULL ); 

            if( mFlag &  IDU_MEMPOOL_USE_MUTEX )
            {
                IDE_TEST( sCurList->mMutex.lock(NULL) != IDE_SUCCESS );
                sState = 1;
            }

            IDE_TEST( sCurList->shrink( &sSize ) != IDE_SUCCESS );

            *aSize += sSize;

            if( mFlag &  IDU_MEMPOOL_USE_MUTEX )
            {
                sState = 0;
                IDE_TEST( sCurList->mMutex.unlock() != IDE_SUCCESS );
            }
        }
    }

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;

    if(sState != 0)
    {
        (void)sCurList->mMutex.unlock();
    }

    return IDE_FAILURE;
}

void iduMemPool::dumpState(SChar *aBuffer, UInt aBufferSize)
{
    UInt sSize = 0;
    UInt i;

    if( mFlag & IDU_MEMPOOL_USE_POOLING )
    {
        for (i = 0; i < mListCount; i++)
        {
            IDE_ASSERT(mMemList[i]->mMutex.lock(NULL) == IDE_SUCCESS);

            sSize += mMemList[i]->getUsedMemory();

            IDE_ASSERT(mMemList[i]->mMutex.unlock() == IDE_SUCCESS);
        }

        idlOS::snprintf(aBuffer, aBufferSize, "MemPool Used Memory:%"ID_UINT32_FMT"\n",sSize);
    }
}

void iduMemPool::status()
{
    UInt i;
    SChar sBuffer[IDE_BUFFER_SIZE];

    if( mFlag & IDU_MEMPOOL_USE_POOLING )
    { 
        idlOS::snprintf(sBuffer, ID_SIZEOF(sBuffer), "  - Memory List Count:%5"ID_UINT32_FMT"\n",
                    (UInt)mListCount);
        IDE_CALLBACK_SEND_SYM_NOLOG(sBuffer);

        idlOS::snprintf(sBuffer, ID_SIZEOF(sBuffer), "  - Slot   Size:%5"ID_UINT32_FMT"\n",
                        (UInt)( mSlotSize));
        IDE_CALLBACK_SEND_SYM_NOLOG(sBuffer);

        for(i = 0; i < mListCount; i++)
        {
            idlOS::snprintf(sBuffer, ID_SIZEOF(sBuffer), "  - Memory List *** [%"ID_UINT32_FMT"]\n",
                            (UInt)(i + 1));
            IDE_CALLBACK_SEND_SYM_NOLOG(sBuffer);

            mMemList[i]->status();
        }
    }
}

/* added by mskim for check memory statement */
ULong iduMemPool::getMemorySize()
{
    UInt i;
    UInt sMemSize = 0;

    if( mFlag & IDU_MEMPOOL_USE_POOLING )
    {
        for(i = 0; i < mListCount; i++)
        {
            sMemSize += mMemList[i]->getUsedMemory();
        }
    }

    return sMemSize;
}


ULong iduMemPool::roundUp2n( ULong aSize ) 
{
        ULong sS = IDL_CACHE_LINE;

        while ( sS < aSize  ) 
        {
            sS *= 2;
        }
        return sS;
}