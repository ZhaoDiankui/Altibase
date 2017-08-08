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

#include <ulpLibInterCoreFuncA.h>


/* =========================================================
 *  << Library internal core functions  >>
 *
 *  Description :
 *     ulpLibInterFuncX.c의 internal 함수들에서 호출되는 core함수들로, 부분적이거나 반복적인 처리가 필요한 부분을 담당한다.
 *
 *  Parameters : 공통적으로 다음 인자를 갖으며, 함수마다 몇몇 인자가 추가/제거될 수 있다.
 *     ulpLibConnNode *aConnNode : env핸들, dbc핸들 그리고 에러 처리를 위한 인자.
 *     SQLHSTMT    *aHstmt    : CLI호출을 위한 statement 핸들.
 *     ulpSqlca    *aSqlca    : 에러 처리를 위한 인자.
 *     ulpSqlcode  *aSqlcode  : 에러 처리를 위한 인자.
 *     ulpSqlstate *aSqlstate : 에러 처리를 위한 인자.
 *
 * ========================================================*/

ACI_RC ulpPrepareCore( ulpLibConnNode *aConnNode,
                       ulpLibStmtNode *aStmtNode,
                       acp_char_t     *aQuery,
                       ulpSqlca    *aSqlca,
                       ulpSqlcode  *aSqlcode,
                       ulpSqlstate *aSqlstate )
{
/***********************************************************************
 *
 * Description :
 *    Statement에 대한 prepare를 수행하며, stmt노드에 query string을 복사해준다.
 *
 * Implementation :
 *
 ***********************************************************************/
    acp_uint32_t sLen;
    SQLRETURN    sSqlRes;

    /* query string copy*/
    ACI_TEST_RAISE( acpCStrLen( aQuery, ACP_SINT32_MAX ) >= MAX_QUERY_STR_LEN,
                    ERR_STMT_QUERY_OVERFLOW );

    /*
     * BUG-36518 Remove SQL_RESET_PARAM in Apre cursor close
     *
     * Cursor close에서 SQL_CLOSE만 수행하기에 SQL_RESET_PARAM,
     * SQL_UNBIND는 여기서 수행한다. 하지 않아도 별 문제는 없을듯...
     */
    SQLFreeStmt( aStmtNode -> mHstmt, SQL_UNBIND );
    SQLFreeStmt( aStmtNode -> mHstmt, SQL_RESET_PARAMS );

    sSqlRes = SQLPrepare( aStmtNode -> mHstmt,
                          (SQLCHAR *) aQuery,
                          SQL_NTS );

    /* BUG-31405 : Failover성공후 Failure of finding statement 에러 발생. */
    ulpSetErrorInfo4CLI( aConnNode,
                         aStmtNode -> mHstmt,
                         sSqlRes,
                         aSqlca,
                         aSqlcode,
                         aSqlstate,
                         ERR_TYPE2 );

    ACI_TEST_RAISE( (sSqlRes == SQL_ERROR) || (sSqlRes == SQL_INVALID_HANDLE),
                    ERR_CLI_PREPARE );

    /* BUG-30789: Memory usage per prepared statement is too BIG. (>300k) */
    if( aStmtNode->mQueryStr != NULL )
    {
        acpMemFree( aStmtNode->mQueryStr );
    }

    sLen = acpCStrLen(aQuery, ACP_SINT32_MAX);
    ACE_ASSERT( acpMemAlloc( (void**)&(aStmtNode->mQueryStr), sLen + 1 ) == ACP_RC_SUCCESS );

    acpSnprintf(aStmtNode->mQueryStr, sLen+1, "%s", aQuery);
    /* Note. '\0'까지 비교하도록 +1 된 값을 넣는다.
     * 그래야 "SELECT * FROM t1"과 "SELECT * FROM t1 WHERE i1=1"을 구분할 수 있다. */
    aStmtNode->mQueryStrLen = sLen + 1;

    /* 이전에 binding된 적이 있을경우 초기화 해준다.*/
    if( aStmtNode -> mInHostVarPtr != NULL )
    {
        acpMemFree( aStmtNode -> mInHostVarPtr );
        aStmtNode -> mInHostVarPtr = NULL;
    }
    if( aStmtNode -> mOutHostVarPtr != NULL )
    {
        acpMemFree( aStmtNode -> mOutHostVarPtr );
        aStmtNode -> mOutHostVarPtr = NULL;
    }
    if( aStmtNode -> mExtraHostVarPtr != NULL )
    {
        acpMemFree( aStmtNode -> mExtraHostVarPtr );
        aStmtNode -> mExtraHostVarPtr = NULL;
    }

    aStmtNode -> mBindInfo.mNumofInHostvar     = 0;
    aStmtNode -> mBindInfo.mNumofOutHostvar    = 0;
    aStmtNode -> mBindInfo.mNumofExtraHostvar  = 0;

    aStmtNode -> mStmtState = S_PREPARE;

    return ACI_SUCCESS;

    ACI_EXCEPTION ( ERR_CLI_PREPARE );
    {
        /* BUG-31405 : Failover성공후 Failure of finding statement 에러 발생. */
        if ( *(aSqlcode) == EMBEDED_ALTIBASE_FAILOVER_SUCCESS )
        {
            (void) ulpSetFailoverFlag( aConnNode );
        }

        /* BUG-29745 */
        /* Prepare 가 실패했다면 쿼리만 초기화하며, 상태는 이전상태 그대로둔다.*/
        /* BUG-30789: Memory usage per prepared statement is too BIG. (>300k) */
        if( aStmtNode->mQueryStr != NULL )
        {
            acpMemFree( aStmtNode->mQueryStr );
            aStmtNode->mQueryStr = NULL;
            aStmtNode->mQueryStrLen = 0;
        }
    }
    ACI_EXCEPTION ( ERR_STMT_QUERY_OVERFLOW );
    {
        ulpErrorMgr sErrorMgr;
        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_Stmt_Query_Overflow );
        ulpSetErrorInfo4PCOMP( aSqlca,
                               aSqlcode,
                               aSqlstate,
                               ulpGetErrorMSG(&sErrorMgr),
                               SQL_ERROR,
                               ulpGetErrorSTATE(&sErrorMgr) );
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}


/******************************************************************************************
 *  Description :
 *     dynamic sql 의 sqlda를 ulpBindParamCore 함수를 통하여 bind 할수 있도록 변환하여 준다.
 *
 *  Parameters : 공통적으로 다음 인자를 갖으며, 함수마다 몇몇 인자가 추가/제거될 수 있다.
 *     ulpLibConnNode *aConnNode : env핸들, dbc핸들 그리고 에러 처리를 위한 인자.
 *     ulpLibStmtNode *aStmtNode : statement 관련 정보를 저장한다.
 *     SQLHSTMT    *aHstmt    : CLI호출을 위한 statement 핸들.
 *     SQLDA       *aBinda    : dynamic sql에 사용되는 struct.
 *     ulpSqlca    *aSqlca    : 에러 처리를 위한 인자.
 *     ulpSqlcode  *aSqlcode  : 에러 처리를 위한 인자.
 *     ulpSqlstate *aSqlstate : 에러 처리를 위한 인자.
 *******************************************************************************************/
