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

#include <ulpLibInterCoreFuncB.h>

ACI_RC ulpSetStmtAttrParamCore( ulpLibConnNode *aConnNode,
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
 *    row / column wise binding�� ���� statement �Ӽ� ����.
 *
 * Implementation :
 *
 ***********************************************************************/

    SQLRETURN sSqlRes;

    if ( aSqlstmt->isarr == 1 )
    {
        if ( aSqlstmt->isstruct == 1 )
        {
            sSqlRes = SQLSetStmtAttr( *aHstmt,
                                      SQL_ATTR_PARAM_BIND_TYPE,
                                      (SQLPOINTER)(acp_slong_t)aSqlstmt->structsize,
                                      0 );

            ACI_TEST_RAISE( ( sSqlRes == SQL_ERROR ) || ( sSqlRes == SQL_INVALID_HANDLE )
                            , ERR_CLI_SETSTMT );
        }
        else
        {
            sSqlRes = SQLSetStmtAttr( *aHstmt,
                                      SQL_ATTR_PARAM_BIND_TYPE,
                                      (SQLPOINTER)SQL_BIND_BY_COLUMN,
                                      0 );

            ACI_TEST_RAISE( ( sSqlRes == SQL_ERROR ) || ( sSqlRes == SQL_INVALID_HANDLE )
                            , ERR_CLI_SETSTMT );
        }

        sSqlRes = SQLSetStmtAttr( *aHstmt,
                                  SQL_ATTR_PARAMSET_SIZE,
                                  (SQLPOINTER)(acp_slong_t)aSqlstmt->arrsize,
                                  0 );

        ACI_TEST_RAISE( ( sSqlRes == SQL_ERROR ) || ( sSqlRes == SQL_INVALID_HANDLE )
                        , ERR_CLI_SETSTMT );

    }
    else
    {
        sSqlRes = SQLSetStmtAttr( *aHstmt,
                                  SQL_ATTR_PARAM_BIND_TYPE,
                                  (SQLPOINTER)SQL_BIND_BY_COLUMN,
                                  0 );

        ACI_TEST_RAISE( ( sSqlRes == SQL_ERROR ) || ( sSqlRes == SQL_INVALID_HANDLE )
                        , ERR_CLI_SETSTMT );

        sSqlRes = SQLSetStmtAttr( *aHstmt,
                                  SQL_ATTR_PARAMSET_SIZE,
                                  (SQLPOINTER)1,
                                  0 );

        ACI_TEST_RAISE( ( sSqlRes == SQL_ERROR ) || ( sSqlRes == SQL_INVALID_HANDLE )
                        , ERR_CLI_SETSTMT );
    }

    if( aSqlstmt->isatomic == 1 /*TRUE*/ )
    {
        sSqlRes = SQLSetStmtAttr( *aHstmt,
                                  ALTIBASE_STMT_ATTR_ATOMIC_ARRAY,
                                  (SQLPOINTER) SQL_TRUE,
                                  0 );

        ACI_TEST_RAISE( ( sSqlRes == SQL_ERROR ) || ( sSqlRes == SQL_INVALID_HANDLE )
                        , ERR_CLI_SETSTMT );
    }
    else
    {
        sSqlRes = SQLSetStmtAttr( *aHstmt,
                                  ALTIBASE_STMT_ATTR_ATOMIC_ARRAY,
                                  (SQLPOINTER) SQL_FALSE,
                                  0 );

        ACI_TEST_RAISE( ( sSqlRes == SQL_ERROR ) || ( sSqlRes == SQL_INVALID_HANDLE )
                        , ERR_CLI_SETSTMT );
    }
/*
    sSqlRes = SQLSetStmtAttr( *aHstmt,
                              SQL_ATTR_PARAM_STATUS_PTR,
                              (SQLPOINTER) aSqlstmt -> statusptr,
                              0 );

    ACI_TEST_RAISE( ( sSqlRes == SQL_ERROR ) || ( sSqlRes == SQL_INVALID_HANDLE )
                    , ERR_CLI_SETSTMT );
*/
    /* stmt Node�� stmt�Ӽ� ���� set*/
    if ( aStmtNode != NULL )
    {
        aStmtNode -> mBindInfo.mIsArray    = (acp_bool_t) aSqlstmt -> isarr ;
        aStmtNode -> mBindInfo.mArraySize  =
                (aSqlstmt -> isarr == 1)?aSqlstmt -> arrsize : 0;
        aStmtNode -> mBindInfo.mIsStruct   = (acp_bool_t) aSqlstmt -> isstruct ;
        aStmtNode -> mBindInfo.mStructSize =
                (aSqlstmt -> isstruct == 1)?aSqlstmt -> structsize : 0;
        aStmtNode -> mBindInfo.mIsAtomic   = (acp_bool_t) aSqlstmt -> isatomic ;
        /*aStmtNode -> mStatusPtr            = (void*) aSqlstmt -> statusptr ;*/
        /* Statement ���¸� SETSTMTATTR�� ������.*/
        aStmtNode -> mStmtState = S_SETSTMTATTR;
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
    ACI_EXCEPTION_END;

    return ACI_FAILURE;

}


ACI_RC ulpSetStmtAttrRowCore(   ulpLibConnNode *aConnNode,
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
 *  array fetch�� ���� statement �Ӽ� ����.
 *
 * Implementation :
 *
 ***********************************************************************/

    SQLRETURN sSqlRes;

    if ( aSqlstmt->isarr == 1 )
    {
        if ( aSqlstmt->isstruct == 1 )
        {
            sSqlRes = SQLSetStmtAttr( *aHstmt,
                                      SQL_ATTR_ROW_BIND_TYPE,
                                      (SQLPOINTER)(acp_slong_t)aSqlstmt->structsize,
                                      0 );

            ACI_TEST_RAISE( ( sSqlRes == SQL_ERROR ) || ( sSqlRes == SQL_INVALID_HANDLE )
                            , ERR_CLI_SETSTMT );
        }
        else
        {
            sSqlRes = SQLSetStmtAttr( *aHstmt,
                                      SQL_ATTR_ROW_BIND_TYPE,
                                      (SQLPOINTER)SQL_BIND_BY_COLUMN,
                                      0 );

            ACI_TEST_RAISE( ( sSqlRes == SQL_ERROR ) || ( sSqlRes == SQL_INVALID_HANDLE )
                            , ERR_CLI_SETSTMT );
        }

        sSqlRes = SQLSetStmtAttr( *aHstmt,
                                  SQL_ATTR_ROW_ARRAY_SIZE,
                                  (SQLPOINTER)(acp_slong_t)aSqlstmt->arrsize,
                                  0 );

        ACI_TEST_RAISE( ( sSqlRes == SQL_ERROR ) || ( sSqlRes == SQL_INVALID_HANDLE )
                        , ERR_CLI_SETSTMT );

    }
    else
    {
        sSqlRes = SQLSetStmtAttr( *aHstmt,
                                  SQL_ATTR_ROW_BIND_TYPE,
                                  (SQLPOINTER)SQL_BIND_BY_COLUMN,
                                  0 );

        ACI_TEST_RAISE( ( sSqlRes == SQL_ERROR ) || ( sSqlRes == SQL_INVALID_HANDLE )
                        , ERR_CLI_SETSTMT );

        sSqlRes = SQLSetStmtAttr( *aHstmt,
                                  SQL_ATTR_ROW_ARRAY_SIZE,
                                  (SQLPOINTER)1,
                                  0 );

        ACI_TEST_RAISE( ( sSqlRes == SQL_ERROR ) || ( sSqlRes == SQL_INVALID_HANDLE )
                        , ERR_CLI_SETSTMT );
    }

    sSqlRes = SQLSetStmtAttr( *aHstmt,
                               SQL_ATTR_ROW_STATUS_PTR,
                               NULL,
                               0 );

    ACI_TEST_RAISE( ( sSqlRes == SQL_ERROR ) || ( sSqlRes == SQL_INVALID_HANDLE )
                    , ERR_CLI_SETSTMT );

    if ( aStmtNode != NULL )
    {
        aStmtNode -> mBindInfo.mIsArray    = (acp_bool_t) aSqlstmt -> isarr ;
        aStmtNode -> mBindInfo.mArraySize  =
                                 (aSqlstmt -> isarr == 1)?aSqlstmt -> arrsize : 0;
        aStmtNode -> mBindInfo.mIsStruct   = (acp_bool_t) aSqlstmt -> isstruct ;
        aStmtNode -> mBindInfo.mStructSize =
                                 (aSqlstmt -> isstruct == 1)?aSqlstmt -> structsize : 0;
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
    ACI_EXCEPTION_END;

    return ACI_FAILURE;

}

/**************************************************************************
 * Description : dynamic sql bind�� ���� statement ����
 *               array bind�� ���� �����Ѵ�.
 *
 **************************************************************************/
ACI_RC ulpSetStmtAttrDynamicParamCore( ulpLibConnNode *aConnNode,
                                       ulpLibStmtNode *aStmtNode,
                                       SQLHSTMT       *aHstmt,
                                       ulpSqlca       *aSqlca,
                                       ulpSqlcode     *aSqlcode,
                                       ulpSqlstate    *aSqlstate )
{

    SQLRETURN sSqlRes;

    sSqlRes = SQLSetStmtAttr( *aHstmt,
                              SQL_ATTR_ROW_BIND_TYPE,
                              (SQLPOINTER) SQL_BIND_BY_COLUMN,
                              0 );
    ACI_TEST_RAISE( (sSqlRes == SQL_ERROR) || (sSqlRes == SQL_INVALID_HANDLE),
                    ERR_CLI_SETSTMT );
    
    if ( aStmtNode != NULL )
    {
        aStmtNode->mBindInfo.mIsStruct = ACP_FALSE;
        aStmtNode->mStmtState = S_SETSTMTATTR;
    }
    else
    {
        /* do nothing */
    }
    
    return ACI_SUCCESS;

    ACI_EXCEPTION( ERR_CLI_SETSTMT )
    {
        (void) ulpSetErrorInfo4CLI( aConnNode,
                                    *aHstmt,
                                    (SQLRETURN)SQL_ERROR,
                                    aSqlca,
                                    aSqlcode,
                                    aSqlstate,
                                    ERR_TYPE2 );
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/**************************************************************************
 * Description : dynamic sql fetch�� ���� statement ����
 *
 **************************************************************************/
ACI_RC ulpSetStmtAttrDynamicRowCore( ulpLibConnNode *aConnNode,
                                     ulpLibStmtNode *aStmtNode,
                                     SQLHSTMT       *aHstmt,
                                     ulpSqlca       *aSqlca,
                                     ulpSqlcode     *aSqlcode,
                                     ulpSqlstate    *aSqlstate )
{
    SQLRETURN sSqlRes;

    sSqlRes = SQLSetStmtAttr( *aHstmt,
                              SQL_ATTR_ROW_BIND_TYPE,
                              (SQLPOINTER) SQL_BIND_BY_COLUMN,
                              0 );
    ACI_TEST_RAISE( (sSqlRes == SQL_ERROR) || (sSqlRes == SQL_INVALID_HANDLE),
                    ERR_CLI_SETSTMT );
    
    sSqlRes = SQLSetStmtAttr( *aHstmt,
                              SQL_ATTR_ROW_ARRAY_SIZE,
                              (SQLPOINTER) 1,
                              0 );
    ACI_TEST_RAISE( (sSqlRes == SQL_ERROR) || (sSqlRes == SQL_INVALID_HANDLE),
                    ERR_CLI_SETSTMT );

    sSqlRes = SQLSetStmtAttr( *aHstmt,
                              SQL_ATTR_ROW_STATUS_PTR,
                              NULL,
                              0 );
    ACI_TEST_RAISE( (sSqlRes == SQL_ERROR) || (sSqlRes == SQL_INVALID_HANDLE),
                    ERR_CLI_SETSTMT );

    if ( aStmtNode != NULL )
    {
        aStmtNode->mBindInfo.mIsArray = ACP_FALSE;
        aStmtNode->mBindInfo.mArraySize = 0;
        aStmtNode->mBindInfo.mIsStruct = ACP_FALSE;
    }
    else
    {
        /* do nothing */
    }
    
    return ACI_SUCCESS;

    ACI_EXCEPTION( ERR_CLI_SETSTMT )
    {
        (void) ulpSetErrorInfo4CLI( aConnNode,
                                    *aHstmt,
                                    (SQLRETURN)SQL_ERROR,
                                    aSqlca,
                                    aSqlcode,
                                    aSqlstate,
                                    ERR_TYPE2 );
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}


ACI_RC ulpBindParamCore( ulpLibConnNode *aConnNode,
                         SQLHSTMT       *aHstmt,
                         ulpSqlstmt     *aSqlstmt,
                         ulpHostVar     *aHostVar,
                         acp_sint16_t    aIndex,
                         ulpSqlca       *aSqlca,
                         ulpSqlcode     *aSqlcode,
                         ulpSqlstate    *aSqlstate,
                         SQLSMALLINT     aInOut )
{
/***********************************************************************
 *
 * Description :
 *    in/out type host ������ ���� bindparam�� ó���Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    SQLRETURN   sSqlRes;
    SQLSMALLINT sCType;
    SQLSMALLINT sSqlType;
    SQLULEN     sPrecision;
    acp_bool_t  sFixedInd;

    sPrecision = 0;
    sFixedInd  = ACP_FALSE;
    sCType = sSqlType = SQL_UNKNOWN_TYPE;

    if ( aHostVar->mType == H_BLOB_FILE )
    {

        sSqlRes = SQLBindFileToParam( *aHstmt,
                                      (SQLSMALLINT)aIndex,
                                      SQL_BLOB,
                                      (SQLCHAR *)aHostVar->mHostVar,
                                      NULL,
                                      aHostVar->mFileopt,
                                      aHostVar->mMaxlen,
                                      aHostVar->mHostInd );

        ACI_TEST_RAISE( (sSqlRes == SQL_ERROR) || (sSqlRes == SQL_INVALID_HANDLE)
                        , ERR_CLI_BINDFILEPARAM );
    }
    else if ( aHostVar->mType == H_CLOB_FILE )
    {

        sSqlRes = SQLBindFileToParam( *aHstmt,
                                      (SQLSMALLINT)aIndex,
                                      SQL_CLOB,
                                      (SQLCHAR *)aHostVar->mHostVar,
                                      NULL,
                                      aHostVar->mFileopt,
                                      aHostVar->mMaxlen,
                                      aHostVar->mHostInd );

        ACI_TEST_RAISE( (sSqlRes == SQL_ERROR) || (sSqlRes == SQL_INVALID_HANDLE)
                        , ERR_CLI_BINDFILEPARAM );
    }
    else
    {
        switch ( aHostVar->mType )
        {
            case H_NUMERIC :
                sSqlType = SQL_NUMERIC;
                sCType   = SQL_C_CHAR;
                if ( aHostVar->mLen > 0 )
                {
                    sPrecision = (SQLULEN)( aHostVar->mLen - 1 );
                }
                else
                {
                    sPrecision = 1;
                }
                break;
            case H_INTEGER :
                sSqlType = SQL_INTEGER;
                sCType   = SQL_C_SLONG;
                sPrecision = 0;
                break;
            case H_INT :
                if ( aHostVar->mUnsign == 1 )
                {
                    sSqlType = SQL_BIGINT;
                    if (aHostVar->mSizeof == 2)
                    {
                        sCType = SQL_C_USHORT;
                    }
                    else if (aHostVar->mSizeof == 4)
                    {
                        sCType = SQL_C_ULONG;
                    }
                    else if (aHostVar->mSizeof == 8) 
                    {
                        sCType = SQL_C_UBIGINT;
                    }
                }
                else
                {
                    sSqlType = SQL_INTEGER;
                    if (aHostVar->mSizeof == 2)
                    {
                        sCType = SQL_C_SSHORT;
                    }
                    else if (aHostVar->mSizeof == 4) 
                    {
                        sCType = SQL_C_SLONG;
                    }
                    else if (aHostVar->mSizeof == 8) 
                    {
                        sCType = SQL_C_SBIGINT;
                    }
                }
                sPrecision = 0;
                break;
            case H_LONG  :
            case H_LONGLONG:
                sSqlType = SQL_BIGINT;
                if ( aHostVar->mUnsign == 1 )
                {
                    if (aHostVar->mSizeof == 2)
                    {
                        sCType = SQL_C_USHORT;
                    }
                    else if (aHostVar->mSizeof == 4)
                    {
                        sCType = SQL_C_ULONG;
                    }
                    else if (aHostVar->mSizeof == 8)
                    {
                        sCType = SQL_C_UBIGINT;
                    }
                }
                else
                {
                    if (aHostVar->mSizeof == 2)
                    {
                        sCType = SQL_C_SSHORT;
                    }
                    else if (aHostVar->mSizeof == 4)
                    {
                        sCType = SQL_C_SLONG;
                    }
                    else if (aHostVar->mSizeof == 8)
                    {
                        sCType = SQL_C_SBIGINT;
                    }
                }
                sPrecision = 0;
                break;
            case H_SHORT :
                if (aHostVar->mUnsign == 1)
                {
                    sSqlType = SQL_INTEGER;
                    if (aHostVar->mSizeof == 2)
                    {
                        sCType = SQL_C_USHORT;
                    }
                    else if (aHostVar->mSizeof == 4)
                    {
                        sCType = SQL_C_ULONG;
                    }
                    else if (aHostVar->mSizeof == 8)
                    {
                        sCType = SQL_C_UBIGINT;
                    }
                }
                else
                {
                    sSqlType = SQL_SMALLINT;
                    if (aHostVar->mSizeof == 2)
                    {
                        sCType = SQL_C_SSHORT;
                    }
                    else if (aHostVar->mSizeof == 4)
                    {
                        sCType = SQL_C_SLONG;
                    }
                    else if (aHostVar->mSizeof == 8)
                    {
                        sCType = SQL_C_SBIGINT;
                    }
                }
                sPrecision = 0;
                break;
            case H_CHAR    :
                sCType   = SQL_C_CHAR;
                sSqlType = SQL_CHAR;
                if (aHostVar->mLen == 0 && !(aHostVar->mIsDynAlloc))
                {
                    sPrecision = 1;
                }
                else if ( ulpGetStmtOption( aSqlstmt,
                                            ULP_OPT_NOT_NULL_PAD ) == ACP_TRUE )
                {
                    sCType     = SQL_C_BINARY;
                    sFixedInd  = ACP_TRUE;
                    sPrecision = (SQLULEN)aHostVar->mLen;
                }
                else
                {
                    sPrecision = (SQLULEN)(aHostVar->mLen - 1);
                }
                break;
            case H_VARCHAR :
                sCType   = SQL_C_CHAR;
                sSqlType = SQL_VARCHAR;
                if ( (aHostVar->mLen == 0) && !( aHostVar->mIsDynAlloc ) )
                {
                    sPrecision = 1;
                }
                else if ( ulpGetStmtOption( aSqlstmt,
                                            ULP_OPT_NOT_NULL_PAD ) == ACP_TRUE )
                {
                    sCType     = SQL_C_BINARY;
                    sFixedInd  = ACP_TRUE;
                    sPrecision = (SQLULEN)aHostVar->mLen;
                }
                else
                {
                    sPrecision = (SQLULEN)(aHostVar->mLen - 1);
                }
                break;
            case H_NCHAR:
            case H_NVARCHAR:
                sSqlType =
                        (aHostVar->mType == H_NCHAR)? SQL_WCHAR: SQL_WVARCHAR;

                /* utf8�� �������ϰ� utf16�� �����Ѵ�.(nchar_utf16 �ɼ��� �� ���)*/
                if ( gUlpLibOption.mIsNCharUTF16 == ACP_TRUE )
                {
                    /* parameter binding�ÿ��� �ݵ�� indicator�� �����ؾ� �Ѵ�*/
                    ACI_TEST_RAISE(aHostVar->mHostInd == NULL, ERR_NO_IND_4NCHAR);
                    sCType     = SQL_C_WCHAR;
                    sPrecision = (SQLULEN)(aHostVar->mLen);
                }
                /* default (nchar_utf16 �ɼ��� ���� ���� ���)*/
                else
                {
                    sCType     = SQL_C_CHAR;
                    sPrecision = (SQLULEN)(aHostVar->mLen - 1);
                }
                /*======================================*/
                /* binding�� ���� mtdEstimate()���� �˻��ϴ�*/
                /* national charset �ִ� precision (���ڼ��� �ǹ�)*/
                /* utf16: 32767, utf8: 16383.*/
                /* qtc.cpp:fixAfterBinding()���� cralloc�� �ϴµ�*/
                /* ncs�� utf16�̸� precision *2, utf8�̸� *4�� ��ŭ �Ҵ�*/
                /* �츮�� utf16/utf8���� �𸣹Ƿ� �ϴ� 2�� ������ �д�*/
                /* ��, pointer(nchar only)Ÿ���� ��� length�� SES_MAX_CHAR*/
                /* (65001)�� ���õǴµ�, nchar limit�� �����ʱ� ����*/
                /* 4�� ���� 16250 �� precision���� ����*/
                /* cf) nchar utf8���� buffer size�� 32767 �̻��̸�*/
                /*     �������� error�� �߻���ų ���̴�*/
                /*======================================*/
                if (aHostVar->mIsDynAlloc != 0)
                {
                    sPrecision /= 4;
                }
                else
                {
                    sPrecision /= 2;
                }

                if ((aHostVar->mLen == 0) && !(aHostVar->mIsDynAlloc))
                {
                    sPrecision = 1;
                }

                break;
            case H_BINARY :
                sSqlType = SQL_BLOB;
                sCType   = SQL_C_BINARY;
                sPrecision = ACP_SINT32_MAX;
                break;
            case H_BIT :
                sSqlType = SQL_VARBIT;
                sCType   = SQL_C_BINARY;
                if (aHostVar->mLen > 4)
                {
                    sPrecision = (SQLULEN)((aHostVar->mLen - 4) * 8);
                }
                else
                {
                    sPrecision = 0;
                }
                break;
            case H_BYTES :
                sSqlType = SQL_BYTES;
                sCType   = SQL_C_BINARY;
                sPrecision = (SQLULEN)aHostVar->mLen;
                break;
            case H_VARBYTE :
                sSqlType = SQL_VARBYTE;
                sCType   = SQL_C_BINARY;
                sPrecision = (SQLULEN)aHostVar->mLen;
                break;
            case H_NIBBLE :
                sSqlType = SQL_NIBBLE;
                sCType   = SQL_C_BINARY;
                if (aHostVar->mLen > 1)
                {
                    sPrecision = (SQLULEN)((aHostVar->mLen - 1) * 2);
                }
                else
                {
                    sPrecision = 0;
                }
                break;
            case H_FLOAT :
                sSqlType = SQL_REAL;
                sCType   = SQL_C_FLOAT;
                sPrecision = 0;
                break;
            case H_DOUBLE :
                sSqlType = SQL_DOUBLE;
                sCType   = SQL_C_DOUBLE;
                sPrecision = 0;
                break;
            case H_DATE :
                sSqlType = SQL_TYPE_DATE;
                sCType   = SQL_C_TYPE_DATE;
                sPrecision = 0;
                break;
            case H_TIME :
                sSqlType = SQL_TYPE_TIME;
                sCType   = SQL_C_TYPE_TIME;
                sPrecision = 0;
                break;
            case H_TIMESTAMP :
                sSqlType = SQL_TYPE_TIMESTAMP;
                sCType   = SQL_C_TYPE_TIMESTAMP;
                sPrecision = 0;
                break;
            case H_BLOB :
                sSqlType = SQL_BLOB;
                sCType   = SQL_C_BINARY;
                sPrecision = ACP_SINT32_MAX;
                break;
            case H_CLOB :
                sSqlType = SQL_CLOB;
                sCType   = SQL_C_CHAR;
                sPrecision = ACP_SINT32_MAX;
                break;
            case H_BLOBLOCATOR :
                sSqlType = SQL_BLOB_LOCATOR;
                sCType   = SQL_C_BLOB_LOCATOR;
                sPrecision = 0;
                break;
            case H_CLOBLOCATOR :
                sSqlType = SQL_CLOB_LOCATOR;
                sCType   = SQL_C_CLOB_LOCATOR;
                sPrecision = 0;
                break;
        }

        sSqlRes = SQLBindParameter( *aHstmt,
                                    (SQLUSMALLINT)aIndex,
                                    aInOut,
                                    sCType,
                                    sSqlType,
                                    sPrecision,
                                    0,
                                    (SQLPOINTER)aHostVar->mHostVar,
                                    aHostVar->mMaxlen,
                                    aHostVar->mHostInd );

        ACI_TEST_RAISE( (sSqlRes == SQL_ERROR) || (sSqlRes == SQL_INVALID_HANDLE)
                        , ERR_CLI_BINDPARAM );
    }

    if ( sFixedInd == ACP_TRUE )
    {
        sSqlRes = SQLSetStmtAttr( *aHstmt,
                                  SQL_ATTR_INPUT_NTS,
                                  (SQLPOINTER)SQL_FALSE,
                                  0);

        ACI_TEST_RAISE( (sSqlRes == SQL_ERROR) || (sSqlRes == SQL_INVALID_HANDLE)
                        , ERR_CLI_SETSTMT );
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION ( ERR_CLI_BINDFILEPARAM );
    {
        ulpSetErrorInfo4CLI( aConnNode,
                             *aHstmt,
                             SQL_ERROR,
                             aSqlca,
                             aSqlcode,
                             aSqlstate,
                             ERR_TYPE2 );

    }
    ACI_EXCEPTION ( ERR_CLI_BINDPARAM );
    {
        ulpSetErrorInfo4CLI( aConnNode,
                             *aHstmt,
                             SQL_ERROR,
                             aSqlca,
                             aSqlcode,
                             aSqlstate,
                             ERR_TYPE2 );

    }
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
    ACI_EXCEPTION ( ERR_NO_IND_4NCHAR );
    {
        ulpErrorMgr sErrorMgr;
        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_NCHAR_No_Indicator_Error );

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

ACI_RC ulpBindColCore( ulpLibConnNode *aConnNode,
                       SQLHSTMT       *aHstmt,
                       ulpSqlstmt     *aSqlstmt,
                       ulpHostVar     *aHostVar,
                       acp_sint16_t    aIndex,
                       ulpSqlca       *aSqlca,
                       ulpSqlcode     *aSqlcode,
                       ulpSqlstate    *aSqlstate )
{
/***********************************************************************
 *
 * Description :
 *    out type host ���������� bindcol�� ó���Ѵ�.
 *    
 *
 * Implementation :
 *
 ***********************************************************************/

    acp_sint32_t sLen;
    SQLRETURN    sSqlRes;
    SQLSMALLINT  sCType = SQL_UNKNOWN_TYPE;



    sLen = aHostVar->mLen;

    if ( (aHostVar->mType == H_BLOB_FILE) ||
         (aHostVar->mType == H_CLOB_FILE) )
    {
        sSqlRes = SQLBindFileToCol( *aHstmt,
                                    (SQLSMALLINT)aIndex,
                                    (SQLCHAR *)aHostVar->mHostVar,
                                    NULL,
                                    aHostVar->mFileopt,
                                    (SQLLEN)sLen,
                                    aHostVar->mHostInd );

        ACI_TEST_RAISE( (sSqlRes == SQL_ERROR) || (sSqlRes == SQL_INVALID_HANDLE)
                        , ERR_CLI_BINDFILECOL );
    }
    else
    {
        switch ( aHostVar->mType )
        {
            case H_INTEGER :
                sCType = SQL_C_SLONG;
                break;
            case H_INT   :
            case H_LONG  :
            case H_LONGLONG:
            case H_SHORT :
                if ( aHostVar->mUnsign == 1 )
                {
                    if (aHostVar->mSizeof == 2)
                    {
                        sCType = SQL_C_USHORT;
                    }
                    else if (aHostVar->mSizeof == 4)
                    {
                        sCType = SQL_C_ULONG;
                    }
                    else if (aHostVar->mSizeof == 8)
                    {
                        sCType = SQL_C_UBIGINT;
                    }
                }
                else
                {
                    if (aHostVar->mSizeof == 2)
                    {
                        sCType = SQL_C_SSHORT;
                    }
                    else if (aHostVar->mSizeof == 4)
                    {
                        sCType = SQL_C_SLONG;
                    }
                    else if (aHostVar->mSizeof == 8)
                    {
                        sCType = SQL_C_SBIGINT;
                    }
                }
                break;
            case H_CHAR    :
            case H_VARCHAR :
                if ( aHostVar->mLen == 0 && !(aHostVar->mIsDynAlloc) )
                {
                    sCType = SQL_C_BINARY;
                    sLen   = 1;
                }
                else if ( ulpGetStmtOption( aSqlstmt,
                                            ULP_OPT_NOT_NULL_PAD ) == ACP_TRUE )
                {
                    sCType = SQL_C_BINARY;
                }
                else
                {
                    sCType = SQL_C_CHAR;
                }
                break;
            case H_NCHAR:
            case H_NVARCHAR:
                if ( aHostVar->mLen == 0 && !(aHostVar->mIsDynAlloc) )
                {
                    sCType = SQL_C_BINARY;
                    sLen   = 1;
                }
                else
                {
                    if ( gUlpLibOption.mIsNCharUTF16 != ACP_TRUE )
                    {
                        sCType = SQL_C_CHAR;
                    }
                    else
                    {
                        sCType = SQL_C_WCHAR;
                    }
                }
                break;
            case H_FLOAT  :
                sCType = SQL_C_FLOAT;
                break;
            case H_DOUBLE :
                sCType = SQL_C_DOUBLE;
                break;
            case H_DATE   :
                sCType = SQL_C_TYPE_DATE;
                break;
            case H_TIME   :
                sCType = SQL_C_TYPE_TIME;
                break;
            case H_TIMESTAMP :
                sCType = SQL_C_TYPE_TIMESTAMP;
                break;
            case H_NIBBLE :
            case H_BYTES  :
            case H_VARBYTE:
            case H_BIT    :
                sCType = SQL_C_BINARY;
                break;
            case H_BLOB   :
            case H_BINARY :
                sCType = SQL_C_BINARY;
                if ( aHostVar->mIsDynAlloc == 1 )
                {
                    if ( aHostVar->mHostInd != NULL )
                    {
                        if ( *(aHostVar->mHostInd) > 0 )
                        {
                            sLen = *(aHostVar->mHostInd);
                        }
                        else
                        {
                            sLen = ACP_SINT32_MAX;
                        }
                    }
                    else
                    {
                        sLen = ACP_SINT32_MAX;
                    }
                }
                break;
            case H_CLOB   :
                sCType = SQL_C_CHAR;
                if ( aHostVar->mIsDynAlloc == 1 )
                {
                    if ( aHostVar->mHostInd != NULL )
                    {
                        if ( *(aHostVar->mHostInd) > 0 )
                        {
                            sLen = *(aHostVar->mHostInd);
                        }
                        else
                        {
                            sLen = ACP_SINT32_MAX;
                        }
                    }
                    else
                    {
                        sLen = ACP_SINT32_MAX;
                    }
                }
                break;
            case H_BLOBLOCATOR :
                sCType = SQL_C_BLOB_LOCATOR;
                break;
            case H_CLOBLOCATOR :
                sCType = SQL_C_CLOB_LOCATOR;
                break;
        }

        sSqlRes = SQLBindCol( *aHstmt,
                              (SQLUSMALLINT)aIndex,
                              sCType,
                              (SQLPOINTER)aHostVar->mHostVar,
                              (SQLLEN)sLen,
                              aHostVar->mHostInd);

        ACI_TEST_RAISE( (sSqlRes == SQL_ERROR) || (sSqlRes == SQL_INVALID_HANDLE)
                        , ERR_CLI_BINDCOL );
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION ( ERR_CLI_BINDFILECOL );
    {
        ulpSetErrorInfo4CLI( aConnNode,
                             *aHstmt,
                             SQL_ERROR,
                             aSqlca,
                             aSqlcode,
                             aSqlstate,
                             ERR_TYPE2 );

    }
    ACI_EXCEPTION ( ERR_CLI_BINDCOL );
    {
        ulpSetErrorInfo4CLI( aConnNode,
                             *aHstmt,
                             SQL_ERROR,
                             aSqlca,
                             aSqlcode,
                             aSqlstate,
                             ERR_TYPE2 );

    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulpSetDateFmtCore( ulpLibConnNode *aConnNode )
{
/***********************************************************************
 *
 * Description :
 *
 *      DBC�� ALTIBASE_DATE_FORMAT�� �������ش�.
 *      ȯ�溯�� ALTIBASE_DATE_FORMAT�� �����Ǿ������� ȯ�溯���� �����ϰ�,
 *      �׷��� ������ �⺻���� ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    acp_char_t *sDateFmt;

    /* BUG-41471 Maintain one APRE source code for HDB and HDB-DA */
    if ( acpEnvGet( (acp_char_t *)ENV_ALTIBASE_DATE_FORMAT, &sDateFmt ) == ACP_RC_SUCCESS )
    {
        ACI_TEST( SQLSetConnectAttr( aConnNode->mHdbc,
                                     ALTIBASE_DATE_FORMAT,
                                     (SQLPOINTER)sDateFmt, SQL_NTS ) );
    }
    else
    {
        /* do nothing */
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}


void ulpSetColRowSizeCore( ulpSqlstmt *aSqlstmt )
{
/***********************************************************************
 *
 * Description :
 *      ulpSqlstmt ����ü�� host�����鿡 ���� isarr/isstruct ������ �о� array/rowwise
 *      binding���θ� �����Ͽ� ulpSqlstmt.isarr �� ulpSqlstmt.arrsize �ʵ带 ������.
 *
 * Implementation :
 *      �ٸ� type�� ȣ��Ʈ ������ �ö����� �˻��Ͽ� arrsize�� �ּҰ����� ������.
 ***********************************************************************/
    acp_sint32_t   sI;
    acp_uint32_t   sSize;

    sSize  = 0;
    aSqlstmt->isarr   = 0;
    aSqlstmt->arrsize = 0;

    aSqlstmt->isstruct = 0;
    aSqlstmt->structsize = 0;

    if ( aSqlstmt->numofhostvar > 0 )
    {
        /* BUG-32952 */
        /* set array size */
        for ( sI = 0 ;
              aSqlstmt->numofhostvar > sI;
              sI++ )
        {
            if ( aSqlstmt->hostvalue[sI].isarr != 0 )
            {
                if ( (sSize == 0) || (sSize > aSqlstmt->hostvalue[sI].arrsize) )
                {
                    sSize = aSqlstmt->hostvalue[sI].arrsize;
                    aSqlstmt->isarr = 1;
                    aSqlstmt->arrsize = sSize;
                }
            }
        }

        /* set struct size */
        for ( sI = 0 ;
              aSqlstmt->numofhostvar > sI;
              sI++ )
        {
            if ( aSqlstmt->hostvalue[sI].isstruct != 0 )
            {
                sSize = aSqlstmt->hostvalue[sI].structsize;
                aSqlstmt->isstruct = 1;
                aSqlstmt->structsize = sSize;
                break;
            }
        }
    }
}


/* BUG-31405 : Failover������ Failure of finding statement ���� �߻�. */
void ulpSetFailoverFlag( ulpLibConnNode *aConnNode )
{
/***********************************************************************
 *
 * Description :
 *      FailOver���� �����ѵ� apre������ �ش� Ŀ�ؼǿ� ���Ե� stmt���� �ٽ� prepare�Ѵ�.
 *      ���߿� �ش籸���� �����ɶ� reprepare�ǵ��� mNeedReprepare�� true�� �������ش�.
 *
 * Implementation :
 *
 ***********************************************************************/

    acp_sint32_t    i;
    ulpLibStmtNode *sStmtNode;

    sStmtNode = NULL;

    /***************************
    * Reprepare statements    *
    ***************************/

    /* 1.1 statements in the unnamedStmtCacheList */
    if ( aConnNode->mUnnamedStmtCacheList.mNumNode > 0 )
    {
        sStmtNode = aConnNode->mUnnamedStmtCacheList.mList;
        for ( ; sStmtNode != NULL ; sStmtNode = sStmtNode->mNextStmt )
        {
            if ( (sStmtNode->mStmtState   != S_INITIAL) &&
                  (sStmtNode->mQueryStr    != NULL) )
            {
                sStmtNode->mNeedReprepare = ACP_TRUE;
            }
        }
    }

    /* 1.2 statements in the statement hash table */
    if ( aConnNode->mStmtHashT.mNumNode > 0 )
    {
        for( i = 0 ; i < MAX_NUM_STMT_HASH_TAB_ROW ; i++ )
        {
            sStmtNode = aConnNode->mStmtHashT.mTable[i].mList;
            for ( ; sStmtNode != NULL ; sStmtNode = sStmtNode->mNextStmt )
            {
                if ( (sStmtNode->mStmtState   != S_INITIAL) &&
                      (sStmtNode->mQueryStr    != NULL) )
                {
                    if ( sStmtNode->mCursorName[0] != '\0' )
                    {   /* cursor�� ��� Freestmt��. */
                        SQLFreeStmt( sStmtNode->mHstmt, SQL_CLOSE );
                    }
                    sStmtNode->mNeedReprepare = ACP_TRUE;
                }
            }
        }
    }

    /* 1.3 statements in the cursor hash table */
    if ( aConnNode->mCursorHashT.mNumNode > 0 )
    {
        for( i = 0 ; i < MAX_NUM_STMT_HASH_TAB_ROW ; i++ )
        {
            sStmtNode = aConnNode->mCursorHashT.mTable[i].mList;
            for ( ; sStmtNode != NULL ; sStmtNode = sStmtNode->mNextStmt )
            {
                /* ������ stmt hash table�� �˻��ϸ鼭 reprepare�� �̹� �߱⶧����,
                   mStmtName�� ���� ��츸 ����ϸ� �ȴ�. */
                if( (sStmtNode->mStmtName[0] == '\0')      &&
                     (sStmtNode->mCurState    != C_INITIAL) &&
                     (sStmtNode->mQueryStr    != NULL) )
                {
                    SQLFreeStmt( sStmtNode->mHstmt, SQL_CLOSE );
                    sStmtNode->mNeedReprepare = ACP_TRUE;
                }
            }
        }
    }

    aConnNode->mFailoveredJustnow = ACP_FALSE;
}

/* BUG-25643 : apre ���� ONERR ������ �߸� �����մϴ�. */
ACI_RC ulpGetOnerrErrCodeCore( ulpLibConnNode *aConnNode,
                               ulpSqlstmt     *aSqlstmt,
                               SQLHSTMT       *aHstmt,
                               acp_sint32_t   *aErrCode )
{
/***********************************************************************
 *
 * Description :
 *      ONERR ������ ���Ǿ��� ��� ����ڰ� ������ �迭�� �� row�� ����
 *      native error code�� �������ش�.
 *
 * @param[out] aErrCode
 *      ����� ���� error code �迭 pointer.
 ***********************************************************************/

    acp_sint32_t i;
    SQLRETURN    sSQLRC;
    acp_sint32_t sNDiagRec;
    SQLLEN       sRowNo;
    acp_sint16_t sStrLen;
    acp_sint32_t sSQLCODE;

    /* Diag rec. ���� �˾Ƴ�. */
    sSQLRC = SQLGetDiagField(SQL_HANDLE_STMT,
                             (SQLHANDLE)*aHstmt,
                             0,
                             SQL_DIAG_NUMBER,
                             (SQLPOINTER)&sNDiagRec,
                             SQL_IS_POINTER,
                             (SQLSMALLINT *)&sStrLen);

    ACI_TEST_RAISE( (sSQLRC == SQL_INVALID_HANDLE)||
                    (sSQLRC == SQL_ERROR), ERR_CLI_GETDIAGFIELD );

    for( i = 1 ; i <= sNDiagRec ; i++ )
    {
        /* �� ��ȣ �˾Ƴ�. */
        sSQLRC = SQLGetDiagField(SQL_HANDLE_STMT,
                                 (SQLHANDLE)*aHstmt,
                                 (SQLSMALLINT)i,
                                 SQL_DIAG_ROW_NUMBER,
                                 (SQLPOINTER)&sRowNo,
                                 SQL_IS_POINTER,
                                 (SQLSMALLINT *)&sStrLen);

        ACI_TEST_RAISE( (sSQLRC == SQL_INVALID_HANDLE)||
                        (sSQLRC == SQL_ERROR), ERR_CLI_GETDIAGFIELD );

        /* native error code �˾Ƴ�. */
        sSQLRC = SQLGetDiagField(SQL_HANDLE_STMT,
                                 (SQLHANDLE)*aHstmt,
                                 (SQLSMALLINT)i,
                                 SQL_DIAG_NATIVE,
                                 (SQLPOINTER)&sSQLCODE,
                                 SQL_IS_POINTER,
                                 (SQLSMALLINT *)&sStrLen);

        ACI_TEST_RAISE( (sSQLRC == SQL_INVALID_HANDLE)||
                        (sSQLRC == SQL_ERROR), ERR_CLI_GETDIAGFIELD );

        if ( (sSQLRC == SQL_SUCCESS) && (sRowNo > 0) )
        {
            aErrCode[(sRowNo -1)] = sSQLCODE;
        }
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION( ERR_CLI_GETDIAGFIELD );
    {
        ulpSetErrorInfo4CLI( aConnNode,
                             *aHstmt,
                             sSQLRC,
                             aSqlstmt->sqlcaerr,
                             aSqlstmt->sqlcodeerr,
                             aSqlstmt->sqlstateerr,
                             ERR_TYPE1 );
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}
