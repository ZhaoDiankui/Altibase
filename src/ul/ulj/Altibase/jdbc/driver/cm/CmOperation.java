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

package Altibase.jdbc.driver.cm;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.charset.CoderResult;
import java.sql.SQLException;
import java.sql.Statement;
import java.util.ArrayList;
import java.util.List;

import Altibase.jdbc.driver.AltibaseVersion;
import Altibase.jdbc.driver.AltibaseXid;
import Altibase.jdbc.driver.LobConst;
import Altibase.jdbc.driver.datatype.Column;
import Altibase.jdbc.driver.datatype.ColumnFactory;
import Altibase.jdbc.driver.datatype.ColumnInfo;
import Altibase.jdbc.driver.datatype.ListBufferHandle;
import Altibase.jdbc.driver.ex.Error;
import Altibase.jdbc.driver.ex.ErrorDef;
import Altibase.jdbc.driver.util.AltibaseProperties;

public class CmOperation extends CmOperationDef
{
    private static final byte HANDSHAKE_MODULE_ID = 1;
    private static final byte HANDSHAKE_FLAG      = 0; // This flag value has been reserved for the future. This value is arbitrary now

    static void writeHandshake(CmChannel aChannel) throws SQLException
    {
        aChannel.checkWritable(6);
        aChannel.writeOp(DB_OP_HANDSHAKE);
        aChannel.writeByte(HANDSHAKE_MODULE_ID);
        aChannel.writeByte(AltibaseVersion.CM_MAJOR_VERSION);
        aChannel.writeByte(AltibaseVersion.CM_MINOR_VERSION);
        aChannel.writeByte(AltibaseVersion.CM_PATCH_VERSION);
        aChannel.writeByte(HANDSHAKE_FLAG);
    }

    static void readHandshake(CmChannel aChannel, CmHandshakeResult aResult) throws SQLException
    {
        byte sOp = aChannel.readOp();
        
        if (sOp == DB_OP_HANDSHAKE_RESULT)
        {
            // �����κ��� ���� base version, module id, module version ��� Ư���� ����� ���� ����.
            aResult.setResult(aChannel.readByte(), aChannel.readByte(), aChannel.readByte(), aChannel.readByte(), aChannel.readByte());
        }
        else 
        if (sOp == DB_OP_ERROR_RESULT)
        {
            aResult.setError(aChannel.readInt(), aChannel.readStringForErrorResult());
        }
        else
        {
            Error.throwInternalError(ErrorDef.INVALID_OPERATION_PROTOCOL, String.valueOf(DB_OP_HANDSHAKE));
        }
    }

    public static void readProtocolResult(CmProtocolContext aContext) throws SQLException
    {
        CmChannel sChannel = aContext.channel();
        while (true)
        {
            byte sResultOp = sChannel.readOp();
            if (sResultOp == CmChannel.NO_OP)
            {
                break;
            }

            switch (sResultOp)
            {
                case DB_OP_MESSAGE:
                    readMessage(sChannel, aContext);
                    break;
                case DB_OP_ERROR_RESULT:
                    readErrorResult(sChannel, aContext);
                    break;
                case DB_OP_CONNECT_EX_RESULT:
                    // BUG-38496 Notify users when their password expiry date is approaching.
                    readConnectExResult(sChannel, (CmConnectExResult)aContext.getCmResult(sResultOp), aContext);
                    break;
                case DB_OP_DISCONNECT_RESULT:
                    // nothing to read
                    break;
                case DB_OP_GET_PROPERTY_RESULT:
                    readGetPropertyResult(sChannel, (CmGetPropertyResult)aContext.getCmResult(sResultOp));
                    break;
                case DB_OP_SET_PROPERTY_RESULT:
                    // nothing to read
                    break;
                case DB_OP_PREPARE_RESULT:
                    readPrepareResult(sChannel, (CmPrepareResult)aContext.getCmResult(sResultOp));
                    break;
                case DB_OP_GET_PLAN_RESULT:
                    readGetPlanResult(sChannel, (CmGetPlanResult)aContext.getCmResult(sResultOp));
                    break;
                case DB_OP_GET_COLUMN_INFO_RESULT:
                    readGetColumnInfoResult(sChannel, (CmGetColumnInfoResult)aContext.getCmResult(sResultOp), sChannel.getByteLengthPerChar());
                    break;
                case DB_OP_GET_COLUMN_INFO_LIST_RESULT:
                    readGetColumnInfoListResult(sChannel, (CmGetColumnInfoResult)aContext.getCmResult(CmGetColumnInfoResult.MY_OP), sChannel.getByteLengthPerChar());
                    break;
                case DB_OP_GET_PARAM_INFO_RESULT:
                    readGetBindParamInfoResult(sChannel, (CmGetBindParamInfoResult)aContext.getCmResult(sResultOp), sChannel.getByteLengthPerChar());
                    break;
                case DB_OP_SET_PARAM_INFO_LIST_RESULT:
                    // nothing to read
                    break;
                case DB_OP_PARAM_DATA_IN_RESULT:
                    // nothing to read
                    break;
                case DB_OP_PARAM_DATA_OUT_LIST:
                    readBindParamDataOutListResult(sChannel, (CmBindParamDataOutResult)aContext.getCmResult(sResultOp));
                    break;
                case DB_OP_PARAM_DATA_IN_LIST_V2_RESULT:
                    readBindParamDataInListV2Result(sChannel, aContext);
                    break;
                case DB_OP_EXECUTE_V2_RESULT:
                    readExecuteV2Result(sChannel, aContext);
                    break;
                case DB_OP_FETCH_BEGIN_RESULT:
                    readFetchBeginResult(sChannel, (CmGetColumnInfoResult)aContext.getCmResult(CmGetColumnInfoResult.MY_OP), (CmFetchResult)aContext.getCmResult(CmFetchResult.MY_OP));
                    break;
                case DB_OP_FETCH_RESULT:
                    readFetchResult(sChannel, (CmFetchResult)aContext.getCmResult(sResultOp));
                    break;
                case DB_OP_FETCH_END_RESULT:
                    readFetchEndResult(sChannel, (CmGetColumnInfoResult)aContext.getCmResult(CmGetColumnInfoResult.MY_OP), (CmFetchResult)aContext.getCmResult(CmFetchResult.MY_OP));
                    break;
                case DB_OP_FREE_RESULT:
                    // nothing to read
                    break;
                case DB_OP_CANCEL_RESULT:
                    // nothing to read
                    break;
                case DB_OP_TRANSACTION_RESULT:
                    // nothing to read
                    break;
                case DB_OP_LOB_GET_SIZE_RESULT:
                    readLobGetSizeResult(sChannel, (CmBlobGetResult)aContext.getCmResult(CmBlobGetResult.MY_OP));
                    break;
                case DB_OP_LOB_GET_RESULT:
                    readLobGetResult(sChannel, (CmProtocolContextLob)aContext);
                    break;
                case DB_OP_LOB_PUT_BEGIN_RESULT:
                    // nothing to read
                    break;
                case DB_OP_LOB_PUT_END_RESULT:
                    // nothing to read
                    break;
                case DB_OP_LOB_FREE_RESULT:
                    // nothing to read
                    break;
                case DB_OP_LOB_FREE_ALL_RESULT:
                    // nothing to read
                    break;
                case DB_OP_XA_XID_RESULT:
                    readXidResult(sChannel, (CmXidResult)aContext.getCmResult(sResultOp));
                    break;
                case DB_OP_XA_RESULT:
                    readXaResult(sChannel, (CmXAResult)aContext.getCmResult(sResultOp));
                    break;
                case DB_OP_LOB_GET_BYTE_POS_CHAR_LEN_RESULT:
                    readClobGetResult(sChannel, (CmProtocolContextLob)aContext);
                    break;
                case DB_OP_LOB_BYTE_POS_RESULT:
                    readLobBytePosResult(sChannel, (CmClobGetResult)aContext.getCmResult(CmClobGetResult.MY_OP));
                    break;
                case DB_OP_LOB_CHAR_LENGTH_RESULT:
                    readLobCharLengthResult(sChannel, (CmClobGetResult)aContext.getCmResult(CmClobGetResult.MY_OP));
                    break;
                case DB_OP_LOB_TRIM_RESULT:
                    break;

                // #region �Ⱦ��� ��������

                // ������ �� ���� ��, ���� �Ⱦ��� ��������
                case DB_OP_FETCH_MOVE_RESULT:

                default:
                    Error.throwInternalError(ErrorDef.INVALID_OPERATION_PROTOCOL, String.valueOf(sResultOp));
                    break;

                // #endregion
            }
        }
    }

