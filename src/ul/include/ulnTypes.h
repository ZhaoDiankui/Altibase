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

#ifndef _O_ULN_TYPES_H_
#define _O_ULN_TYPES_H_ 1

/*
 * �ش� PRECISION���� ������ ����� ã�� ����  DB Type�� ������ �ִ� �ִ�
 * ũ��� ���� ���ߵǾ�����, DB Type�� ������� �������� ����̴�.
 */
// BUG-24378 ������ CHAR_MAX_PRECISION �� 32000���� �پ�����
// BUG-23061 nibble �� �ִ���̰� 254�Դϴ�. (MTD_NIBBLE_PRECISION_MAXIMUM)
#define ULN_DEFAULT_PRECISION         0
#define ULN_MAX_NUMERIC_PRECISION     38
#define ULN_MAX_NUMERIC_SCALE         20
#define ULN_MAX_CHAR_PRECISION        (32000)
#define ULN_MAX_GEOMETRY_PRECISION    (32000)
#define ULN_MAX_VARCHAR_PRECISION     (32000)
#define ULN_MAX_VARBIT_PRECISION      (32000)
#define ULN_MAX_NIBBLE_PRECISION      (254)
#define ULN_MAX_BYTE_PRECISION        (32000)
#define ULN_MAX_BINARY_PRECISION      (32000)

/*
 * SQL_C_XXXX �� enumeration
 */
typedef enum ulnCTypeID
{
    ULN_CTYPE_NULL,         // 0

    ULN_CTYPE_DEFAULT,      // 1

    ULN_CTYPE_CHAR,         // 2

    ULN_CTYPE_NUMERIC,

    ULN_CTYPE_BIT,

    ULN_CTYPE_STINYINT,     // 5
    ULN_CTYPE_UTINYINT,

    ULN_CTYPE_SSHORT,       // 7
    ULN_CTYPE_USHORT,

    ULN_CTYPE_SLONG,        // 9
    ULN_CTYPE_ULONG,

    ULN_CTYPE_SBIGINT,      // 11
    ULN_CTYPE_UBIGINT,

    ULN_CTYPE_FLOAT,        // 13
    ULN_CTYPE_DOUBLE,

    ULN_CTYPE_BINARY,       // 15

    ULN_CTYPE_DATE,         // 16
    ULN_CTYPE_TIME,
    ULN_CTYPE_TIMESTAMP,
    ULN_CTYPE_INTERVAL,

    ULN_CTYPE_BLOB_LOCATOR, // 20
    ULN_CTYPE_CLOB_LOCATOR,
    ULN_CTYPE_FILE,

    ULN_CTYPE_WCHAR,        // 23

    ULN_CTYPE_MAX
} ulnCTypeID;

/*
 * MTD_XXXX_ID �� enumeration
 */
typedef enum ulnMTypeID
{
    ULN_MTYPE_NULL,

    ULN_MTYPE_CHAR,
    ULN_MTYPE_VARCHAR,

    ULN_MTYPE_NUMBER,
    ULN_MTYPE_NUMERIC,

    ULN_MTYPE_BIT,

    ULN_MTYPE_SMALLINT,
    ULN_MTYPE_INTEGER,
    ULN_MTYPE_BIGINT,

    ULN_MTYPE_REAL,
    ULN_MTYPE_FLOAT,
    ULN_MTYPE_DOUBLE,

    ULN_MTYPE_BINARY,
    ULN_MTYPE_VARBIT,
    ULN_MTYPE_NIBBLE,
    ULN_MTYPE_BYTE,
    ULN_MTYPE_VARBYTE,

    ULN_MTYPE_TIMESTAMP,    // bind column info ���� ����� �� ������ �ϱ� ���ؼ� TIMESTAMP �� ���� �տ� ���Ҵ�.
    ULN_MTYPE_DATE,         // �������� �ö���� ����. fetch �ÿ��� ���� �ʿ����. insert, update �ÿ��� �ʿ���.
    ULN_MTYPE_TIME,         // �������� �ö���� ����. fetch �ÿ��� ���� �ʿ����. insert, update �ÿ��� �ʿ���.
    ULN_MTYPE_INTERVAL,

    ULN_MTYPE_BLOB,
    ULN_MTYPE_CLOB,

    ULN_MTYPE_BLOB_LOCATOR,
    ULN_MTYPE_CLOB_LOCATOR,

    ULN_MTYPE_GEOMETRY,

    ULN_MTYPE_NCHAR,
    ULN_MTYPE_NVARCHAR,

    ULN_MTYPE_MAX
} ulnMTypeID;

