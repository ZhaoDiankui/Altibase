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

#include <uln.h>
#include <ulnPrivate.h>
#include <ulnFailOver.h>
#include <ulnCommunication.h>

/* PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning */
#include <ulnFetchOpAsync.h>
#include <ulnSemiAsyncPrefetch.h>

/*
 * ======================================
 * Basic cmi Wrapper functions
 * ======================================
 */

typedef struct ulnCmProtocolEntry
{
    acp_sint32_t      mNeedResult;
    const acp_char_t *mProtocolName;
} ulnCmProtocolEntry;

static ulnCmProtocolEntry gUlnCmProtocolTable[CMP_OP_DB_MAX + 1] =
{
    /*  0 */ { 0, "CMP_OP_DB_Message" },
    /*  1 */ { 0, "CMP_OP_DB_ErrorResult" },
    /*  2 */ {-1, "CMP_OP_DB_ErrorInfo" },
    /*  3 */ {-1, "CMP_OP_DB_ErrorInfoResult" },
    /*  4 */ {-1, "CMP_OP_DB_Connect" }, /* replaced with CMP_OP_DB_ConnectEx */
    /*  5 */ {-1, "CMP_OP_DB_ConnectResult" }, /* replaced with CMP_OP_DB_ConnectExResult */
    /*  6 */ { 1, "CMP_OP_DB_Disconnect" },
    /*  7 */ { 0, "CMP_OP_DB_DisconnectResult" },
    /*  8 */ { 1, "CMP_OP_DB_PropertyGet" },
    /*  9 */ { 0, "CMP_OP_DB_PropertyGetResult" },
    /* 10 */ {-1, "CMP_OP_DB_PropertySet" },   /* replaced with PropertySetV2 */
    /* 11 */ { 0, "CMP_OP_DB_PropertySetResult" },
    /* 12 */ { 1, "CMP_OP_DB_Prepare" },
    /* 13 */ { 0, "CMP_OP_DB_PrepareResult" },
    /* 14 */ { 1, "CMP_OP_DB_PlanGet" },
    /* 15 */ { 0, "CMP_OP_DB_PlanGetResult" },
    /* 16 */ { 1, "CMP_OP_DB_ColumnInfoGet" },
    /* 17 */ { 0, "CMP_OP_DB_ColumnInfoGetResult" },
    /* 18 */ { 0, "CMP_OP_DB_ColumnInfoGetListResult" },
    /* 19 */ {-1, "CMP_OP_DB_ColumnInfoSet" },
    /* 20 */ {-1, "CMP_OP_DB_ColumnInfoSetResult" },
    /* 21 */ { 1, "CMP_OP_DB_ParamInfoGet" },
    /* 22 */ { 0, "CMP_OP_DB_ParamInfoGetResult" },
    /* 23 */ {-1, "CMP_OP_DB_ParamInfoSet" },
    /* 24 */ {-1, "CMP_OP_DB_ParamInfoSetResult" },
    /* 25 */ { 1, "CMP_OP_DB_ParamInfoSetList" },
    /* 26 */ { 0, "CMP_OP_DB_ParamInfoSetListResult" },
    /* 27 */ { 0, "CMP_OP_DB_ParamDataIn" },
    /* 28 */ {-1, "CMP_OP_DB_ParamDataOut" },
    /* 29 */ { 0, "CMP_OP_DB_ParamDataOutList" },
    /* 30 */ {-1, "CMP_OP_DB_ParamDataInList" },       /* replaced with ParamDataInListV2 */
    /* 31 */ {-1, "CMP_OP_DB_ParamDataInListResult" }, /* replaced with ParamDataInListV2Result */
    /* 32 */ {-1, "CMP_OP_DB_Execute" },               /* replaced with ExecuteV2 */
    /* 33 */ {-1, "CMP_OP_DB_ExecuteResult" },         /* replaced with ExecuteV2Result */
    /* 34 */ { 1, "CMP_OP_DB_FetchMove" },
    /* 35 */ { 0, "CMP_OP_DB_FetchMoveResult" },
    /* 36 */ { 1, "CMP_OP_DB_Fetch" },   /* replaced with FetchV2 */
    /* 37 */ { 0, "CMP_OP_DB_FetchBeginResult" },
    /* 38 */ { 0, "CMP_OP_DB_FetchResult" },
    /* 39 */ {-1, "CMP_OP_DB_FetchListResult" },
    /* 40 */ { 0, "CMP_OP_DB_FetchEndResult" },
    /* 41 */ { 1, "CMP_OP_DB_Free" },
    /* 42 */ { 0, "CMP_OP_DB_FreeResult" },
    /* 43 */ { 1, "CMP_OP_DB_Cancel" },
    /* 44 */ { 0, "CMP_OP_DB_CancelResult" },
    /* 45 */ { 1, "CMP_OP_DB_Transaction" },
    /* 46 */ { 0, "CMP_OP_DB_TransactionResult" },
    /* 47 */ { 1, "CMP_OP_DB_LobGetSize" },
    /* 48 */ { 0, "CMP_OP_DB_LobGetSizeResult" },
    /* 49 */ { 1, "CMP_OP_DB_LobGet" },
    /* 50 */ { 0, "CMP_OP_DB_LobGetResult" },
    /* 51 */ { 1, "CMP_OP_DB_LobPutBegin" },
    /* 52 */ { 0, "CMP_OP_DB_LobPutBeginResult" },
    /* 53 */ { 0, "CMP_OP_DB_LobPut" },
    /* 54 */ { 1, "CMP_OP_DB_LobPutEnd" },
    /* 55 */ { 0, "CMP_OP_DB_LobPutEndResult" },
    /* 56 */ { 0, "CMP_OP_DB_LobFree" },               /* bug-19138 */
    /* 57 */ { 0, "CMP_OP_DB_LobFreeResult" },
    /* 58 */ { 0, "CMP_OP_DB_LobFreeAll" },            /* PROJ-1752 */
    /* 59 */ { 0, "CMP_OP_DB_LobFreeAllResult" },
             /* PROJ-1573 XA */
    /* 60 */ { 1, "CMP_OP_DB_XaOperation" },
    /* 61 */ { 1, "CMP_OP_DB_XaXid" },
    /* 62 */ { 0, "CMP_OP_DB_XaResult" },
    /* 63 */ { 1, "CMP_OP_DB_XaTransaction" },
    /* 64 */ { 0, "CMP_OP_DB_LobGetBytePosCharLen"  },
    /* 65 */ { 0, "CMP_OP_DB_LobGetBytePosCharLenResult" },
    /* 66 */ { 0, "CMP_OP_DB_LobGetCharPosCharLen"  },
    /* 67 */ { 0, "CMP_OP_DB_LobBytePos"            },
    /* 68 */ { 0, "CMP_OP_DB_LobBytePosResult"      },
    /* 69 */ { 0, "CMP_OP_DB_LobCharLength"         },
    /* 70 */ { 0, "CMP_OP_DB_LobCharLengthResult"   },
    /* 71 */ { 0, "CMP_OP_DB_ParamDataInResult" }, // bug-28259 for ipc
    /* 72 */ { 0, "CMP_OP_DB_Handshake" },         // proj_2160 cm_type
    /* 73 */ { 0, "CMP_OP_DB_HandshakeResult" },
             /* PROJ-2177 User Interface - Cancel */
    /* 74 */ { 1, "CMP_OP_DB_PrepareByCID" },
    /* 75 */ { 1, "CMP_OP_DB_CancelByCID" },
             /* PROJ-2047 Strengthening LOB - Added Interfaces */
    /* 76 */ { 1, "CMP_OP_DB_LobTrim" },
    /* 77 */ { 0, "CMP_OP_DB_LobTrimResult" },
             /* BUG-38496 */
    /* 78 */ { 1, "CMP_OP_DB_ConnectEx" },
    /* 79 */ { 0, "CMP_OP_DB_ConnectExResult" },
             /* BUG-39463 Add new fetch protocol that can request over 65535 rows. */
    /* 80 */ { 1, "CMP_OP_DB_FetchV2" },
             /* BUG-41793 Keep a compatibility among tags */
    /* 81 */ { 1, "CMP_OP_DB_PropertySetV2" },
             /* PROJ-2616 */
    /* 82 */ { 0, "CMP_OP_DB_IPCDALastOpEnded" },
             /* BUG-44572 */
    /* 83 */ { 1, "CMP_OP_DB_ExecuteV2" },
    /* 84 */ { 0, "CMP_OP_DB_ExecuteV2Result" },
    /* 85 */ { 1, "CMP_OP_DB_ParamDataInListV2" },
    /* 86 */ { 0, "CMP_OP_DB_ParamDataInListV2Result" },
             /* PROJ-2598 altibase sharding */
    /* 87 */ { 1, "CMP_OP_DB_ShardPrepare" },
    /* 88 */ { 0, "CMP_OP_DB_ShardPrepareResult" },
    /* 89 */ { 1, "CMP_OP_DB_ShardNodeUpdateList" },
    /* 90 */ { 0, "CMP_OP_DB_ShardNodeUpdateListResult" },
    /* 91 */ { 1, "CMP_OP_DB_ShardNodeGetList" },
    /* 92 */ { 0, "CMP_OP_DB_ShardNodeGetListResult" },
    /* 93 */ { 0, "CMP_OP_DB_ShardHandshake" },
    /* 94 */ { 0, "CMP_OP_DB_ShardHandshakeResult" },
    /* -- */ {-1, "CMP_OP_DB_MAX" }
};

ACI_RC ulnWriteProtocol(ulnFnContext *aFnContext, ulnPtContext *aPtContext, cmiProtocol *aPacket)
{
    ulnDbc       *sDbc;
    ulnErrorMgr   sErrorMgr;

    cmiProtocolContext *sCtx = &aPtContext->mCmiPtContext;

    ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);
    ACI_TEST_RAISE(sDbc == NULL, LABEL_MEM_MAN_ERR);

    ACI_TEST_RAISE(ulnDbcIsConnected(sDbc) != ACP_TRUE, LABEL_CON_CLOSED);

    if (cmiGetLinkImpl(sCtx) == CMI_LINK_IMPL_IPCDA)
    {
        aPtContext->mNeedReadProtocol += 1;
        /* Increase protocol-data-count in cmnBlockIPCDA. */
        ((cmbBlockIPCDA*)sCtx->mWriteBlock)->mOperationCount++;
    }
    else
    {
        aPtContext->mNeedFlush = ACP_TRUE;

        ACE_ASSERT(aPacket->mOpID < CMP_OP_DB_MAX);
        ACE_DASSERT(gUlnCmProtocolTable[aPacket->mOpID].mNeedResult >= 0);
        aPtContext->mNeedReadProtocol += gUlnCmProtocolTable[aPacket->mOpID].mNeedResult;

        // bug-28259: ipc needs paramDataInResult
        // paramDataIn ��û�� ���� ipc�� ��쿡�� �������� ������ ��.
        if((sDbc->mConnType == ULN_CONNTYPE_IPC) &&
           (aPacket->mOpID == CMP_OP_DB_ParamDataIn))
        {
            aPtContext->mNeedReadProtocol += 1;
        }
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_CON_CLOSED)
    {
        ulnErrorMgrSetUlError( &sErrorMgr,
                               ulERR_ABORT_COMMUNICATION_LINK_FAILURE,
                               "Server closed the connection." );

        //PROJ-1645 UL FailOver.
        if (ulnFailoverDoSTF(aFnContext) == ACI_FAILURE)
        {
            ulnError(aFnContext,
                 ulERR_ABORT_COMMUNICATION_LINK_FAILURE,
                 "Server closed the connection.");
        }
        else
        {
            ulnError( aFnContext, ulERR_ABORT_FAILOVER_SUCCESS,
                      ulnErrorMgrGetErrorCode(&sErrorMgr),
                      ulnErrorMgrGetSQLSTATE(&sErrorMgr),
                      ulnErrorMgrGetErrorMessage(&sErrorMgr) );
        }
    }

    ACI_EXCEPTION(LABEL_MEM_MAN_ERR)
    {
        ulnError(aFnContext,
                 ulERR_FATAL_MEMORY_MANAGEMENT_ERROR,
                 "ulnWriteProtocol : object type is neither DBC nor STMT.");
    }
    ACI_EXCEPTION_END;

    aPtContext->mNeedReadProtocol = 0;
    aPtContext->mNeedFlush        = ACP_FALSE;

    return ACI_FAILURE;
}