    private static void readMessage(CmChannel aChannel, CmProtocolContext aResult) throws SQLException
    {
        String sMessage = aChannel.readStringForMessage();
        aResult.addWarning(sMessage);
    }

    private static void readErrorResult(CmChannel aChannel, CmProtocolContext aContext) throws SQLException
    {
        CmErrorResult sError = new CmErrorResult();
        sError.readFrom(aChannel);
        if (aContext instanceof CmProtocolContextDirExec)
        {
            CmExecutionResult sExecResult = (CmExecutionResult)aContext.getCmResult(CmExecutionResult.MY_OP);

            if (sExecResult.isBatchMode())
            {
                sError.setBatchError(true);
            }

            switch (sError.getErrorOp())
            {
                case DB_OP_PREPARE_BY_CID:
                    // prepare ������ Error Index�� �������� �����Ƿ�(�׻� 0), ���� RowNumber ������ �ʱ�
                    sError.initErrorIndex(sExecResult.getUpdatedRowCountArraySize());
                case DB_OP_PARAM_DATA_IN_LIST_V2:
                case DB_OP_EXECUTE_V2:
                    sExecResult.setUpdatedRowCount(Statement.EXECUTE_FAILED);
                    break;
                default:
                    break;
            }
        }
        aContext.addError(sError);
    }

    static void writeConnectEx(CmChannel aChannel, String aDBName, String aUserName, String aPassword, short aMode) throws SQLException
    {
        int[] sLimit = new int[3];
        ByteBuffer sBuf = prepareToWriteStringForConnect(aChannel, new String[] { aDBName, aUserName, aPassword }, sLimit);

        aChannel.checkWritable(9 + sBuf.remaining());
        aChannel.writeOp(DB_OP_CONNECT_EX);
        for (int sSLimit : sLimit)
        {
            sBuf.limit(sSLimit);
            aChannel.writeShort((short)sBuf.remaining());
            aChannel.writeBytes(sBuf);
        }
        aChannel.writeShort(aMode);
    }

    private static ByteBuffer prepareToWriteStringForConnect(CmChannel aChannel, String[] aStrs, int[] sLimit) throws SQLException
    {
        int sBufLen = aStrs[0].length();
        for (int i = 1; i < aStrs.length; i++)
        {
            sBufLen += aStrs[i].length();
        }
        sBufLen *= aChannel.getByteLengthPerChar();
        ByteBuffer sBuf = ByteBuffer.allocate(sBufLen);
        for (int i = 0; i < aStrs.length; i++)
        {
            CoderResult sEncResult = aChannel.encodeString(aStrs[i], sBuf);
            if (sEncResult == CoderResult.OVERFLOW)
            {
                Error.throwInternalError(ErrorDef.INTERNAL_BUFFER_OVERFLOW);
            }
            sLimit[i] = sBuf.position();
        }
        sBuf.flip();
        return sBuf;
    }

    // BUG-38496 Notify users when their password expiration date is approaching.
    private static void readConnectExResult(CmChannel aChannel, CmConnectExResult aResult, CmProtocolContext aContext) throws SQLException
    {
        aResult.setSessionID(aChannel.readInt());
        aResult.setResultCode(aChannel.readInt());
        aResult.setResultInfo(aChannel.readInt());
        aResult.setReserved(aChannel.readLong());

        if (Error.toClientSideErrorCode(aResult.getResultCode()) != ErrorDef.IGNORE_NO_ERROR)
        {
            CmErrorResult sError = new CmErrorResult(aResult.getResultOp(), aResult.getResultInfo(), aResult.getResultCode(), null, false);
            aContext.addError(sError);
        }
    }

    static void writeDisconnect(CmChannel aChannel) throws SQLException
    {
        aChannel.checkWritable(2);
        aChannel.writeOp(DB_OP_DISCONNECT);
        aChannel.writeByte((byte)0); // option�� ������� ����
    }

    static void writeCommit(CmChannel aChannel) throws SQLException
    {
        aChannel.checkWritable(2);
        aChannel.writeOp(DB_OP_TRANSACTION);
        aChannel.writeByte(TRANSACTION_OP_COMMIT);
    }

