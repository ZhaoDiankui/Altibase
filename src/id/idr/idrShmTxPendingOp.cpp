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
#include <idu.h>
#include <idw.h>
#include <idrDef.h>
#include <iduVLogShmMgr.h>
#include <idrVLogUpdate.h>
#include <iduVLogShmMemPool.h>
#include <iduVLogShmMemList.h>
#include <iduVLogShmLatch.h>
#include <idwVLogPMMgr.h>
#include <iduVLogShmList.h>
#include <idrShmTxPendingOp.h>

idrDoPendingOpFunc  gDoPendingOpFunc[IDR_PENDING_OPERATION_MAX];

IDE_RC idrShmTxPendingOp::initialize()
{
    gDoPendingOpFunc[IDR_PENDING_OPERATION_NULL]  = NULL;

    return IDE_SUCCESS;
}

IDE_RC idrShmTxPendingOp::destroy()
{
    return IDE_SUCCESS;
}

IDE_RC idrShmTxPendingOp::registerPendingOpFunc( idrPendingOpType   aPendingOpType,
                                                 idrDoPendingOpFunc aPendingOpFunc)
{
    gDoPendingOpFunc[aPendingOpType] = aPendingOpFunc;

    return IDE_SUCCESS;
}

/***********************************************************************
 * Description : PendingOperation List�� �ʱ�ȭ�Ѵ�.
 *
 * [IN]  aShmTxInfo   - PendingOpList�� �ʱ�ȭ�Ϸ��� Shared Memory Tx Info
 **********************************************************************/
void idrShmTxPendingOp::initializePendingOpList( iduShmTxInfo * aShmTxInfo )
{
    idShmAddr   sAddr4Obj;

    sAddr4Obj = IDU_SHM_GET_ADDR_SUBMEMBER( aShmTxInfo->mSelfAddr,
                                            iduShmTxInfo,
                                            mPendingOpList );

    iduShmList::initBase( &aShmTxInfo->mPendingOpList,
                          sAddr4Obj,
                          IDU_SHM_NULL_ADDR );

    sAddr4Obj = IDU_SHM_GET_ADDR_SUBMEMBER( aShmTxInfo->mSelfAddr,
                                            iduShmTxInfo,
                                            mPendingOpFreeList );

    iduShmList::initBase( &aShmTxInfo->mPendingOpFreeList,
                          sAddr4Obj,
                          IDU_SHM_NULL_ADDR );
}

/***********************************************************************
 * Description : aShmTxInfo�� PendingOperationList���� aPendingOp Node��
 *               �߰��Ѵ�.
 *
 * [IN]  aStatistics    - Statement
 * [IN]  aShmTxInfo     - PendingOperation�� �߰��� Shared Memory Tx Info
 * [IN]  aPendingOpType - Pending Operation�� ������ ���� TypeEnum
 * [IN]  aParam         - Pending Operation ����� ���޵� Parameters
 * [IN]  aParamSize     - aParam�� ������(Byte)
 * [OUT] aPendingOp     - �߰��� PendingOp�� �ּҸ� ��ȯ�Ѵ�.
 **********************************************************************/