/**
 * ulnReadProtocolIPCDA.
 *
 * @param[in] aFnContext
 * @param[in] aPtContext
 * @param[in] aTimeout
 *
 * �־��� protocol context �� ���ŵ� ��Ŷ�� �а�, �ش��ϴ� �ݹ��� ȣ���Ѵ�.
 */
ACI_RC ulnReadProtocolIPCDA(ulnFnContext   *aFnContext,
                            ulnPtContext   *aPtContext,
                            acp_time_t      aTimeout)
{
    ulnDbc       *sDbc;
    ulnErrorMgr   sErrorMgr;

    ACP_UNUSED(aTimeout);
    
    ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);

    ACI_TEST_RAISE(sDbc == NULL, LABEL_MEM_MAN_ERR);
    /*
     * ���� �ȵǾ� ������ ������ ����
     */
    ACI_TEST_RAISE(ulnDbcIsConnected(sDbc) != ACP_TRUE, LABEL_CON_CLOSED);

    /*
     * ReadProtocol �� �ʿ� ��� �׳� ������ ����
     */
    ACI_TEST_RAISE( cmiRecvIPCDA( &(aPtContext->mCmiPtContext),
                                  aFnContext,
                                  sDbc->mIPCDAULExpireCount,
                                  sDbc->mIPCDAMicroSleepTime )
                    != ACI_SUCCESS, LABEL_CM_ERR);

    aPtContext->mNeedReadProtocol = 0;

    /*
     * Note : �������� ������ ���۵Ǿ� �� ���, ErrorResult ��Ŷ�� ���ŵȴ�.
     *        �׷�����, cmiReadProtocolAndCallback() �Լ��� ACI_SUCCESS �� �����Ѵ�.
     *        �׷���, ���� Function Context �� mSqlReturn ���� üũ�ؼ� �����κ�����
     *        ���� ���� ���θ� �Ǵ��ؾ� �Ѵ�.
     */
    ACI_TEST(SQL_SUCCEEDED(ULN_FNCONTEXT_GET_RC(aFnContext)) == 0);

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_CON_CLOSED)
    {
        ulnErrorMgrSetUlError( &sErrorMgr,
                               ulERR_ABORT_COMMUNICATION_LINK_FAILURE,
                               "Server closed the connection." );
    }

    ACI_EXCEPTION(LABEL_MEM_MAN_ERR)
    {
        ulnError(aFnContext,
                 ulERR_FATAL_MEMORY_MANAGEMENT_ERROR,
                 "ulnReadProtocol : object type is neither DBC nor STMT.");
    }

    ACI_EXCEPTION(LABEL_CM_ERR)
    {
        ulnErrorMgrSetCmError( sDbc, &sErrorMgr, aciGetErrorCode() );
        /*
         * cm �� MARSHALL_ERROR ���� �߻����� ��� ���⼭ ������ ���ϵǰ�,
         * ���������� read �� ulnFetch() ���� �Լ��� ACI_EXCEPTION() ��ƾ����
         * �ٽ��ѹ� flush / read �� �ϰ� �Ǵµ�, �̶� �ٽ��ѹ� read �� �ϰ� �ǹǷ�
         * ���Ѵ�⸦ �ϰ� �ȴ�.
         *
         * �̸� �����ϱ� ���ؼ� mNeedReadProtocol �� ACP_FALSE �� ����� �ش�.
         */
        aPtContext->mNeedReadProtocol = 0;
    }

    ACI_EXCEPTION_END;
    
    return ACI_FAILURE;
}

/*
 * BUGBUG : read �� flush �� static ���� ���� �־�� �Ѵ�.
 *          �ϴ��� ��� ��ġ�� �������� �״�� ����.
 */

/**
 * ulnReadProtocol.
 *
 * @param[in] aFnContext
 * @param[in] aPtContext
 * @param[in] aTimeout
 *
 * �־��� protocol context �� ���ŵ� ��Ŷ�� �а�, �ش��ϴ� �ݹ��� ȣ���Ѵ�.
 * ����, semi-async prefetch �� ���� �񵿱������� fetch protocol �� ��û�Ͽ��ٸ�,
 * fetch result �� ���� read �Ѵ�.
 */