    /**
     * ClientAutoCommit�� On�� ��쿡�� ���ۿ� Ŀ������������ �����Ѵ�.
     * @param aChannel Socket����� ���� CmChannel��ü
     * @param aClientSideAutoCommit Ŭ���̾�Ʈ���̵����Ŀ�Կ���
     * @throws SQLException Ŀ���������� ������ ������ ���
     */
    static void writeClientCommit(CmChannel aChannel, boolean aClientSideAutoCommit) throws SQLException
    {
        if (aClientSideAutoCommit)
        {
            CmOperation.writeCommit(aChannel);
        }
    }

    static void writeRollback(CmChannel aChannel) throws SQLException
    {
        aChannel.checkWritable(2);
        aChannel.writeOp(DB_OP_TRANSACTION);
        aChannel.writeByte(TRANSACTION_OP_ROLLBACK);
    }

    static void writePrepare(CmChannel aChannel, int aCID, String aSql, byte aExecMode, byte aHoldMode, byte aKeySetDrivenMode, boolean aNliteralReplace) throws SQLException
    {
        // BUG-45156 encoding�� ���ܰ� �߻��Ҽ��� �ֱ� ������ ���ۿ� write�ϱ� ���� encoding ���� �����Ѵ�.
        int sWriteStringMode = aNliteralReplace ? WRITE_STRING_MODE_NLITERAL : WRITE_STRING_MODE_DB;
        int sBytesLen = aChannel.prepareToWriteString(aSql, sWriteStringMode);

        aChannel.checkWritable(10);
        aChannel.writeOp(DB_OP_PREPARE_BY_CID);
        aChannel.writeInt(aCID); // client���� ���� session statement id
        aChannel.writeByte((byte)(aExecMode | aHoldMode | aKeySetDrivenMode));
        aChannel.writeInt(sBytesLen);
        aChannel.writePreparedString();
    }

    private static void readPrepareResult(CmChannel aChannel, CmPrepareResult aResult) throws SQLException
    {
        aResult.setStatementId(aChannel.readInt());
        aResult.setStatementType(aChannel.readInt());
        aResult.setParameterCount(aChannel.readShort());
        aResult.setResultSetCount(aChannel.readShort());
        aResult.setDataArrived(true);    // BUG-42424 �����κ��� ���� ���޹��� �� flag�� enable���ش�.
    }

    static void writeExecuteV2(CmChannel aChannel, int aStatementId, int aRowNumber, byte aMode) throws SQLException
    {
        aChannel.checkWritable(10);
        aChannel.writeOp(DB_OP_EXECUTE_V2);
        aChannel.writeInt(aStatementId);
        aChannel.writeInt(aRowNumber);
        aChannel.writeByte(aMode);
    }

    private static void readExecuteV2Result(CmChannel aChannel, CmProtocolContext aContext) throws SQLException
    {
        CmExecutionResult sExecResult = (CmExecutionResult)aContext.getCmResult(CmExecutionResult.MY_OP);
        sExecResult.setStatementId(aChannel.readInt());
        int sRowNumber = aChannel.readInt();
        int sResultSetCount = aChannel.readUnsignedShort();
        long sUpdatedRowCount = aChannel.readLong();
        aChannel.readLong(); // unused. fetched row count
        if (sExecResult.isBatchMode() && sResultSetCount > 0)
        {
            // batch �۾��ȿ� fetch ������ ������ ���ܸ� �÷��� �Ѵ�.
            CmErrorResult sError = new CmErrorResult(DB_OP_EXECUTE_V2,
                                                     sRowNumber,
                                                     Error.toServerSideErrorCode(ErrorDef.INVALID_BATCH_OPERATION_WITH_SELECT, 0),
                                                     ErrorDef.getErrorMessage(ErrorDef.INVALID_BATCH_OPERATION_WITH_SELECT),
                                                     true);
            aContext.addError(sError);
            sUpdatedRowCount = Statement.EXECUTE_FAILED; // overwrite
        }
        sExecResult.setRowNumber(sRowNumber);
        sExecResult.setResultSetCount(sResultSetCount);
        sExecResult.setUpdatedRowCount(sUpdatedRowCount);
    }

    /* BUG-39463 Add new fetch protocol that can request over 65535 rows.*/
    static void writeFetchV2(CmChannel aChannel, int aStatementId, short aResultSetId, int aFetchCount) throws SQLException
    {
        aChannel.checkWritable(23);
        aChannel.writeOp(DB_OP_FETCH_V2);
        aChannel.writeInt(aStatementId);
        aChannel.writeShort(aResultSetId);
        aChannel.writeInt(aFetchCount);
        aChannel.writeShort((short) 1); // fetch�� ���� �÷� ��ġ
        aChannel.writeShort((short) 0); // fetch�� �� �÷� ��ġ: 0�̸� �÷� ������ŭ
        aChannel.writeLong((long) 0);   // Reserved
    }

    private static void readFetchBeginResult(CmChannel aChannel, CmGetColumnInfoResult aGetColResult, CmFetchResult aFetchResult) throws SQLException
    {
        aFetchResult.setStatementId(aChannel.readInt());
        aFetchResult.setResultSetId(aChannel.readShort());
        beginFetch(aGetColResult, aFetchResult);
    }

    private static void beginFetch(CmGetColumnInfoResult aGetColResult, CmFetchResult aFetchResult)
    {
        List<Column> sColumns = aGetColResult.getColumns();
        if (sColumns == null)
        {
            return;
        }

        // Statement�� setMaxFieldSize()�� �����ϱ� ���� �����Ѵ�.
        for (Column sColumn : sColumns)
        {
            sColumn.setMaxBinaryLength(aFetchResult.getMaxFieldSize());
        }
        aFetchResult.fetchBegin(sColumns);
    }

    private static void readFetchResult(CmChannel aChannel, CmFetchResult aResult) throws SQLException
    {
        long sRowSize = aChannel.readUnsignedInt();

        // BUG-43807 Rowhandle�� �����ϴ� �κ��� ���� ���� �ʰ� �Ѳ����� �����Ѵ�.
        aResult.increaseStoreCursor();
        List<Column> sColumns = aResult.getColumns();
        for (int i = 0; i < sColumns.size(); i++)
        {
            sColumns.get(i).readFrom(aChannel, aResult);
        }
        aResult.updateFetchStat(sRowSize);
    }

    private static void readFetchEndResult(CmChannel aChannel, CmGetColumnInfoResult aGetColResult, CmFetchResult aResult) throws SQLException
    {
        aResult.setStatementId(aChannel.readInt());
        aResult.setResultSetId(aChannel.readShort());
        if (!aResult.isBegun())
        {
            // fetch ����� ������ (no rows�̸�) fetch begin���� �ٷ� fetch end�� �´�.
            // �̶� �� �۾��� ����� �Ѵ�.
            beginFetch(aGetColResult, aResult);
        }
        aResult.fetchEnd();
    }

