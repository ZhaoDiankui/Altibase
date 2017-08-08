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

#include <iduVersionDef.h>
#include <uln.h>
#include <ulnPrivate.h>
#include <ulnConnectCore.h>
#include <ulnConnAttribute.h>
#include <ulnSetConnectAttr.h>
#include <ulnDataSource.h>

#define UNIX_FILE_PATH_LEN 1024
#define IPC_FILE_PATH_LEN  1024

extern const acp_char_t *ulcGetClientType();

ACI_RC ulnDrvConnStrToInt(acp_char_t *aString, acp_uint32_t aLength, acp_sint32_t *aPortNo)
{
    acp_uint32_t i;
    acp_sint32_t sRetValue = 0;

    for(i = 0; i < aLength; i++)
    {
        if (*(aString + i) == ' ')
        {
            if (sRetValue == 0)
            {
                /*
                 * �տ� ���� ȭ��Ʈ �����̽��� ����
                 */
                continue;
            }
            else
            {
                /*
                 * �ϴ� ���ڰ� ���� ���� ȭ��Ʈ �����̽��� ������ ��
                 */
                break;
            }
        }
        else
        {
            if ('0' <= *(aString + i) && *(aString + i) <= '9')
            {
                sRetValue *= 10;
                sRetValue += *(aString + i) - '0';
            }
            else
            {
                /*
                 * ���ڸ� �޴´�.
                 */
                ACI_RAISE(LABEL_INVALID_LETTER);
                break;
            }
        }
    }

    *aPortNo = sRetValue;

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_INVALID_LETTER);

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC ulnDrvConnReceivePropertySetRes(ulnFnContext *aFnContext, ulnPtContext *aPtContext)
{
    ulnDbc     *sDbc;
    acp_time_t  sTimeout;

    //PROJ-1645 UL-FailOver
    //STF�������� Function Context�� Object type��
    //statement �ϼ� �ִ�.
    ULN_FNCONTEXT_GET_DBC(aFnContext,sDbc);

    ACI_TEST( sDbc == NULL );           //BUG-28623 [CodeSonar]Null Pointer Dereference

    /*
     * Ÿ�Ӿƿ� ����
     */
    sTimeout = acpTimeFrom(ulnDbcGetLoginTimeout(sDbc), 0);

    if (cmiGetLinkImpl(&(aPtContext->mCmiPtContext)) == CMI_LINK_IMPL_IPCDA)
    {
        ACI_TEST(ulnReadProtocolIPCDA(aFnContext, aPtContext, sTimeout) != ACI_SUCCESS);
    }
    else
    {
        ACI_TEST(ulnReadProtocol(aFnContext, aPtContext, sTimeout) != ACI_SUCCESS);
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC ulnDrvConnInitialPropertySet(ulnFnContext *aFnContext, ulnPtContext *aPtContext)
{
    ulnDbc           *sDbc;
    acp_char_t        sClientType[20];
    acp_uint64_t      sPid;
    const acp_char_t *sVersion = IDU_ALTIBASE_VERSION_STRING;

    //PROJ-1645 UL-FailOver
    //STF�������� Function Context�� Object type��
    //statement �ϼ� �ִ�.
    ULN_FNCONTEXT_GET_DBC(aFnContext,sDbc);

    ACI_TEST( sDbc == NULL );           //BUG-28623 [CodeSonar]Null Pointer Dereference

    acpSnprintf(sClientType,
                    ACI_SIZEOF(sClientType),
#ifdef ENDIAN_IS_BIG_ENDIAN
# ifdef COMPILE_64BIT
                    "%s-64BE",
# else
                    "%s-32BE",
# endif
#else
# ifdef COMPILE_64BIT
                    "%s-64LE",
# else
                    "%s-32LE",
# endif
#endif
                    ulcGetClientType());

    /*
     * PropertySet Request ����
     */
    ACI_TEST(ulnWritePropertySetV2REQ(aFnContext,
                                      aPtContext,
                                      ULN_PROPERTY_CLIENT_PACKAGE_VERSION,
                                      (void *)sVersion)
             != ACI_SUCCESS);

    ACI_TEST(ulnWritePropertySetV2REQ(aFnContext,
                                      aPtContext,
                                      ULN_PROPERTY_CLIENT_PROTOCOL_VERSION,
                                      (void *)(&cmProtocolVersion))
             != ACI_SUCCESS);

    sPid = acpProcGetSelfID();

    ACI_TEST(ulnWritePropertySetV2REQ(aFnContext,
                                      aPtContext,
                                      ULN_PROPERTY_CLIENT_PID,
                                      (void *)(&sPid))
             != ACI_SUCCESS);

    /* APP_INFO set before
     * BUG-28866 : Logging�� ���� APP_INFO��
     * ���� ���� ���� ó��
     */
    
    if (sDbc->mAppInfo != NULL)
    {
        ACI_TEST(ulnWritePropertySetV2REQ(aFnContext,
                                          aPtContext,
                                          ULN_PROPERTY_APP_INFO,
                                          (void *)sDbc->mAppInfo) != ACI_SUCCESS);
    }
    ACI_TEST(ulnWritePropertySetV2REQ(aFnContext,
                                      aPtContext,
                                      ULN_PROPERTY_CLIENT_TYPE,
                                      (void *)sClientType)
             != ACI_SUCCESS);

    ACI_TEST(ulnWritePropertySetV2REQ(aFnContext,
                                      aPtContext,
                                      ULN_PROPERTY_NLS,
                                      (void *)ulnDbcGetNlsLangString(sDbc))
             != ACI_SUCCESS);

    ACI_TEST(ulnWritePropertyGetREQ(aFnContext,
                                    aPtContext,
                                    ULN_PROPERTY_ENDIAN) != ACI_SUCCESS);

    /*  PROJ-1579 NCHAR */
    /* ULN_PROPERTY_NLS_NCHAR_LITERAL_REPLACE */
    ACI_TEST(ulnWritePropertySetV2REQ(aFnContext,
                                      aPtContext,
                                      ULN_PROPERTY_NLS_NCHAR_LITERAL_REPLACE,
                                      (void*)(acp_slong_t)sDbc->mNlsNcharLiteralReplace) != ACI_SUCCESS);

    /* PROJ-2683 */
    /* ULN_PROPERTY_SHARD_LINKER_TYPE */
    if ( sDbc->mShardDbcCxt.mShardLinkerType > 0 )
    {
        ACI_TEST(ulnWritePropertySetV2REQ(aFnContext,
                                          aPtContext,
                                          ULN_PROPERTY_SHARD_LINKER_TYPE,
                                          (void*)(acp_slong_t)sDbc->mShardDbcCxt.mShardLinkerType) != ACI_SUCCESS);
    }

    /* PROJ-2683 */
    /* ULN_PROPERTY_SHARD_NODE_NAME */
    if ( sDbc->mShardDbcCxt.mShardTargetDataNodeName[0] != '\0' )
    {
        ACI_TEST(ulnWritePropertySetV2REQ(aFnContext,
                                          aPtContext,
                                          ULN_PROPERTY_SHARD_NODE_NAME,
                                          (void*)sDbc->mShardDbcCxt.mShardTargetDataNodeName) != ACI_SUCCESS);
    }

    /*  PROJ-1579 NCHAR */
    /* ULN_PROPERTY_NLS_CHARACTERSET */
    ACI_TEST(ulnWritePropertyGetREQ(aFnContext,
                                    aPtContext,
                                    ULN_PROPERTY_NLS_CHARACTERSET) != ACI_SUCCESS);

    /*  PROJ-1579 NCHAR */
    /* ULN_PROPERTY_NLS_NCHAR_CHARACTERSET */
    ACI_TEST(ulnWritePropertyGetREQ(aFnContext,
                                    aPtContext,
                                    ULN_PROPERTY_NLS_NCHAR_CHARACTERSET) != ACI_SUCCESS);

    // PROJ-1681
    if ( sDbc->mWcharCharsetLangModule == NULL )
    {
#ifdef SQL_WCHART_CONVERT
        ACI_TEST(mtlModuleByName((const mtlModule **)&(sDbc->mWcharCharsetLangModule),
                                   "UTF32",
                                   5)
                 != ACI_SUCCESS);
#else
        ACI_TEST(mtlModuleByName((const mtlModule **)&(sDbc->mWcharCharsetLangModule),
                                   "UTF16",
                                   5)
                 != ACI_SUCCESS);
#endif
    }

    /* DateFormat does Set if it prepered otherwise get */
    if (sDbc->mDateFormat != NULL)
    {
        ACI_TEST(ulnWritePropertySetV2REQ(aFnContext,
                                          aPtContext,
                                          ULN_PROPERTY_DATE_FORMAT,
                                          (void *)sDbc->mDateFormat)
                 != ACI_SUCCESS);
    }
    else
    {
        ACI_TEST(ulnWritePropertyGetREQ(aFnContext,
                                        aPtContext,
                                        ULN_PROPERTY_DATE_FORMAT) != ACI_SUCCESS);
    }

    if ( sDbc->mTimezoneString != NULL )
    {
        ACI_TEST( ulnWritePropertySetV2REQ( aFnContext,
                                            aPtContext,
                                            ULN_PROPERTY_TIME_ZONE,
                                            (void *)sDbc->mTimezoneString )
                  != ACI_SUCCESS);
    }
    else
    {
        /* nothing to do. */
    }

    /*  ULN_PROPERTY_AUTOCOMMIT             */
    if (sDbc->mAttrAutoCommit != SQL_UNDEF)
    {
        ACI_TEST(ulnWritePropertySetV2REQ(aFnContext,
                                          aPtContext,
                                          ULN_PROPERTY_AUTOCOMMIT,
                                          (void*)(acp_slong_t)sDbc->mAttrAutoCommit) != ACI_SUCCESS);
    }
    else
    {
        ACI_TEST(ulnWritePropertyGetREQ(aFnContext,
                                        aPtContext,
                                        ULN_PROPERTY_AUTOCOMMIT) != ACI_SUCCESS);
    }

    /*  ULN_PROPERTY_EXPLAIN_PLAN           */
    if (sDbc->mAttrExplainPlan != SQL_UNDEF)
    {
        ACI_TEST(ulnWritePropertySetV2REQ(aFnContext,
                                          aPtContext,
                                          ULN_PROPERTY_EXPLAIN_PLAN,
                                          (void*)(acp_slong_t)sDbc->mAttrExplainPlan) != ACI_SUCCESS);
    }
    else
    {
        ACI_TEST(ulnWritePropertyGetREQ(aFnContext,
                                        aPtContext,
                                        ULN_PROPERTY_EXPLAIN_PLAN) != ACI_SUCCESS);
    }


    /*  ULN_PROPERTY_OPTIMIZER_MODE         */
    if (sDbc->mAttrOptimizerMode != ACP_UINT32_MAX)
    {
        ACI_TEST(ulnWritePropertySetV2REQ(aFnContext,
                                          aPtContext,
                                          ULN_PROPERTY_OPTIMIZER_MODE,
                                          (void*)(acp_slong_t)sDbc->mAttrOptimizerMode) != ACI_SUCCESS);
    }
    else
    {
        ACI_TEST(ulnWritePropertyGetREQ(aFnContext,
                                        aPtContext,
                                        ULN_PROPERTY_OPTIMIZER_MODE) != ACI_SUCCESS);
    }


    /*  ULN_PROPERTY_STACK_SIZE             */
    if (sDbc->mAttrStackSize != ACP_UINT32_MAX)
    {
        ACI_TEST(ulnWritePropertySetV2REQ(aFnContext,
                                          aPtContext,
                                          ULN_PROPERTY_STACK_SIZE,
                                          (void*)(acp_slong_t)sDbc->mAttrStackSize) != ACI_SUCCESS);
    }
    else
    {
        ACI_TEST(ulnWritePropertyGetREQ(aFnContext,
                                        aPtContext,
                                        ULN_PROPERTY_STACK_SIZE) != ACI_SUCCESS);
    }

    /* BUG-32885 Timeout for DDL must be distinct to query_timeout or utrans_timeout */
    /*  ULN_PROPERTY_QUERY_TIMEOUT          */
    if (sDbc->mAttrQueryTimeout != ULN_QUERY_TMOUT_DEFAULT)
    {
        ACI_TEST(ulnWritePropertySetV2REQ(aFnContext,
                                          aPtContext,
                                          ULN_PROPERTY_QUERY_TIMEOUT,
                                          (void*)(acp_slong_t)sDbc->mAttrQueryTimeout) != ACI_SUCCESS);
    }
    else
    {
        ACI_TEST(ulnWritePropertyGetREQ(aFnContext,
                                        aPtContext,
                                        ULN_PROPERTY_QUERY_TIMEOUT) != ACI_SUCCESS);
    }

    /* BUG-32885 Timeout for DDL must be distinct to query_timeout or utrans_timeout */
    /*  ULN_PROPERTY_DDL_TIMEOUT          */
    if (sDbc->mAttrDdlTimeout != ULN_DDL_TMOUT_DEFAULT)
    {
        ACI_TEST(ulnWritePropertySetV2REQ(aFnContext,
                                          aPtContext,
                                          ULN_PROPERTY_DDL_TIMEOUT,
                                          (void*)(acp_slong_t)sDbc->mAttrDdlTimeout) != ACI_SUCCESS);
    }
    else
    {
        ACI_TEST(ulnWritePropertyGetREQ(aFnContext,
                                        aPtContext,
                                        ULN_PROPERTY_DDL_TIMEOUT) != ACI_SUCCESS);
    }

    /*  ULN_PROPERTY_UTRANS_TIMEOUT         */
    if (sDbc->mAttrUtransTimeout != ACP_UINT32_MAX)
    {
        ACI_TEST(ulnWritePropertySetV2REQ(aFnContext,
                                          aPtContext,
                                          ULN_PROPERTY_UTRANS_TIMEOUT,
                                          (void*)(acp_slong_t)sDbc->mAttrUtransTimeout) != ACI_SUCCESS);
    }
    else
    {
        ACI_TEST(ulnWritePropertyGetREQ(aFnContext,
                                        aPtContext,
                                        ULN_PROPERTY_UTRANS_TIMEOUT) != ACI_SUCCESS);
    }


    /*  ULN_PROPERTY_FETCH_TIMEOUT          */
    if (sDbc->mAttrFetchTimeout != ACP_UINT32_MAX)
    {
        ACI_TEST(ulnWritePropertySetV2REQ(aFnContext,
                                          aPtContext,
                                          ULN_PROPERTY_FETCH_TIMEOUT,
                                          (void*)(acp_slong_t)sDbc->mAttrFetchTimeout) != ACI_SUCCESS);
    }
    else
    {
        ACI_TEST(ulnWritePropertyGetREQ(aFnContext,
                                        aPtContext,
                                        ULN_PROPERTY_FETCH_TIMEOUT)     != ACI_SUCCESS);
    }

    /*  ULN_PROPERTY_IDLE_TIMEOUT           */
    if (sDbc->mAttrIdleTimeout != ACP_UINT32_MAX)
    {
        ACI_TEST(ulnWritePropertySetV2REQ(aFnContext,
                                          aPtContext,
                                          ULN_PROPERTY_IDLE_TIMEOUT,
                                          (void*)(acp_slong_t)sDbc->mAttrIdleTimeout)  != ACI_SUCCESS);
    }
    else
    {
        ACI_TEST(ulnWritePropertyGetREQ(aFnContext,
                                        aPtContext,
                                        ULN_PROPERTY_IDLE_TIMEOUT)      != ACI_SUCCESS);
    }

    /*  ULN_PROPERTY_HEADER_DISPLAY_MODE    */
    if (sDbc->mAttrHeaderDisplayMode != ACP_UINT32_MAX)
    {
        ACI_TEST(ulnWritePropertySetV2REQ(aFnContext,
                                          aPtContext,
                                          ULN_PROPERTY_HEADER_DISPLAY_MODE,
                                          (void*)(acp_slong_t)sDbc->mAttrHeaderDisplayMode)!= ACI_SUCCESS);
    }
    else
    {
        ACI_TEST(ulnWritePropertyGetREQ(aFnContext,
                                        aPtContext,
                                        ULN_PROPERTY_HEADER_DISPLAY_MODE)   != ACI_SUCCESS);
    }

    /*  BUG-31144 MAX_STATEMENTS_PER_SESSION */
    if (sDbc->mAttrMaxStatementsPerSession != ACP_UINT32_MAX)
    {
        ACI_TEST(ulnWritePropertySetV2REQ(aFnContext,
                                          aPtContext,
                                          ULN_PROPERTY_MAX_STATEMENTS_PER_SESSION,
                                          (void*)(acp_slong_t)sDbc->mAttrMaxStatementsPerSession) != ACI_SUCCESS);
    }

    /*  BUG-31390 Failover info for v$session */
    if (sDbc->mFailoverSource != NULL)
    {
        ACI_TEST(ulnWritePropertySetV2REQ(aFnContext,
                                          aPtContext,
                                          ULN_PROPERTY_FAILOVER_SOURCE,
                                          (void *)sDbc->mFailoverSource) != ACI_SUCCESS);
    }

    // fix BUG-18971
    ACI_TEST(ulnWritePropertyGetREQ(aFnContext,
                                    aPtContext,
                                    ULN_PROPERTY_SERVER_PACKAGE_VERSION) != ACI_SUCCESS);

    /* PROJ-2047 Strengthening LOB - LOBCACHE */
    if (sDbc->mAttrLobCacheThreshold != ACP_UINT32_MAX)
    {
        ACI_TEST(ulnWritePropertySetV2REQ(aFnContext,
                                          aPtContext,
                                          ULN_PROPERTY_LOB_CACHE_THRESHOLD,
                                          (void *)(acp_slong_t)
                                          sDbc->mAttrLobCacheThreshold)
                 != ACI_SUCCESS);
    }
    else
    {
        ACI_TEST(ulnWritePropertyGetREQ(aFnContext,
                                        aPtContext,
                                        ULN_PROPERTY_LOB_CACHE_THRESHOLD)
                 != ACI_SUCCESS);
    }

    /*
     * BUG-29148 [CodeSonar]Null Pointer Dereference
     */
    ACE_ASSERT(aPtContext->mCmiPtContext.mModule != NULL);

    /* BUG-39817 */
    if (sDbc->mAttrTxnIsolation != ULN_ISOLATION_LEVEL_DEFAULT)
    {
        ACI_TEST(ulnWritePropertySetV2REQ(aFnContext,
                                          aPtContext,
                                          ULN_PROPERTY_ISOLATION_LEVEL,
                                          (void*)(acp_slong_t)sDbc->mAttrTxnIsolation) != ACI_SUCCESS);
    }
    else
    {
        ACI_TEST(ulnWritePropertyGetREQ(aFnContext,
                                        aPtContext,
                                        ULN_PROPERTY_ISOLATION_LEVEL) != ACI_SUCCESS);
    }

    /* BUG-40521 */
    if (sDbc->mAttrDeferredPrepare == SQL_UNDEF)
    {
        /* Set mAttrDeferredPrepare to the default value */
        sDbc->mAttrDeferredPrepare = gUlnConnAttrTable[ULN_CONN_ATTR_DEFERRED_PREPARE].mDefValue;
    }
    else
    {
        /* Since this attribute is a client-only attribute, 
         * it doesn't need to be sent to the server to set its value. */
    }

    if (sDbc->mAttrSslVerify == SQL_UNDEF)
    {
        /* Set mAttrSslVerify to the default value */
        sDbc->mAttrSslVerify = gUlnConnAttrTable[ULN_CONN_ATTR_SSL_VERIFY].mDefValue;
    }
    else
    {
        /* Since this attribute is a client-only attribute, 
         * it doesn't need to be sent to the server to set its value. */
    }

    /*
     * ��Ŷ ����
     */
    ACI_TEST(ulnFlushProtocol(aFnContext, aPtContext) != ACI_SUCCESS);

    /*
     * ���� ���� ���
     */
    ACI_TEST(ulnDrvConnReceivePropertySetRes(aFnContext, aPtContext) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static acp_uint16_t ulnSafeStrLen16(acp_char_t *aStr)
{
    acp_uint16_t sLen = 0;
    if (aStr != NULL)
    {
        sLen = acpCStrLen(aStr, ACP_UINT16_MAX);
    }
    return sLen;
}

static ACI_RC ulnDrvConnSendConnectReq(ulnFnContext *aFnContext, ulnPtContext *aPtContext)
{
    cmiProtocolContext *sCtx = &(aPtContext->mCmiPtContext);
    ulnDbc             *sDbc;
    cmiProtocol         sPacket;
    ulnPrivilege        sPrivilege;
    acp_char_t         *sDbmsName;
    acp_uint16_t        sDbmsNameLen;
    acp_char_t         *sUserName;
    acp_uint16_t        sUserNameLen;
    acp_char_t         *sPassword;
    acp_uint16_t        sPasswordLen;
    acp_uint16_t        sMode;
    acp_uint16_t        sOrgWriteCursor = CMI_GET_CURSOR(sCtx);
    acp_uint8_t         sState          = 0;

    //PROJ-1645 UL-FailOver
    //STF�������� Function Context�� Object type��
    //statement �ϼ� �ִ�.
    ULN_FNCONTEXT_GET_DBC(aFnContext,sDbc);

    ACI_TEST( sDbc == NULL );           //BUG-28623 [CodeSonar]Null Pointer Dereference

    sDbmsName = ulnDbcGetCurrentCatalog(sDbc);
    sUserName = ulnDbcGetUserName(sDbc);
    sPassword = ulnDbcGetPassword(sDbc);
    sDbmsNameLen = ulnSafeStrLen16(sDbmsName);
    sUserNameLen = ulnSafeStrLen16(sUserName);
    sPasswordLen = ulnSafeStrLen16(sPassword);

    // bug-19279 remote sysdba enable
    // PRIVILEGE=SYSDBA : tcp/unix
    sPrivilege = ulnDbcGetPrivilege(sDbc);
    if (sPrivilege == ULN_PRIVILEGE_SYSDBA)
    {
        sMode = CMP_DB_CONNECT_MODE_SYSDBA;
    }
    else
    {
        sMode = CMP_DB_CONNECT_MODE_NORMAL;
    }

    sPacket.mOpID = CMP_OP_DB_ConnectEx;
    
    CMI_WRITE_CHECK(sCtx, 9 + sDbmsNameLen + sUserNameLen + sPasswordLen);
    sState = 1;

    /* PROJ-2177: CliendID ������ �ʿ��� SessionID�� �޾ƿ��� ���� Connect�� ���� */
    CMI_WOP(sCtx, CMP_OP_DB_ConnectEx);
    CMI_WR2(sCtx, &sDbmsNameLen);
    CMI_WCP(sCtx, sDbmsName, sDbmsNameLen);
    CMI_WR2(sCtx, &sUserNameLen);
    CMI_WCP(sCtx, sUserName, sUserNameLen);
    CMI_WR2(sCtx, &sPasswordLen);
    CMI_WCP(sCtx, sPassword, sPasswordLen);
    CMI_WR2(sCtx, &sMode);

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

static ACI_RC ulnDrvConnLogin(ulnFnContext *aFnContext, ulnPtContext *aPtContext)
{
    /*
     * Connect Request ����
     */
    ACI_TEST(ulnDrvConnSendConnectReq(aFnContext, aPtContext) != ACI_SUCCESS);


    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

//PROJ-1645 UL-FailOver.
static ACI_RC ulnDrvConnOrganizeConnectArgTcp(ulnFnContext *aFnContext)
{
    ulnFailoverServerInfo *sServer         = NULL;
    ulnDbc                *sDbc            = aFnContext->mHandle.mDbc;
    /* proj-1538 ipv6: 127.0.0.1 -> localhost */
    acp_char_t            *sLocalHost      = (acp_char_t *)"localhost";
    acp_sint32_t           sPortNumber     = 0;
    acp_char_t            *sPortNoEnvValue = NULL;

    /*
     * Hostname ����
     */
    if(sDbc->mAlternateServers == NULL)
    {
        if (ulnDbcGetDsnString(sDbc) == NULL)
        {
            /*
             * Note : ������ idlOS::malloc ���ϰ�, uluMemory �� alloc �̹Ƿ� ���߿�
             *        DBC �� destroy �� �� �޸𸮵� �Բ� ���� �ȴ�.
             *        ����, DBC �� destroy �� �� mDSNString �� ���� free �� �� �ʿ� ����.
             *        ��, Constant string �� ����Ű�� �����Ͱ� ���� �����ϴ�.
             */
            ulnSetConnAttrById(aFnContext,
                               ULN_CONN_ATTR_DSN,
                               sLocalHost,
                               acpCStrLen(sLocalHost, ACP_SINT32_MAX));

            /*
             * 01S02
             *
             * Note : DSN �� connection string �� ���� �� SQL_SUCCESS_WITH_INFO �� �ƴ϶�
             *        SQL_SUCCESS �� �����ؾ� �Ѵٰ� ��.
             *        �� �׷����� �� �𸣰����� �ƹ�ư, �׷��� �ؾ� �Ѵٰ� ��.
             *        �׷��� �Ʒ��� ���� �ּ�ó����.
             */
            // ulnError(aFnContext, ulERR_IGNORE_TCP_HOSTNAME_NOT_SET);
        }

        if (ulnDbcGetPortNumber(sDbc) == 0)
        {
            sPortNoEnvValue = NULL;

            /*
             * port number �� ���� 0 �̶�� ���� connection string �� port no ��
             * ���õ��� �ʾҴٴ� �̾߱��̴�.
             *
             * connection string �� port no �� ���� ��,
             *
             * ȯ�溯�� ALTIBASE_PORT_NO �� ���õ��� �ʾ��� �� ������ ����,
             *                              ���õǾ� ������ �� ������ �Ѵ�.
             */
            ACI_TEST_RAISE(acpEnvGet("ALTIBASE_PORT_NO", &sPortNoEnvValue) != ACP_RC_SUCCESS,
                           LABEL_PORT_NO_NOT_SET);

            /*
             * 32 ��Ʈ int �� �ִ밪 : 4294967295 : 10�ڸ�
             */
            ACI_TEST_RAISE(ulnDrvConnStrToInt(sPortNoEnvValue,
                                              acpCStrLen(sPortNoEnvValue, 10),
                                              &sPortNumber) != ACI_SUCCESS,
                           LABEL_INVALID_PORT_NO);

            ulnDbcSetPortNumber(sDbc, sPortNumber);

            /*
             * PORT_NO �� ���� ��Ʈ���� ���� �� ALTIBASE_PORT_NO ȯ�溯���� Ȯ���Ͽ���
             * ���� ��� SQL_SUCCESS_WITH_INFO �� �ƴ� SQL_SUCCESS �� �����ϰ� �������ؾ� �Ѵٰ�
             * �Ѵ�.
             */
            // ulnError(aFnContext, ulERR_IGNORE_PORT_NO_NOT_SET, sPortNumber);
        }

        /* ------------------------------------------------
         * SQLCLI������ DSN�� Host�� �ּ� ��, IP Address�� ����ϰ�,
         *
         * ODBC������ "Server"�� Ip Address�� ����Ѵ�.
         * �̷��� ȥ���� �����ϰ�, ����� �����ϴ� ����
         * �����ϱ� ����,
         *  1. HostName�� �켱 �˻��Ѵ�.
         *  2. ���� �� ���� NULL�̶��, SQLCLI��� �ǹ��̹Ƿ�,
         *     Dsn�� String�� ����Ѵ�. �� String���� �ݵ�� Ip Address��
         *     ��� ���� ���̴�.
         *  3. ���� �� ���� NULL�� �ƴ϶��,
         *     �� ���α׷��� ODBC�� ����̹Ƿ� (ProfileString�� ���� ����)
         *     �� ���� ���ӽ� ����Ѵ�.
         *
         *  sqlcli���� ����ڰ� DSN=abcd;ServerName=192.168.3.1 �̷��� ���� ��쿡��
         *  ���� ������ ���� ������ ������ �� �ִ�.
         *  odbc ������ ������ ProfileString���� �����ǹǷ� ������ ����.
         * ----------------------------------------------*/

        if (ulnDbcGetHostNameString(sDbc) == NULL)
        {
            ulnDbcGetConnectArg(sDbc)->mTCP.mAddr = ulnDbcGetDsnString(sDbc);
        }
        else
        {
            ulnDbcGetConnectArg(sDbc)->mTCP.mAddr = ulnDbcGetHostNameString(sDbc);
        }

        ulnDbcGetConnectArg(sDbc)->mTCP.mPort = ulnDbcGetPortNumber(sDbc);
    }
    else
    {
        //PROJ-1645 UL-FailOver.
        sServer = ulnDbcGetCurrentServer(sDbc);
        ulnDbcGetConnectArg(sDbc)->mTCP.mAddr = sServer->mHost;
        ulnDbcGetConnectArg(sDbc)->mTCP.mPort = sServer->mPort;
    }
    /* proj-1538 ipv6 */
    ulnDbcGetConnectArg(sDbc)->mTCP.mPreferIPv6 =
        (sDbc->mAttrPreferIPv6 == ACP_TRUE)? 1: 0;

    //fix BUG-26048 Embeded���� ConnType=5�� �־�����
    //  SYSDBA������ �ȵ�.
    if(ulnDbcGetConnType(sDbc) == ULN_CONNTYPE_INVALID)
    {
        ulnDbcSetConnType(sDbc, ULN_CONNTYPE_TCP);
    }

    /* BUG-44271 */
    ulnDbcGetConnectArg(sDbc)->mTCP.mBindAddr = ulnDbcGetSockBindAddr(sDbc);

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_INVALID_PORT_NO)
    {
        /*
         * SQLSTATE �� ���� �ұ� _--
         */
        ulnError(aFnContext, ulERR_ABORT_INVALID_ALTIBASE_PORT_NO, sPortNoEnvValue);
    }

    ACI_EXCEPTION(LABEL_PORT_NO_NOT_SET)
    {
        /*
         * BUGBUG : �����ڵ�
         */
        ulnError(aFnContext, ulERR_ABORT_PORT_NO_ALTIBASE_PORT_NO_NOT_SET);
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC ulnDrvConnOrganizeConnectArgSsl(ulnFnContext *aFnContext)
{
    ulnFailoverServerInfo *sServer = NULL;
    ulnDbc                *sDbc       = aFnContext->mHandle.mDbc;
    /* proj-1538 ipv6: 127.0.0.1 -> localhost */
    acp_char_t            *sLocalHost = (acp_char_t *)"localhost";
    acp_sint32_t           sPortNumber = 0;
    acp_char_t            *sPortNoEnvValue = NULL;

    /*
     * Hostname ����
     */
    if (sDbc->mAlternateServers == NULL)
    {
        if (ulnDbcGetDsnString(sDbc) == NULL)
        {
            /*
             * Note : ������ idlOS::malloc ���ϰ�, uluMemory �� alloc �̹Ƿ� ���߿�
             *        DBC �� destroy �� �� �޸𸮵� �Բ� ���� �ȴ�.
             *        ����, DBC �� destroy �� �� mDSNString �� ���� free �� �� �ʿ� ����.
             *        ��, Constant string �� ����Ű�� �����Ͱ� ���� �����ϴ�.
             */
            ulnSetConnAttrById(aFnContext,
                               ULN_CONN_ATTR_DSN,
                               sLocalHost,
                               acpCStrLen(sLocalHost, ACP_SINT32_MAX));

            /*
             * 01S02
             *
             * Note : DSN �� connection string �� ���� �� SQL_SUCCESS_WITH_INFO �� �ƴ϶�
             *        SQL_SUCCESS �� �����ؾ� �Ѵٰ� ��.
             *        �� �׷����� �� �𸣰����� �ƹ�ư, �׷��� �ؾ� �Ѵٰ� ��.
             *        �׷��� �Ʒ��� ���� �ּ�ó����.
             */
        }
        else
        {
            /* The DSN has been set. */
        }

        if (ulnDbcGetPortNumber(sDbc) == 0)
        {
            sPortNoEnvValue = NULL;

            /*
             * port number �� ���� 0 �̶�� ���� connection string �� port no ��
             * ���õ��� �ʾҴٴ� �̾߱��̴�.
             *
             * connection string �� port no �� ���� ��,
             *
             * ȯ�溯�� ALTIBASE_PORT_NO �� ���õ��� �ʾ��� �� ������ ����,
             *                              ���õǾ� ������ �� ������ �Ѵ�.
             */
            ACI_TEST_RAISE(acpEnvGet("ALTIBASE_PORT_NO", &sPortNoEnvValue) != ACP_RC_SUCCESS,
                           LABEL_PORT_NO_NOT_SET);

            /*
             * 32 ��Ʈ int �� �ִ밪 : 4294967295 : 10�ڸ�
             */
            ACI_TEST_RAISE(ulnDrvConnStrToInt(sPortNoEnvValue,
                                              acpCStrLen(sPortNoEnvValue, 10),
                                              &sPortNumber) != ACI_SUCCESS,
                           LABEL_INVALID_PORT_NO);

            ulnDbcSetPortNumber(sDbc, sPortNumber);

            /*
             * PORT_NO �� ���� ��Ʈ���� ���� �� ALTIBASE_PORT_NO ȯ�溯���� Ȯ���Ͽ���
             * ���� ��� SQL_SUCCESS_WITH_INFO �� �ƴ� SQL_SUCCESS �� �����ϰ� �������ؾ� �Ѵٰ�
             * �Ѵ�.
             */
        }
        else
        {
            /* The SSL port number has been set. */
        }

        /* ------------------------------------------------
         * SQLCLI������ DSN�� Host�� �ּ� ��, IP Address�� ����ϰ�,
         *
         * ODBC������ "Server"�� Ip Address�� ����Ѵ�.
         * �̷��� ȥ���� �����ϰ�, ����� �����ϴ� ����
         * �����ϱ� ����,
         *  1. HostName�� �켱 �˻��Ѵ�.
         *  2. ���� �� ���� NULL�̶��, SQLCLI��� �ǹ��̹Ƿ�,
         *     Dsn�� String�� ����Ѵ�. �� String���� �ݵ�� Ip Address��
         *     ��� ���� ���̴�.
         *  3. ���� �� ���� NULL�� �ƴ϶��,
         *     �� ���α׷��� ODBC�� ����̹Ƿ� (ProfileString�� ���� ����)
         *     �� ���� ���ӽ� ����Ѵ�.
         *
         *  sqlcli���� ����ڰ� DSN=abcd;ServerName=192.168.3.1 �̷��� ���� ��쿡��
         *  ���� ������ ���� ������ ������ �� �ִ�.
         *  odbc ������ ������ ProfileString���� �����ǹǷ� ������ ����.
         * ----------------------------------------------*/

        if (ulnDbcGetHostNameString(sDbc) == NULL)
        {
            ulnDbcGetConnectArg(sDbc)->mSSL.mAddr = ulnDbcGetDsnString(sDbc);
        }
        else
        {
            ulnDbcGetConnectArg(sDbc)->mSSL.mAddr = ulnDbcGetHostNameString(sDbc);
        }

        ulnDbcGetConnectArg(sDbc)->mSSL.mPort = ulnDbcGetPortNumber(sDbc);
    }
    else
    {
        //PROJ-1645 UL-FailOver.
        sServer = ulnDbcGetCurrentServer(sDbc);
        ulnDbcGetConnectArg(sDbc)->mSSL.mAddr = sServer->mHost;
        ulnDbcGetConnectArg(sDbc)->mSSL.mPort = sServer->mPort;
    }

    /* proj-1538 ipv6 */
    ulnDbcGetConnectArg(sDbc)->mSSL.mPreferIPv6 =
        (sDbc->mAttrPreferIPv6 == ACP_TRUE)? 1: 0;

    //fix BUG-26048 Embeded���� ConnType=6�� �־�����
    //  SYSDBA������ �ȵ�.
    if (ulnDbcGetConnType(sDbc) == ULN_CONNTYPE_INVALID)
    {
        ulnDbcSetConnType(sDbc, ULN_CONNTYPE_SSL);
    }
    else
    {
        /* The Connection type is valid. */
    }

    /* Set certificate info for SSL communication */
    ulnDbcGetConnectArg(sDbc)->mSSL.mCert   = ulnDbcGetSslCert(sDbc);
    ulnDbcGetConnectArg(sDbc)->mSSL.mCa     = ulnDbcGetSslCa(sDbc);
    ulnDbcGetConnectArg(sDbc)->mSSL.mCaPath = ulnDbcGetSslCaPath(sDbc);
    ulnDbcGetConnectArg(sDbc)->mSSL.mCipher = ulnDbcGetSslCipher(sDbc);
    ulnDbcGetConnectArg(sDbc)->mSSL.mKey    = ulnDbcGetSslKey(sDbc);
    ulnDbcGetConnectArg(sDbc)->mSSL.mVerify = ulnDbcGetSslVerify(sDbc);

    /* BUG-44530 SSL���� ALTIBASE_SOCK_BIND_ADDR ���� */
    ulnDbcGetConnectArg(sDbc)->mSSL.mBindAddr = ulnDbcGetSockBindAddr(sDbc);

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_INVALID_PORT_NO)
    {
        /*
         * SQLSTATE �� ���� �ұ� _--
         */
        ulnError(aFnContext, ulERR_ABORT_INVALID_ALTIBASE_PORT_NO, sPortNoEnvValue);
    }

    ACI_EXCEPTION(LABEL_PORT_NO_NOT_SET)
    {
        /*
         * BUGBUG : �����ڵ�
         */
        ulnError(aFnContext, ulERR_ABORT_PORT_NO_ALTIBASE_PORT_NO_NOT_SET);
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}
static ACI_RC ulnDrvConnOrganizeConnectArgUnix(ulnFnContext *aFnContext)
{
    acp_char_t *sHome;
    ulnDbc     *sDbc = aFnContext->mHandle.mDbc;

    /*
     * BUGBUG : ȯ�溯�� �̸��� ����� �� ���� �ƴ϶� #define ���� ���� �Ѵ�.
     */

    /*
     * BUGBUG : �̰��� sDbc->mDsnString ���� ���� �;� ���� �ʳ�?...
     */
    if (acpEnvGet("ALTIBASE_HOME", &sHome) != ACP_RC_SUCCESS)
    {
        ACI_RAISE(LABEL_ENV_NOT_SET);
    }
    else
    {
        /* BUG-35332 The socket files can be moved */
        ACE_DASSERT(sDbc->mUnixdomainFilepath[0] != '\0');
        ulnDbcGetConnectArg(sDbc)->mUNIX.mFilePath = sDbc->mUnixdomainFilepath;
    }

    if (ulnDbcGetPortNumber(sDbc) != 0)
    {
        /*
         * 01S00
         *
         * Note : UNIX domain ������ �� PORT_NO �� �����Ǿ� �־ SQL_SUCCESS �� �����ؾ� ��
         *        �ٰ� ��.
         *        �� �׷����� �� �𸣰����� �ƹ�ư, �׷��� �ؾ� �Ѵٰ� ��.
         *        �׷��� �Ʒ��� ���� �ּ�ó����.
         */
        // ACI_TEST(ulnError(aFnContext, ulERR_IGNORE_PORT_NO_IGNORED) != ACI_SUCCESS);
    }

    //fix BUG-26048 Embeded���� ConnType=5�� �־�����
    //  SYSDBA������ �ȵ�.
    if(ulnDbcGetConnType(sDbc) == ULN_CONNTYPE_INVALID)
    {
        ulnDbcSetConnType(sDbc, ULN_CONNTYPE_UNIX);
    }
    else
    {
        /* The connect type is valid. */
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_ENV_NOT_SET)
    {
        /*
         * HY024
         */
        ulnError(aFnContext, ulERR_ABORT_ALTIBASE_HOME_NOT_SET);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC ulnDrvConnOrganizeConnectArgIpc(ulnFnContext *aFnContext)
{
    acp_char_t *sHome;
    ulnDbc     *sDbc = aFnContext->mHandle.mDbc;

    ACI_TEST_RAISE(ulnDbcSetCmiLinkImpl(sDbc, CMI_LINK_IMPL_IPC) != ACI_SUCCESS,
                   LABEL_NOT_SUPPORTED_LINK);

    /*
     * BUGBUG : ȯ�溯�� �̸��� ����� �� ���� �ƴ϶� #define ���� ���� �Ѵ�.
     */

    /*
     * BUGBUG : �̰��� sDbc->mDsnString ���� ���� �;� ���� �ʳ�?...
     */
    if (acpEnvGet("ALTIBASE_HOME", &sHome) != ACP_RC_SUCCESS)
    {
        ACI_RAISE(LABEL_ENV_NOT_SET);
    }
    else
    {
        ACE_DASSERT(sDbc->mIpcFilepath[0] != '\0');
        ulnDbcGetConnectArg(sDbc)->mIPC.mFilePath = sDbc->mIpcFilepath;
    }

    if (ulnDbcGetPortNumber(sDbc) != 0)
    {
        /*
         * 01S00
         *
         * Note : UNIX domain ������ �� PORT_NO �� �����Ǿ� �־ SQL_SUCCESS �� �����ؾ� ��
         *        �ٰ� ��.
         *        �� �׷����� �� �𸣰����� �ƹ�ư, �׷��� �ؾ� �Ѵٰ� ��.
         *        �׷��� �Ʒ��� ���� �ּ�ó����.
         */
        // ACI_TEST(ulnError(aFnContext, ulERR_IGNORE_PORT_NO_IGNORED) != ACI_SUCCESS);
    }

    ulnDbcSetConnType(sDbc, ULN_CONNTYPE_IPC);

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_NOT_SUPPORTED_LINK)
    {
        ulnError(aFnContext,
                 ulERR_ABORT_CONNTYPE_NOT_SUPPORTED,
                 ulnDbcGetCmiLinkImpl(sDbc));
    }
    ACI_EXCEPTION(LABEL_ENV_NOT_SET)
    {
        /*
         * HY024
         */    
        ulnError(aFnContext, ulERR_ABORT_ALTIBASE_HOME_NOT_SET);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC ulnDrvConnOrganizeConnectArgIPCDA(ulnFnContext *aFnContext)
{
    acp_char_t *sHome;
    ulnDbc     *sDbc = aFnContext->mHandle.mDbc;

    ACI_TEST_RAISE(ulnDbcSetCmiLinkImpl(sDbc, CMI_LINK_IMPL_IPCDA) != ACI_SUCCESS,
                   LABEL_NOT_SUPPORTED_LINK);
    /*
     * BUGBUG : ȯ�溯�� �̸��� ����� �� ���� �ƴ϶� #define ���� ���� �Ѵ�.
     */

    /*
     * BUGBUG : �̰��� sDbc->mDsnString ���� ���� �;� ���� �ʳ�?...
     */
    if (acpEnvGet("ALTIBASE_HOME", &sHome) != ACP_RC_SUCCESS)
    {
        ACI_RAISE(LABEL_ENV_NOT_SET);
    }
    else
    {
        ACE_DASSERT(sDbc->mIPCDAFilepath[0] != '\0');
        ulnDbcGetConnectArg(sDbc)->mIPC.mFilePath = sDbc->mIPCDAFilepath;
    }

    ulnDbcSetConnType(sDbc, ULN_CONNTYPE_IPCDA);

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_NOT_SUPPORTED_LINK)
    {
        ulnError(aFnContext,
                 ulERR_ABORT_CONNTYPE_NOT_SUPPORTED,
                 ulnDbcGetCmiLinkImpl(sDbc));
    }
    ACI_EXCEPTION(LABEL_ENV_NOT_SET)
    {
        /*
         *HY024
         */
        ulnError(aFnContext, ulERR_ABORT_ALTIBASE_HOME_NOT_SET);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*
 * BUGBUG : ulnFnContext �� ������ �Լ��� �ǹ̰� �ణ �Ҹ�Ȯ�����µ�.. �ϴ� �״�� ����.
 */
static ACI_RC ulnDrvConnOrganizeConnectArg(ulnFnContext *aFnContext)
{
    ulnDbc *sDbc = aFnContext->mHandle.mDbc;

#ifdef DEBUG
    /* This routine is to test the entire ul test cases over SSL. */
    acp_char_t   *sValue = NULL;
    acp_sint32_t  sPortNumber = 0;

    if ((ulnDbcGetCmiLinkImpl(sDbc) == CMI_LINK_IMPL_TCP) ||
        (ulnDbcGetCmiLinkImpl(sDbc) == CMI_LINK_IMPL_INVALID))
    {
        if (acpEnvGet("ALTIBASE___SSL_TEST", &sValue) == ACP_RC_SUCCESS)
        {    
            if (acpCStrCmp(sValue, "true", 4) == 0)
            {
                ACI_TEST_RAISE(ulnDbcSetCmiLinkImpl(sDbc, CMI_LINK_IMPL_SSL) != ACI_SUCCESS, 
                               LABEL_NOT_SUPPORTED_LINK);

                ulnDbcSetConnType(sDbc, ULN_CONNTYPE_SSL);

                ACI_TEST_RAISE(acpEnvGet("ALTIBASE_SSL_PORT_NO", &sValue) != ACP_RC_SUCCESS,
                               LABEL_PORT_NO_NOT_SET);

                ACI_TEST_RAISE(ulnDrvConnStrToInt(sValue,
                                                  acpCStrLen(sValue, 10), 
                                                  &sPortNumber) != ACI_SUCCESS,
                               LABEL_INVALID_PORT_NO);

                ulnDbcSetPortNumber(sDbc, sPortNumber);
            }
            else
            {
                /* do nothing */
            }
        }    
        else 
        {    
            /* do nothing */
        }  
    }
    else
    {
        /* UNIX or IPC .. */
    }
#endif

    if (ulnDbcGetCmiLinkImpl(sDbc) == CMI_LINK_IMPL_INVALID)
    {
        ACI_TEST_RAISE(ulnDbcSetCmiLinkImpl(sDbc, CMI_LINK_IMPL_TCP) != ACI_SUCCESS,
                       LABEL_NOT_SUPPORTED_LINK);

        /*
         * 01s02
         *
         * BUGBUG : ��ó�� ����Ʈ Ÿ������ �ٲ㵵 SUCCESS_WITH_INFO �� �ƴ϶� SQL_SUCCESS ��
         *          ���� ��� ������ ������ ��������... -_-;
         */
        // ulnError(aFnContext, ulERR_IGNORE_CONNTYPE_NOT_SET);
    }

    switch(ulnDbcGetCmiLinkImpl(sDbc))
    {
        case CMI_LINK_IMPL_TCP:
            ACI_TEST(ulnDrvConnOrganizeConnectArgTcp(aFnContext) != ACI_SUCCESS);
            break;

        case CMI_LINK_IMPL_UNIX:
            ACI_TEST(ulnDrvConnOrganizeConnectArgUnix(aFnContext) != ACI_SUCCESS);
            break;

        case CMI_LINK_IMPL_IPC:
            ACI_TEST(ulnDrvConnOrganizeConnectArgIpc(aFnContext) != ACI_SUCCESS);
            break;

        case CMI_LINK_IMPL_SSL:
            ACI_TEST(ulnDrvConnOrganizeConnectArgSsl(aFnContext) != ACI_SUCCESS);
            break;
        
        case CMI_LINK_IMPL_IPCDA:
            ACI_TEST(ulnDrvConnOrganizeConnectArgIPCDA(aFnContext) != ACI_SUCCESS);
            break;

        default:
            /*
             * ����. ���� ũ�� �߸��Ǿ���.
             */
            ACE_ASSERT(0);
            break;
    }

    return ACI_SUCCESS;

#ifdef DEBUG
    ACI_EXCEPTION(LABEL_INVALID_PORT_NO)
    {
        ulnError(aFnContext, ulERR_ABORT_INVALID_ALTIBASE_PORT_NO, sValue);
    }
    ACI_EXCEPTION(LABEL_PORT_NO_NOT_SET)
    {
        ulnError(aFnContext, ulERR_ABORT_PORT_NO_ALTIBASE_PORT_NO_NOT_SET);
    }
#endif
    ACI_EXCEPTION(LABEL_NOT_SUPPORTED_LINK)
    {
        ulnError(aFnContext,
                 ulERR_ABORT_CONNTYPE_NOT_SUPPORTED,
                 ulnDbcGetCmiLinkImpl(sDbc));
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC ulnConnectCorePhysicalConn(ulnFnContext *aFnContext)
{
    ulnDbc     *sDbc = ulnFnContextGetDbc(aFnContext);
    acp_time_t  sTimeout;

    ACE_DASSERT(sDbc != NULL);

    /*
     * SetConnectAttr�̳� Connection string ���κ��� �������
     *      mPortNumber
     *      mCmiLinkImpl
     *      mDsnString
     * ���� ���� �̿��ؼ� cmiConnectArg �� �����Ѵ�.
     */

    ACI_TEST(ulnDrvConnOrganizeConnectArg(aFnContext) != ACI_SUCCESS);

    /*
     * cmiLink �� �����Ѵ�
     *
     * Note : ������ �ִ� mLink �����Ͱ� ����Ű�� �޸𸮴�
     *        ������ ��� ulnDbcAllocNewLink() �Լ��� �����Ѵ�)
     *
     * Note : ���⿡�� ������ cmiLink �� ulnDbcAllocNewLink(), ulnDbcFreeLink() Ȥ��
     *        ulnDbcDestroy() �� �ϸ� �˾Ƽ� �����ȴ�.
     */

    // fix BUG-28133
    // ulnDbcAllocNewLink() ���н� ���� ��ȯ
    ACI_TEST_RAISE(ulnDbcAllocNewLink(sDbc) != ACI_SUCCESS, LABEL_ALLOC_LINK_ERROR);

    /*
     * ���� �õ�
     */
    sTimeout = acpTimeFrom(ulnDbcGetLoginTimeout(sDbc), 0);

    ACI_TEST_RAISE( cmiConnect(&(sDbc->mPtContext.mCmiPtContext),
                               &(sDbc->mConnectArg),
                               sTimeout,
                               SO_LINGER),
                    LABEL_CM_ERR );

    /* PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning */
    ACI_TEST(ulnDbcSetSockRcvBufBlockRatio(aFnContext,
                                           sDbc->mAttrSockRcvBufBlockRatio)
            != ACI_SUCCESS);

    return ACI_SUCCESS;

    // fix BUG-28133
    // ulnDbcAllocNewLink() ���н� ���� ��ȯ
    ACI_EXCEPTION(LABEL_ALLOC_LINK_ERROR)
    {
        ulnError(aFnContext,
                 ulERR_FATAL_MEMORY_MANAGEMENT_ERROR,
                 "ulnConnectCorePhysicalConn");
    }

    ACI_EXCEPTION(LABEL_CM_ERR)
    {
        ulnErrHandleCmError(aFnContext, NULL);
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

void ulnClosePhysicalConn(ulnDbc *aDbc)
{
    ACI_TEST_RAISE(aDbc->mLink == NULL, SKIP_CLOSE);

    (void) cmiShutdownLink(aDbc->mLink, CMI_DIRECTION_RDWR);
    (void) cmiCloseLink(aDbc->mLink);

    ACI_EXCEPTION_CONT(SKIP_CLOSE);
}

ACI_RC ulnConnectCoreLogicalConn(ulnFnContext *aFnContext)
{
    ulnDbc *sDbc = ulnFnContextGetDbc(aFnContext);

    ULN_FLAG(sNeedFinPtContext);

    ACE_DASSERT(sDbc != NULL);

    ACI_TEST(ulnInitializeProtocolContext(aFnContext,
                                          &(sDbc->mPtContext),
                                          &(sDbc->mSession)) != ACI_SUCCESS);
    ULN_FLAG_UP(sNeedFinPtContext);

    ACI_TEST(ulnDrvConnLogin(aFnContext, &(sDbc->mPtContext)) != ACI_SUCCESS);

    ACI_TEST(ulnDrvConnInitialPropertySet(aFnContext, &(sDbc->mPtContext)) != ACI_SUCCESS);

    ULN_FLAG_DOWN(sNeedFinPtContext);
    ACI_TEST(ulnFinalizeProtocolContext(aFnContext,&(sDbc->mPtContext)) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    ULN_IS_FLAG_UP(sNeedFinPtContext)
    {
        ulnFinalizeProtocolContext(aFnContext, &(sDbc->mPtContext));
    }

    return ACI_FAILURE;
}

ACI_RC ulnConnectCore(ulnDbc       *aDbc,
                      ulnFnContext *aFC)
{
    acp_bool_t sNeedFailover = ACP_TRUE;

    ACI_TEST_RAISE(ulnConnectCorePhysicalConn(aFC) != ACI_SUCCESS, DoFailover);
    ulnDbcSetIsConnected(aDbc, ACP_TRUE);

#ifdef COMPILE_SHARDCLI
    ACI_TEST(ulsdModuleHandshake(aFC) != ACI_SUCCESS);
#endif /* COMPILE_SHARDCLI */

    ACI_TEST_RAISE(ulnConnectCoreLogicalConn(aFC) != ACI_SUCCESS, DoFailover);
    sNeedFailover = ACP_FALSE;

    ACI_EXCEPTION_CONT(DoFailover);
    if (sNeedFailover == ACP_TRUE)
    {
        ulnDbcSetIsConnected(aDbc, ACP_FALSE);
        ulnClosePhysicalConn(aDbc);

        ACI_TEST(ulnFailoverDoCTF(aFC) != ACI_SUCCESS);
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    ulnDbcSetIsConnected(aDbc, ACP_FALSE);
    ulnClosePhysicalConn(aDbc);

    if (aDbc->mLink != NULL)
    {
        (void)ulnDbcFreeLink(aDbc);
    }

    return ACI_FAILURE;
}

