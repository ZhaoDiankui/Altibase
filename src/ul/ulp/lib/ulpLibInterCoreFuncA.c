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
 *     ulpLibInterFuncX.c�� internal �Լ��鿡�� ȣ��Ǵ� core�Լ����, �κ����̰ų� �ݺ����� ó���� �ʿ��� �κ��� ����Ѵ�.
 *
 *  Parameters : ���������� ���� ���ڸ� ������, �Լ����� ��� ���ڰ� �߰�/���ŵ� �� �ִ�.
 *     ulpLibConnNode *aConnNode : env�ڵ�, dbc�ڵ� �׸��� ���� ó���� ���� ����.
 *     SQLHSTMT    *aHstmt    : CLIȣ���� ���� statement �ڵ�.
 *     ulpSqlca    *aSqlca    : ���� ó���� ���� ����.
 *     ulpSqlcode  *aSqlcode  : ���� ó���� ���� ����.
 *     ulpSqlstate *aSqlstate : ���� ó���� ���� ����.
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
 *    Statement�� ���� prepare�� �����ϸ�, stmt��忡 query string�� �������ش�.
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
     * Cursor close���� SQL_CLOSE�� �����ϱ⿡ SQL_RESET_PARAM,
     * SQL_UNBIND�� ���⼭ �����Ѵ�. ���� �ʾƵ� �� ������ ������...
     */
    SQLFreeStmt( aStmtNode -> mHstmt, SQL_UNBIND );
    SQLFreeStmt( aStmtNode -> mHstmt, SQL_RESET_PARAMS );

    sSqlRes = SQLPrepare( aStmtNode -> mHstmt,
                          (SQLCHAR *) aQuery,
                          SQL_NTS );

    /* BUG-31405 : Failover������ Failure of finding statement ���� �߻�. */
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
    /* Note. '\0'���� ���ϵ��� +1 �� ���� �ִ´�.
     * �׷��� "SELECT * FROM t1"�� "SELECT * FROM t1 WHERE i1=1"�� ������ �� �ִ�. */
    aStmtNode->mQueryStrLen = sLen + 1;

    /* ������ binding�� ���� ������� �ʱ�ȭ ���ش�.*/
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
        /* BUG-31405 : Failover������ Failure of finding statement ���� �߻�. */
        if ( *(aSqlcode) == EMBEDED_ALTIBASE_FAILOVER_SUCCESS )
        {
            (void) ulpSetFailoverFlag( aConnNode );
        }

        /* BUG-29745 */
        /* Prepare �� �����ߴٸ� ������ �ʱ�ȭ�ϸ�, ���´� �������� �״�εд�.*/
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
 *     dynamic sql �� sqlda�� ulpBindParamCore �Լ��� ���Ͽ� bind �Ҽ� �ֵ��� ��ȯ�Ͽ� �ش�.
 *
 *  Parameters : ���������� ���� ���ڸ� ������, �Լ����� ��� ���ڰ� �߰�/���ŵ� �� �ִ�.
 *     ulpLibConnNode *aConnNode : env�ڵ�, dbc�ڵ� �׸��� ���� ó���� ���� ����.
 *     ulpLibStmtNode *aStmtNode : statement ���� ������ �����Ѵ�.
 *     SQLHSTMT    *aHstmt    : CLIȣ���� ���� statement �ڵ�.
 *     SQLDA       *aBinda    : dynamic sql�� ���Ǵ� struct.
 *     ulpSqlca    *aSqlca    : ���� ó���� ���� ����.
 *     ulpSqlcode  *aSqlcode  : ���� ó���� ���� ����.
 *     ulpSqlstate *aSqlstate : ���� ó���� ���� ����.
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
 *    host �����鿡 ���� bindingó���� ����Ѵ�.
 *    ���� type�� ���� ulpBindParamCore Ȥ�� ulpBindColCore �Լ��� ȣ����.
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
                /* utpBindColCore �� ������ CLI�Լ� SQLBindCol�� ȣ���Ѵ�.
                 * SQL_C_TYPE �� �������� ǥ�� ���� �����ȴ�. */
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

    /* stmt node �� binding ����&value set*/
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

        /* application host ���� pointer�� �����ؿ�.*/
        for ( sI = 0 ; sI < aSqlstmt->numofhostvar ; sI++ )
        {
            switch ( aSqlstmt->hostvalue[sI].mInOut )
            {
                case H_IN:
                    ACE_ASSERT( aStmtNode->mInHostVarPtr != NULL );

                    aStmtNode->mInHostVarPtr[sInCount].mHostVar =
                            aSqlstmt->hostvalue[sI].mHostVar;
                    /* BUG-27833 :
                        ������ �ٸ����� �����Ͱ� ������ �ֱ� ������ type���� ���ؾ���. */
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
                        ������ �ٸ����� �����Ͱ� ������ �ֱ� ������ type���� ���ؾ���. */
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
                        ������ �ٸ����� �����Ͱ� ������ �ֱ� ������ type���� ���ؾ���. */
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

        /* Statement ���¸� BINDING���� ������.*/
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
 *    SQL������ ���� Execute�� ����Ѵ�.
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

    /* array binding���� Ȯ��. */
    if ( sIsParamArr == ACP_TRUE )
    {
        if( aSqlstmt->statusptr == NULL )
        {
            /* ���� ������ �� parameter���� ��� ���°��� �����ϱ� ����  *
            *  array ������ŭ status�迭 �Ҵ���.                    */
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

        /* statement �Ӽ� SQL_ATTR_PARAM_STATUS_PTR ����. */
        sSqlRes = SQLSetStmtAttr ( *aHstmt,
                                   SQL_ATTR_PARAM_STATUS_PTR,
                                   (SQLPOINTER)sParamStatus,
                                   SQL_IS_POINTER );

        ACI_TEST_RAISE( (sSqlRes == SQL_ERROR) || (sSqlRes == SQL_INVALID_HANDLE),
                        ERR_CLI_SETSTMT );
    }

    /* BUG-30099 : indicator���� �߸��Ȱ��� �ԷµǾ� data-at-execution param���� �νĵǸ�
    Function sequence�����߻�.  */
    /* APRE lib.���� SQL_DATA_AT_EXEC �� �������� �ʴ´�.*/
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

    /* BUG-25643 : apre ���� ONERR ������ �߸� �����մϴ�. */
    /* ONERR ������ �̿��Ͽ� ����ڰ� ���� �ڵ带 ���� �迭�� �����������.*/
    /* Excute�� error�� �߻��ߴٸ�...*/
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
                         /* dml�ϰ�� ERR_TYPE1_DML �ƴϸ�, ERR_TYPE1*/
                         ( (aSqlstmt->stmttype == S_DirectDML) ||
                           (aSqlstmt->stmttype == S_ExecDML) ) ? ERR_TYPE1_DML : ERR_TYPE1 );

    /* array binding���� Ȯ��. */
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
            /* sParamStatus�迭�� �����Ͽ� sqlca->sqlerrd[3]�� ������. */
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

        /* statement �Ӽ� SQL_ATTR_PARAM_STATUS_PTR�� NULL�� ������. */
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

    /* BUG-30099 : indicator���� �߸��Ȱ��� �ԷµǾ� data-at-execution param���� �νĵǸ�
                   Function sequence�����߻�.  */
    ACI_TEST_RAISE( (sSqlResExec == SQL_ERROR)||
                    (sSqlResExec == SQL_INVALID_HANDLE)||
                    (sSqlResExec == SQL_NEED_DATA),
                    ERR_CLI_EXECUTE );

    /* statement ���¸� EXECUTE�� ������. */
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
        /* BUG-31405 : Failover������ Failure of finding statement ���� �߻�. */
        if ( *(aSqlstmt->sqlcodeerr) == EMBEDED_ALTIBASE_FAILOVER_SUCCESS )
        {
            (void) ulpSetFailoverFlag( aConnNode );
        }
    }
    /* BUG-30099 : indicator���� �߸��Ȱ��� �ԷµǾ� data-at-execution param���� �νĵǸ�
                   Function sequence�����߻�.  */
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
 *     ulpRunDirectQuery �Լ������� Unnamed select ������ ���� Fetch�� ����Ѵ�.
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

    /* Ŀ�� ���¸� FETCH�� �ٲ�;*/
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
        /* BUG-31405 : Failover������ Failure of finding statement ���� �߻�. */
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

        /* BUG-31405 : Failover������ Failure of finding statement ���� �߻�. */
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
 *    ulpRunDirectQuery, ulpFor, ulpExecuteImmediate �Լ�����
 *    SQLExecDirect ȣ���ϱ� ���� ����.
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

    /* array binding���� Ȯ��. */
    if ( sIsArr == ACP_TRUE )
    {
        if( aSqlstmt->statusptr == NULL )
        {
            /* ���� ������ �� parameter���� ��� ���°��� �����ϱ� ����  *
             *  array ������ŭ status�迭 �Ҵ���.                    */
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

        /* statement �Ӽ� SQL_ATTR_PARAM_STATUS_PTR ����. */
        sSqlRes = SQLSetStmtAttr ( *aHstmt,
                                   SQL_ATTR_PARAM_STATUS_PTR,
                                   (SQLPOINTER)sParamStatus,
                                   SQL_IS_POINTER );

        ACI_TEST_RAISE( (sSqlRes == SQL_ERROR) || (sSqlRes == SQL_INVALID_HANDLE),
                        ERR_CLI_SETSTMT );
    }

    sSqlResExec = SQLExecDirect( *aHstmt, (SQLCHAR *)aQuery, SQL_NTS );

    /* BUG-25643 : apre ���� ONERR ������ �߸� �����մϴ�. */
    /* ONERR ������ �̿��Ͽ� ����ڰ� ���� �ڵ带 ���� �迭�� �����������.*/
    /* Excute�� error�� �߻��ߴٸ�...*/
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
                         /* EXEC SQL <DML query> (S_DirectDML)Ȥ�� EXEC SQL IMMEDIATE <DML query> (S_ExecIm) �ϰ��
                            ERR_TYPE1_DML�� ���ڷ��־� affected row count�� ������ �۾��� �߰������� �����Ѵ� */
                         ((aSqlstmt->stmttype == S_DirectDML) ||
                          (aSqlstmt->stmttype == S_ExecIm)) ? ERR_TYPE1_DML : ERR_TYPE1
                       );

    /* array binding���� Ȯ��. */
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
            /* sParamStatus�迭�� �����Ͽ� sqlca->sqlerrd[3]�� ������. */
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

        /* statement �Ӽ� SQL_ATTR_PARAM_STATUS_PTR�� NULL�� ������. */
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

    /* statement ���¸� EXECUTE�� ������. */
    if( aStmtNode != NULL )
    {
        /* BUG-28553 : failover�� ��prepare�� �����ʴ� ����.  */
        /* SQLExecDirect �� prepare�� ���� �����Ƿ�, ���´� �״�� S_INITIAL �̴�.*/
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
        /* BUG-31405 : Failover������ Failure of finding statement ���� �߻�. */
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
 *    statement�� close�ϱ� ���� ����. SQL_CLOSE �Ͽ� ��Ȱ�� �����ϴ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    SQLRETURN sSqlRes;

    /* 
     * BUG-36518 Remove SQL_RESET_PARAM in Apre cursor close
     *
     * Cursor�� Close �� �� SQL_RESET_PARAM, SQL_UNBIND�� �����ؼ�
     * Cursor�� Reopen (In Bind) �ϴ� ��� ������ �߻��Ѵ�.
     * �׷��� SQL_CLOSE�� �����ϵ��� ����.
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
 *    Rebinding�� �ʿ����� �˻��ϴ� �Լ�. Col/Param ��� �˻�.
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
                    ������ �ٸ����� �����Ͱ� ������ �ֱ� ������ type���� ���ؾ���. */
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
                ������ �ٸ����� �����Ͱ� ������ �ֱ� ������ type���� ���ؾ���. */
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
                    ������ �ٸ����� �����Ͱ� ������ �ֱ� ������ type���� ���ؾ���. */
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

    return ACP_FALSE; /* ��ȣ��� �ʿ� ����.*/

    ACI_EXCEPTION_END;

    return ACP_TRUE;  /* ��ȣ�� �Ǿ�� �Ѵ�.*/
}


acp_bool_t ulpCheckNeedReBindParamCore( ulpLibStmtNode *aStmtNode,
                                    ulpSqlstmt  *aSqlstmt )
{
/***********************************************************************
 *
 * Description :
 *    Rebinding�� �ʿ����� �˻��ϴ� �Լ�. BindParam�� �˻�.
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
            ������ �ٸ����� �����Ͱ� ������ �ֱ� ������ type���� ���ؾ���. */
        ACI_TEST( aSqlstmt ->hostvalue[sX].mType !=
                  aStmtNode->mInHostVarPtr[sX].mHostVarType );
        ACI_TEST( aSqlstmt ->hostvalue[sX].mHostInd !=
                  aStmtNode->mInHostVarPtr[sX].mHostInd );
    }

    return ACP_FALSE; /* ��ȣ��� �ʿ� ����.*/

    ACI_EXCEPTION_END;

    return ACP_TRUE;  /* ��ȣ�� �Ǿ�� �Ѵ�.*/
}