    static void writeGetColumn(CmChannel aChannel, int aStatementId, short aResultSetId) throws SQLException
    {
        aChannel.checkWritable(9);
        aChannel.writeOp(DB_OP_GET_COLUMN_INFO);
        aChannel.writeInt(aStatementId);
        aChannel.writeShort(aResultSetId);
        aChannel.writeShort((short) 0);
    }

    private static void readGetColumnInfoListResult(CmChannel aChannel, CmGetColumnInfoResult aResult, int aMaxBytesPerChar) throws SQLException
    {
        aResult.setStatementId(aChannel.readInt());
        aChannel.readShort(); // ResultSet ID�� ������� ����

        short sColCount = aChannel.readShort();
        List<Column> sColumnsList = new ArrayList<Column>(sColCount);
        for (int i = 0; i < sColCount; i++)
        {
            int sDataType = aChannel.readInt();
            int sLanguage = aChannel.readInt();
            byte sArguments = aChannel.readByte();
            int sPrecision = aChannel.readInt();
            int sScale = aChannel.readInt();
            byte sFlag = aChannel.readByte();

            String sDisplayName = aChannel.readStringForColumnInfo();
            String sColumnName = aChannel.readStringForColumnInfo();
            String sBaseColumnName = aChannel.readStringForColumnInfo();
            String sTableName = aChannel.readStringForColumnInfo();
            String sBaseTableName = aChannel.readStringForColumnInfo();
            String sSchemaName = aChannel.readStringForColumnInfo();
            String sCatalogName = aChannel.readStringForColumnInfo();

            boolean sNullable = (sFlag & COLUMN_INFO_FLAG_NULLABLE) > 0;
            boolean sUpdatable = (sFlag & COLUMN_INFO_FLAG_UPDATABLE) > 0;

            ColumnInfo sColumnInfo = new ColumnInfo();
            sColumnInfo.setColumnInfo(sDataType,
                                      sLanguage,
                                      sArguments,
                                      sPrecision,
                                      sScale,
                                      ColumnInfo.IN_OUT_TARGET_TYPE_TARGET,
                                      sNullable,
                                      sUpdatable,
                                      sCatalogName,
                                      sTableName,
                                      sBaseTableName,
                                      sColumnName,
                                      sDisplayName,
                                      sBaseColumnName,
                                      sSchemaName,
                                      aMaxBytesPerChar);
            Column sColumn = aChannel.getColumnFactory().getInstance(sColumnInfo.getDataType());
            sColumn.setColumnIndex(i); // BUG-43807 DynamicArray�� �ٷ� ã������ Column��ü�� index�� �Ҵ��Ѵ�.
            sColumnsList.add(sColumn);
            if (sColumnsList.get(i) == null)
            {
                Error.throwInternalError(ErrorDef.INVALID_OPERATION_PROTOCOL, String.valueOf(aResult.getResultOp()));
            }
            sColumnsList.get(i).setColumnInfo(sColumnInfo);
        }
        aResult.setColumns(sColumnsList);
    }

    private static void readGetColumnInfoResult(CmChannel aChannel, CmGetColumnInfoResult aResult, int aMaxBytesPerChar) throws SQLException
    {
        aResult.setStatementId(aChannel.readInt());
        aChannel.readShort(); // ResultSet ID�� ������� ����
        aChannel.readShort(); // BUG-42987 column index�� ������� ����

        int sDataType = aChannel.readInt();
        int sLanguage = aChannel.readInt();
        byte sArguments = aChannel.readByte();
        int sPrecision = aChannel.readInt();
        int sScale = aChannel.readInt();
        byte sFlag = aChannel.readByte();
        boolean sNullable = (sFlag & COLUMN_INFO_FLAG_NULLABLE) > 0;
        boolean sUpdatable = (sFlag & COLUMN_INFO_FLAG_UPDATABLE) > 0;
        String sDisplayName = aChannel.readStringForColumnInfo();
        String sColumnName = aChannel.readStringForColumnInfo();
        String sBaseColumnName = aChannel.readStringForColumnInfo();
        String sTableName = aChannel.readStringForColumnInfo();
        String sBaseTableName = aChannel.readStringForColumnInfo();
        String sSchemaName = aChannel.readStringForColumnInfo();
        String sCatalogName = aChannel.readStringForColumnInfo();

        ColumnInfo sColumnInfo = new ColumnInfo();
        sColumnInfo.setColumnInfo(sDataType,
                sLanguage,
                sArguments,
                sPrecision,
                sScale,
                ColumnInfo.IN_OUT_TARGET_TYPE_TARGET,
                sNullable,
                sUpdatable,
                sCatalogName,
                sTableName,
                sBaseTableName,
                sColumnName,
                sDisplayName,
                sBaseColumnName,
                sSchemaName,
                aMaxBytesPerChar);


        Column sColumn = aChannel.getColumnFactory().getInstance(sColumnInfo.getDataType());
        sColumn.setColumnInfo(sColumnInfo);

        aResult.addColumn(sColumn);
    }

    static void writeGetBindParamInfo(CmChannel aChannel, int aStatementId) throws SQLException
    {
        aChannel.checkWritable(7);
        aChannel.writeOp(DB_OP_GET_PARAM_INFO);
        aChannel.writeInt(aStatementId);
        aChannel.writeShort((short)0); // parameter index: �׻� ��� �÷� ������ ��û�ϱ� ������ 0�� �ش�.
    }

    private static void readGetBindParamInfoResult(CmChannel aChannel, CmGetBindParamInfoResult aResult, int aMaxBytesPerChar) throws SQLException
    {
        aResult.setStatementId(aChannel.readInt());
        int sParamIdx = aChannel.readShort();    // BUG-42424 parameter index�� ���� Ű�� ����Ѵ�.
        int sDataType = aChannel.readInt();
        int sLanguage = aChannel.readInt();
        byte sArguments = aChannel.readByte();
        int sPrecision = aChannel.readInt();
        int sScale = aChannel.readInt();
        aChannel.readByte(); // �������� �ִ� in-out type�� ������. �ǹ̾��� �����̴�.
        byte sInOutType = ColumnInfo.IN_OUT_TARGET_TYPE_NONE;
        byte sNullable = aChannel.readByte();

        ColumnInfo sColumnInfo = new ColumnInfo();
        sColumnInfo.setColumnInfo(sDataType,
                                  sLanguage,
                                  sArguments,
                                  sPrecision,
                                  sScale,
                                  sInOutType,
                                  (sNullable & COLUMN_INFO_FLAG_NULLABLE) > 0,
                                  false,
                                  null,
                                  null,
                                  null,
                                  null,
                                  null,
                                  null,
                                  null,
                                  aMaxBytesPerChar);

        aResult.addColumnInfo(sParamIdx, sColumnInfo);
    }