/*
 * ���� Ÿ�� ��Ī, ��ȯ, �⺻�� ���� ��� �Լ�
 */
acp_uint32_t ulnTypeMap_MTYPE_MTD(ulnMTypeID aMTYPE);
ulnMTypeID   ulnTypeMap_MTD_MTYPE(acp_uint32_t aMTD_TYPE);

acp_sint16_t ulnTypeMap_MTYPE_SQL(ulnMTypeID aMTYPE);
ulnMTypeID   ulnTypeMap_SQL_MTYPE(acp_sint16_t aSQL_TYPE);

acp_sint16_t ulnTypeMap_CTYPE_SQLC(ulnCTypeID aCTYPE);
ulnCTypeID   ulnTypeMap_SQLC_CTYPE(acp_sint16_t aSQL_C_TYPE);

acp_sint16_t ulnTypeMap_LOB_SQLTYPE(acp_sint16_t aSQLTYPE, acp_bool_t aLongDataCompat);

acp_uint32_t ulnTypeGetColumnSizeOfType(ulnMTypeID aMTYPE, ulnMeta *aMeta);
acp_sint16_t ulnTypeGetDecimalDigitsOfType(ulnMTypeID aMTYPE, ulnMeta *aMeta);

acp_sint16_t ulnTypeGetDefault_SQL_C_TYPE(ulnMTypeID aMTYPE);

acp_sint16_t ulnTypeGetOdbcDatetimeIntCode(acp_sint16_t aType);
acp_sint16_t ulnTypeGetOdbcVerboseType(acp_sint16_t aType);

ACP_INLINE acp_bool_t ulnTypeIsMemBoundLob(ulnMTypeID aMTYPE, ulnCTypeID aCTYPE)
{
    if (aMTYPE == ULN_MTYPE_CLOB || aMTYPE == ULN_MTYPE_BLOB)
    {
        /*
         * ����ڰ� lob locator �� ���ε����� �ʾ�����
         *      --> SQL_C_FILE, SQL_C_BINARY, SQL_C_CHAR --> memory bound lob
         */
        if (aCTYPE != ULN_CTYPE_BLOB_LOCATOR && aCTYPE != ULN_CTYPE_CLOB_LOCATOR)
        {
            return ACP_TRUE;
        }
    }

    return ACP_FALSE;
}

acp_bool_t ulnTypeIsOdbcConciseType(acp_sint16_t aType);
acp_bool_t ulnTypeIsVariableLength(ulnCTypeID aCTYPE);

acp_sint32_t ulnTypeGetDisplaySize(ulnMTypeID aMTYPE, ulnMeta *aMeta);

acp_sint16_t  ulnTypeGetInfoSearchable(ulnMTypeID aMTYPE);
acp_char_t   *ulnTypeGetInfoLiteralPrefix(ulnMTypeID aMTYPE);
acp_char_t   *ulnTypeGetInfoLiteralSuffix(ulnMTypeID aMTYPE);
acp_char_t   *ulnTypeGetInfoName(ulnMTypeID aMTYPE);

acp_sint32_t ulnTypeGetSizeOfFixedType(ulnMTypeID aMTYPE);
acp_bool_t   ulnTypeIsFixedMType(ulnMTypeID aMTYPE);

/* PROJ-2160 CM Ÿ������
   ����Ÿ���� MT ����Ÿ�� �ִ� ����� �����Ѵ�. */
/* BUG-35016 */
acp_uint32_t ulnTypeGetMaxMtSize(ulnDbc *aDbc, ulnMeta *aMeta);

#endif /* _O_ULN_TYPES_H_ */