ACI_RC ulnReadProtocol(ulnFnContext   *aFnContext,
                       ulnPtContext   *aPtContext,
                       acp_time_t      aTimeout)
{
    ulnDbc       *sDbc;
    ulnStmt      *sAsyncPrefetchStmt;
    ulnErrorMgr   sErrorMgr;

    ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);
    ACI_TEST_RAISE(sDbc == NULL, LABEL_MEM_MAN_ERR);

    /*
     * ���� �ȵǾ� ������ ������ ����
     */
    ACI_TEST_RAISE(ulnDbcIsConnected(sDbc) != ACP_TRUE, LABEL_CON_CLOSED);

    /*
     * ReadProtocol �� �ʿ� ��� �׳� ������ ����
     */

    if(aPtContext->mNeedReadProtocol > 0)
    {
        /*
         * �ݹ� ȣ��
         */

        /**
         * PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning
         * Should read CMP_OP_DB_FetchResult before, if asynchronous CMP_OP_DB_FetchV2 was sent
         */

        sAsyncPrefetchStmt = ulnDbcGetAsyncPrefetchStmt(sDbc);
        if (sAsyncPrefetchStmt != NULL)
        {
            ACI_TEST(ulnFetchReceiveFetchResultForSync(aFnContext,
                                                       aPtContext,
                                                       sAsyncPrefetchStmt)
                    != ACI_SUCCESS);
        }
        else
        {
            /* nothing to do */
        }

        ACI_TEST_RAISE( cmiRecv( &(aPtContext->mCmiPtContext),
                                 aFnContext,
                                 aTimeout)
                        != ACI_SUCCESS, LABEL_CM_ERR);

        aPtContext->mNeedReadProtocol = 0;

        /*
         * Note : �������� ������ ���۵Ǿ� �� ���, ErrorResult ��Ŷ�� ���ŵȴ�.
         *        �׷�����, cmiReadProtocolAndCallback() �Լ��� ACI_SUCCESS �� �����Ѵ�.
         *        �׷���, ���� Function Context �� mSqlReturn ���� üũ�ؼ� �����κ�����
         *        ���� ���� ���θ� �Ǵ��ؾ� �Ѵ�.
         */
        ACI_TEST(SQL_SUCCEEDED(ULN_FNCONTEXT_GET_RC(aFnContext)) == 0);
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_CON_CLOSED)
    {
        ulnErrorMgrSetUlError( &sErrorMgr,
                               ulERR_ABORT_COMMUNICATION_LINK_FAILURE,
                               "Server closed the connection." );

        //PROJ-1645 UL FailOver.
        if (ulnFailoverDoSTF(aFnContext) == ACI_FAILURE)
        {
            ulnError(aFnContext,
                 ulERR_ABORT_COMMUNICATION_LINK_FAILURE,
                 "Server closed the connection.");
        }
        else
        {
            ulnError( aFnContext, ulERR_ABORT_FAILOVER_SUCCESS,
                      ulnErrorMgrGetErrorCode(&sErrorMgr),
                      ulnErrorMgrGetSQLSTATE(&sErrorMgr),
                      ulnErrorMgrGetErrorMessage(&sErrorMgr) );
        }
    }

    ACI_EXCEPTION(LABEL_MEM_MAN_ERR)
    {
        ulnError(aFnContext,
                 ulERR_FATAL_MEMORY_MANAGEMENT_ERROR,
                 "ulnReadProtocol : object type is neither DBC nor STMT.");
    }

    ACI_EXCEPTION(LABEL_CM_ERR)
    {
        ulnErrorMgrSetCmError( sDbc, &sErrorMgr, aciGetErrorCode() );

#ifdef COMPILE_SHARDCLI
        ulsdModuleOnCmError(aFnContext, sDbc, &sErrorMgr);
#else
        //PROJ-1645 UL FailOver.
        if (ulnFailoverDoSTF(aFnContext) == ACI_FAILURE)
        {
            ulnErrHandleCmError(aFnContext, NULL);
        }
        else
        {
            ulnError( aFnContext, ulERR_ABORT_FAILOVER_SUCCESS,
                      ulnErrorMgrGetErrorCode(&sErrorMgr),
                      ulnErrorMgrGetSQLSTATE(&sErrorMgr),
                      ulnErrorMgrGetErrorMessage(&sErrorMgr) );
        }
#endif /* COMPILE_SHARDCLI */

        /*
         * cm �� MARSHALL_ERROR ���� �߻����� ��� ���⼭ ������ ���ϵǰ�,
         * ���������� read �� ulnFetch() ���� �Լ��� ACI_EXCEPTION() ��ƾ����
         * �ٽ��ѹ� flush / read �� �ϰ� �Ǵµ�, �̶� �ٽ��ѹ� read �� �ϰ� �ǹǷ�
         * ���Ѵ�⸦ �ϰ� �ȴ�.
         *
         * �̸� �����ϱ� ���ؼ� mNeedReadProtocol �� ACP_FALSE �� ����� �ش�.
         */

        aPtContext->mNeedReadProtocol = 0;
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/**
 * ulnReadProtocolAsync
 *
 * @param[in] aFnContext
 * @param[in] aPtContext
 * @param[in] aTimeout
 *
 * PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning
 * �־��� protocol context �� ���ŵ� ��Ŷ�� �а�, �ش��ϴ� �ݹ��� ȣ���Ѵ�.
 */
ACI_RC ulnReadProtocolAsync(ulnFnContext   *aFnContext,
                            ulnPtContext   *aPtContext,
                            acp_time_t      aTimeout)
{
    ulnDbc       *sDbc = NULL;
    ulnErrorMgr   sErrorMgr;

    ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);
    ACI_TEST_RAISE(sDbc == NULL, LABEL_MEM_MAN_ERR);

    /*
     * ���� �ȵǾ� ������ ������ ����
     */
    ACI_TEST_RAISE(ulnDbcIsConnected(sDbc) != ACP_TRUE, LABEL_CON_CLOSED);

    /*
     * ReadProtocol �� �ʿ� ��� �׳� ������ ����
     */

    /*
     * �ݹ� ȣ��
     */

    ACI_TEST_RAISE( cmiRecv( &(aPtContext->mCmiPtContext),
                             aFnContext,
                             aTimeout)
                    != ACI_SUCCESS, LABEL_CM_ERR);

    /*
     * Note : �������� ������ ���۵Ǿ� �� ���, ErrorResult ��Ŷ�� ���ŵȴ�.
     *        �׷�����, cmiReadProtocolAndCallback() �Լ��� ACI_SUCCESS �� �����Ѵ�.
     *        �׷���, ���� Function Context �� mSqlReturn ���� üũ�ؼ� �����κ�����
     *        ���� ���� ���θ� �Ǵ��ؾ� �Ѵ�.
     */
    ACI_TEST(SQL_SUCCEEDED(ULN_FNCONTEXT_GET_RC(aFnContext)) == 0);

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_CON_CLOSED)
    {
        ulnErrorMgrSetUlError( &sErrorMgr,
                               ulERR_ABORT_COMMUNICATION_LINK_FAILURE,
                               "Server closed the connection." );

        //PROJ-1645 UL FailOver.
        if (ulnFailoverDoSTF(aFnContext) == ACI_FAILURE)
        {
            ulnError(aFnContext,
                 ulERR_ABORT_COMMUNICATION_LINK_FAILURE,
                 "Server closed the connection.");
        }
        else
        {
            ulnError( aFnContext, ulERR_ABORT_FAILOVER_SUCCESS,
                      ulnErrorMgrGetErrorCode(&sErrorMgr),
                      ulnErrorMgrGetSQLSTATE(&sErrorMgr),
                      ulnErrorMgrGetErrorMessage(&sErrorMgr) );
        }
    }

    ACI_EXCEPTION(LABEL_MEM_MAN_ERR)
    {
        ulnError(aFnContext,
                 ulERR_FATAL_MEMORY_MANAGEMENT_ERROR,
                 "ulnReadProtocol : object type is neither DBC nor STMT.");
    }

    ACI_EXCEPTION(LABEL_CM_ERR)
    {
        ulnErrorMgrSetCmError( sDbc, &sErrorMgr, aciGetErrorCode() );

        //PROJ-1645 UL FailOver.
        if (ulnFailoverDoSTF(aFnContext) == ACI_FAILURE)
        {
            ulnErrHandleCmError(aFnContext, NULL);
        }
        else
        {
            ulnError( aFnContext, ulERR_ABORT_FAILOVER_SUCCESS,
                      ulnErrorMgrGetErrorCode(&sErrorMgr),
                      ulnErrorMgrGetSQLSTATE(&sErrorMgr),
                      ulnErrorMgrGetErrorMessage(&sErrorMgr) );
        }
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/**
 * ulnFlushProtocol.
 *
 * @param[in] aFnContext
 * @param[in] aPtContext
 *
 * �־��� protocol context �� ���� ��Ŷ�� �����Ѵ�.
 */
ACI_RC ulnFlushProtocol(ulnFnContext *aFnContext, ulnPtContext *aPtContext)
{
    //fix PROJ-1749
    ulnDbc             *sDbc        = NULL;
    cmiProtocolContext *sCtx        = &aPtContext->mCmiPtContext; /* PROJ-2616 */
    cmbBlockIPCDA      *sWriteBlock = (cmbBlockIPCDA *)sCtx->mWriteBlock; /* PROJ-2616 */
    ulnErrorMgr         sErrorMgr;

    ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);
    ACI_TEST_RAISE(sDbc == NULL, LABEL_MEM_MAN_ERR);

    if (cmiGetLinkImpl(sCtx) == CMI_LINK_IMPL_IPCDA)
    {
        /* PROJ-2616 */
        sWriteBlock->mBlock.mDataSize = sWriteBlock->mBlock.mCursor;
        sWriteBlock->mBlock.mBlockSize = CMB_BLOCK_DEFAULT_SIZE - sizeof(cmbBlockIPCDA);
        acpMemBarrier();
        /* Finalize to write data block. */
        (void)cmiFinalizeSendBufferForIPCDA(sCtx);
    }
    else
    {
        ACI_TEST_RAISE(cmiSend(&(aPtContext->mCmiPtContext), ACP_TRUE) != ACI_SUCCESS, LABEL_CM_ERR);
    }

    aPtContext->mNeedFlush = ACP_FALSE;

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_CM_ERR)
    {
#ifdef COMPILE_SHARDCLI
        ulsdModuleOnCmError(aFnContext, sDbc, &sErrorMgr);
#else
        ulnErrorMgrSetCmError( sDbc, &sErrorMgr, aciGetErrorCode() );

        //PROJ-1645 UL FailOver.
        if (ulnFailoverDoSTF(aFnContext) == ACI_FAILURE)
        {
            ulnErrHandleCmError(aFnContext, aPtContext);
        }
        else
        {
            ulnError( aFnContext, ulERR_ABORT_FAILOVER_SUCCESS,
                      ulnErrorMgrGetErrorCode(&sErrorMgr),
                      ulnErrorMgrGetSQLSTATE(&sErrorMgr),
                      ulnErrorMgrGetErrorMessage(&sErrorMgr) );
        }
#endif /* COMPILE_SHARDCLI */
    }
    ACI_EXCEPTION(LABEL_MEM_MAN_ERR)
    {
        ulnError(aFnContext,
                 ulERR_FATAL_MEMORY_MANAGEMENT_ERROR,
                 "ulnFlushProtol : object type is neither DBC nor STMT.");
    }


    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulnFlushAndReadProtocol(ulnFnContext   *aFnContext,
                               ulnPtContext   *aPtContext,
                               acp_time_t      aTimeout)
{
    if (cmiGetLinkImpl(&(aPtContext->mCmiPtContext)) == CMI_LINK_IMPL_IPCDA)
    {
        (void)cmiFinalizeSendBufferForIPCDA(&aPtContext->mCmiPtContext);
        ACI_TEST(ulnReadProtocolIPCDA(aFnContext,
                                      aPtContext, 
                                      aTimeout) 
                 != ACI_SUCCESS);
    }
    else
    {
        ACI_TEST(ulnFlushProtocol(aFnContext, aPtContext) != ACI_SUCCESS);

        if(aPtContext->mNeedReadProtocol > 0)
        {
            ACI_TEST(ulnReadProtocol(aFnContext, aPtContext, aTimeout) != ACI_SUCCESS);
        }
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulnInitializeProtocolContext(ulnFnContext *aFnContext,
                                    ulnPtContext *aPtContext,
                                    cmiSession   *aSession)
{
    ACP_UNUSED(aFnContext);
    ACP_UNUSED(aSession);

    aPtContext->mNeedReadProtocol = 0;
    aPtContext->mNeedFlush        = ACP_FALSE;


    return ACI_SUCCESS;
}

/*
 * BUG-44942 SQLPutData(ParamDataIn)�� ��ź���� ���̱� ���� �ѹ��� ����
 *           SQLParamData()�� ���ϰ��� SQL_NEED_DATA�� ��� Flush�� ���� �ʱ� ����
 *           ulnFinalizeProtocolContext()�� ȣ������ �ʴ´�. (ulnParamData.c ����)
 */
ACI_RC ulnFinalizeProtocolContext(ulnFnContext *aFnContext, ulnPtContext *aPtContext)
{
    ulnDbc *sDbc;

    ACI_TEST(aFnContext->mHandle.mObj == NULL);      //BUG-28184 : [CodeSonar] Null Pointer Dereference

    ACI_TEST_RAISE(cmiGetLinkImpl(&(aPtContext->mCmiPtContext)) == CMI_LINK_IMPL_IPCDA,
                   ContFinalizeCTX);

    ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);
    ACI_TEST_RAISE(sDbc == NULL, LABEL_MEM_MAN_ERR);

    if(aPtContext->mNeedFlush == ACP_TRUE)
    {
        ACI_TEST_RAISE(ulnFlushProtocol(aFnContext, aPtContext) != ACI_SUCCESS,
                       LABEL_PT_CTXT_FIN);
    }

    if(aPtContext->mNeedReadProtocol > 0)
    {
        /*
         * Note : ReadProtocol ���� ������ ���� finalize �ؾ� �Ѵ�.
         *        �� ������
         *          1. cm error
         *          2. sql error
         *        �� �ΰ��� ��찡 �ִµ� ����Ǿ���, finalize �ؾ� �Ѵ�.
         */
        if(ulnDbcIsConnected(sDbc) == ACP_TRUE)
        {
            ACI_TEST_RAISE(ulnReadProtocol(aFnContext,
                                           aPtContext,
                                           sDbc->mConnTimeoutValue) != ACI_SUCCESS,
                           LABEL_PT_CTXT_FIN);
        }
    }

    ACI_EXCEPTION_CONT(ContFinalizeCTX);

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_PT_CTXT_FIN)
    {
        /*
        if(cmiFinalizeProtocolContext(&(aPtContext->mCmiPtContext)) != ACI_SUCCESS)
        {
            ACI_RAISE(LABEL_CM_ERR);
        }
        */
    }
    ACI_EXCEPTION(LABEL_MEM_MAN_ERR)
    {
        ulnError(aFnContext,
                 ulERR_FATAL_MEMORY_MANAGEMENT_ERROR,
                 "ulnFinalizeProtocolContext : object type is neither DBC nor STMT.");
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*
 * ======================================
 * Protocol Write Functions
 * ======================================
 */

ACI_RC ulnWritePrepareREQ(ulnFnContext  *aFnContext,
                          ulnPtContext  *aProtocolContext,
                          acp_char_t    *aString,
                          acp_sint32_t   aLength,
                          acp_uint8_t    aExecMode)
{
    cmiProtocol         sPacket;
    cmiProtocolContext *sCtx            = &(aProtocolContext->mCmiPtContext);
    ulnStmt            *sStmt           = aFnContext->mHandle.mStmt;
    acp_uint8_t        *sRow            = (acp_uint8_t*)aString;
    acp_uint8_t         sPrepareMode    = ulnStmtGetPrepareMode(sStmt, aExecMode);
    acp_uint16_t        sOrgWriteCursor = CMI_GET_CURSOR(sCtx);
    acp_uint8_t         sState          = 0;

    sPacket.mOpID = CMP_OP_DB_Prepare;

    sStmt->mIsSimpleQuery = ACP_FALSE; /*PROJ-2616*/
   
    /* BUG-44125 [mm-cli] IPCDA ��� �׽�Ʈ �� hang - iloader CLOB */
    CMI_WRITE_CHECK_WITH_IPCDA(sCtx, 10, 10 + aLength);
    sState = 1;

    CMI_WOP(sCtx, CMP_OP_DB_Prepare);
    CMI_WR4(sCtx, &(sStmt->mStatementID));
    CMI_WR1(sCtx, sPrepareMode);
    CMI_WR4(sCtx, (acp_uint32_t*)&aLength);

    ACI_TEST( cmiSplitWrite( sCtx,
                             aLength,
                             sRow )
              != ACI_SUCCESS );

    ACI_TEST(ulnWriteProtocol(aFnContext, aProtocolContext, &sPacket) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    if( (sState == 0) && (cmiGetLinkImpl(sCtx) == CMN_LINK_IMPL_IPCDA) )
    {
        ulnError(aFnContext, ulERR_ABORT_IPCDA_MESSAGE_TOO_LONG, CMB_BLOCK_DEFAULT_SIZE);
    }

    CMI_SET_CURSOR(sCtx, sOrgWriteCursor);

    return ACI_FAILURE;
}

ACI_RC ulnWriteColumnInfoGetREQ(ulnFnContext  *aFnContext,
                                ulnPtContext  *aPtContext,
                                acp_uint16_t   aColumnNumber)
{
    cmiProtocol                sPacket;
    cmiProtocolContext*        sCtx            = &(aPtContext->mCmiPtContext);
    acp_uint16_t               sOrgWriteCursor = CMI_GET_CURSOR(sCtx);
    acp_uint8_t                sState          = 0;

    sPacket.mOpID = CMP_OP_DB_ColumnInfoGet;

    CMI_WRITE_CHECK(sCtx, 9);
    sState = 1;

    CMI_WOP(sCtx, CMP_OP_DB_ColumnInfoGet);
    CMI_WR4(sCtx, &aFnContext->mHandle.mStmt->mStatementID);
    CMI_WR2(sCtx, &aFnContext->mHandle.mStmt->mCurrentResultSetID);
    CMI_WR2(sCtx, &aColumnNumber);

    ACI_TEST(ulnWriteProtocol(aFnContext, aPtContext, &sPacket) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    if( (sState == 0) && (cmiGetLinkImpl(sCtx) == CMN_LINK_IMPL_IPCDA) )
    {
        ulnError(aFnContext, ulERR_ABORT_IPCDA_MESSAGE_TOO_LONG, CMB_BLOCK_DEFAULT_SIZE);
    }

    CMI_SET_CURSOR(sCtx, sOrgWriteCursor);

    return ACI_FAILURE;
}

ACI_RC ulnWriteParamInfoGetREQ(ulnFnContext *aFnContext,
                               ulnPtContext *aPtContext,
                               acp_uint16_t  aParamNumber)
{
    cmiProtocol         sPacket;
    cmiProtocolContext *sCtx            = &(aPtContext->mCmiPtContext);
    acp_uint16_t        sOrgWriteCursor = CMI_GET_CURSOR(sCtx);
    acp_uint8_t         sState          = 0;

    sPacket.mOpID = CMP_OP_DB_ParamInfoGet;

    CMI_WRITE_CHECK(sCtx, 7);
    sState = 1;

    CMI_WOP(sCtx, CMP_OP_DB_ParamInfoGet);
    CMI_WR4(sCtx, &(aFnContext->mHandle.mStmt->mStatementID));
    CMI_WR2(sCtx, &aParamNumber);

    ACI_TEST(ulnWriteProtocol(aFnContext, aPtContext, &sPacket) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    if( (sState == 0) && (cmiGetLinkImpl(sCtx) == CMN_LINK_IMPL_IPCDA) )
    {
        ulnError(aFnContext, ulERR_ABORT_IPCDA_MESSAGE_TOO_LONG, CMB_BLOCK_DEFAULT_SIZE);
    }

    CMI_SET_CURSOR(sCtx, sOrgWriteCursor);

    return ACI_FAILURE;
}

ACI_RC ulnWriteParamInfoSetListREQ(ulnFnContext *aFnContext,
                                   ulnPtContext *aPtContext,
                                   acp_uint16_t  aParamCount)
{
    cmiProtocol         sPacket;
    cmiProtocolContext *sCtx = &(aPtContext->mCmiPtContext);
    acp_uint16_t        sParamNumber;
    ulnBindInfo*        sBindInfo;
    acp_uint32_t        sDataType;
    ulnStmt            *sStmt = aFnContext->mHandle.mStmt;
    ulnDescRec         *sDescRecApd;
    acp_sint16_t        sCType          = 0;
    acp_uint16_t        sOrgWriteCursor = CMI_GET_CURSOR(sCtx);
    acp_uint8_t         sState          = 0;

    sPacket.mOpID = CMP_OP_DB_ParamInfoSetList;

    /* BUG-44125 [mm-cli] IPCDA ��� �׽�Ʈ �� hang - iloader CLOB */
    CMI_WRITE_CHECK_WITH_IPCDA(sCtx, 7 + (aParamCount * 20), 7 + (aParamCount * 22));
    sState = 1;

    CMI_WOP(sCtx, CMP_OP_DB_ParamInfoSetList);
    CMI_WR4(sCtx, &(sStmt->mStatementID));
    CMI_WR2(sCtx, &aParamCount);

    for (sParamNumber = 1;
         sParamNumber <= aParamCount;
         sParamNumber++)
    {
        sDescRecApd = ulnStmtGetApdRec(sStmt, sParamNumber);

        sBindInfo = &(sDescRecApd->mBindInfo);
        sDataType = ulnTypeMap_MTYPE_MTD(sBindInfo->mMTYPE);

        CMI_WR2(sCtx, &sParamNumber);
        CMI_WR4(sCtx, &sDataType);
        CMI_WR4(sCtx, &(sBindInfo->mLanguage));
        CMI_WR1(sCtx, sBindInfo->mArguments);
        CMI_WR4(sCtx, (acp_uint32_t*)&(sBindInfo->mPrecision));
        CMI_WR4(sCtx, (acp_uint32_t*)&(sBindInfo->mScale));
        CMI_WR1(sCtx, ulnBindInfoCmParamInOutType(sBindInfo));

        /****************************************************************
         * PROJ-2616
         * 1) IPCDA-SIMPLEQUERY������ ����� �������� CTYPE�� �ٷ� �޾Ƽ�,
         * �����Ϳ� MT Ÿ������ ��ȯ�Ͽ� �����Ѵ�. ����, CTYPE�� ����
         * ������ �ʿ��ϴ�. �̰��� ���Ͽ� ulnWriteParamInfoSetListREQ����
         * �ش� ������ �����Ѵ�.
         *
         * 2) FastExecute���� DATE�� ���� Ÿ���� ����� ���� Ÿ�Կ� �°� ��ȯ�ϱ�
         * ���Ͽ� ���� SQLCTYPE�� �� �ʿ䰡 �ִ�. ���� CTYPE�� ULNCTYPE�̴�.
         ****************************************************************/
        if (cmiGetLinkImpl(sCtx) == CMI_LINK_IMPL_IPCDA)
        {
            /* mCTYPE */
            sCType = ulnTypeMap_CTYPE_SQLC(sDescRecApd->mMeta.mCTYPE);
            CMI_WR2(sCtx, (acp_uint16_t*)&sCType);
        }
    }

    ACI_TEST(ulnWriteProtocol(aFnContext, aPtContext, &sPacket) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    if( (sState == 0) && (cmiGetLinkImpl(sCtx) == CMN_LINK_IMPL_IPCDA) )
    {
        ulnError(aFnContext, ulERR_ABORT_IPCDA_MESSAGE_TOO_LONG, CMB_BLOCK_DEFAULT_SIZE);
    }

    CMI_SET_CURSOR(sCtx, sOrgWriteCursor);

    return ACI_FAILURE;
}

ACI_RC ulnWriteParamDataInListREQForSimpleQuery(ulnFnContext *aFnContext,
                                                ulnPtContext *aPtContext,
                                                acp_uint8_t   aExecuteOption)
{
    cmiProtocolContext *sCtx            = &(aPtContext->mCmiPtContext);
    ulnStmt            *sStmt           = aFnContext->mHandle.mStmt;
    acp_uint32_t        sDataSizeCurPos = 0;
    acp_uint32_t        sCursorCurPos   = 0;
    acp_uint64_t        sDataSize       = 0;

    acp_uint32_t        sFromRowNumber  = 1;
    acp_uint32_t        sToRowNumber    = 1;
    acp_list_node_t    *sIterator       = NULL; /*PROJ-2616*/
    acp_uint16_t        sOrgWriteCursor = CMI_GET_CURSOR(sCtx);
    acp_uint8_t         sState          = 0;

    /* BUG-44125 [mm-cli] IPCDA ��� �׽�Ʈ �� hang - iloader CLOB */
    CMI_WRITE_CHECK_WITH_IPCDA(sCtx, 22, 22 + 2 + (sStmt->mAttrArd->mDescRecCount * 2));
    sState = 1;

    CMI_WOP(sCtx, CMP_OP_DB_ParamDataInListV2);
    CMI_WR4(sCtx, &(sStmt->mStatementID));
    CMI_WR4(sCtx, &sFromRowNumber);
    CMI_WR4(sCtx, &sToRowNumber);
    CMI_WR1(sCtx, aExecuteOption);

    /* �Ʒ��� �������� ��ü ũ�⸦ ���� ��ġ�̴�.
     * ���� ���������� �������� ũ�⸦ �� �� ���� ������ sDataSizeCurPos�� ������
     * ��ġ�� ��� ��Ű��, �Ʒ��� ulnParamProcess_IPCDA_DATAs�� ���� �Ŀ�,
     * ���� ROW�� ������ ũ�⸦ sDataSizeCurPos ������ ����ϰ� �ȴ�.
     */
    sDataSizeCurPos = sCtx->mWriteBlock->mCursor;
    sCtx->mWriteBlock->mCursor += 8;

    /* SimpleQuery-Fetch���� ���� �÷��� ���ε� ������ ���� �����͸�
     * ���ڵ��ϱ� ���ؼ� �Ʒ��� ������ �ʿ��ϴ�.
     */
    CMI_NE_WR2(sCtx, &sStmt->mAttrArd->mDescRecCount);
    ACP_LIST_ITERATE(&sStmt->mAttrArd->mDescRecList, sIterator)
    {
        CMI_NE_WR2(sCtx, &((ulnDescRec *)sIterator)->mIndex);
    }

    /* ����� �Է� �������� ��� */
    ACI_TEST_RAISE(ulnParamProcess_IPCDA_DATAs(aFnContext,
                                               aPtContext,
                                               sFromRowNumber,
                                               &sDataSize) != ACI_SUCCESS,
                   err_param_process);

    /* ulnParamProcess_IPCDA_DATAs���� ��ȯ �� sDataSize�� sCursorCurPos�� ����Ѵ�. */
    sCursorCurPos = sCtx->mWriteBlock->mCursor;
    sCtx->mWriteBlock->mCursor = sDataSizeCurPos;
    CMI_WR8(sCtx, &sDataSize);
    /* ������ ������ CURSOR�� ���� ��Ų��. */
    sCtx->mWriteBlock->mCursor = sCursorCurPos;

    ACI_TEST(ulnWriteProtocol(aFnContext, aPtContext, NULL) != ACI_SUCCESS);

    return ACI_SUCCESS;

    /* array �̸� �����Ǹ� skip �ϰ� ������ ó�� */
    ACI_EXCEPTION(err_param_process)
    {
        ulnStmtIncreaseParamsProcessedValue(sStmt);
        ulnStmtSetAttrParamStatusValue(sStmt, sToRowNumber, SQL_PARAM_ERROR);
    }
    ACI_EXCEPTION_END;

    if( (sState == 0) && (cmiGetLinkImpl(sCtx) == CMN_LINK_IMPL_IPCDA) )
    {
        ulnError(aFnContext, ulERR_ABORT_IPCDA_MESSAGE_TOO_LONG, CMB_BLOCK_DEFAULT_SIZE);
    }

    CMI_SET_CURSOR(sCtx, sOrgWriteCursor);

    return ACI_FAILURE;
}

ACI_RC ulnWriteParamDataInListREQ(ulnFnContext *aFnContext,
                                  ulnPtContext *aPtContext,
                                  acp_uint32_t  aFromRowNumber,
                                  acp_uint32_t  aToRowNumber,
                                  acp_uint8_t   aExecuteOption)
{
    acp_uint64_t                 sRowSize;
    acp_uint8_t                 *sRow;

    cmiProtocol                  sPacket;
    cmiProtocolContext          *sCtx            = &(aPtContext->mCmiPtContext);
    ulnStmt                     *sStmt           = aFnContext->mHandle.mStmt;

    acp_list_node_t             *sIterator       = NULL; /*PROJ-2616*/
    acp_uint16_t                 sOrgWriteCursor = CMI_GET_CURSOR(sCtx);
    acp_uint8_t                  sState          = 0;

    sRowSize = ulnStmtChunkGetSize( sStmt );
    sRow     = ulnStmtChunkGetData( sStmt );

    sPacket.mOpID = CMP_OP_DB_ParamDataInListV2;

    /* BUG-44125 [mm-cli] IPCDA ��� �׽�Ʈ �� hang - iloader CLOB */
    CMI_WRITE_CHECK_WITH_IPCDA(sCtx, 22, 22 + 2 + (sStmt->mAttrArd->mDescRecCount * 2) + sRowSize);
    sState = 1;

    CMI_WOP(sCtx, CMP_OP_DB_ParamDataInListV2);
    CMI_WR4(sCtx, &(sStmt->mStatementID));
    CMI_WR4(sCtx, &aFromRowNumber);
    CMI_WR4(sCtx, &aToRowNumber);
    CMI_WR1(sCtx, aExecuteOption);
    CMI_WR8(sCtx, &sRowSize);

    if (cmiGetLinkImpl(sCtx) == CMI_LINK_IMPL_IPCDA)
    {
        /* SimpleQuery-Fetch���� ���� �÷��� ���ε� ������ ���� �����͸�
         * ���ڵ��ϱ� ���ؼ� �Ʒ��� ������ �ʿ��ϴ�.
         *
         * - �Ʒ��� ������ Prepare�� ������ �ٷ� Execute�� ������
         * ��쿡 SimpleQuery������ �� �� ���� ������ IPCDA�� ��쿡��
         * ������ �� ������ ������ �ȴ�.
         */
        CMI_NE_WR2(sCtx, &sStmt->mAttrArd->mDescRecCount);
        ACP_LIST_ITERATE(&sStmt->mAttrArd->mDescRecList, sIterator)
        {
            CMI_NE_WR2(sCtx, &((ulnDescRec *)sIterator)->mIndex);
        }
    }
    ACI_TEST( cmiSplitWrite( sCtx,
                             sRowSize,
                             sRow )
              != ACI_SUCCESS );

    ACI_TEST(ulnWriteProtocol(aFnContext, aPtContext, &sPacket) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    if( (sState == 0) && (cmiGetLinkImpl(sCtx) == CMN_LINK_IMPL_IPCDA) )
    {
        ulnError(aFnContext, ulERR_ABORT_IPCDA_MESSAGE_TOO_LONG, CMB_BLOCK_DEFAULT_SIZE);
    }

    CMI_SET_CURSOR(sCtx, sOrgWriteCursor);
    
    return ACI_FAILURE;
}

/*
 * aDescRecApd �� �����ϴ� Descriptor Record ��
 * aRow ��° row �� �ش��ϴ� ����� ���ε� �����͸�
 * BINDDATA IN REQ �� �Ǿ ������.
 */
ACI_RC ulnWriteParamDataInREQ(ulnFnContext *aFnContext,
                              ulnPtContext *aPtContext,
                              acp_uint16_t  aParamNumber,
                              void         *aUserDataPtr,
                              acp_sint32_t  aUserOctetLength,
                              ulnMeta      *aApdMeta,
                              ulnMeta      *aIpdMeta)
{
    ulnStmt                 *sStmt    = aFnContext->mHandle.mStmt;
    acp_uint32_t             sRowSize;
    acp_uint8_t             *sRow;

    cmiProtocol              sPacket;
    cmiProtocolContext      *sCtx            = &(aPtContext->mCmiPtContext);
    acp_uint8_t              sBitData;
    acp_uint16_t             sState          = 0;
    acp_uint16_t             sOrgWriteCursor = CMI_GET_CURSOR(sCtx);

    ACP_UNUSED(aUserOctetLength);
    ACP_UNUSED(aIpdMeta);

    sRowSize = ulnStmtChunkGetSize( sStmt );
    sRow     = ulnStmtChunkGetData( sStmt );

    sPacket.mOpID = CMP_OP_DB_ParamDataIn;

    /* BUG-44125 [mm-cli] IPCDA ��� �׽�Ʈ �� hang - iloader CLOB */
    CMI_WRITE_CHECK_WITH_IPCDA(sCtx, 11, 11 + sRowSize);
    sState = 1;

    CMI_WOP(sCtx, CMP_OP_DB_ParamDataIn);
    CMI_WR4(sCtx, &(aFnContext->mHandle.mStmt->mStatementID));
    CMI_WR2(sCtx, &aParamNumber);
    CMI_WR4(sCtx, &sRowSize);

    /*
     * ����ڰ� ���ε��� �޸� �������� �����͸� �����ͼ� ����.
     */

    /*
     * Note : BUG-16050
     *        mm ���� bit Ÿ���� ���ʿ��� ���������� �Ѻ�Ʈ�� �д´�.
     *        ��, 0xc0 �� precision 3 �� cmtBit �� �ؼ� ������ �ְ� �Ǹ�,
     *              1 1 0
     *        �� ��Ʈ ���� �ν��ϰ� �ȴ�.
     *        �׷� ������, �Ϲ����� 1, ��, 0x01 �� cmtBit �� �־ �ָ� ��Ʈ 0 ���� �ν��� ������.
     *
     *        �׷��� �� �� ���� ���ÿ� sBitData �� ���� �ű⿡ 0 Ȥ�� 1 �� ����־� �д�.
     *        cmtVariable �� �������� �����͸� ������ �ְ�, cmiWriteProtocol �ÿ�
     *        �����Ͱ� ����Ű�� �޸𸮸� �о �ϱ� ������ ulnDataWriteBufferToPacket() �Լ���
     *        ���ʿ� sBitData �� �� ���� ���� �븩�̴�.
     *
     *        ���⼭ ���� ������ sBitData �� ����ص� �Ǵ� ����, cmiWriteProtocol() ����
     *        �����Ͱ� ����Ű�� �޸𸮸� �����ϱ� ������ �� ���Ŀ��� ���ú����� �������
     *        ���� ���� ������ ������ �ᵵ �ȴ�. ��, �� �Լ������� ��� �Ѵ�.
     */
    if(ulnMetaGetCTYPE(aApdMeta) == ULN_CTYPE_BIT)
    {
        if(aUserDataPtr != NULL)
        {
            if(*(acp_uint8_t *)aUserDataPtr != 0)
            {
                sBitData = 0x80;
            }
            else
            {
                sBitData = 0;
            }

            aUserDataPtr = &sBitData;
        }
    }

    ACI_TEST( cmiSplitWrite( sCtx,
                             sRowSize,
                             sRow )
              != ACI_SUCCESS );

    /*
     * ��Ŷ�� ����.
     */
    ACI_TEST(ulnWriteProtocol(aFnContext, aPtContext, &sPacket) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    if( (sState == 0) && (cmiGetLinkImpl(sCtx) == CMN_LINK_IMPL_IPCDA) )
    {
        ulnError(aFnContext, ulERR_ABORT_IPCDA_MESSAGE_TOO_LONG, CMB_BLOCK_DEFAULT_SIZE);
    }

    CMI_SET_CURSOR(sCtx, sOrgWriteCursor);

    return ACI_FAILURE;
}

ACI_RC ulnWriteFetchMoveREQ(ulnFnContext *aFnContext,
                            ulnPtContext *aPtContext,
                            acp_uint8_t   aWhence,
                            acp_sint64_t  aOffset)
{
    cmiProtocol         sPacket;
    cmiProtocolContext *sCtx            = &(aPtContext->mCmiPtContext);
    acp_uint16_t        sOrgWriteCursor = CMI_GET_CURSOR(sCtx);
    acp_uint8_t         sState          = 0;

    sPacket.mOpID = CMP_OP_DB_FetchMove;

    CMI_WRITE_CHECK(sCtx, 16);
    sState = 1;

    CMI_WOP(sCtx, CMP_OP_DB_FetchMove);
    CMI_WR4(sCtx, &(aFnContext->mHandle.mStmt->mStatementID));
    CMI_WR2(sCtx, &(aFnContext->mHandle.mStmt->mCurrentResultSetID));
    CMI_WR1(sCtx, aWhence);
    CMI_WR8(sCtx, (acp_uint64_t*)&aOffset);

    ACI_TEST(ulnWriteProtocol(aFnContext, aPtContext, &sPacket) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    if( (sState == 0) && (cmiGetLinkImpl(sCtx) == CMN_LINK_IMPL_IPCDA) )
    {
        ulnError(aFnContext, ulERR_ABORT_IPCDA_MESSAGE_TOO_LONG, CMB_BLOCK_DEFAULT_SIZE);
    }

    CMI_SET_CURSOR(sCtx, sOrgWriteCursor);

    return ACI_FAILURE;
}

/* BUG-39463 Add new fetch protocol that can request over 65535 rows. */
ACI_RC ulnWriteFetchV2REQ(ulnFnContext *aFnContext,
                          ulnPtContext *aPtContext,
                          acp_uint32_t  aRowCountToFetch,
                          acp_uint16_t  aFirstColumnIndex,
                          acp_uint16_t  aLastColumnIndex)
{
    cmiProtocol          sPacket;
    cmiProtocolContext  *sCtx  = &(aPtContext->mCmiPtContext);

    acp_uint16_t         sOrgWriteCursor = CMI_GET_CURSOR(sCtx);
    acp_uint64_t         sReserved  = 0;
    acp_uint8_t          sState     = 0;

    sPacket.mOpID = CMP_OP_DB_FetchV2;

    /* BUG-44125 [mm-cli] IPCDA ��� �׽�Ʈ �� hang - iloader CLOB */
    CMI_WRITE_CHECK_WITH_IPCDA(sCtx, 23, 23 + 1);
    sState = 1;

    CMI_WOP(sCtx, CMP_OP_DB_FetchV2);
    CMI_WR4(sCtx, &(aFnContext->mHandle.mStmt->mStatementID));
    CMI_WR2(sCtx, &(aFnContext->mHandle.mStmt->mCurrentResultSetID));
    CMI_WR4(sCtx, &aRowCountToFetch);
    CMI_WR2(sCtx, &aFirstColumnIndex);
    CMI_WR2(sCtx, &aLastColumnIndex);
    CMI_WR8(sCtx, &sReserved);

    ACI_TEST(ulnWriteProtocol(aFnContext, aPtContext, &sPacket) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    if( (sState == 0) && (cmiGetLinkImpl(sCtx) == CMN_LINK_IMPL_IPCDA) )
    {
        ulnError(aFnContext, ulERR_ABORT_IPCDA_MESSAGE_TOO_LONG, CMB_BLOCK_DEFAULT_SIZE);
    }

    CMI_SET_CURSOR(sCtx, sOrgWriteCursor);

    return ACI_FAILURE;
}

/**
 * ulnExecWriteExecuteREQ.
 *
 * @param[in] aOption
 *  �Ʒ��� ���� �߿� �ϳ�
 *      CMP_DB_EXECUTE_NOAMAL_EXECUTE
 *      CMP_DB_EXECUTE_ARRAY_EXECUTE
 *      CMP_DB_EXECUTE_ARRAY_BEGIN
 *      CMP_DB_EXECUTE_ARRAY_END
 */
ACI_RC ulnWriteExecuteREQ(ulnFnContext *aFnContext,
                          ulnPtContext *aPtContext,
                          acp_uint32_t  aStatementID,
                          acp_uint32_t  aRowNumber,
                          acp_uint8_t   aOption)
{
    cmiProtocol         sPacket;
    cmiProtocolContext *sCtx             = &(aPtContext->mCmiPtContext);
    acp_uint32_t        sRowNumber       = aRowNumber + 1; // 1 ���̽��̴�.
    acp_uint16_t        sOrgWriteCursor  = CMI_GET_CURSOR(sCtx);
    acp_uint32_t        sOldReadProtocol = aPtContext->mNeedReadProtocol;
    ulnStmt            *sStmt            = aFnContext->mHandle.mStmt;
    acp_list_node_t    *sIterator        = NULL; /*PROJ-2616*/
    acp_uint8_t         sState           = 0;

    sPacket.mOpID = CMP_OP_DB_ExecuteV2;

    /* BUG-44125 [mm-cli] IPCDA ��� �׽�Ʈ �� hang - iloader CLOB */
    CMI_WRITE_CHECK_WITH_IPCDA(sCtx, 10, 10 + 2 + (sStmt->mAttrArd->mDescRecCount * 2));
    sState = 1;

    CMI_WOP(sCtx, CMP_OP_DB_ExecuteV2);
    CMI_WR4(sCtx, &aStatementID);
    CMI_WR4(sCtx, &sRowNumber);
    CMI_WR1(sCtx, aOption);

   /* PROJ-2616 
    * SimpleQuery-Fetch���� ���� �÷��� ���ε� ������ ���� �����͸�
     * ���ڵ��ϱ� ���ؼ� �Ʒ��� ������ �ʿ��ϴ�.
     *
     * - �Ʒ��� ������ Prepare�� ������ �ٷ� Execute�� ������
     * ��쿡 SimpleQuery������ �� �� ���� ������ IPCDA�� ��쿡��
     * ������ �� ������ ������ �ȴ�.
     */
    if (cmiGetLinkImpl(sCtx) == CMI_LINK_IMPL_IPCDA)
    {
        CMI_NE_WR2(sCtx, &sStmt->mAttrArd->mDescRecCount);
        ACP_LIST_ITERATE(&sStmt->mAttrArd->mDescRecList, sIterator)
        {
            CMI_NE_WR2(sCtx, &((ulnDescRec *)sIterator)->mIndex);
        }
    }

    ACI_TEST(ulnWriteProtocol(aFnContext, aPtContext, &sPacket) != ACI_SUCCESS);

    if( aOption == CMP_DB_EXECUTE_ARRAY_BEGIN ||
        aOption == CMP_DB_EXECUTE_ARRAY_END   ||
        aOption == CMP_DB_EXECUTE_ATOMIC_EXECUTE ||
        aOption == CMP_DB_EXECUTE_ATOMIC_BEGIN )
    {
        aPtContext->mNeedReadProtocol = sOldReadProtocol;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    if( aOption == CMP_DB_EXECUTE_ARRAY_BEGIN ||
        aOption == CMP_DB_EXECUTE_ARRAY_END   ||
        aOption == CMP_DB_EXECUTE_ATOMIC_EXECUTE ||
        aOption == CMP_DB_EXECUTE_ATOMIC_BEGIN )
    {
        aPtContext->mNeedReadProtocol = sOldReadProtocol;
    }

    if( (sState == 0) && (cmiGetLinkImpl(sCtx) == CMN_LINK_IMPL_IPCDA) )
    {
        ulnError(aFnContext, ulERR_ABORT_IPCDA_MESSAGE_TOO_LONG, CMB_BLOCK_DEFAULT_SIZE);
    }

    CMI_SET_CURSOR(sCtx, sOrgWriteCursor);

    return ACI_FAILURE;
}

ACI_RC ulnWritePlanGetREQ(ulnFnContext *aFnContext, ulnPtContext *aPtContext, acp_uint32_t aStatementID)
{
    cmiProtocol         sPacket;
    cmiProtocolContext *sCtx            = &(aPtContext->mCmiPtContext);
    acp_uint16_t        sOrgWriteCursor = CMI_GET_CURSOR(sCtx);
    acp_uint8_t         sState = 0;

    sPacket.mOpID = CMP_OP_DB_PlanGet;

    CMI_WRITE_CHECK(sCtx, 5);
    sState = 1;

    CMI_WOP(sCtx, CMP_OP_DB_PlanGet);
    CMI_WR4(sCtx, &aStatementID);

    ACI_TEST(ulnWriteProtocol(aFnContext, aPtContext, &sPacket) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    if( (sState == 0) && (cmiGetLinkImpl(sCtx) == CMN_LINK_IMPL_IPCDA) )
    {
        ulnError(aFnContext, ulERR_ABORT_IPCDA_MESSAGE_TOO_LONG, CMB_BLOCK_DEFAULT_SIZE);
    }

    CMI_SET_CURSOR(sCtx, sOrgWriteCursor);

    return ACI_FAILURE;
}

ACI_RC ulnWriteTransactionREQ(ulnFnContext     *aFnContext,
                              ulnPtContext     *aPtContext,
                              ulnTransactionOp  aTransactOp)
{
    cmiProtocol         sPacket;
    cmiProtocolContext *sCtx            = &(aPtContext->mCmiPtContext);
    acp_uint16_t        sOrgWriteCursor = CMI_GET_CURSOR(sCtx);
    acp_uint8_t         sState          = 0;

    sPacket.mOpID = CMP_OP_DB_Transaction;

    CMI_WRITE_CHECK(sCtx, 2);
    sState = 1;

    CMI_WOP(sCtx, CMP_OP_DB_Transaction);

    switch(aTransactOp)
    {
        case ULN_TRANSACT_COMMIT:
            CMI_WR1(sCtx, 1);
            break;

        case ULN_TRANSACT_ROLLBACK:
            CMI_WR1(sCtx, 2);
            break;

        default:
            ACI_RAISE(LABEL_INVALID_OPCODE);
            break;
    }

    ACI_TEST(ulnWriteProtocol(aFnContext, aPtContext, &sPacket) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_INVALID_OPCODE)
    {
        ulnError(aFnContext, ulERR_ABORT_INVALID_TRANSACTION_OPCODE);
    }
    ACI_EXCEPTION_END;

    if( (sState == 0) && (cmiGetLinkImpl(sCtx) == CMN_LINK_IMPL_IPCDA) )
    {
        ulnError(aFnContext, ulERR_ABORT_IPCDA_MESSAGE_TOO_LONG, CMB_BLOCK_DEFAULT_SIZE);
    }

    CMI_SET_CURSOR(sCtx, sOrgWriteCursor);

    return ACI_FAILURE;
}

/*
 * ===================================
 * LOB packet write functions
 * ===================================
 */

/* PROJ-2047 Strengthening LOB - Removed aOldSize */
ACI_RC ulnWriteLobPutBeginREQ(ulnFnContext *aFnContext,
                              ulnPtContext *aPtContext,
                              acp_uint64_t  aLocatorID,
                              acp_uint32_t  aOffset,
                              acp_uint32_t  aSize)
{
    cmiProtocol         sPacket;
    cmiProtocolContext *sCtx            = &(aPtContext->mCmiPtContext);
    acp_uint16_t        sOrgWriteCursor = CMI_GET_CURSOR(sCtx);
    acp_uint8_t         sState          = 0;

    ACI_TEST(cmiGetLinkImpl(sCtx) == CMI_LINK_IMPL_IPCDA);

    sPacket.mOpID = CMP_OP_DB_LobPutBegin;

    CMI_WRITE_CHECK(sCtx, 17);
    sState = 1;

    CMI_WOP(sCtx, CMP_OP_DB_LobPutBegin);
    CMI_WR8(sCtx, &aLocatorID);
    CMI_WR4(sCtx, &aOffset);
    CMI_WR4(sCtx, &aSize);

    ACI_TEST(ulnWriteProtocol(aFnContext, aPtContext, &sPacket) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    if( (sState == 0) && (cmiGetLinkImpl(sCtx) == CMN_LINK_IMPL_IPCDA) )
    {
        ulnError(aFnContext, ulERR_ABORT_IPCDA_MESSAGE_TOO_LONG, CMB_BLOCK_DEFAULT_SIZE);
    }

    CMI_SET_CURSOR(sCtx, sOrgWriteCursor);

    return ACI_FAILURE;
}

ACI_RC ulnWriteLobPutEndREQ(ulnFnContext *aFnContext, ulnPtContext *aPtContext, acp_uint64_t aLocatorID)
{
    cmiProtocol          sPacket;
    cmiProtocolContext  *sCtx            = &(aPtContext->mCmiPtContext);
    acp_uint16_t         sOrgWriteCursor = CMI_GET_CURSOR(sCtx);
    acp_uint8_t          sState          = 0;

    ACI_TEST(cmiGetLinkImpl(sCtx) == CMI_LINK_IMPL_IPCDA);

    sPacket.mOpID = CMP_OP_DB_LobPutEnd;

    CMI_WRITE_CHECK(sCtx, 9);
    sState = 1;

    CMI_WOP(sCtx, CMP_OP_DB_LobPutEnd);
    CMI_WR8(sCtx, &aLocatorID);

    ACI_TEST(ulnWriteProtocol(aFnContext, aPtContext, &sPacket) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    if( (sState == 0) && (cmiGetLinkImpl(sCtx) == CMN_LINK_IMPL_IPCDA) )
    {
        ulnError(aFnContext, ulERR_ABORT_IPCDA_MESSAGE_TOO_LONG, CMB_BLOCK_DEFAULT_SIZE);
    }

    CMI_SET_CURSOR(sCtx, sOrgWriteCursor);

    return ACI_FAILURE;
}

ACI_RC ulnWriteLobFreeREQ(ulnFnContext *aFnContext, ulnPtContext *aPtContext, acp_uint64_t aLocator)
{
    ulnDbc              *sDbc;
    cmiProtocolContext  *sCtx            = &(aPtContext->mCmiPtContext);
    cmiProtocol          sPacket;
    acp_uint16_t         sOrgWriteCursor = CMI_GET_CURSOR(sCtx);
    acp_uint8_t          sState          = 0;

    ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);

    ACI_TEST(cmiGetLinkImpl(sCtx) == CMI_LINK_IMPL_IPCDA);

    ACI_TEST( sDbc == NULL );           //BUG-28623 [CodeSonar]Null Pointer Dereference

    sPacket.mOpID = CMP_OP_DB_LobFree;

    CMI_WRITE_CHECK(sCtx, 9);
    sState = 1;

    CMI_WOP(sCtx, CMP_OP_DB_LobFree);
    CMI_WR8(sCtx, &aLocator);

    if(sDbc->mConnType == ULN_CONNTYPE_IPC || sDbc->mConnType == ULN_CONNTYPE_IPCDA)
    {
        gUlnCmProtocolTable[CMP_OP_DB_LobFree].mNeedResult = 1;
    }

    /*
     * ��Ŷ ����
     */
    ACI_TEST(ulnWriteProtocol(aFnContext, aPtContext, &sPacket) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    if( (sState == 0) && (cmiGetLinkImpl(sCtx) == CMN_LINK_IMPL_IPCDA) )
    {
        ulnError(aFnContext, ulERR_ABORT_IPCDA_MESSAGE_TOO_LONG, CMB_BLOCK_DEFAULT_SIZE);
    }

    CMI_SET_CURSOR(sCtx, sOrgWriteCursor);

    return ACI_FAILURE;
}

ACI_RC ulnWriteLobFreeAllREQ(ulnFnContext *aFnContext,
                             ulnPtContext *aPtContext,
                             acp_uint64_t  aLocatorCount,
                             void         *aBuffer )
{
    ulnDbc              *sDbc;
    cmiProtocol          sPacket;
    acp_uint32_t         sCount;
    acp_uint64_t        *sBuffer         = (acp_uint64_t*)aBuffer;
    cmiProtocolContext  *sCtx            = &(aPtContext->mCmiPtContext);
    acp_uint16_t         sOrgWriteCursor = CMI_GET_CURSOR(sCtx);
    acp_uint8_t          sState          = 0;

    ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);

    ACI_TEST(cmiGetLinkImpl(sCtx) == CMI_LINK_IMPL_IPCDA);

    ACI_TEST( sDbc == NULL );           //BUG-28623 [CodeSonar]Null Pointer Dereference

    sPacket.mOpID = CMP_OP_DB_LobFreeAll;

    CMI_WRITE_CHECK(sCtx, 9 + (aLocatorCount * 8));
    sState = 1;

    CMI_WOP(sCtx, CMP_OP_DB_LobFreeAll);
    CMI_WR8(sCtx, &aLocatorCount);

    for( sCount = 0; sCount < aLocatorCount; sCount++ )
    {
        CMI_WR8(sCtx, &(sBuffer[sCount]));
    }

    if(sDbc->mConnType == ULN_CONNTYPE_IPC || sDbc->mConnType == ULN_CONNTYPE_IPCDA)
    {
        gUlnCmProtocolTable[CMP_OP_DB_LobFreeAll].mNeedResult = 1;
    }

    /*
     * ��Ŷ ����
     */
    ACI_TEST(ulnWriteProtocol(aFnContext, aPtContext, &sPacket) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    if( (sState == 0) && (cmiGetLinkImpl(sCtx) == CMN_LINK_IMPL_IPCDA) )
    {
        ulnError(aFnContext, ulERR_ABORT_IPCDA_MESSAGE_TOO_LONG, CMB_BLOCK_DEFAULT_SIZE);
    }

    CMI_SET_CURSOR(sCtx, sOrgWriteCursor);

    return ACI_FAILURE;
}

/*
 * Getting lob data from the server
 */

ACI_RC ulnWriteLobGetREQ(ulnFnContext *aFnContext,
                         ulnPtContext *aPtContext,
                         acp_uint64_t  aLobLocator,
                         acp_uint32_t  aStartingOffset,
                         acp_uint32_t  aSizeToGet)
{
    cmiProtocol          sPacket;
    cmiProtocolContext  *sCtx            = &(aPtContext->mCmiPtContext);
    acp_uint16_t         sOrgWriteCursor = CMI_GET_CURSOR(sCtx);
    acp_uint8_t          sState          = 0;

    ACI_TEST(cmiGetLinkImpl(sCtx) == CMI_LINK_IMPL_IPCDA);

    sPacket.mOpID = CMP_OP_DB_LobGet;

    CMI_WRITE_CHECK(sCtx, 17);
    sState = 1;

    CMI_WOP(sCtx, CMP_OP_DB_LobGet);
    CMI_WR8(sCtx, &aLobLocator);
    CMI_WR4(sCtx, &aStartingOffset);
    CMI_WR4(sCtx, &aSizeToGet);

    ACI_TEST(ulnWriteProtocol(aFnContext, aPtContext, &sPacket) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    if( (sState == 0) && (cmiGetLinkImpl(sCtx) == CMN_LINK_IMPL_IPCDA) )
    {
        ulnError(aFnContext, ulERR_ABORT_IPCDA_MESSAGE_TOO_LONG, CMB_BLOCK_DEFAULT_SIZE);
    }

    CMI_SET_CURSOR(sCtx, sOrgWriteCursor);

    return ACI_FAILURE;
}

/*
 * Getting Lob Size
 */

ACI_RC ulnWriteLobGetSizeREQ(ulnFnContext *aFnContext, ulnPtContext *aPtContext, acp_uint64_t aLocator)
{
    cmiProtocol          sPacket;
    cmiProtocolContext  *sCtx            = &(aPtContext->mCmiPtContext);
    acp_uint16_t         sOrgWriteCursor = CMI_GET_CURSOR(sCtx);
    acp_uint8_t          sState          = 0;

    ACI_TEST(cmiGetLinkImpl(sCtx) == CMI_LINK_IMPL_IPCDA);

    sPacket.mOpID = CMP_OP_DB_LobGetSize;

    CMI_WRITE_CHECK(sCtx, 9);
    sState = 1;

    CMI_WOP(sCtx, CMP_OP_DB_LobGetSize);
    CMI_WR8(sCtx, &aLocator);
    
    /*
     * ��Ŷ ����
     */
    ACI_TEST(ulnWriteProtocol(aFnContext, aPtContext, &sPacket) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    if( (sState == 0) && (cmiGetLinkImpl(sCtx) == CMN_LINK_IMPL_IPCDA) )
    {
        ulnError(aFnContext, ulERR_ABORT_IPCDA_MESSAGE_TOO_LONG, CMB_BLOCK_DEFAULT_SIZE);
    }

    CMI_SET_CURSOR(sCtx, sOrgWriteCursor);

    return ACI_FAILURE;
}

/* PROJ-2047 Strengthening LOB - Added Interfaces */
ACI_RC ulnWriteLobTrimREQ(ulnFnContext *aFnContext,
                          ulnPtContext *aPtContext,
                          acp_uint64_t  aLocatorID,
                          acp_uint32_t  aOffset)
{
    cmiProtocol         sPacket;
    cmiProtocolContext *sCtx            = &(aPtContext->mCmiPtContext);
    acp_uint16_t        sOrgWriteCursor = CMI_GET_CURSOR(sCtx);
    acp_uint8_t         sState          = 0;

    ACI_TEST(cmiGetLinkImpl(sCtx) == CMI_LINK_IMPL_IPCDA);

    sPacket.mOpID = CMP_OP_DB_LobTrim;

    CMI_WRITE_CHECK(sCtx, 13);
    sState = 1;

    CMI_WOP(sCtx, CMP_OP_DB_LobTrim);
    CMI_WR8(sCtx, &aLocatorID);
    CMI_WR4(sCtx, &aOffset);

    ACI_TEST(ulnWriteProtocol(aFnContext, aPtContext, &sPacket) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    if( (sState == 0) && (cmiGetLinkImpl(sCtx) == CMN_LINK_IMPL_IPCDA) )
    {
        ulnError(aFnContext, ulERR_ABORT_IPCDA_MESSAGE_TOO_LONG, CMB_BLOCK_DEFAULT_SIZE);
    }

    CMI_SET_CURSOR(sCtx, sOrgWriteCursor);

    return ACI_FAILURE;
}

/*
 * ulnWritePropertySetV2REQ.
 *
 * protocol context �� Property Set Req �� ���⸸ �Ѵ�.
 */
ACI_RC ulnWritePropertySetV2REQ(ulnFnContext  *aContext,
                                ulnPtContext  *aPtContext,
                                ulnPropertyId  aCmPropertyId,
                                void          *aValuePtr)
{
    cmiProtocolContext  *sCtx = &(aPtContext->mCmiPtContext);
    acp_uint8_t          s8;
    acp_uint32_t         s32;
    acp_uint32_t         sLen;
    cmiProtocol          sPacket;
    acp_uint16_t         sID             = (acp_uint16_t) aCmPropertyId;
    acp_uint16_t         sOrgWriteCursor = CMI_GET_CURSOR(sCtx);
    acp_uint8_t          sState          = 0;

    /* 
     * BUG-41793 Keep a compatibility among tags
     *
     * CMP_OP_DB_PropertySetV2(1) | PropetyID(2) | ValueLen(4) | Value(?)
     */
    acp_uint32_t         sValueLen = 0;

    /*
     * protocol �ʱ�ȭ
     */
    sPacket.mOpID = CMP_OP_DB_PropertySetV2;

    switch(sID)
    {
        /*
         * Null terminated string
         */
        case ULN_PROPERTY_CLIENT_PACKAGE_VERSION:
        case ULN_PROPERTY_CLIENT_TYPE:
        case ULN_PROPERTY_DATE_FORMAT:
        case ULN_PROPERTY_NLS:
        case ULN_PROPERTY_NLS_CHARACTERSET:       /* PROJ-1579 NCHAR */
        case ULN_PROPERTY_NLS_NCHAR_CHARACTERSET: /* PROJ-1579 NCHAR */
        case ULN_PROPERTY_APP_INFO:
        case ULN_PROPERTY_FAILOVER_SOURCE: /* BUG-31390 Failover info for v$session */
        case ULN_PROPERTY_TIME_ZONE:              /* PROJ-2209 DBTIMEZONE */
            sLen = acpCStrLen((acp_char_t *)aValuePtr, ACP_SINT32_MAX);
            sValueLen = 4 + sLen;

            CMI_WRITE_CHECK(sCtx, PROPERTY_SETV2_FIXED_LEN + sValueLen);
            sState = 1;

            CMI_WOP(sCtx, CMP_OP_DB_PropertySetV2);
            CMI_WR2(sCtx, &sID);
            CMI_WR4(sCtx, &sValueLen);
            CMI_WR4(sCtx, &sLen);
            CMI_WCP(sCtx, aValuePtr, sLen);
            break;

            /*
             * acp_uint8_t
             */
        case ULN_PROPERTY_AUTOCOMMIT:
        case ULN_PROPERTY_EXPLAIN_PLAN:
            s8 = (acp_uint8_t)((acp_ulong_t)aValuePtr);
            sValueLen = 1;

            CMI_WRITE_CHECK(sCtx, PROPERTY_SETV2_FIXED_LEN + sValueLen);
            sState = 1;

            CMI_WOP(sCtx, CMP_OP_DB_PropertySetV2);
            CMI_WR2(sCtx, &sID);
            CMI_WR4(sCtx, &sValueLen);
            CMI_WR1(sCtx, s8);
            break;

            /*
             * acp_uint32_t
             */
        case ULN_PROPERTY_OPTIMIZER_MODE:
        case ULN_PROPERTY_HEADER_DISPLAY_MODE:
        case ULN_PROPERTY_STACK_SIZE:
        case ULN_PROPERTY_IDLE_TIMEOUT:
        case ULN_PROPERTY_QUERY_TIMEOUT:
        /* BUG-32885 Timeout for DDL must be distinct to query_timeout or utrans_timeout */
        case ULN_PROPERTY_DDL_TIMEOUT:
        case ULN_PROPERTY_FETCH_TIMEOUT:
        case ULN_PROPERTY_UTRANS_TIMEOUT:
        case ULN_PROPERTY_NLS_NCHAR_LITERAL_REPLACE: /* PROJ-1579 NCHAR */
        case ULN_PROPERTY_MAX_STATEMENTS_PER_SESSION: /* BUG-31144 */
        case ULN_PROPERTY_LOB_CACHE_THRESHOLD: /* PROJ-2047 Strengthening LOB - LOBCACHE */
            s32 = (acp_uint32_t)((acp_ulong_t)aValuePtr);
            sValueLen = 4;

            CMI_WRITE_CHECK(sCtx, PROPERTY_SETV2_FIXED_LEN + sValueLen);
            sState = 1;

            CMI_WOP(sCtx, CMP_OP_DB_PropertySetV2);
            CMI_WR2(sCtx, &sID);
            CMI_WR4(sCtx, &sValueLen);
            CMI_WR4(sCtx, &s32);
            break;

            /* BUG-39817 */
        case ULN_PROPERTY_ISOLATION_LEVEL:
            switch ((acp_uint32_t)((acp_ulong_t)aValuePtr))
            {
                case SQL_TXN_READ_COMMITTED:
                    s32 = ULN_SERVER_ISOLATION_LEVEL_READ_COMMITTED;
                    break;

                case SQL_TXN_REPEATABLE_READ:
                    s32 = ULN_SERVER_ISOLATION_LEVEL_REPEATABLE_READ;
                    break;

                case SQL_TXN_SERIALIZABLE:
                    s32 = ULN_SERVER_ISOLATION_LEVEL_SERIALIZABLE;
                    break;

                default:
                    ACI_RAISE(LABEL_INVALID_ATTR_VALUE);
                    break;
            }

            sValueLen = 4;

            CMI_WRITE_CHECK(sCtx, PROPERTY_SETV2_FIXED_LEN + sValueLen);
            sState = 1;

            CMI_WOP(sCtx, CMP_OP_DB_PropertySetV2);
            CMI_WR2(sCtx, &sID);
            CMI_WR4(sCtx, &sValueLen);
            CMI_WR4(sCtx, &s32);
            break;

            /*
             * acp_uint64_t
             */
        case ULN_PROPERTY_CLIENT_PROTOCOL_VERSION:
        case ULN_PROPERTY_CLIENT_PID:
            sValueLen = 8;

            CMI_WRITE_CHECK(sCtx, PROPERTY_SETV2_FIXED_LEN + sValueLen);
            sState = 1;

            CMI_WOP(sCtx, CMP_OP_DB_PropertySetV2);
            CMI_WR2(sCtx, &sID);
            CMI_WR4(sCtx, &sValueLen);
            CMI_WR8(sCtx, (acp_ulong_t*)aValuePtr);
            break;

        case ULN_PROPERTY_SHARD_LINKER_TYPE: /* PROJ-2638 shard native linker */
            s8 = (acp_uint8_t)((acp_ulong_t)aValuePtr);
            CMI_WRITE_CHECK( sCtx, 8 );
            sValueLen = 1;

            CMI_WOP( sCtx, CMP_OP_DB_PropertySetV2 );
            CMI_WR2( sCtx, &sID );
            CMI_WR4( sCtx, &sValueLen );
            CMI_WR1( sCtx, s8 );
            break;

        case ULN_PROPERTY_SHARD_NODE_NAME: /* PROJ-2638 shard native linker */

            sLen = acpCStrLen( (acp_char_t *)aValuePtr, ULSD_MAX_NODE_NAME_LEN );
            sValueLen = sLen + 4; /* data'length + sizeof int */

            CMI_WRITE_CHECK( sCtx, ( 7 + sValueLen ) );

            CMI_WOP( sCtx, CMP_OP_DB_PropertySetV2 );
            CMI_WR2( sCtx, &sID );
            CMI_WR4( sCtx, &sValueLen );
            CMI_WR4( sCtx, &sLen );
            CMI_WCP( sCtx, aValuePtr, sLen );
            break;

        default:
            /*
             * HY092 : Invalid attribute/option identifier
             */
            ACI_RAISE(LABEL_INVALID_ATTR_OPTION);
            break;
    }

    /*
     * ��Ŷ ����
     */
    ACI_TEST(ulnWriteProtocol(aContext, aPtContext, &sPacket) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_INVALID_ATTR_OPTION)
    {
        /*
         * HY092 : Invalid attribute/option identifier
         */
        ulnError(aContext, ulERR_ABORT_INVALID_ATTR_OPTION, aCmPropertyId);
    }
    ACI_EXCEPTION(LABEL_INVALID_ATTR_VALUE);
    {
        ulnError(aContext, ulERR_ABORT_INVALID_ATTRIBUTE_VALUE);
    }
    ACI_EXCEPTION_END;

    if( (sState == 0) && (cmiGetLinkImpl(sCtx) == CMN_LINK_IMPL_IPCDA) )
    {
        ulnError(aContext, ulERR_ABORT_IPCDA_MESSAGE_TOO_LONG, CMB_BLOCK_DEFAULT_SIZE);
    }

    CMI_SET_CURSOR(sCtx, sOrgWriteCursor);

    return ACI_FAILURE;
}

/*
 * ulnWritePropertyGetREQ.
 */
ACI_RC ulnWritePropertyGetREQ(ulnFnContext  *aContext,
                              ulnPtContext  *aPtContext,
                              ulnPropertyId  aCmPropertyId)
{
    cmiProtocol         sPacket;
    cmiProtocolContext *sCtx            = &(aPtContext->mCmiPtContext);
    acp_uint16_t        sID             = (acp_uint16_t) aCmPropertyId;
    acp_uint16_t        sOrgWriteCursor = CMI_GET_CURSOR(sCtx);
    acp_uint8_t         sState          = 0;

    sPacket.mOpID = CMP_OP_DB_PropertyGet;

    CMI_WRITE_CHECK(sCtx, 3);
    sState = 1;

    CMI_WOP(sCtx, CMP_OP_DB_PropertyGet);
    CMI_WR2(sCtx, &sID);

    /* 1. write request */
    ACI_TEST(ulnWriteProtocol(aContext, aPtContext, &sPacket) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    if( (sState == 0) && (cmiGetLinkImpl(sCtx) == CMN_LINK_IMPL_IPCDA) )
    {
        ulnError(aContext, ulERR_ABORT_IPCDA_MESSAGE_TOO_LONG, CMB_BLOCK_DEFAULT_SIZE);
    }

    CMI_SET_CURSOR(sCtx, sOrgWriteCursor);

    return ACI_FAILURE;
}

ACI_RC ulnSendConnectAttr(ulnFnContext  *aContext,
                          ulnPropertyId  aCmPropertyID,
                          void          *aValuePtr)
{
    ULN_FLAG(sNeedPtContextFin);

    ulnDbc              *sDbc;
    acp_time_t       sTimeout;
    sDbc = aContext->mHandle.mDbc;

    cmiProtocolContext *sCtx = NULL;

    /* does not call protocol if no link */
    if (ulnDbcIsConnected(sDbc) == ACP_TRUE)
    {
        /*
         * protocol context �ʱ�ȭ.
         */
        //fix BUG-17722
        ACI_TEST(ulnInitializeProtocolContext(aContext,
                                              &(sDbc->mPtContext),
                                              &(sDbc->mSession))
                 != ACI_SUCCESS);

        ULN_FLAG_UP(sNeedPtContextFin);

        /*
         * ������ ��Ŷ�� ���� protocol context �� ����.
         */
        ACI_TEST(ulnWritePropertySetV2REQ(aContext,  &(sDbc->mPtContext),
                                          aCmPropertyID, aValuePtr)
                 != ACI_SUCCESS);

        /*
         * ���� ����
         */
        ACI_TEST(ulnFlushProtocol(aContext, &(sDbc->mPtContext) ) != ACI_SUCCESS);

        /*
         * ���� ���
         */
        sTimeout = acpTimeFrom(ulnDbcGetLoginTimeout(sDbc), 0);

        sCtx = &sDbc->mPtContext.mCmiPtContext;
        if (cmiGetLinkImpl(sCtx) == CMI_LINK_IMPL_IPCDA)
        {
            ACI_TEST(ulnReadProtocolIPCDA(aContext, &(sDbc->mPtContext), sTimeout) != ACI_SUCCESS);
        }
        else
        {
            ACI_TEST(ulnReadProtocol(aContext, &(sDbc->mPtContext), sTimeout) != ACI_SUCCESS);
        }

        /*
         * protocol context ����
         */
        ULN_FLAG_DOWN(sNeedPtContextFin);

        ACI_TEST(ulnFinalizeProtocolContext(aContext,&(sDbc->mPtContext)) != ACI_SUCCESS);
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    ULN_IS_FLAG_UP(sNeedPtContextFin)
    {
        ulnFinalizeProtocolContext(aContext,&(sDbc->mPtContext));
    }

    return ACI_FAILURE;
}

/* PROJ-2177 User Interface - Cancel */
ACI_RC ulnWriteCancelREQ(ulnFnContext  *aFnContext,
                         ulnPtContext  *aPtContext,
                         acp_uint32_t  aStatementID)
{
    cmiProtocol         sPacket;
    cmiProtocolContext *sCtx            = &(aPtContext->mCmiPtContext);
    acp_uint16_t        sOrgWriteCursor = CMI_GET_CURSOR(sCtx);
    acp_uint8_t         sState          = 0;

    sPacket.mOpID = CMP_OP_DB_Cancel;

    CMI_WRITE_CHECK(sCtx, 5);
    sState = 1;

    CMI_WOP(sCtx, CMP_OP_DB_Cancel);
    CMI_WR4(sCtx, &aStatementID);

    ACI_TEST(ulnWriteProtocol(aFnContext, aPtContext, &sPacket) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    if( (sState == 0) && (cmiGetLinkImpl(sCtx) == CMN_LINK_IMPL_IPCDA) )
    {
        ulnError(aFnContext, ulERR_ABORT_IPCDA_MESSAGE_TOO_LONG, CMB_BLOCK_DEFAULT_SIZE);
    }

    CMI_SET_CURSOR(sCtx, sOrgWriteCursor);

    return ACI_FAILURE;
}

ACI_RC ulnWritePrepareByCIDREQ(ulnFnContext  *aFnContext,
                               ulnPtContext  *aProtocolContext,
                               acp_char_t    *aString,
                               acp_sint32_t   aLength)
{
    cmiProtocol         sPacket;
    cmiProtocolContext *sCtx            = &(aProtocolContext->mCmiPtContext);
    ulnStmt            *sStmt           = aFnContext->mHandle.mStmt;
    acp_uint8_t        *sRow            = (acp_uint8_t*)aString;
    acp_uint64_t        sRowSize        = aLength;
    acp_uint8_t         sPrepareMode    = ulnStmtGetPrepareMode(sStmt, CMP_DB_PREPARE_MODE_EXEC_DIRECT);
    acp_uint16_t        sOrgWriteCursor = CMI_GET_CURSOR(sCtx);
    acp_uint8_t         sState          = 0;

    /* PROJ-1381 FAC, PROJ-1789 Updatable Scrollable Cursor */

    sPacket.mOpID = CMP_OP_DB_PrepareByCID;

    sStmt->mIsSimpleQuery = ACP_FALSE; /*PROJ-2616*/

    /* BUG-44125 [mm-cli] IPCDA ��� �׽�Ʈ �� hang - iloader CLOB */
    CMI_WRITE_CHECK_WITH_IPCDA(sCtx, 10, 10 + sRowSize);
    sState = 1;

    CMI_WOP(sCtx, CMP_OP_DB_PrepareByCID);
    CMI_WR4(sCtx, &(sStmt->mStmtCID));
    CMI_WR1(sCtx, sPrepareMode);
    CMI_WR4(sCtx, (acp_uint32_t*)&aLength);

    ACI_TEST( cmiSplitWrite( sCtx,
                             sRowSize,
                             sRow )
              != ACI_SUCCESS );

    ACI_TEST(ulnWriteProtocol(aFnContext, aProtocolContext, &sPacket) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    if( (sState == 0) && (cmiGetLinkImpl(sCtx) == CMN_LINK_IMPL_IPCDA) )
    {
        ulnError(aFnContext, ulERR_ABORT_IPCDA_MESSAGE_TOO_LONG, CMB_BLOCK_DEFAULT_SIZE);
    }

    CMI_SET_CURSOR(sCtx, sOrgWriteCursor);

    return ACI_FAILURE;
}

ACI_RC ulnWriteCancelByCIDREQ(ulnFnContext  *aFnContext,
                              ulnPtContext  *aPtContext,
                              acp_uint32_t  aStmtCID)
{
    cmiProtocol         sPacket;
    cmiProtocolContext *sCtx     = &(aPtContext->mCmiPtContext);
    acp_uint8_t        sState = 0;

    sPacket.mOpID = CMP_OP_DB_CancelByCID;

    CMI_WRITE_CHECK(sCtx, 5);
    sState = 1;

    CMI_WOP(sCtx, CMP_OP_DB_CancelByCID);
    CMI_WR4(sCtx, &aStmtCID);

    ACI_TEST(ulnWriteProtocol(aFnContext, aPtContext, &sPacket) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    if (sState == 1)
    {
        ulnError(aFnContext, ulERR_ABORT_IPCDA_MESSAGE_TOO_LONG, CMB_BLOCK_DEFAULT_SIZE);
        (void)cmiFinalizeSendBufferForIPCDA(sCtx);
    }

    return ACI_FAILURE;
}