    static void writeSetBindParamInfoList(CmChannel aChannel, int aStatementId, List<Column> aParams) throws SQLException
    {
        if (CmOperation.isChangedBindInfo(aParams))
        {
            CmOperation.unChangeBindInfo(aParams);
        }
        else
        {
            // BUG-38947 ���ε��Ķ���������� ��ȭ�� ������ ��Ÿ������ ������ �������� �ʰ� �ٷ� �����Ѵ�.
            return;
        }

        aChannel.checkWritable(7 + (20 * aParams.size()));
        aChannel.writeOp(DB_OP_SET_PARAM_INFO_LIST);
        aChannel.writeInt(aStatementId);
        aChannel.writeShort((short) aParams.size());
        for (int i = 0; i < aParams.size(); i++)
        {
            ColumnInfo sParamInfo = aParams.get(i).getColumnInfo();
            aChannel.writeShort((short)(i + 1));
            aChannel.writeInt(sParamInfo.getDataType());
            aChannel.writeInt(sParamInfo.getLanguage());
            aChannel.writeByte(sParamInfo.getArguments());
            aChannel.writeInt(sParamInfo.getPrecision()); // char, varchar�� ���� charset�� ����� precision�� ������ ������.
            aChannel.writeInt(sParamInfo.getScale());
            aChannel.writeByte(sParamInfo.getInOutTargetType());
        }
    }

    /**
     * �Ķ���������� ����Ǿ����� üũ�Ѵ�..
     * @param aColumns ���ε�� �Ķ������ �÷����� �迭
     * @return ����� �Ķ���Ͱ� ���� ��� true, ���� ��쿣 false
     */
    private static boolean isChangedBindInfo(List<Column> aColumns)
    {
        for (Column sColumn : aColumns)
        {
            if (sColumn.getColumnInfo().isChanged())
            {
                return true;
            }
        }
        return false;
    }

    /**
     * ����� �Ķ���Ͱ� ���� ��� �Ķ������ ���¸� unchanged�� �ٲ��ش�.
     * @param aColumns ���ε�� �Ķ������ �÷����� �迭
     */
    private static void unChangeBindInfo(List<Column> aColumns)
    {
        for (Column sColumn : aColumns)
        {
            ColumnInfo sColumnInfo = sColumn.getColumnInfo();
            if (sColumnInfo.isChanged())
            {
                // BUG-38947 �ߺ��ؼ� �Ķ���������� ������ ������ ���� �������� ColumnInfo ��ü�� mChange������ false��
                // �������ش�. mChange������ Column��ü�� ������ �� �Ǵ� setXXX�޼ҵ�� �Ķ���� ��Ÿ������ ����� ��
                // true�� ���õȴ�.
                sColumnInfo.unchange();
            }
        }
    }

    static void writeBindParamDataIn(CmChannel aChannel, int aStatementId, List<Column> aColumns) throws SQLException
    {
        for (int i = 0; i < aColumns.size(); i++)
        {
            if (aColumns.get(i).getColumnInfo().hasInType())
            {
                aChannel.checkWritable(11);
                aChannel.writeOp(DB_OP_PARAM_DATA_IN);
                aChannel.writeInt(aStatementId);
                aChannel.writeShort((short)(i + 1));
                int sDataSize = aColumns.get(i).prepareToWrite(aChannel);
                aChannel.writeInt(sDataSize);
                int sActSize = aColumns.get(i).writeTo(aChannel);
                if (sActSize != sDataSize)
                {
                    Error.throwInternalError(ErrorDef.INCONSISTANCE_DATA_LENGTH,
                                             String.valueOf(sDataSize),
                                             String.valueOf(sActSize));
                }
            }
        }
    }

    /**
     * ListBufferHandle �� Buffer �� ���  Data �� channel�� ����.
     *
     * @param aChannel Data�� �� channel
     * @param aStatementId Statement ID
     * @param aBufferHandle  List protocol �� data buffer �� handle �ϴ� ��ü
     * @param aIsAtomic Atomic Operation ��� ����
     * @throws SQLException channel�� ���� ���� ���
     */
    static void writeBindParamDataInListV2(CmChannel aChannel, int aStatementId, ListBufferHandle aBufferHandle, boolean aIsAtomic) throws SQLException
    {
        aChannel.checkWritable(22);
        aChannel.writeOp(DB_OP_PARAM_DATA_IN_LIST_V2);
        aChannel.writeInt(aStatementId);
        // List Protocol ���� FromRowNumber �� �׻� 1
        aChannel.writeInt(1);
        aChannel.writeInt(aBufferHandle.size());
        aChannel.writeByte(aIsAtomic ? CmOperation.EXECUTION_MODE_ATOMIC : CmOperation.EXECUTION_MODE_ARRAY);
        aChannel.writeLong(aBufferHandle.getBufferPosition());

        aBufferHandle.flipBuffer();
        aChannel.writeBytes(aBufferHandle.getBuffer());
    }

    private static void readBindParamDataInListV2Result(CmChannel aChannel, CmProtocolContext aContext) throws SQLException
    {
        CmExecutionResult sExecResult = (CmExecutionResult)aContext.getCmResult(CmExecutionResult.MY_OP);
        sExecResult.setStatementId(aChannel.readInt());
        int sFromRowNumber  = aChannel.readInt();
        int sToRowNumber     = aChannel.readInt();
        aChannel.readUnsignedShort(); // Select ������ ��ȿ, resultset count
        long sUpdatedRowCount = aChannel.readLong();
        aChannel.readLong(); // unused. fetched row count

        for (int i = sFromRowNumber; i <= sToRowNumber; i++)
        {
            sExecResult.setUpdatedRowCount(sUpdatedRowCount);
        }
    }