ACI_RC ulpDynamicBindParameter( ulpLibConnNode *aConnNode,
                                ulpLibStmtNode *aStmtNode,
                                SQLHSTMT       *aHstmt,
                                ulpSqlstmt     *aSqlstmt,
                                SQLDA          *aBinda,
                                ulpSqlca       *aSqlca,
                                ulpSqlcode     *aSqlcode,
                                ulpSqlstate    *aSqlstate )
{
    acp_rc_t        sRes;
    ulpHostVar      sUlpHostVar;
    acp_sint32_t    sI = 0;

    ACP_UNUSED( aStmtNode );

    sUlpHostVar.mIsDynAlloc = 0;
    for ( sI = 0; sI < aBinda->N; sI++ )
    {
        switch ( aBinda->T[sI] )
        {
            case SQLDA_TYPE_CHAR:
                sUlpHostVar.mType = H_CHAR;
                break;
            case SQLDA_TYPE_VARCHAR:
                sUlpHostVar.mType = H_VARCHAR;
                break;
            case SQLDA_TYPE_SSHORT:
                sUlpHostVar.mType = H_SHORT;
                sUlpHostVar.mUnsign = 0;
                break;
            case SQLDA_TYPE_SINT:
                sUlpHostVar.mType = H_INT;
                sUlpHostVar.mUnsign = 0;
                break;
            case SQLDA_TYPE_SLONG:
                sUlpHostVar.mType = H_LONG;
                sUlpHostVar.mUnsign = 0;
                break;
            case SQLDA_TYPE_SLONGLONG:
                sUlpHostVar.mType = H_LONGLONG;
                sUlpHostVar.mUnsign = 0;
                break;
            case SQLDA_TYPE_DOUBLE:
                sUlpHostVar.mType = H_DOUBLE;
                break;
            case SQLDA_TYPE_FLOAT:
                sUlpHostVar.mType = H_FLOAT;
                break;
            case SQLDA_TYPE_USHORT:
                sUlpHostVar.mType = H_SHORT;
                sUlpHostVar.mUnsign = 1;
                break;
            case SQLDA_TYPE_UINT:
                sUlpHostVar.mType = H_INT;
                sUlpHostVar.mUnsign = 1;
                break;
            case SQLDA_TYPE_ULONG:
                sUlpHostVar.mType = H_LONG;
                sUlpHostVar.mUnsign = 1;
                break;
            case SQLDA_TYPE_ULONGLONG:
                sUlpHostVar.mType = H_LONGLONG;
                sUlpHostVar.mUnsign = 1;
                break;
            case SQLDA_TYPE_DATE:
                sUlpHostVar.mType = H_DATE;
                break;
            case SQLDA_TYPE_TIME:
                sUlpHostVar.mType = H_TIME;
                break;
            case SQLDA_TYPE_TIMESTAMP:
                sUlpHostVar.mType = H_TIMESTAMP;
                break;
            default:
                ACI_TEST_RAISE( ACP_TRUE, ERR_INVALID_TYPE );
                break;
        }
        sUlpHostVar.mHostVar = aBinda->V[sI];
        sUlpHostVar.mLen = aBinda->L[sI];
        sUlpHostVar.mSizeof = aBinda->L[sI];
        sUlpHostVar.mMaxlen = aBinda->L[sI];
        sUlpHostVar.mHostInd = (SQLLEN*)aBinda->I[sI];

        sRes = ulpBindParamCore( aConnNode,
                                 aHstmt,
                                 aSqlstmt,
                                 &sUlpHostVar,
                                 (acp_sint16_t)(sI + 1),
                                 aSqlca,
                                 aSqlcode,
                                 aSqlstate,
                                 SQL_PARAM_INPUT );
        
        ACI_TEST(ACP_RC_NOT_SUCCESS(sRes));
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION ( ERR_INVALID_TYPE );
    {
        ulpErrorMgr sErrorMgr;
        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_Type_Not_Exist_Error );
        ulpSetErrorInfo4PCOMP( aSqlca,
                               aSqlcode,
                               aSqlstate,
                               ulpGetErrorMSG(&sErrorMgr),
                               SQL_ERROR/* sqlcode */,
                               NULL );
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}
                            
ACI_RC ulpDynamicBindCol( ulpLibConnNode *aConnNode,
                          ulpLibStmtNode *aStmtNode,
                          SQLHSTMT       *aHstmt,
                          ulpSqlstmt     *aSqlstmt,
                          SQLDA          *aBinda,
                          ulpSqlca       *aSqlca,
                          ulpSqlcode     *aSqlcode,
                          ulpSqlstate    *aSqlstate )
{
    acp_rc_t        sRes;
    ulpHostVar      sUlpHostVar;

    acp_sint32_t    sI;

    ACP_UNUSED( aStmtNode );

    sUlpHostVar.mIsDynAlloc = 0;
    for ( sI = 0; sI < aBinda->N; sI++ )
    {
        switch ( aBinda->T[sI] )
        {
            case SQLDA_TYPE_CHAR:
                sUlpHostVar.mType = H_CHAR;
                break;
            case SQLDA_TYPE_VARCHAR:
                sUlpHostVar.mType = H_VARCHAR;
                break;
            case SQLDA_TYPE_SSHORT:
                sUlpHostVar.mType = H_SHORT;
                sUlpHostVar.mUnsign = 0;
                break;
            case SQLDA_TYPE_SINT:
                sUlpHostVar.mType = H_INT;
                sUlpHostVar.mUnsign = 0;
                break;
            case SQLDA_TYPE_SLONG:
                sUlpHostVar.mType = H_LONG;
                sUlpHostVar.mUnsign = 0;
                break;
            case SQLDA_TYPE_SLONGLONG:
                sUlpHostVar.mType = H_LONGLONG;
                sUlpHostVar.mUnsign = 0;
                break;
            case SQLDA_TYPE_DOUBLE:
                sUlpHostVar.mType = H_DOUBLE;
                break;
            case SQLDA_TYPE_FLOAT:
                sUlpHostVar.mType = H_FLOAT;
                break;
            case SQLDA_TYPE_USHORT:
                sUlpHostVar.mType = H_SHORT;
                sUlpHostVar.mUnsign = 1;
                break;
            case SQLDA_TYPE_UINT:
                sUlpHostVar.mType = H_INT;
                sUlpHostVar.mUnsign = 1;
                break;
            case SQLDA_TYPE_ULONG:
                sUlpHostVar.mType = H_LONG;
                sUlpHostVar.mUnsign = 1;
                break;
            case SQLDA_TYPE_ULONGLONG:
                sUlpHostVar.mType = H_LONGLONG;
                sUlpHostVar.mUnsign = 1;
                break;
            case SQLDA_TYPE_DATE:
                sUlpHostVar.mType = H_DATE;
                break;
            case SQLDA_TYPE_TIME:
                sUlpHostVar.mType = H_TIME;
                break;
            case SQLDA_TYPE_TIMESTAMP:
                sUlpHostVar.mType = H_TIMESTAMP;
                break;
            default:
                ACI_TEST_RAISE( ACP_TRUE, ERR_INVALID_TYPE );
                break;
        }

        sUlpHostVar.mHostVar = (void*) aBinda->V[sI];
        sUlpHostVar.mLen = aBinda->L[sI];
        sUlpHostVar.mSizeof = aBinda->L[sI];
        sUlpHostVar.mHostInd = (SQLLEN*) aBinda->I[sI];

        sRes = ulpBindColCore( aConnNode,
                               aHstmt,
                               aSqlstmt,
                               &sUlpHostVar,
                               (acp_sint16_t)(sI + 1),
                               aSqlca,
                               aSqlcode,
                               aSqlstate );

        ACI_TEST(ACP_RC_NOT_SUCCESS(sRes));
    }

    return ACI_SUCCESS;
    
    ACI_EXCEPTION ( ERR_INVALID_TYPE );
    {
        ulpErrorMgr sErrorMgr;
        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_Type_Not_Exist_Error );
        ulpSetErrorInfo4PCOMP( aSqlca,
                               aSqlcode,
                               aSqlstate,
                               ulpGetErrorMSG(&sErrorMgr),
                               SQL_ERROR/* sqlcode */,
                               NULL );
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulpBindHostVarCore ( ulpLibConnNode *aConnNode,
                            ulpLibStmtNode *aStmtNode,
                            SQLHSTMT    *aHstmt,
                            ulpSqlstmt  *aSqlstmt,
                            ulpSqlca    *aSqlca,
                            ulpSqlcode  *aSqlcode,
                            ulpSqlstate *aSqlstate )
{
/***********************************************************************
 *
 * Description :
 *    host 변수들에 대한 binding처리를 담당한다.
 *    변수 type에 따라 ulpBindParamCore 혹은 ulpBindColCore 함수를 호출함.
 *
 * Implementation :
 *
 ***********************************************************************/
    acp_sint16_t sI;
    acp_uint32_t sInCount ;
    acp_uint32_t sOutCount;
    acp_uint32_t sInOutCount;
    ACI_RC       sRc;

    sInCount = sOutCount = sInOutCount = 0 ;

    for ( sI =0 ; sI < ( aSqlstmt->numofhostvar ) ; sI++ )
    {
        switch ( aSqlstmt->hostvalue[sI].mInOut )
        {
            case H_IN:
                /* Input host variable */

                sRc = ulpBindParamCore( aConnNode,
                                        aHstmt,
                                        aSqlstmt,
                                        &( aSqlstmt->hostvalue[sI] ),
                                        sInCount + sInOutCount + 1,
                                        aSqlca,
                                        aSqlcode,
                                        aSqlstate,
                                        SQL_PARAM_INPUT );
                ACI_TEST( sRc == ACI_FAILURE );
                sInCount++;
                break;
            case H_OUT:
                /* Output host variable */
                /* utpBindColCore 의 역할은 CLI함수 SQLBindCol을 호출한다.
                 * SQL_C_TYPE 은 위에서본 표와 같이 설정된다. */
                sRc = ulpBindColCore( aConnNode,
                                      aHstmt,
                                      aSqlstmt,
                                      &( aSqlstmt->hostvalue[sI] ),
                                      sOutCount + 1,
                                      aSqlca,
                                      aSqlcode,
                                      aSqlstate );
                ACI_TEST( sRc == ACI_FAILURE );
                sOutCount++;
                break;
            case H_INOUT:
                /* In&Out host variable */
                sRc = ulpBindParamCore( aConnNode,
                                        aHstmt,
                                        aSqlstmt,
                                        &( aSqlstmt->hostvalue[sI] ),
                                        sInCount + sInOutCount + 1,
                                        aSqlca,
                                        aSqlcode,
                                        aSqlstate,
                                        SQL_PARAM_INPUT_OUTPUT );
                ACI_TEST( sRc == ACI_FAILURE );
                sInOutCount++;
                break;
            case H_OUT_4PSM:
                /* Out host variable for PSM */
                sRc = ulpBindParamCore( aConnNode,
                                        aHstmt,
                                        aSqlstmt,
                                        &( aSqlstmt->hostvalue[sI] ),
                                        sInCount + sInOutCount + 1,
                                        aSqlca,
                                        aSqlcode,
                                        aSqlstate,
                                        SQL_PARAM_OUTPUT );
                ACI_TEST( sRc == ACI_FAILURE );
                sInOutCount++;
                break;
            default:
                ACE_ASSERT(0);
                break;
        }
    }

    /* stmt node 에 binding 정보&value set*/
    if( aStmtNode != NULL )
    {

        if ( sInCount > 0 )
        {
            if ( aStmtNode -> mInHostVarPtr != NULL )
            {
                acpMemFree( aStmtNode -> mInHostVarPtr );
                aStmtNode -> mInHostVarPtr = NULL;
            }

            ACI_TEST_RAISE( acpMemAlloc( (void**)&(aStmtNode->mInHostVarPtr),
                                         sInCount *
                                         ACI_SIZEOF(ulpHostVarPtr) )
                            != ACP_RC_SUCCESS, ERR_MEMORY_ALLOC );

            aStmtNode -> mBindInfo.mNumofInHostvar = sInCount;
        }

        if ( sOutCount > 0 )
        {
            if ( aStmtNode -> mOutHostVarPtr != NULL )
            {
                acpMemFree( aStmtNode -> mOutHostVarPtr );
                aStmtNode -> mOutHostVarPtr = NULL;
            }

            ACI_TEST_RAISE( acpMemAlloc( (void**)&(aStmtNode->mOutHostVarPtr),
                                         sOutCount *
                                         ACI_SIZEOF(ulpHostVarPtr) )
                            != ACP_RC_SUCCESS, ERR_MEMORY_ALLOC );

            aStmtNode -> mBindInfo.mNumofOutHostvar = sOutCount;
        }

        if ( sInOutCount > 0 )
        {
            if ( aStmtNode -> mExtraHostVarPtr != NULL )
            {
                acpMemFree( aStmtNode -> mExtraHostVarPtr );
                aStmtNode -> mExtraHostVarPtr = NULL;
            }

            ACI_TEST_RAISE( acpMemAlloc( (void**)&(aStmtNode->mExtraHostVarPtr),
                                         sInOutCount *
                                         ACI_SIZEOF(ulpHostVarPtr) )
                            != ACP_RC_SUCCESS, ERR_MEMORY_ALLOC );

            aStmtNode -> mBindInfo.mNumofExtraHostvar = sInOutCount;
        }

        sInCount = sOutCount = sInOutCount = 0 ;

        /* application host 변수 pointer를 복사해옴.*/
        for ( sI = 0 ; sI < aSqlstmt->numofhostvar ; sI++ )
        {
            switch ( aSqlstmt->hostvalue[sI].mInOut )
            {
                case H_IN:
                    ACE_ASSERT( aStmtNode->mInHostVarPtr != NULL );

                    aStmtNode->mInHostVarPtr[sInCount].mHostVar =
                            aSqlstmt->hostvalue[sI].mHostVar;
                    /* BUG-27833 :
                        변수가 다르더라도 포인터가 같을수 있기 때문에 type까지 비교해야함. */
                    aStmtNode->mInHostVarPtr[sInCount].mHostVarType =
                            aSqlstmt->hostvalue[sI].mType;
                    aStmtNode->mInHostVarPtr[sInCount].mHostInd =
                            aSqlstmt->hostvalue[sI].mHostInd;
                    sInCount++;
                    break;
                case H_OUT:
                    aStmtNode->mOutHostVarPtr[sOutCount].mHostVar =
                            aSqlstmt->hostvalue[sI].mHostVar;
                    /* BUG-27833 :
                        변수가 다르더라도 포인터가 같을수 있기 때문에 type까지 비교해야함. */
                    aStmtNode->mOutHostVarPtr[sOutCount].mHostVarType =
                            aSqlstmt->hostvalue[sI].mType;
                    aStmtNode->mOutHostVarPtr[sOutCount].mHostInd =
                            aSqlstmt->hostvalue[sI].mHostInd;
                    sOutCount++;
                    break;
                case H_INOUT:
                case H_OUT_4PSM:
                    aStmtNode->mExtraHostVarPtr[sInOutCount].mHostVar =
                            aSqlstmt->hostvalue[sI].mHostVar;
                    /* BUG-27833 :
                        변수가 다르더라도 포인터가 같을수 있기 때문에 type까지 비교해야함. */
                    aStmtNode->mExtraHostVarPtr[sInOutCount].mHostVarType =
                            aSqlstmt->hostvalue[sI].mType;
                    aStmtNode->mExtraHostVarPtr[sInOutCount].mHostInd =
                            aSqlstmt->hostvalue[sI].mHostInd;
                    sInOutCount++;
                    break;
                default:
                    ACE_ASSERT(0);
                    break;
            }
        }

        /* Statement 상태를 BINDING으로 변경함.*/
        aStmtNode->mStmtState = S_BINDING;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION (ERR_MEMORY_ALLOC);
    {
        ulpErrorMgr sErrorMgr;
        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_Memory_Alloc_Error );
        ulpPrintfErrorCode( ACP_STD_ERR,
                            &sErrorMgr);
        ACE_ASSERT(0);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulpExecuteCore( ulpLibConnNode *aConnNode,
                       ulpLibStmtNode *aStmtNode,
                       ulpSqlstmt  *aSqlstmt,
                       SQLHSTMT    *aHstmt )
{
/***********************************************************************
 *
 * Description :
 *    SQL구문에 대한 Execute을 담당한다.
 *
 * Implementation :
 *
 ***********************************************************************/
    acp_uint32_t      sI;
    SQLRETURN         sSqlRes;
    SQLRETURN         sSqlResExec;
    SQLUSMALLINT     *sParamStatus;
    acp_bool_t        sIsParamArr;
    acp_bool_t        sIsAtomic;
    acp_bool_t        sIsAlloc;

    sParamStatus = NULL;
    sIsAlloc     = ACP_FALSE;
    sIsParamArr = ((aStmtNode->mBindInfo.mIsArray == ACP_TRUE) &&
                   (aSqlstmt->stmttype != S_DirectSEL) &&
                   (aSqlstmt->stmttype != S_Open))? ACP_TRUE : ACP_FALSE;
    sIsAtomic    = aStmtNode->mBindInfo.mIsAtomic;

    /* array binding인지 확인. */
    if ( sIsParamArr == ACP_TRUE )
    {
        if( aSqlstmt->statusptr == NULL )
        {
            /* 구문 수행후 각 parameter들의 결과 상태값을 저장하기 위해  *
            *  array 개수만큼 status배열 할당함.                    */
            ACI_TEST_RAISE( acpMemAlloc( (void**)&(sParamStatus),
                                         aStmtNode->mBindInfo.mArraySize *
                                         ACI_SIZEOF(SQLUSMALLINT) )
                            != ACP_RC_SUCCESS, ERR_MEMORY_ALLOC );

            sIsAlloc = ACP_TRUE;
        }
        else
        {
            sParamStatus = (SQLUSMALLINT *)(aSqlstmt->statusptr);
        }

        if( aSqlstmt->errcodeptr != NULL )
        {
            acpMemSet( aSqlstmt->errcodeptr,
                       0,
                       ACI_SIZEOF(int) * (aStmtNode->mBindInfo.mArraySize) );
        }

        /* statement 속성 SQL_ATTR_PARAM_STATUS_PTR 설정. */
        sSqlRes = SQLSetStmtAttr ( *aHstmt,
                                   SQL_ATTR_PARAM_STATUS_PTR,
                                   (SQLPOINTER)sParamStatus,
                                   SQL_IS_POINTER );

        ACI_TEST_RAISE( (sSqlRes == SQL_ERROR) || (sSqlRes == SQL_INVALID_HANDLE),
                        ERR_CLI_SETSTMT );
    }

    /* BUG-30099 : indicator값에 잘못된값이 입력되어 data-at-execution param으로 인식되면
    Function sequence에러발생.  */
    /* APRE lib.에서 SQL_DATA_AT_EXEC 는 지원하지 않는다.*/
    for( sI = 0 ; sI < aStmtNode->mBindInfo.mNumofInHostvar ; sI++ )
    {
        if( aStmtNode->mInHostVarPtr[sI].mHostInd != NULL )
        {
            ACI_TEST_RAISE( *((acp_sint32_t*)aStmtNode->mInHostVarPtr[sI].mHostInd) < SQL_NTS /*-3*/,
                            ERR_INVALID_INDICATOR_VALUE
                          );
        }
    }

    sSqlResExec = SQLExecute( *aHstmt );

    /* BUG-25643 : apre 에서 ONERR 구문이 잘못 동작합니다. */
    /* ONERR 구문을 이용하여 사용자가 에러 코드를 담을 배열을 지정했을경우.*/
    /* Excute중 error가 발생했다면...*/
    if( (aSqlstmt->errcodeptr != NULL) &&
        ((sSqlResExec == SQL_SUCCESS_WITH_INFO) || (sSqlResExec == SQL_ERROR))
      )
    {
        ACI_TEST( ulpGetOnerrErrCodeCore(
                                aConnNode,
                                aSqlstmt,
                                aHstmt,
                                (acp_sint32_t*)aSqlstmt->errcodeptr )
                        == ACI_FAILURE );
    }

    ulpSetErrorInfo4CLI( aConnNode,
                         *aHstmt,
                         sSqlResExec,
                         aSqlstmt->sqlcaerr,
                         aSqlstmt->sqlcodeerr,
                         aSqlstmt->sqlstateerr,
                         /* dml일경우 ERR_TYPE1_DML 아니면, ERR_TYPE1*/
                         ( (aSqlstmt->stmttype == S_DirectDML) ||
                           (aSqlstmt->stmttype == S_ExecDML) ) ? ERR_TYPE1_DML : ERR_TYPE1 );

    /* array binding인지 확인. */
    if ( sIsParamArr == ACP_TRUE )
    {
        if( sIsAtomic == ACP_TRUE )
        {
            if( sSqlResExec == SQL_SUCCESS)
            {
                aSqlstmt->sqlcaerr->sqlerrd[3] = 1;
            }
            else
            {
                aSqlstmt->sqlcaerr->sqlerrd[3] = 0;
            }
        }
        else
        {
            aSqlstmt->sqlcaerr->sqlerrd[3] = 0;
            /* sParamStatus배열을 조사하여 sqlca->sqlerrd[3]를 설정함. */
            for ( sI = 0 ; sI < ( aStmtNode->mBindInfo.mArraySize ) ; sI++ )
            {
                if ( sParamStatus[sI] == SQL_PARAM_DIAG_UNAVAILABLE )
                {
                    aSqlstmt->sqlcaerr->sqlerrd[3] = 0;
                    break;
                }
                else
                {
                    if ( (sParamStatus[sI] == SQL_PARAM_SUCCESS) ||
                        (sParamStatus[sI] == SQL_PARAM_SUCCESS_WITH_INFO) )
                    {
                        aSqlstmt->sqlcaerr->sqlerrd[3]++;
                    }
                }
            }
        }

        /* statement 속성 SQL_ATTR_PARAM_STATUS_PTR을 NULL로 설정함. */
        sSqlRes = SQLSetStmtAttr( *aHstmt,
                                  SQL_ATTR_PARAM_STATUS_PTR,
                                  NULL,
                                  SQL_IS_POINTER );

        ACI_TEST_RAISE( ( sSqlRes == SQL_ERROR ) || ( sSqlRes == SQL_INVALID_HANDLE ) ,
                        ERR_CLI_SETSTMT );

    }
    else
    {
        aSqlstmt->sqlcaerr->sqlerrd[3] = 0;
    }

    /* BUG-30099 : indicator값에 잘못된값이 입력되어 data-at-execution param으로 인식되면
                   Function sequence에러발생.  */
    ACI_TEST_RAISE( (sSqlResExec == SQL_ERROR)||
                    (sSqlResExec == SQL_INVALID_HANDLE)||
                    (sSqlResExec == SQL_NEED_DATA),
                    ERR_CLI_EXECUTE );

    /* statement 상태를 EXECUTE로 변경함. */
    aStmtNode->mStmtState = S_EXECUTE;

    if ( sIsAlloc != ACP_FALSE )
    {
        acpMemFree ( sParamStatus );
        sIsAlloc = ACP_FALSE;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION (ERR_MEMORY_ALLOC);
    {
        ulpErrorMgr sErrorMgr;
        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_Memory_Alloc_Error );
        ulpPrintfErrorCode( ACP_STD_ERR,
                            &sErrorMgr);
        ACE_ASSERT(0);
    }
    ACI_EXCEPTION ( ERR_CLI_SETSTMT );
    {

        ulpSetErrorInfo4CLI( aConnNode,
                             *aHstmt,
                             SQL_ERROR,
                             aSqlstmt->sqlcaerr,
                             aSqlstmt->sqlcodeerr,
                             aSqlstmt->sqlstateerr,
                             ERR_TYPE2 );

    }
    ACI_EXCEPTION ( ERR_CLI_EXECUTE );
    {
        /* BUG-31405 : Failover성공후 Failure of finding statement 에러 발생. */
        if ( *(aSqlstmt->sqlcodeerr) == EMBEDED_ALTIBASE_FAILOVER_SUCCESS )
        {
            (void) ulpSetFailoverFlag( aConnNode );
        }
    }
    /* BUG-30099 : indicator값에 잘못된값이 입력되어 data-at-execution param으로 인식되면
                   Function sequence에러발생.  */
    ACI_EXCEPTION ( ERR_INVALID_INDICATOR_VALUE );
    {
        ulpErrorMgr sErrorMgr;
        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_Invalid_Indicator_Value_Error,
                         *((acp_sint32_t*)aStmtNode->mInHostVarPtr[sI].mHostInd) );

        ulpSetErrorInfo4PCOMP( aSqlstmt->sqlcaerr,
                               aSqlstmt->sqlcodeerr,
                               aSqlstmt->sqlstateerr,
                               ulpGetErrorMSG(&sErrorMgr),
                               SQL_ERROR/* sqlcode */,
                               NULL );
    }
    ACI_EXCEPTION_END;

    if ( sIsAlloc != ACP_FALSE )
    {
        acpMemFree ( sParamStatus );
    }

    return ACI_FAILURE;
}

ACI_RC ulpFetchCore( ulpLibConnNode *aConnNode,
                     ulpLibStmtNode *aStmtNode,
                     SQLHSTMT       *aHstmt,
                     ulpSqlstmt     *aSqlstmt,
                     ulpSqlca       *aSqlca,
                     ulpSqlcode     *aSqlcode,
                     ulpSqlstate    *aSqlstate )
{
/***********************************************************************
 *
 * Description :
 *     ulpRunDirectQuery 함수내에서 Unnamed select 구문에 대한 Fetch를 담당한다.
 *
 * Implementation :
 *
 ***********************************************************************/
    SQLRETURN     sSqlRes;
    SQLRETURN     sSqlRes2;
    SQLUINTEGER   sNumFetched;

    sSqlRes = SQLSetStmtAttr( *aHstmt,
                              SQL_ATTR_ROW_STATUS_PTR,
                              NULL,
                              0 );

    ACI_TEST_RAISE( (sSqlRes == SQL_ERROR) || (sSqlRes == SQL_INVALID_HANDLE)
                    , ERR_CLI_SETSTMT );

    sSqlRes = SQLSetStmtAttr( *aHstmt,
                              SQL_ATTR_ROWS_FETCHED_PTR,
                              (SQLPOINTER) &sNumFetched,
                               0 );

    ACI_TEST_RAISE( (sSqlRes == SQL_ERROR) || (sSqlRes == SQL_INVALID_HANDLE)
                    , ERR_CLI_SETSTMT );


    sSqlRes = SQLFetch( *aHstmt );

    ulpSetErrorInfo4CLI( aConnNode,
                         *aHstmt,
                         sSqlRes,
                         aSqlca,
                         aSqlcode,
                         aSqlstate,
                         ERR_TYPE1 );

    if( (acpCStrCmp( *aSqlstate,
                     SQLSTATE_UNSAFE_NULL,
                     MAX_ERRSTATE_LEN - 1 ) == 0) &&
        (ulpGetStmtOption( aSqlstmt,
                           ULP_OPT_UNSAFE_NULL ) == ACP_TRUE) )
    {
        sSqlRes = SQL_SUCCESS;
        ulpSetErrorInfo4PCOMP( aSqlca,
                               aSqlcode,
                               aSqlstate,
                               SQLMSG_SUCCESS,
                               sSqlRes,
                               SQLSTATE_SUCCESS );
    }

    aSqlca->sqlerrd[2] = sNumFetched;

    ACI_TEST_RAISE( (sSqlRes == SQL_ERROR) || (sSqlRes == SQL_INVALID_HANDLE)
                    , ERR_CLI_FETCH );

    /* 커서 상태를 FETCH로 바꿈;*/
    if ( aStmtNode != NULL )
    {
        aStmtNode -> mCurState  = C_FETCH;
    }

    /* check if too many rows error*/
    if ( (sSqlRes == SQL_SUCCESS) || (sSqlRes == SQL_SUCCESS_WITH_INFO) )
    {
        sSqlRes2 = SQLFetch( *aHstmt );

        ACI_TEST_RAISE( (sSqlRes2 == SQL_ERROR) || (sSqlRes2 == SQL_INVALID_HANDLE)
                        , ERR_CLI_FETCH2 );

        ACI_TEST_RAISE( sSqlRes2 != SQL_NO_DATA , ERR_ERR_TOO_MANY_ROWS );
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION ( ERR_CLI_SETSTMT );
    {
        ulpSetErrorInfo4CLI( aConnNode,
                             *aHstmt,
                             SQL_ERROR,
                             aSqlca,
                             aSqlcode,
                             aSqlstate,
                             ERR_TYPE2 );

    }
    ACI_EXCEPTION ( ERR_CLI_FETCH );
    {
        /* BUG-31405 : Failover성공후 Failure of finding statement 에러 발생. */
        if ( *(aSqlcode) == EMBEDED_ALTIBASE_FAILOVER_SUCCESS )
        {
            (void) ulpSetFailoverFlag( aConnNode );
        }
    }
    ACI_EXCEPTION ( ERR_CLI_FETCH2 );
    {
        ulpSetErrorInfo4CLI( aConnNode,
                             *aHstmt,
                             sSqlRes2,
                             aSqlca,
                             aSqlcode,
                             aSqlstate,
                             ERR_TYPE1 );

        /* BUG-31405 : Failover성공후 Failure of finding statement 에러 발생. */
        if ( *(aSqlcode) == EMBEDED_ALTIBASE_FAILOVER_SUCCESS )
        {
            (void) ulpSetFailoverFlag( aConnNode );
        }

        if( (acpCStrCmp( *aSqlstate,
                         SQLSTATE_UNSAFE_NULL,
                         MAX_ERRSTATE_LEN-1 ) == 0) &&
            (ulpGetStmtOption( aSqlstmt,
                               ULP_OPT_UNSAFE_NULL ) == ACP_TRUE) )
        {
            ulpErrorMgr sErrorMgr;
            ulpSetErrorCode( &sErrorMgr,
                             ulpERR_ABORT_Too_Many_Rows_Error );

            ulpSetErrorInfo4PCOMP(  aSqlca,
                                    aSqlcode,
                                    aSqlstate,
                                    ulpGetErrorMSG(&sErrorMgr),
                                    SQL_ERROR,
                                    NULL );
            aSqlca->sqlerrd[2] = sNumFetched;
        }

    }
    ACI_EXCEPTION ( ERR_ERR_TOO_MANY_ROWS );
    {
        ulpErrorMgr sErrorMgr;
        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_Too_Many_Rows_Error );

        ulpSetErrorInfo4PCOMP( aSqlca,
                               aSqlcode,
                               aSqlstate,
                               ulpGetErrorMSG(&sErrorMgr),
                               ulpGetErrorCODE(&sErrorMgr),
                               NULL );

        aSqlca->sqlerrd[2] = sNumFetched;
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulpExecDirectCore( ulpLibConnNode *aConnNode,
                          ulpLibStmtNode *aStmtNode,
                          ulpSqlstmt     *aSqlstmt,
                          SQLHSTMT       *aHstmt,
                          acp_char_t     *aQuery )
{
/***********************************************************************
 *
 * Description :
 *    ulpRunDirectQuery, ulpFor, ulpExecuteImmediate 함수에서
 *    SQLExecDirect 호출하기 위해 사용됨.
 * Implementation :
 *
 ***********************************************************************/
    acp_uint32_t      sI;
    SQLRETURN         sSqlRes;
    SQLRETURN         sSqlResExec;
    SQLUSMALLINT     *sParamStatus;
    acp_bool_t        sIsArr;
    acp_bool_t        sIsAtomic;
    acp_bool_t        sIsAlloc;

    sParamStatus = NULL;
    sIsAlloc     = ACP_FALSE;

    if ( aSqlstmt->stmttype == S_ExecIm )
    {
        sIsArr       = ACP_FALSE;
        sIsAtomic    = ACP_FALSE;
    }
    else
    {
        sIsArr       = (acp_bool_t) aSqlstmt->isarr;
        sIsAtomic    = (acp_bool_t) aSqlstmt->isatomic;
    }

    /* array binding인지 확인. */
    if ( sIsArr == ACP_TRUE )
    {
        if( aSqlstmt->statusptr == NULL )
        {
            /* 구문 수행후 각 parameter들의 결과 상태값을 저장하기 위해  *
             *  array 개수만큼 status배열 할당함.                    */
            ACI_TEST_RAISE( acpMemAlloc( (void**)&(sParamStatus),
                                         aSqlstmt->arrsize *
                                         ACI_SIZEOF(SQLUSMALLINT) )
                            != ACP_RC_SUCCESS, ERR_MEMORY_ALLOC );

            sIsAlloc = ACP_TRUE;
        }
        else
        {
            sParamStatus = (SQLUSMALLINT *)(aSqlstmt->statusptr);
        }

        if( aSqlstmt->errcodeptr != NULL )
        {
            acpMemSet( aSqlstmt->errcodeptr,
                       0,
                       ACI_SIZEOF(int) * (aSqlstmt->arrsize) );
        }

        /* statement 속성 SQL_ATTR_PARAM_STATUS_PTR 설정. */
        sSqlRes = SQLSetStmtAttr ( *aHstmt,
                                   SQL_ATTR_PARAM_STATUS_PTR,
                                   (SQLPOINTER)sParamStatus,
                                   SQL_IS_POINTER );

        ACI_TEST_RAISE( (sSqlRes == SQL_ERROR) || (sSqlRes == SQL_INVALID_HANDLE),
                        ERR_CLI_SETSTMT );
    }

    sSqlResExec = SQLExecDirect( *aHstmt, (SQLCHAR *)aQuery, SQL_NTS );

    /* BUG-25643 : apre 에서 ONERR 구문이 잘못 동작합니다. */
    /* ONERR 구문을 이용하여 사용자가 에러 코드를 담을 배열을 지정했을경우.*/
    /* Excute중 error가 발생했다면...*/
    if( (aSqlstmt->errcodeptr != NULL) &&
        ((sSqlResExec == SQL_SUCCESS_WITH_INFO) || (sSqlResExec == SQL_ERROR))
      )
    {
        ACI_TEST( ulpGetOnerrErrCodeCore(
                                aConnNode,
                                aSqlstmt,
                                aHstmt,
                                (acp_sint32_t*)aSqlstmt->errcodeptr )
                        == ACI_FAILURE );
    }

    ulpSetErrorInfo4CLI( aConnNode,
                         *aHstmt,
                         sSqlResExec,
                         aSqlstmt->sqlcaerr,
                         aSqlstmt->sqlcodeerr,
                         aSqlstmt->sqlstateerr,
                         /* BUG-31946 : There is a no process to get affected row counts
                            atfer excuting SQL EXEC IMMEDIATE statement. */
                         /* EXEC SQL <DML query> (S_DirectDML)혹은 EXEC SQL IMMEDIATE <DML query> (S_ExecIm) 일경우
                            ERR_TYPE1_DML을 인자로주어 affected row count를 얻어오는 작업을 추가적으로 수행한다 */
                         ((aSqlstmt->stmttype == S_DirectDML) ||
                          (aSqlstmt->stmttype == S_ExecIm)) ? ERR_TYPE1_DML : ERR_TYPE1
                       );

    /* array binding인지 확인. */
    if ( sIsArr == ACP_TRUE )
    {
        if( sIsAtomic == ACP_TRUE )
        {
            if(sSqlResExec == SQL_SUCCESS)
            {
                aSqlstmt->sqlcaerr->sqlerrd[3] = 1;
            }
            else
            {
                aSqlstmt->sqlcaerr->sqlerrd[3] = 0;
            }
        }
        else
        {
            aSqlstmt->sqlcaerr->sqlerrd[3] = 0;
            /* sParamStatus배열을 조사하여 sqlca->sqlerrd[3]를 설정함. */
            for ( sI = 0 ; sI < ( aSqlstmt->arrsize ) ; sI++ )
            {
                if ( sParamStatus[sI] == SQL_PARAM_DIAG_UNAVAILABLE )
                {
                    aSqlstmt->sqlcaerr->sqlerrd[3] = 0;
                    break;
                }
                else
                {
                    if ( (sParamStatus[sI] == SQL_PARAM_SUCCESS) ||
                         (sParamStatus[sI] == SQL_PARAM_SUCCESS_WITH_INFO) )
                    {
                        aSqlstmt->sqlcaerr->sqlerrd[3]++;
                    }
                }
            }
        }

        /* statement 속성 SQL_ATTR_PARAM_STATUS_PTR을 NULL로 설정함. */
        sSqlRes = SQLSetStmtAttr( *aHstmt,
                                  SQL_ATTR_PARAM_STATUS_PTR,
                                  NULL,
                                  SQL_IS_POINTER );

        ACI_TEST_RAISE( ( sSqlRes == SQL_ERROR ) || ( sSqlRes == SQL_INVALID_HANDLE ) ,
                        ERR_CLI_SETSTMT );
    }
    else
    {
        aSqlstmt->sqlcaerr->sqlerrd[3] = 0;
    }

    ACI_TEST_RAISE( (sSqlResExec == SQL_ERROR) || (sSqlResExec == SQL_INVALID_HANDLE),
                     ERR_CLI_EXECDIRECT );

    /* statement 상태를 EXECUTE로 변경함. */
    if( aStmtNode != NULL )
    {
        /* BUG-28553 : failover시 재prepare가 되지않는 문제.  */
        /* SQLExecDirect 는 prepare를 하지 않으므로, 상태는 그대로 S_INITIAL 이다.*/
        aStmtNode->mStmtState = S_INITIAL;
    }

    if ( sIsAlloc != ACP_FALSE )
    {
        acpMemFree( sParamStatus );
        sIsAlloc = ACP_FALSE;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION (ERR_MEMORY_ALLOC);
    {
        ulpErrorMgr mErrorMgr;
        ulpSetErrorCode( &mErrorMgr,
                         ulpERR_ABORT_Memory_Alloc_Error );
        ulpPrintfErrorCode( ACP_STD_ERR,
                            &mErrorMgr);
        ACE_ASSERT(0);
    }
    ACI_EXCEPTION ( ERR_CLI_SETSTMT );
    {
        ulpSetErrorInfo4CLI( aConnNode,
                             *aHstmt,
                             SQL_ERROR,
                             aSqlstmt->sqlcaerr,
                             aSqlstmt->sqlcodeerr,
                             aSqlstmt->sqlstateerr,
                             ERR_TYPE2 );

    }
    ACI_EXCEPTION ( ERR_CLI_EXECDIRECT );
    {
        /* BUG-31405 : Failover성공후 Failure of finding statement 에러 발생. */
        if ( *(aSqlstmt->sqlcodeerr) == EMBEDED_ALTIBASE_FAILOVER_SUCCESS )
        {
            (void) ulpSetFailoverFlag( aConnNode );
        }
    }
    ACI_EXCEPTION_END;

    if ( sIsAlloc != ACP_FALSE )
    {
        acpMemFree( sParamStatus );
    }

    return ACI_FAILURE;
}

ACI_RC ulpCloseStmtCore( ulpLibConnNode *aConnNode,
                         ulpLibStmtNode *aStmtNode,
                         SQLHSTMT    *aHstmt,
                         ulpSqlca    *aSqlca,
                         ulpSqlcode  *aSqlcode,
                         ulpSqlstate *aSqlstate )
{
/***********************************************************************
 *
 * Description :
 *    statement를 close하기 위해 사용됨. SQL_CLOSE 하여 재활용 가능하다.
 *
 * Implementation :
 *
 ***********************************************************************/
    SQLRETURN sSqlRes;

    /* 
     * BUG-36518 Remove SQL_RESET_PARAM in Apre cursor close
     *
     * Cursor를 Close 할 때 SQL_RESET_PARAM, SQL_UNBIND를 수행해서
     * Cursor를 Reopen (In Bind) 하는 경우 문제가 발생한다.
     * 그래서 SQL_CLOSE만 수행하도록 수정.
     */
    sSqlRes = SQLFreeStmt( *aHstmt, SQL_CLOSE );
    ACI_TEST_RAISE( (sSqlRes == SQL_ERROR) || (sSqlRes == SQL_INVALID_HANDLE)
                    , ERR_CLI_FREE_STMT );

    if ( aStmtNode != NULL )
    {
        aStmtNode -> mStmtState = S_CLOSE;
        aStmtNode -> mCurState  = C_CLOSE;
        aStmtNode -> mBindInfo.mNumofInHostvar  = 0;
        aStmtNode -> mBindInfo.mNumofOutHostvar = 0;
        aStmtNode -> mBindInfo.mNumofExtraHostvar = 0;
        aStmtNode -> mBindInfo.mIsArray         = ACP_FALSE;
        aStmtNode -> mBindInfo.mIsStruct        = ACP_FALSE;
        if( aStmtNode -> mOutHostVarPtr != NULL )
        {
            acpMemFree( aStmtNode -> mOutHostVarPtr );
            aStmtNode -> mOutHostVarPtr = NULL;
        }
        if( aStmtNode -> mInHostVarPtr != NULL )
        {
            acpMemFree( aStmtNode -> mInHostVarPtr );
            aStmtNode -> mInHostVarPtr = NULL;
        }
        if( aStmtNode -> mExtraHostVarPtr != NULL )
        {
            acpMemFree( aStmtNode -> mExtraHostVarPtr );
            aStmtNode -> mExtraHostVarPtr = NULL;
        }
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION ( ERR_CLI_FREE_STMT );
    {
        ulpSetErrorInfo4CLI( aConnNode,
                             *aHstmt,
                             sSqlRes,
                             aSqlca,
                             aSqlcode,
                             aSqlstate,
                             ERR_TYPE2 );

    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}


acp_bool_t ulpCheckNeedReBindAllCore( ulpLibStmtNode *aStmtNode,
                                      ulpSqlstmt     *aSqlstmt  )
{
/***********************************************************************
 *
 * Description :
 *    Rebinding이 필요한지 검사하는 함수. Col/Param 모두 검사.
 *
 * Implementation :
 *
 ***********************************************************************/

    acp_sint16_t sX;
    acp_sint16_t sY;
    acp_sint16_t sZ;
    acp_sint16_t sA;
    sY = sZ = sA = 0;

    ACI_TEST( aSqlstmt->numofhostvar !=
              ( aStmtNode->mBindInfo.mNumofInHostvar  +
                aStmtNode->mBindInfo.mNumofOutHostvar +
                aStmtNode->mBindInfo.mNumofExtraHostvar ) );

    ACI_TEST( ( aStmtNode->mStmtState != S_BINDING )    &&
              ( aStmtNode->mStmtState != S_SETSTMTATTR) &&
              ( aStmtNode->mStmtState != S_EXECUTE) );

    for ( sX = 0 ; sX < aSqlstmt->numofhostvar ; sX++ )
    {
        switch ( aSqlstmt->hostvalue[sX].mInOut )
        {
            case H_IN:
                ACI_TEST( aStmtNode->mBindInfo.mNumofInHostvar <= sY );
                ACI_TEST( aSqlstmt ->hostvalue[sX].mHostVar !=
                          aStmtNode->mInHostVarPtr[sY].mHostVar );
                /* BUG-27833 :
                    변수가 다르더라도 포인터가 같을수 있기 때문에 type까지 비교해야함. */
                ACI_TEST( aSqlstmt ->hostvalue[sX].mType !=
                          aStmtNode->mInHostVarPtr[sY].mHostVarType );
                ACI_TEST( aSqlstmt ->hostvalue[sX].mHostInd !=
                          aStmtNode->mInHostVarPtr[sY++].mHostInd );
                break;
            case H_OUT:
                ACI_TEST( aStmtNode->mBindInfo.mNumofOutHostvar <= sZ );
                ACI_TEST( aSqlstmt ->hostvalue[sX].mHostVar !=
                          aStmtNode->mOutHostVarPtr[sZ].mHostVar );
                /* BUG-27833 :
                변수가 다르더라도 포인터가 같을수 있기 때문에 type까지 비교해야함. */
                ACI_TEST( aSqlstmt ->hostvalue[sX].mType !=
                          aStmtNode->mOutHostVarPtr[sZ].mHostVarType );
                ACI_TEST( aSqlstmt ->hostvalue[sX].mHostInd !=
                          aStmtNode->mOutHostVarPtr[sZ++].mHostInd );
                break;
            case H_INOUT:
            case H_OUT_4PSM:
                ACI_TEST( aStmtNode->mBindInfo.mNumofExtraHostvar <= sA );
                ACI_TEST( aSqlstmt ->hostvalue[sX].mHostVar !=
                          aStmtNode->mExtraHostVarPtr[sA].mHostVar );
                /* BUG-27833 :
                    변수가 다르더라도 포인터가 같을수 있기 때문에 type까지 비교해야함. */
                ACI_TEST( aSqlstmt ->hostvalue[sX].mType !=
                          aStmtNode->mExtraHostVarPtr[sA].mHostVarType );
                ACI_TEST( aSqlstmt ->hostvalue[sX].mHostInd !=
                          aStmtNode->mExtraHostVarPtr[sA++].mHostInd );
                break;
            default:
                /* no way...what the ....??*/
                return ACP_TRUE;
        }
    }

    return ACP_FALSE; /* 재호출될 필요 없다.*/

    ACI_EXCEPTION_END;

    return ACP_TRUE;  /* 재호출 되어야 한다.*/
}


acp_bool_t ulpCheckNeedReBindParamCore( ulpLibStmtNode *aStmtNode,
                                    ulpSqlstmt  *aSqlstmt )
{
/***********************************************************************
 *
 * Description :
 *    Rebinding이 필요한지 검사하는 함수. BindParam만 검사.
 *
 * Implementation :
 *
 ***********************************************************************/

    acp_sint16_t sX;

    ACI_TEST( aSqlstmt->numofhostvar != aStmtNode->mBindInfo.mNumofInHostvar );

    ACI_TEST( ( aStmtNode->mStmtState != S_BINDING ) &&
              ( aStmtNode->mStmtState != S_SETSTMTATTR) &&
              ( aStmtNode->mStmtState != S_EXECUTE) );

    for ( sX = 0 ; sX < aSqlstmt->numofhostvar ; sX++ )
    {
        ACI_TEST( aSqlstmt ->hostvalue[sX].mHostVar !=
                  aStmtNode->mInHostVarPtr[sX].mHostVar );
        /* BUG-27833 :
            변수가 다르더라도 포인터가 같을수 있기 때문에 type까지 비교해야함. */
        ACI_TEST( aSqlstmt ->hostvalue[sX].mType !=
                  aStmtNode->mInHostVarPtr[sX].mHostVarType );
        ACI_TEST( aSqlstmt ->hostvalue[sX].mHostInd !=
                  aStmtNode->mInHostVarPtr[sX].mHostInd );
    }

    return ACP_FALSE; /* 재호출될 필요 없다.*/

    ACI_EXCEPTION_END;

    return ACP_TRUE;  /* 재호출 되어야 한다.*/
}


acp_bool_t ulpCheckNeedReBindColCore( ulpLibStmtNode *aStmtNode,
                                      ulpSqlstmt     *aSqlstmt )
{
/***********************************************************************
 *
 * Description :
 *    Rebinding이 필요한지 검사하는 함수. BindCol만 검사.
 *
 * Implementation :
 *
 ***********************************************************************/

    acp_sint16_t sX;

    ACI_TEST( aSqlstmt->numofhostvar != aStmtNode->mBindInfo.mNumofOutHostvar );

    ACI_TEST( ( aStmtNode->mStmtState != S_BINDING ) &&
              ( aStmtNode->mStmtState != S_SETSTMTATTR ) &&
              ( aStmtNode->mStmtState != S_EXECUTE ) );

    for ( sX = 0 ; sX < aSqlstmt->numofhostvar ; sX++ )
    {
        ACI_TEST( aSqlstmt ->hostvalue[sX].mHostVar !=
                  aStmtNode->mOutHostVarPtr[sX].mHostVar );
        /* BUG-27833 :
            변수가 다르더라도 포인터가 같을수 있기 때문에 type까지 비교해야함. */
        ACI_TEST( aSqlstmt ->hostvalue[sX].mType !=
                  aStmtNode->mOutHostVarPtr[sX].mHostVarType );
        ACI_TEST( aSqlstmt->hostvalue[sX].mHostInd !=
                  aStmtNode->mOutHostVarPtr[sX].mHostInd );
    }

    return ACP_FALSE; /* 재호출될 필요 없다.*/

    ACI_EXCEPTION_END;

    return ACP_TRUE;  /* 재호출 되어야 한다.*/

}


acp_bool_t ulpCheckNeedReSetStmtAttrCore( ulpLibStmtNode *aStmtNode,
                                          ulpSqlstmt     *aSqlstmt  )
{
/***********************************************************************
 *
 * Description :
 *    stmtement 속성을 다시 설정해야 하는지 검사해주는 함수.
 *
 * Implementation :
 *
 ***********************************************************************/

    if ( (aSqlstmt->isarr      == aStmtNode->mBindInfo.mIsArray)   &&
         (aSqlstmt->isstruct   == aStmtNode->mBindInfo.mIsStruct)  &&
         (aSqlstmt->arrsize    == aStmtNode->mBindInfo.mArraySize) &&
         (aSqlstmt->structsize == aStmtNode->mBindInfo.mStructSize)
       )
    {
        return ACP_FALSE; /*SQLSetStmtAttr 이 재호출될 필요 없다.*/
    }
    else
    {
        return ACP_TRUE;  /*SQLSetStmtAttr 이 재호출 되어야 한다.*/
    }
}