IDE_RC idrShmTxPendingOp::addStPendingOp( idvSQL           * aStatistics,
                                          iduShmTxInfo     * aShmTxInfo,
                                          idrPendingOpType   aPendingOpType,
                                          void             * aParam,
                                          UInt               aParamSize,
                                          idrStPendingOp  ** aPendingOp )
{
    idShmAddr        sAddrObj;
    idrStPendingOp * sPendingOp;
    idrSVP           sVSavepoint;

    IDE_ASSERT( aParamSize <= IDR_PENDING_OP_PARAM_SIZE );

    idrLogMgr::setSavepoint( aShmTxInfo, &sVSavepoint );

    IDE_TEST( iduShmMgr::allocMem( aStatistics,
                                   aShmTxInfo,
                                   IDU_MEM_ID_IDU,
                                   ID_SIZEOF(idrStPendingOp),
                                   (void**)&sPendingOp )
              != IDE_SUCCESS );

    sAddrObj = iduShmMgr::getAddr(sPendingOp);

    iduShmList::initNode( &sPendingOp->mNode,
                          sAddrObj,
                          IDU_SHM_NULL_ADDR /* aData */ );

    sPendingOp->mPendingOpType = aPendingOpType;
    sPendingOp->mParamSize     = aParamSize;
    idlOS::memcpy( &sPendingOp->mParam, aParam, aParamSize );

    iduShmList::addLast( aShmTxInfo,
                         &aShmTxInfo->mPendingOpList,
                         &sPendingOp->mNode );

    IDE_TEST( idrLogMgr::commit2Svp( aStatistics, aShmTxInfo, &sVSavepoint )
              != IDE_SUCCESS );

    *aPendingOp = sPendingOp;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_ASSERT( idrLogMgr::abort2Svp( aStatistics, aShmTxInfo, &sVSavepoint )
                == IDE_SUCCESS );

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aShmTxInfo�� PendingOperationList���� aPendingOp Node��
 *               �����Ѵ�.
 *
 * [IN]  aShmTxInfo - Shared Memory Tx Info
 * [IN]  aPendingOp - �����Ϸ��� PendingOperation Node
 **********************************************************************/
IDE_RC idrShmTxPendingOp::removeStPendingOp( iduShmTxInfo    * aShmTxInfo,
                                             idrStPendingOp  * aPendingOp )
{
    iduShmList::remove( aShmTxInfo, &aPendingOp->mNode );

    iduShmList::addLast( aShmTxInfo,
                         &aShmTxInfo->mPendingOpFreeList,
                         &aPendingOp->mNode);

    return IDE_SUCCESS;

}

/***********************************************************************
 * Description : mPendingOpFreeList���� Node�� �����ϰ� free�Ѵ�.
 *
 * [IN] aStatistics - Statement
 * [IN] aShmTxInfo  - Shared Memory Tx Info
 * [IN] aPendingOp  - free�Ϸ��� PendingOperation Node
 **********************************************************************/
IDE_RC idrShmTxPendingOp::freeStPendingOp( idvSQL          * aStatistics,
                                           iduShmTxInfo    * aShmTxInfo,
                                           idrStPendingOp  * aPendingOp )
{
    idrSVP           sVSavepoint;

    idrLogMgr::setSavepoint( aShmTxInfo, &sVSavepoint );

    iduShmList::remove( aShmTxInfo, &aPendingOp->mNode );

    IDE_ASSERT( iduShmMgr::freeMem( aStatistics,
                                    aShmTxInfo,
                                    &sVSavepoint,
                                    aPendingOp )
                == IDE_SUCCESS );

    return IDE_SUCCESS;
}

/***********************************************************************
 * Description : aPendingOp�� �ش��ϴ� PendingOperation �Լ��� �����Ѵ�.
 *
 * [IN] aStatistics - Statement
 * [IN] aShmTxInfo  - Shared Memory Tx Info
 * [IN] aVSavepoint - ����Ϸ�� commit�� VLog�� Savepoint
 * [IN] aPendingOp  - �����ϰ��� �ϴ� PendingOperation Node
 **********************************************************************/
IDE_RC idrShmTxPendingOp::executePendingOp( idvSQL         * aStatistics,
                                            iduShmTxInfo   * aShmTxInfo,
                                            idrSVP         * aVSavepoint,
                                            idrStPendingOp * aPendingOp )
{
    idrSVP   sVSavepoint;

    if( aVSavepoint == NULL )
    {
        idrLogMgr::setSavepoint( aShmTxInfo, &sVSavepoint );
        aVSavepoint = &sVSavepoint;
    }

    IDE_ASSERT( aPendingOp != NULL );

    IDE_TEST( gDoPendingOpFunc[aPendingOp->mPendingOpType]( aStatistics,
                                                            aShmTxInfo,
                                                            aVSavepoint,
                                                            aPendingOp )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_ASSERT( idrLogMgr::abort2Svp( aStatistics, aShmTxInfo, aVSavepoint )
                == IDE_SUCCESS );

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aShmTxInfo�� �Ŵ޸� ��� PendingOperation�� �����Ѵ�.
 *
 * [IN] aStatistics - Statement
 * [IN] aShmTxInfo  - Pending Operation�� ������ Shared Memory Transaction
 **********************************************************************/
IDE_RC idrShmTxPendingOp::executePendingOpAll( idvSQL       * aStatistics,
                                               iduShmTxInfo * aShmTxInfo )
{
    iduShmListNode * sOpNode;
    iduShmListNode * sBaseNode;
    iduShmListNode * sNextNode;
    idrSVP           sVSavepoint;

    sBaseNode = &aShmTxInfo->mPendingOpList;

    for( sOpNode = iduShmList::getFstPtr( sBaseNode );
         sOpNode != sBaseNode;
         sOpNode = sNextNode )
    {
        idrLogMgr::setSavepoint( aShmTxInfo, &sVSavepoint );

        sNextNode = iduShmList::getNxtPtr( sOpNode );

        iduShmList::remove( aShmTxInfo, sOpNode );

        IDE_TEST( executePendingOp( aStatistics,
                                    aShmTxInfo,
                                    &sVSavepoint,
                                    (idrStPendingOp *)sOpNode )
                  != IDE_SUCCESS );

        /* PendingOp�Լ� ������ Commit���� ���� Node�� Undo�� remove�� ���·� �����Ѵ�.
         * �׷��� ���⼭ �װ� �Ǹ� ���ŵ� Node�� ���� Memory Leak�� �߻��� �� �ִ�.  */
        IDE_TEST( iduShmMgr::freeMem( aStatistics,
                                      aShmTxInfo,
                                      NULL,
                                      sOpNode )
                  != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_ASSERT( idrLogMgr::abort2Svp( aStatistics, aShmTxInfo, &sVSavepoint )
            == IDE_SUCCESS );

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : Free List�� �Ŵ޸� PendingOpNode���� free�Ѵ�.
 *
 * [IN] aStatistics - Statement
 * [IN] aShmTxInfo  - �����ϰ��� �ϴ� ShmTxInfo
 **********************************************************************/
IDE_RC idrShmTxPendingOp::freeAllStFreePendingOpList( idvSQL       * aStatistics,
                                                      iduShmTxInfo * aShmTxInfo )
{
    idrSVP           sVSavepoint;
    iduShmListNode * sOpNode;
    iduShmListNode * sBaseNode;
    iduShmListNode * sNextNode;

    idrLogMgr::setSavepoint( aShmTxInfo, &sVSavepoint );

    sBaseNode = &aShmTxInfo->mPendingOpFreeList;

    for( sOpNode = iduShmList::getFstPtr( sBaseNode );
         sOpNode != sBaseNode;
         sOpNode = sNextNode )
    {
        sNextNode = iduShmList::getNxtPtr( sOpNode );

        iduShmList::remove( aShmTxInfo, sOpNode );

        IDE_TEST( iduShmMgr::freeMem( aStatistics,
                                      aShmTxInfo,
                                      &sVSavepoint,
                                      sOpNode )
                  != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_ASSERT( idrLogMgr::abort2Svp( aStatistics, aShmTxInfo, &sVSavepoint )
                == IDE_SUCCESS );

    return IDE_FAILURE;
}