    private static void readBindParamDataOutListResult(CmChannel aChannel, CmBindParamDataOutResult aResult) throws SQLException
    {
        aResult.setStatementId(aChannel.readInt());
        aChannel.readInt(); // BUG-43807 rownumber�� ������ �ʱ� ������ skip�Ѵ�.
        aChannel.readUnsignedInt(); // skip RowSize
        for (Column sColumn : aResult.getBindParams())
        {
            if (sColumn.getColumnInfo().getInOutTargetType() != ColumnInfo.IN_OUT_TARGET_TYPE_IN)
            {
                sColumn.readParamsFrom(aChannel);
            }
        }
        aResult.storeLobDataAtBatch();
    }

    static void writeGetProperty(CmChannel aChannel, short aPropertyKey) throws SQLException
    {
        aChannel.checkWritable(3);
        aChannel.writeOp(DB_OP_GET_PROPERTY);
        aChannel.writeShort(aPropertyKey);
    }

    private static void readGetPropertyResult(CmChannel aChannel, CmGetPropertyResult aResult) throws SQLException
    {
        short sPropertyKey = aChannel.readShort();
        int sLength;
        String sStrValue;
        Column sValueContainer = null;
        byte sByteValue;
        int sIntValue;

        switch (sPropertyKey)
        {
            case (AltibaseProperties.PROP_CODE_NLS):
                sLength = aChannel.readInt();
                sStrValue = aChannel.readString(sLength, 0);
                sValueContainer = aChannel.getColumnFactory().createVarcharColumn();
                sValueContainer.setValue(sStrValue);
                break;
            case (AltibaseProperties.PROP_CODE_AUTOCOMMIT):
                sByteValue = aChannel.readByte();
                sValueContainer = ColumnFactory.createBooleanColumn();
                sValueContainer.setValue((int)sByteValue);
                break;
            case (AltibaseProperties.PROP_CODE_EXPLAIN_PLAN):
                sByteValue = aChannel.readByte();
                sValueContainer = ColumnFactory.createTinyIntColumn();
                sValueContainer.setValue(sByteValue);
                break;
            case (AltibaseProperties.PROP_CODE_ISOLATION_LEVEL): /* BUG-39817 */
                sIntValue = aChannel.readInt();
                sValueContainer = ColumnFactory.createIntegerColumn();
                sValueContainer.setValue(sIntValue);
                break;
            case (AltibaseProperties.PROP_CODE_OPTIMIZER_MODE):
                sIntValue = aChannel.readInt();
                sValueContainer = ColumnFactory.createIntegerColumn();
                sValueContainer.setValue(sIntValue);
                break;
            case (AltibaseProperties.PROP_CODE_HEADER_DISPLAY_MODE):
                sIntValue = aChannel.readInt();
                sValueContainer = ColumnFactory.createIntegerColumn();
                sValueContainer.setValue(sIntValue);
                break;
            case (AltibaseProperties.PROP_CODE_STACK_SIZE):
                sIntValue = aChannel.readInt();
                sValueContainer = ColumnFactory.createIntegerColumn();
                sValueContainer.setValue(sIntValue);
                break;
            case (AltibaseProperties.PROP_CODE_IDLE_TIMEOUT):
                sIntValue = aChannel.readInt();
                sValueContainer = ColumnFactory.createIntegerColumn();
                sValueContainer.setValue(sIntValue);
                break;
            case (AltibaseProperties.PROP_CODE_QUERY_TIMEOUT):
                sIntValue = aChannel.readInt();
                sValueContainer = ColumnFactory.createIntegerColumn();
                sValueContainer.setValue(sIntValue);
                break;
            case (AltibaseProperties.PROP_CODE_FETCH_TIMEOUT):
                sIntValue = aChannel.readInt();
                sValueContainer = ColumnFactory.createIntegerColumn();
                sValueContainer.setValue(sIntValue);
                break;
            case (AltibaseProperties.PROP_CODE_UTRANS_TIMEOUT):
                sIntValue = aChannel.readInt();
                sValueContainer = ColumnFactory.createIntegerColumn();
                sValueContainer.setValue(sIntValue);
                break;
            case (AltibaseProperties.PROP_CODE_DATE_FORMAT):
                sLength = aChannel.readInt();
                sStrValue = aChannel.readString(sLength, 0);
                sValueContainer = aChannel.getColumnFactory().createVarcharColumn();
                sValueContainer.setValue(sStrValue);
                break;
            case (AltibaseProperties.PROP_CODE_NORMALFORM_MAXIMUM):
                sIntValue = aChannel.readInt();
                sValueContainer = ColumnFactory.createIntegerColumn();
                sValueContainer.setValue(sIntValue);
                break;
            case (AltibaseProperties.PROP_CODE_SERVER_PACKAGE_VERSION):
                sLength = aChannel.readInt();
                sStrValue = aChannel.readString(sLength, 0);
                sValueContainer = aChannel.getColumnFactory().createVarcharColumn();
                sValueContainer.setValue(sStrValue);
                break;
            case (AltibaseProperties.PROP_CODE_NLS_NCHAR_LITERAL_REPLACE):
                sIntValue = aChannel.readInt();
                sValueContainer = ColumnFactory.createIntegerColumn();
                sValueContainer.setValue(sIntValue);
                break;
            case (AltibaseProperties.PROP_CODE_NLS_CHARACTERSET):
                sLength = aChannel.readInt();
                sStrValue = aChannel.readString(sLength, 0);
                sValueContainer = aChannel.getColumnFactory().createVarcharColumn();
                sValueContainer.setValue(sStrValue);
                break;
            case (AltibaseProperties.PROP_CODE_NLS_NCHAR_CHARACTERSET):
                sLength = aChannel.readInt();
                sStrValue = aChannel.readString(sLength, 0);
                sValueContainer = aChannel.getColumnFactory().createVarcharColumn();
                sValueContainer.setValue(sStrValue);
                break;
            case (AltibaseProperties.PROP_CODE_ENDIAN):
                sByteValue = aChannel.readByte();
                sValueContainer = ColumnFactory.createBooleanColumn();
                sValueContainer.setValue((int)sByteValue);
                break;
            case (AltibaseProperties.PROP_CODE_DDL_TIMEOUT):
                sIntValue = aChannel.readInt();
                sValueContainer = ColumnFactory.createIntegerColumn();
                sValueContainer.setValue(sIntValue);
                break;
            case (AltibaseProperties.PROP_CODE_LOB_CACHE_THRESHOLD):
                sIntValue = aChannel.readInt();
                sValueContainer = ColumnFactory.createIntegerColumn();
                sValueContainer.setValue(sIntValue);
                break;
            default:
                Error.throwSQLException(ErrorDef.INVALID_PROPERTY_ID, String.valueOf(sPropertyKey));
                break;
        }

        aResult.addProperty(sPropertyKey, sValueContainer);
    }