acp_bool_t ulpCheckNeedReBindColCore( ulpLibStmtNode *aStmtNode,
                                      ulpSqlstmt     *aSqlstmt )
{
/***********************************************************************
 *
 * Description :
 *    Rebinding�� �ʿ����� �˻��ϴ� �Լ�. BindCol�� �˻�.
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
            ������ �ٸ����� �����Ͱ� ������ �ֱ� ������ type���� ���ؾ���. */
        ACI_TEST( aSqlstmt ->hostvalue[sX].mType !=
                  aStmtNode->mOutHostVarPtr[sX].mHostVarType );
        ACI_TEST( aSqlstmt->hostvalue[sX].mHostInd !=
                  aStmtNode->mOutHostVarPtr[sX].mHostInd );
    }

    return ACP_FALSE; /* ��ȣ��� �ʿ� ����.*/

    ACI_EXCEPTION_END;

    return ACP_TRUE;  /* ��ȣ�� �Ǿ�� �Ѵ�.*/

}


acp_bool_t ulpCheckNeedReSetStmtAttrCore( ulpLibStmtNode *aStmtNode,
                                          ulpSqlstmt     *aSqlstmt  )
{
/***********************************************************************
 *
 * Description :
 *    stmtement �Ӽ��� �ٽ� �����ؾ� �ϴ��� �˻����ִ� �Լ�.
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
        return ACP_FALSE; /*SQLSetStmtAttr �� ��ȣ��� �ʿ� ����.*/
    }
    else
    {
        return ACP_TRUE;  /*SQLSetStmtAttr �� ��ȣ�� �Ǿ�� �Ѵ�.*/
    }
}