    static void writeSetPropertyV2(CmChannel aChannel, short aPropertyKey, Column aValue) throws SQLException
    {
        // BUG-41793 Keep a compatibility among tags
        //
        // Replaced   : CMP_OP_DB_PropertySetV2(1) | PropetyID(2) | ValueLen(4) | Value(?)
        // Deprecated : CMP_OP_DB_PropertySet  (1) | PropetyID(2) | Value(?)
        //
        // Deprecated writeSetProperty() since CM 7.1.3
        int sValueLen = aValue.prepareToWrite(aChannel);

        aChannel.checkWritable(7 + sValueLen);
        aChannel.writeOp(DB_OP_SET_PROPERTY_V2);
        aChannel.writeShort(aPropertyKey);
        aChannel.writeInt(sValueLen);
        aValue.writeTo(aChannel);
    }

    static void writeCloseCursor(CmChannel aChannel, int aStmtID, short aResultSetID) throws SQLException
    {
        aChannel.checkWritable(8);
        aChannel.writeOp(DB_OP_FREE);
        aChannel.writeInt(aStmtID);
        aChannel.writeShort(aResultSetID);
        aChannel.writeByte(FREE_MODE_CLOSE);
    }

    static void writeDropStatement(CmChannel aChannel, int aStmtID) throws SQLException
    {
        aChannel.checkWritable(8);
        aChannel.writeOp(DB_OP_FREE);
        aChannel.writeInt(aStmtID);
        aChannel.writeShort(FREE_ALL_RESULTSET);
        aChannel.writeByte(FREE_MODE_DROP);
    }

    static void writeCancelStatement(CmChannel aChannel, int aCID) throws SQLException
    {
        aChannel.checkWritable(5);
        aChannel.writeOp(DB_OP_CANCEL_BY_CID);
        aChannel.writeInt(aCID);
    }

    static void writeLobGet(CmChannel aChannel, long aLocatorId, long aOffset, long aSize) throws SQLException
    {
        aChannel.checkWritable(17);
        aChannel.writeOp(DB_OP_LOB_GET);
        aChannel.writeLong(aLocatorId);
        aChannel.writeUnsignedInt(aOffset);
        aChannel.writeUnsignedInt(aSize);
    }

    private static void readLobGetResult(CmChannel aChannel, CmProtocolContextLob aContext) throws SQLException
    {
        CmBlobGetResult sResult = (CmBlobGetResult)aContext.getCmResult(CmBlobGetResult.MY_OP);
        sResult.setLocatorId(aChannel.readLong());
        sResult.setOffset(aChannel.readInt());
        sResult.setLobLength(aChannel.readUnsignedInt());
        aChannel.readBytes(aContext.getBlobData(), aContext.getDstOffset(), (int)sResult.getLobLength());
    }

    static void writeLobGetSize(CmChannel aChannel, long aLocatorId) throws SQLException
    {
        aChannel.checkWritable(9);
        aChannel.writeOp(DB_OP_LOB_GET_SIZE);
        aChannel.writeLong(aLocatorId);
    }

    private static void readLobGetSizeResult(CmChannel aChannel, CmBlobGetResult aResult) throws SQLException
    {
        aResult.setLocatorId(aChannel.readLong());
        aResult.setLobLength(aChannel.readInt());
    }

    static void writeLobPutBegin(CmChannel aChannel, long aLocatorId, long aOffset) throws SQLException
    {
        aChannel.checkWritable(17);
        aChannel.writeOp(DB_OP_LOB_PUT_BEGIN);
        aChannel.writeLong(aLocatorId);
        aChannel.writeUnsignedInt(aOffset);
        aChannel.writeUnsignedInt(LobConst.LOB_BUFFER_SIZE);
    }

    /**
     * LobPut ���������� ����.
     * 
     * @param aContext �������� ���ؽ�Ʈ
     * @param aSource �������ݷ� �� ����Ÿ
     * @return �������ݿ� �� ����Ÿ ������(byte ����)
     * @throws SQLException ���������� ���µ� �������� ���
     * @throws IOException ����Ÿ�� �дµ� �������� ���
     */
    static long writeLobPut(CmProtocolContextLob aContext, ReadableCharChannel aSource) throws SQLException, IOException
    {
        long sTotalWrited = 0;
        while (true)
        {
            int sWrited = aContext.channel().writeBytes4Clob(aSource, aContext.locatorId(), aContext.isCopyMode());
            if (sWrited == -1)
            {
                break;
            }
            sTotalWrited += sWrited;
        }
        return sTotalWrited;
    }

    static void writeLobPut(CmChannel aChannel, long aLocatorId, byte[] aSource, int aSourceOffset, int aLengthPerOp) throws SQLException
    {
        while(aLengthPerOp > 0)
        {
            int sWritableSize = aChannel.checkWritable4Lob(aLengthPerOp);
            aChannel.writeOp(DB_OP_LOB_PUT);
            aChannel.writeLong(aLocatorId);
            aChannel.writeUnsignedInt(sWritableSize);
            aChannel.writeBytes(aSource, aSourceOffset, sWritableSize);

            aSourceOffset += sWritableSize;
            aLengthPerOp -= sWritableSize;
        }
    }
    
    static void writeLobPutEnd(CmChannel aChannel, long aLocatorId) throws SQLException
    {
        aChannel.checkWritable(9);
        aChannel.writeOp(DB_OP_LOB_PUT_END);
        aChannel.writeLong(aLocatorId);
    }

    static void writeLobTruncate(CmChannel aChannel, long aLocatorId, int aOffset) throws SQLException
    {
        aChannel.checkWritable(13);
        aChannel.writeOp(DB_OP_LOB_TRIM);
        aChannel.writeLong(aLocatorId);
        aChannel.writeUnsignedInt(aOffset);
    }

    static void writeLobGetBytePosCharLen(CmChannel aChannel, long aLocatorId, long aByteOffset, long aCharLength) throws SQLException
    {
        aChannel.checkWritable(17);
        aChannel.writeOp(DB_OP_LOB_GET_BYTE_POS_CHAR_LEN);
        aChannel.writeLong(aLocatorId);
        aChannel.writeUnsignedInt(aByteOffset);
        aChannel.writeUnsignedInt(aCharLength);
    }
    
    static void writeLobGetCharPosCharLen(CmChannel aChannel, long aLocatorId, long aCharOffset, long aCharLength) throws SQLException
    {
        aChannel.checkWritable(17);
        aChannel.writeOp(DB_OP_LOB_GET_CHAR_POS_CHAR_LEN);
        aChannel.writeLong(aLocatorId);
        aChannel.writeUnsignedInt(aCharOffset);
        aChannel.writeUnsignedInt(aCharLength);
    }
    
    private static void readClobGetResult(CmChannel aChannel, CmProtocolContextLob aContext) throws SQLException
    {
        CmClobGetResult sResult = (CmClobGetResult)aContext.getCmResult(CmClobGetResult.MY_OP);
        long sLocatorId = aChannel.readLong();
        if (sLocatorId != sResult.getLocatorId())
        {
            Error.throwInternalError(ErrorDef.INTERNAL_ASSERTION);
        }
        long sOffset = aChannel.readUnsignedInt(); // no use
        long sCharLength = aChannel.readUnsignedInt();
        long sByteLength = aChannel.readUnsignedInt();
        if (sResult.getOffset() == sOffset) // first data
        {
            sResult.setCharLength(sCharLength);
            sResult.setByteLength(sByteLength);
        }
        else // following data
        {
            sResult.addCharLength(sCharLength);
            sResult.addByteLength(sByteLength);
        }
        aChannel.readCharArrayTo(aContext.getClobData(), aContext.getDstOffset(), (int)sByteLength, (int)sCharLength);
        aContext.setDstOffset((int)(aContext.getDstOffset() + sCharLength));
    }
    
    static void writeLobBytePos(CmChannel aChannel, long aLocatorId, int aCharLength) throws SQLException
    {
        aChannel.checkWritable(13);
        aChannel.writeOp(DB_OP_LOB_BYTE_POS);
        aChannel.writeLong(aLocatorId);
        aChannel.writeInt(aCharLength);
    }
    
    private static void readLobBytePosResult(CmChannel aChannel, CmClobGetResult aResult) throws SQLException
    {
        aResult.setLocatorId(aChannel.readLong());
        aResult.setOffset(aChannel.readInt());
    }
    
    static void writeLobCharLength(CmChannel aChannel, long aLocatorId) throws SQLException
    {
        aChannel.checkWritable(9);
        aChannel.writeOp(DB_OP_LOB_CHAR_LENGTH);
        aChannel.writeLong(aLocatorId);
    }
    
    private static void readLobCharLengthResult(CmChannel aChannel, CmClobGetResult aResult) throws SQLException
    {
        aResult.setLocatorId(aChannel.readLong());
        aResult.setCharLength(aChannel.readInt());
    }
    
    static void writeLobFree(CmChannel aChannel, long aLocatorId) throws SQLException
    {
        aChannel.checkWritable(9);
        aChannel.writeOp(DB_OP_LOB_FREE);
        aChannel.writeLong(aLocatorId);
    }

    static void writeXaOperation(CmChannel aChannel, byte aOp, int aRmId, long aOpFlag, long aArgument) throws SQLException
    {
        aChannel.checkWritable(22);
        aChannel.writeOp(DB_OP_XA_OPERATION);
        aChannel.writeByte(aOp);
        aChannel.writeInt(aRmId);
        aChannel.writeLong(aOpFlag);
        aChannel.writeLong(aArgument);
    }
    
    static void writeXaTransaction(CmChannel aChannel, byte aOp, int aRmId, long aOpFlag, long aArgument, long aFormatId, byte[] aGlobalTransactionId, byte[] aBranchQualifier) throws SQLException
    {
        aChannel.checkWritable(46 + AltibaseXid.MAXDATASIZE);
        aChannel.writeOp(DB_OP_XA_TRANSACTION);
        aChannel.writeByte(aOp);
        aChannel.writeInt(aRmId);
        aChannel.writeLong(aOpFlag);
        aChannel.writeLong(aArgument);
        aChannel.writeLong(aFormatId);
        aChannel.writeLong(aGlobalTransactionId.length);
        aChannel.writeLong(aBranchQualifier.length);
        aChannel.writeBytes(aGlobalTransactionId);
        aChannel.writeBytes(aBranchQualifier);
        // Xid Data�� 128B�� �ǵ��� padding
        int sPadLen = AltibaseXid.MAXDATASIZE - (aGlobalTransactionId.length + aBranchQualifier.length);
        for (int i = 0; i < sPadLen; i++)
        {
            aChannel.writeByte((byte) 0);
        }
    }
    
    private static void readXidResult(CmChannel aChannel, CmXidResult aResult) throws SQLException
    {
        long sFormatId = aChannel.readLong();
        int sGlobalTransIdLen = (int) aChannel.readLong();
        int sBranchQualifierLen = (int) aChannel.readLong();
        byte[] sGlobalTransId = new byte[sGlobalTransIdLen];
        byte[] sBranchQualifier = new byte[sBranchQualifierLen];
        aChannel.readBytes(sGlobalTransId);
        aChannel.readBytes(sBranchQualifier);
        // Xid Data�� �׻� 128B�� ä���� ������ �Ǿ��ִ�. �׷��Ƿ�, ������ skip �ؾ��Ѵ�.
        aChannel.skip(AltibaseXid.MAXDATASIZE - (sGlobalTransIdLen + sBranchQualifierLen));

        aResult.addXid(new AltibaseXid(sFormatId, sGlobalTransId, sBranchQualifier));
    }

    private static void readXaResult(CmChannel aChannel, CmXAResult aResult) throws SQLException
    {
        aChannel.readByte(); // XA op
        aResult.setResultValue(aChannel.readInt());
    }

    /**
     * Get Plan operation�� ����.
     *
     * @param aChannel operation�� �� channel 
     * @param aStmtID  Statement ID
     *
     * @throws SQLException channel�� ���� ���� ���
     */
    static void writeGetPlan(CmChannel aChannel, int aStmtID) throws SQLException
    {
        aChannel.checkWritable(5);
        aChannel.writeOp(DB_OP_GET_PLAN);
        aChannel.writeInt(aStmtID);
    }

    /**
     * Get Plan operation ����� �д´�. 
     *
     * @param aChannel ����� ���� channel
     * @param aResult  ����� ���� ��ü
     *
     * @throws SQLException ����� ���� �� ���� ���
     */
    private static void readGetPlanResult(CmChannel aChannel, CmGetPlanResult aResult) throws SQLException
    {
        aResult.setStatementId(aChannel.readInt());
        int sPlanTextSize = aChannel.readInt();
        aResult.setPlanText(aChannel.readString(sPlanTextSize, 0));
    }
}
