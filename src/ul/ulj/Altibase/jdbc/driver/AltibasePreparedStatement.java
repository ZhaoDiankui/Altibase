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

package Altibase.jdbc.driver;

import java.io.InputStream;
import java.io.Reader;
import java.math.BigDecimal;
import java.net.URL;
import java.sql.Array;
import java.sql.Blob;
import java.sql.Clob;
import java.sql.Date;
import java.sql.ParameterMetaData;
import java.sql.PreparedStatement;
import java.sql.Ref;
import java.sql.ResultSet;
import java.sql.ResultSetMetaData;
import java.sql.SQLException;
import java.sql.Time;
import java.sql.Timestamp;
import java.sql.Types;
import java.util.ArrayList;
import java.util.BitSet;
import java.util.Calendar;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.logging.Level;
import java.util.logging.Logger;

import Altibase.jdbc.driver.cm.CmBufferWriter;
import Altibase.jdbc.driver.cm.CmExecutionResult;
import Altibase.jdbc.driver.cm.CmGetBindParamInfoResult;
import Altibase.jdbc.driver.cm.CmOperation;
import Altibase.jdbc.driver.cm.CmPrepareResult;
import Altibase.jdbc.driver.cm.CmProtocol;
import Altibase.jdbc.driver.cm.CmProtocolContextDirExec;
import Altibase.jdbc.driver.cm.CmProtocolContextPrepExec;
import Altibase.jdbc.driver.datatype.*;
import Altibase.jdbc.driver.ex.Error;
import Altibase.jdbc.driver.ex.ErrorDef;
import Altibase.jdbc.driver.logging.LoggingProxy;
import Altibase.jdbc.driver.logging.TraceFlag;
import Altibase.jdbc.driver.util.AltiSqlProcessor;

public class AltibasePreparedStatement extends AltibaseStatement implements PreparedStatement
{
    // protected RowHandle mBatchRowHandle;
    protected int mBatchJobCount;
    protected boolean mBatchAdded = false;
    protected List mContext;
    protected List<Column> mBindColumns;
    protected BatchDataHandle mBatchRowHandle;
    // LobUpdator�� �ش� ���̺��� LOB �÷��� INSERT�� UPDATE�� �߻��� ��� ���ȴ�.
    // PreparedStatement�� setter method�� �ش� ���̺��� LOB �÷��� ���� ����� ���
    // execute protocol�� �ƴ� LOB ���� protocol�� ���� �����Ͱ� ���Եȴ�.
    // �̸� ���� �ش� column�� ������ �����͵��� LobUpdator�� �����ص� ��, execute �Ŀ� LOB protocol�� ���� �����Ѵ�.
    protected LobUpdator mLobUpdator = null;
    // PROJ-2368 Batch �۾��� Atomic Operation ���� ���� Flag, Default �� false
    private boolean mIsAtomicBatch = false;
    // BUG-40081 batchó���� �ӽ��÷����� �����Ҷ� ���ȴ�.
    private List mTempArgValueList = new ArrayList();
    private static Logger mLogger;

    AltibasePreparedStatement(AltibaseConnection aConnection, String aSql, int aResultSetType, int aResultSetConcurrency, int aResultSetHoldability) throws SQLException
    {
        super(aConnection, aResultSetType, aResultSetConcurrency, aResultSetHoldability);

        if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
        {
            mLogger = Logger.getLogger(LoggingProxy.JDBC_LOGGER_DEFAULT);
        }  
        
        if (aSql == null)
        {
            Error.throwSQLException(ErrorDef.NULL_SQL_STRING);
        }

        if (mEscapeProcessing)
        {
            aSql = AltiSqlProcessor.processEscape(aSql);
        }
        setSql(aSql);

        CmProtocolContextPrepExec sContext = (CmProtocolContextPrepExec)mContext.get(mCurrentResultIndex);
        
        try
        {
            aSql = procDowngradeAndGetTargetSql(aSql);
            // BUG-42712 deferred �����϶��� prepare ������ ���� ArrayList�� ���常 �ϰ� �Ѿ��.
            if (mIsDeferred)
            {
                Map sMethodInfo = new HashMap();
                sMethodInfo.put("methodname", "prepare");
                sMethodInfo.put("args", new Object[] {
                        sContext,
                        mStmtCID,
                        aSql,
                        Boolean.valueOf(getResultSetHoldability() == ResultSet.HOLD_CURSORS_OVER_COMMIT),
                        Boolean.valueOf(usingKeySetDriven()),
                        Boolean.valueOf(mConnection.nliteralReplaceOn()),
                        Boolean.valueOf(mIsDeferred) });
                mDeferredRequests.add(sMethodInfo);
            }
            else
            {
                CmProtocol.prepare(sContext,
                                   mStmtCID,
                                   aSql,
                                   (getResultSetHoldability() == ResultSet.HOLD_CURSORS_OVER_COMMIT),
                                   usingKeySetDriven(),
                                   mConnection.nliteralReplaceOn(),
                                   false);
            }
        }
        catch (SQLException ex)
        {
            AltibaseFailover.trySTF(mConnection.failoverContext(), ex);
        }

        if (sContext.getError() != null)
        {
            mWarning = Error.processServerError(mWarning, sContext.getError());
        }
        mPrepareResult = sContext.getPrepareResult();
        if (mPrepareResultColumns == null)
        {
            mPrepareResultColumns = getProtocolContext().getGetColumnInfoResult().getColumns();
        }

        //BUG-42424 deferred�� ��쿡�� �Ķ������ ������ �� ������ �� �� �����Ƿ� ArrayList�� ����
        mBindColumns = new ArrayList<Column>();
        if (mIsDeferred)
        {
            // BUG-42424 deferred�� ��� PrepareResult ������ ���������Ѵ�.
            CmPrepareResult sPrepareResult = (CmPrepareResult)sContext.getCmResult(CmPrepareResult.MY_OP);
            sPrepareResult.setResultSetCount(1);
            sPrepareResult.setStatementId(0);
        }
        else
        {
            // BUG-42424 deferred�� �ƴ� ��� �Ķ���� ������ŭ List�� null�� ä���.
            for (int i = 0; i < mPrepareResult.getParameterCount(); i++)
            {
                mBindColumns.add(null);
            }
        }
    }

    public void setAtomicBatch(boolean aValue) throws SQLException
    {
    	// Target Table �� Lob Column �� �����ϴ� ���, Atomic Batch �� ����� �� ����
        if ((aValue == true && mBatchRowHandle instanceof RowHandle))
        {
            Error.throwSQLException(ErrorDef.UNSUPPORTED_FEATURE, "atomic batch with LOB column");
        }
    	mIsAtomicBatch = aValue;
    }

    public boolean getAtomicBatch()
    {
    	return mIsAtomicBatch;
    }

    protected void createProtocolContext()
    {
        if(mContext==null)
        {
            mContext = new ArrayList(1);
        }
        
        mContext.add(new CmProtocolContextPrepExec(mConnection.channel()));
    }

    protected CmProtocolContextDirExec getProtocolContext()
    {
        return (CmProtocolContextPrepExec)mContext.get(mCurrentResultIndex);
    }
    
    protected void closeAllResultSet() throws SQLException
    {
        Iterator iter = mExecuteResultMgr.iterator();
        while(iter.hasNext())
        {
            ((ExecuteResult)iter.next()).mReturned = true;
        }
    }
    
    protected void closeAllPrevResultSet() throws SQLException
    {
        for(int i=0; i<mExecuteResultMgr.size(); i++)
        {
            mExecuteResultMgr.get(i).mReturned = true;
            if(i==mCurrentResultIndex)
            {
                break;
            }
        }
    }
    
    protected void closeCurrentResultSet() throws SQLException
    {
        if(mExecuteResultMgr.size() > 0 && mExecuteResultMgr.size() >= (mCurrentResultIndex - 1))
        {
            (mExecuteResultMgr.get(mCurrentResultIndex)).mReturned = true;
        }
    }

    protected Column getColumn(int aIndex, int aSqlType) throws SQLException
    {
        return getColumn(aIndex, aSqlType, null);
    }

    protected Column getColumn(int aIndex, int aSqlType, Object aValue) throws SQLException
    {
        throwErrorForClosed();
        checkBindColumnLength(aIndex);
        
        CmProtocolContextPrepExec sContext = (CmProtocolContextPrepExec)mContext.get(mCurrentResultIndex);
        ColumnInfo sColumnInfo = sContext.getBindParamResult().getColumnInfo(aIndex);
        if (sColumnInfo == null)
        {
            Error.throwSQLException(ErrorDef.UNSUPPORTED_FEATURE, "DataType: " + String.valueOf(aSqlType));    
        }
        if (aSqlType == Types.NULL)
        {
            if (mBindColumns.get(aIndex - 1) == null)
            {
                createBindColumnInfo(aIndex - 1, sColumnInfo);
            }
        }
        else
        {
            // BUG-43719 setBytes�� ������ byte[]�� ũ�⿡ ���� lob locator�� ����ؾ� �ϴ��� ���θ� �����Ѵ�.
            boolean sChangeToLob = (aValue instanceof byte[] &&
                                    ((byte[])aValue).length > ColumnConst.MAX_BYTE_LENGTH);
            changeBindColumnInfo(changeSqlType(aSqlType, sColumnInfo.getDataType(), sChangeToLob), aIndex, sColumnInfo);
        }
        return mBindColumns.get(aIndex - 1);
    }

    /**
     * Bind Column ������ ���� �� ���� �����Ѵ�.<br/>�̶� ColumnInfo��ü�� change type flag�� enable��Ų��.
     * @param aIndex
     * @param aColumnInfo
     */
    private void createBindColumnInfo(int aIndex, ColumnInfo aColumnInfo)
    {
        int sColumntype = this.changeColumnType(aColumnInfo.getDataType());
        mBindColumns.set(aIndex, mConnection.channel().getColumnFactory().getInstance(sColumntype));
        ColumnInfo sClonedColumnInfo = (ColumnInfo)aColumnInfo.clone();
        // BUG-40081 setNull�� ȣ��Ǿ������� ColumnInfo ��ü�� shouldChangeType flag�� true�� �����Ѵ�.
        sClonedColumnInfo.setShouldChangeType(true);
        mBindColumns.get(aIndex).getDefaultColumnInfo(sClonedColumnInfo);
        mBindColumns.get(aIndex).setColumnInfo(sClonedColumnInfo);
    }

    /**
     * ����Ÿ�� �ְ�޴µ� ������� �ʴ� Ÿ����, ������ Ÿ������ �ٲ��ش�.</br>
     * CLOB, BLOB, GEOMETRY, CHAR, NCHAR�� �÷�Ÿ���� ���� CLOB_LOCATOR, BLOB_LOCATOR, BINARY, </br>
     * VARCHAR, NVARCHAR�� �ٲ��ش�.
     */
    private int changeColumnType(int aColumnType)
    {
        int sResultColumnType = aColumnType;
        switch (aColumnType)
        {
            case ColumnTypes.CLOB:
                sResultColumnType = ColumnTypes.CLOB_LOCATOR;
                break;
            case ColumnTypes.BLOB:
                sResultColumnType = ColumnTypes.BLOB_LOCATOR;
                break;
            case ColumnTypes.GEOMETRY:
                sResultColumnType = ColumnTypes.BINARY;
                break;

            // Batch������ CHAR ���� ��ȸ�� ���� �ļ� (ref. BUG-39624)
            case ColumnTypes.CHAR:
                sResultColumnType = ColumnTypes.VARCHAR;
                break;
            case ColumnTypes.NCHAR:
                sResultColumnType = ColumnTypes.NVARCHAR;
                break;
            default:
                break;
        }
        return sResultColumnType;
    }

    /**
     * NCHAR �÷��� GEOMETRY, LOB �÷��� ��Ȳ�� �°� ������ �ش�.
     * @param aSqlType
     * @param aColumnType
     * @param aChangeToLob lob �÷����� �����ؾ� �ϴ����� ��Ÿ���� flag
     * @return
     */
    private int changeSqlType(int aSqlType, int aColumnType, boolean aChangeToLob)
    {
        int sResultSqlType = aSqlType;

        switch (aSqlType)
        {
            // NCHAR �÷��� CHAR �÷��� charset�� �ٸ��Ƿ� �������־�� �Ѵ�.
            case Types.CHAR:
                if (ColumnTypes.isNCharType(aColumnType))
                {
                    sResultSqlType = AltibaseTypes.NVARCHAR;
                }
                else
                {
                    // CHAR ���� ��ȸ�� ���� �ļ�: padding (ref. BUG-27818), Batch (ref. BUG-39624)
                    sResultSqlType = AltibaseTypes.VARCHAR;
                }
                break;
            case Types.VARCHAR:
                if (ColumnTypes.isNCharType(aColumnType))
                {
                    sResultSqlType = AltibaseTypes.NVARCHAR;
                }
                break;

            // setBytes(), setObject(BINARY)�ε� geometry�� ������ �� �ְ� �ϱ� ����.
            // BUGBUG (2012-12-14) GEOMFROMWKB() ���� �������� ������ �ȵȴ�; ���� ColumnType�� VARCHAR�� ���ͼ���;
            case Types.BINARY:
                if (ColumnTypes.isGeometryType(aColumnType))
                {
                    sResultSqlType = AltibaseTypes.GEOMETRY;
                }
                /* clientsideautocommit�� Ȱ��ȭ �Ǿ� �հų� BUG-43719 byte[] ����� 65534�� ���� ���� lob type���� �����Ѵ�. */
                if (mConnection.isClientSideAutoCommit() || aChangeToLob)
                {
                    if (aColumnType == ColumnTypes.BLOB)
                    {
                        sResultSqlType = AltibaseTypes.BLOB;
                    }
                    else if (aColumnType == ColumnTypes.CLOB)
                    {
                        sResultSqlType = AltibaseTypes.CLOB;
                    }
                }

                break;
            case Types.NUMERIC:
                // BUG-41061 overflowó���� ���� type�� float�� ��� numeric���� float���� �����Ų��. 
                if (aColumnType == ColumnTypes.FLOAT)
                {
                    sResultSqlType = AltibaseTypes.FLOAT;
                }
                break;
            default:
                break;
        }
        return sResultSqlType;
    }

    /**
     * bind column ������ ���ų� type�� ����Ǿ����� ������ �籸���Ѵ�.<br/>
     * �̶� ColumnInfo ��ü�� change flag�� enable �Ǿ� �ִ� ��� exception�� �߻���Ű�� �ʰ� bind �÷�Ÿ���� ��ü�Ѵ�.
     * @param aSqlType
     * @param aIndex
     * @param aColumnInfo
     * @throws SQLException
     */
    private void changeBindColumnInfo(int aSqlType, int aIndex, ColumnInfo aColumnInfo) throws SQLException
    {
        ColumnInfo sColumnInfo = aColumnInfo;
        int sColumnType = sColumnInfo.getDataType();
        Column sBindColumn = mBindColumns.get(aIndex - 1);
        if (sBindColumn == null || !sBindColumn.isMappedJDBCType(aSqlType))
        {
            // IN-OUT Ÿ���� �������� �޾ƿ��°� �ƴϹǷ� ����ص״ٰ� �ٽ� ����Ѵ�.
            byte sInOutType = (sBindColumn == null) ? ColumnInfo.IN_OUT_TARGET_TYPE_NONE : 
                                  sBindColumn.getColumnInfo().getInOutTargetType(); 
            Column sMappedColumn = mConnection.channel().getColumnFactory().getMappedColumn(aSqlType);
            // BUG-40081 batch����̰� ColumnInfo�� changetype flag�� true�϶��� bind column type�� �ٲ��ش�.
            if (mBatchAdded && shouldChangeBindColumnType(aSqlType, aIndex))
            {
                mBatchRowHandle.changeBindColumnType(aIndex - 1, sMappedColumn, sColumnInfo, sInOutType);
                return;
            }
            else
            {
                mBindColumns.set(aIndex - 1, sMappedColumn);
            }

            if (mBindColumns.get(aIndex - 1) == null)
            {
                Error.throwSQLException(ErrorDef.UNSUPPORTED_FEATURE, AltibaseTypes.toString(aSqlType) + " type");
            }
            
            if (mBindColumns.get(aIndex - 1).getDBColumnType() != sColumnType)
            {
                sColumnInfo = (ColumnInfo)sColumnInfo.clone();
                sColumnInfo.modifyInOutType(sInOutType);
                mBindColumns.get(aIndex - 1).getDefaultColumnInfo(sColumnInfo);
            }
            mBindColumns.get(aIndex - 1).setColumnInfo(sColumnInfo);
        }
    }
    
    /**
     * mArgValueList�� ũ�⸸ŭ ������ ���鼭 ��������ҿ� �����ߴ� ������ RowHandle�� store�Ѵ�.
     * @throws SQLException
     */
    private void storeTempArgValuesToRowHandle() throws SQLException
    {
        ArrayList sArgOriValue = new ArrayList();
        // BUG-40081 ���� ������ ������ ArrayList�� �����Ѵ�.
        for (Column sColumn : mBindColumns)
        {
            sArgOriValue.add(sColumn.getObject());
        }

        int sStoreCursorPos = mTempArgValueList.size();
        // BUG-40081 �ӽ�����ҿ� �����ߴ� ������ bind column�� ������ �� store�Ѵ�.
        for (int sIdx=0 ; sIdx < sStoreCursorPos; sIdx++)
        {
            ArrayList sArgumentInfo = (ArrayList)mTempArgValueList.get(sIdx);
            for (int sColumnCnt = 0; sColumnCnt < mBindColumns.size(); sColumnCnt++)
            {
                setObjectInternal(sArgumentInfo.get(sColumnCnt), mBindColumns.get(sColumnCnt));
            }
            mBatchRowHandle.store();
        }
        mTempArgValueList.clear();
        // BUG-40081 ���� ������ �����Ѵ�.
        for (int sIdx=0; sIdx < sArgOriValue.size(); sIdx++)
        {
            mBindColumns.get(sIdx).setValue(sArgOriValue.get(sIdx));
        }
    }

    /**
     * BUG-40081 batch ����϶� ���ε�� �÷� �� type�� �ٲ�� �ϴ��� Ȯ���Ѵ�.</br>
     * ColumnInfo��ü�� change flag�� false�̰ų� ���� �÷��� LOB_LOCATOR�� ��쿡�� 
     * CANNOT_BIND_THE_DATA_TYPE_DURING_ADDING_BATCH_JOB exception�� ������.
     * @param aSqlType
     * @param aIndex
     * @return
     * @throws SQLException
     */
    private boolean shouldChangeBindColumnType(int aSqlType, int aIndex) throws SQLException
    {
        if (mBindColumns.get(aIndex - 1) != null)
        {
            ColumnInfo sColumnInfo = mBindColumns.get(aIndex - 1).getColumnInfo();
            // BUG-40081 ���� �÷��� LOB_LOCATOR�̰ų� set null flag�� false�϶��� Exception�� ������.
            if (!(sColumnInfo.shouldChangeType()) || sColumnInfo.getDataType() == ColumnTypes.CLOB_LOCATOR || 
                    sColumnInfo.getDataType() == ColumnTypes.BLOB_LOCATOR)
            {
                Error.throwSQLException(ErrorDef.CANNOT_BIND_THE_DATA_TYPE_DURING_ADDING_BATCH_JOB,
                                        String.valueOf(mBindColumns.get(aIndex - 1).getMappedJDBCTypes()[0]),
                                        String.valueOf(aSqlType));
            }
            else 
            {
                return true;
            }
        }
        return false;
    }

    protected Column getColumnForInType(int aIndex, int aSqlType) throws SQLException
    {
        return getColumnForInType(aIndex, aSqlType, null);
    }

    private Column getColumnForInType(int aIndex, int aSqlType, Object aValue) throws SQLException
    {
        throwErrorForClosed();

        if (mIsDeferred)
        {
            // BUG-42424 deferred�� ��� ColumnInfo�� ���������Ѵ�.
            addMetaColumnInfo(aIndex, aSqlType, getDefaultPrecisionForDeferred(aSqlType, aValue));
        }
        
        Column sColumn = getColumn(aIndex, aSqlType, aValue);
        sColumn.getColumnInfo().addInType();
        return sColumn;
    }

    public ResultSet getResultSet() throws SQLException
    {
        throwErrorForClosed();

        if(mCurrentResultIndex != 0)
        {
            createProtocolContext();
            copyProtocolContextExceptFetchContext();
        }
        return super.getResultSet();
    }
    
    protected void incCurrentResultSetIndex()
    {
        mCurrentResultIndex++;
        createProtocolContext();
        copyProtocolContextExceptFetchContext();
    }
    
    private void copyProtocolContextExceptFetchContext()
    {
        int sPrevResultIndex = mCurrentResultIndex - 1;
        CmProtocolContextPrepExec sCurProtocolContextPrepExec = (CmProtocolContextPrepExec)mContext.get(mCurrentResultIndex);
        CmProtocolContextPrepExec sPrevProtocolContextPrepExec = (CmProtocolContextPrepExec)mContext.get(sPrevResultIndex);
        
        sCurProtocolContextPrepExec.addCmResult(sPrevProtocolContextPrepExec.getExecutionResult());
        sCurProtocolContextPrepExec.addCmResult(sPrevProtocolContextPrepExec.getPrepareResult());
    }
    
    private void checkParameters() throws SQLException
    {
        for (int i = 0; i < mBindColumns.size(); i++)
        {
            if (mBindColumns.get(i) == null)
            {
                Error.throwSQLException(ErrorDef.NEED_MORE_PARAMETER, String.valueOf(i + 1));
            }
        }
    }

    public void addBatch() throws SQLException
    {
        throwErrorForClosed();
        
        // ù  ��° addBatch �� ���� ���� 
        if (!mBatchAdded)
        {
            checkParameters();
            
            if (mLobUpdator != null)
            {
                mBatchRowHandle = new RowHandle();	
            }
            else
            {
                mBatchRowHandle = new ListBufferHandle();
                // ListBufferHandle �� Encoder �� ������ charset name ���� �����;��Ѵ�.
                ((CmBufferWriter)mBatchRowHandle).setCharset(mConnection.channel().getCharset(), 
                                                             mConnection.channel().getNCharset());
            }
            
            mBatchRowHandle.setColumns(mBindColumns);
            mBatchRowHandle.initToStore();
        }        
        // BUG-24704 parameter�� ������ 1�Ǹ� add
        else if (mBindColumns.size() == 0)
        {
            return;
        }
        
        if (mBatchRowHandle.size() == Integer.MAX_VALUE)
        {
            Error.throwSQLException(ErrorDef.TOO_MANY_BATCH_JOBS);
        }

        // BUG-40081 LOB�÷��� ���� �÷��� setNull�� ȣ��Ǿ��ٸ� RowHandle�� ���������ʰ� ��������ҿ� value������ �����Ѵ�.
        if (mLobUpdator == null)
        {
            if (setNullColumnExist())
            {
                List sArgumentInfo = new ArrayList();
                for (int sIdx = 0; sIdx < mBindColumns.size(); sIdx++)
                {
                    sArgumentInfo.add(mBindColumns.get(sIdx).getObject());
                }
                mTempArgValueList.add(sArgumentInfo);
                mBatchAdded = true;
                return;
            }
            /* BUG-40081 null column�� ������ �ӽ�argument���� �����Ѵٸ� �ӽ�����ҿ� �ִ� argument������ </br>
               RowHandle�� �̵���Ų��.  */
            else if (mTempArgValueList.size() > 0)
            {
                storeTempArgValuesToRowHandle();
            }
        }
        mBatchRowHandle.store();
        mBatchAdded = true;
    }

    /**
     * Bind�� �÷� �� setNull�� ���� Null������ �Ǿ� �ִ� �÷��� �ִ��� Ȯ���Ѵ�.
     */
    private boolean setNullColumnExist()
    {
        boolean sNullColumnExist = false;
        for (Column sColumn : mBindColumns)
        {
            if (sColumn.getColumnInfo().shouldChangeType())
            {
                sNullColumnExist = true;
                break;
            }
        }
        return sNullColumnExist;
    }

    public void clearParameters() throws SQLException
    {
        throwErrorForClosed();
        for (int i = 0; i < mBindColumns.size(); i++)
        {
            mBindColumns.set(i, null);
        }
    }

    public void clearBatch() throws SQLException
    {
        mBatchAdded = false;
        clearParameters();
    }

    public int[] executeBatch() throws SQLException
    {
        throwErrorForClosed();
        if (mPrepareResult.isSelectStatement())
        {
            Error.throwBatchUpdateException(ErrorDef.INVALID_BATCH_OPERATION_WITH_SELECT);
        }

        clearAllResults();

        if (!mBatchAdded)
        {
            mExecuteResultMgr.add(new ExecuteResult(false, -1));
            return EMPTY_BATCH_RESULT;
        }
        
        // Lob Column �� �����ϴ� ��� ���� RowHandle ���
        if (mLobUpdator != null)
        {
        	((RowHandle)mBatchRowHandle).beforeFirst();
        }
        else if (setNullColumnExist()) // BUG-40081 setNull�÷� flag�� �ִ� ��� �ӽ�������� ������ RowHandle�� store�Ѵ�.
        {
            storeTempArgValuesToRowHandle();
        }
        
        mBatchJobCount = mBatchRowHandle.size();
        
        if (mBatchJobCount == 0)
        {
            Error.throwSQLException(ErrorDef.NO_BATCH_JOB);
        }
        
        try
        {
            invokeDeferredRequests();
            // LOB Column �� �������� �ʴ� ��쿡�� List Protocol �� ��� 
            if(mLobUpdator == null)
            {
                CmProtocol.preparedExecuteBatchUsingList((CmProtocolContextPrepExec)mContext.get(mCurrentResultIndex), 
                                                          mBindColumns,
                                                          (ListBufferHandle)mBatchRowHandle, 
                                                          mBatchJobCount,
                                                          mIsAtomicBatch);
            }
            else
            {
                RowHandle sRowHandle = (RowHandle)mBatchRowHandle;
                
                CmProtocol.preparedExecuteBatch((CmProtocolContextPrepExec)mContext.get(mCurrentResultIndex),
                                               mBindColumns,
                                               sRowHandle,
                                               mBatchJobCount);
                sRowHandle.beforeFirst();
                mLobUpdator.updateLobColumns();
            }
            /* PROJ-2190 executeBatch �� commit �������� ����  */
            CmProtocol.clientCommit((CmProtocolContextPrepExec)mContext.get(mCurrentResultIndex), mConnection.isClientSideAutoCommit());
        }
        catch (SQLException ex)
        {
            AltibaseFailover.trySTF(mConnection.failoverContext(), ex);
        }
        finally
        {
            // BUGBUG (2012-11-28) ���忡���� executeBatch ������ clear�Ǵ°� ��Ȯ�ϰ� ������ ���� ��. oracle�� ������.
            clearBatch();
        }
        try
        {
            super.afterExecution();
        }
        catch (SQLException sEx)
        {
            CmExecutionResult sExecResult = getProtocolContext().getExecutionResult();
            Error.throwBatchUpdateException(sEx, sExecResult.getUpdatedRowCounts());
        }
        int[] sUpdatedRowCounts = mExecutionResult.getUpdatedRowCounts();
        int sUpdateCount = 0;

        if (mIsAtomicBatch && mPrepareResult.isInsertStatement())
        {
            /* Atomic Operation Insert �� ���� ���� ���, array index 0 �� ���Կ� ������ ���ڵ� ������ ���� ��ȯ �ȴ�. 
               (Atomic �� ���� Insert ������ ����Ͽ� �����Ͽ��� ������, ���� Insert ������ �ƴ� ���, ������ ������ �� ����.)
               Atomic Operation ���� �����ϸ� ����� SQLException ������ ó�� �ǹǷ� ���⼭�� ���� ���� ��츸 ����ϸ� �ȴ�.
                              ���� executeBatch Interface �� �����ֱ� ���� ���Կ� ������ ���ڵ� ���� Size �� int array �� ����� ���� 1 �� �־� ��ȯ�Ѵ�. */
            sUpdatedRowCounts = new int[sUpdatedRowCounts[0]];

            for(int i = 0; i < sUpdatedRowCounts.length; i++)
            {
                sUpdatedRowCounts[i] = 1;
            }
        }
        else
        {
            for (int i = 0; i < sUpdatedRowCounts.length; i++)
            {
                sUpdateCount += sUpdatedRowCounts[i];
            }
        }

        mExecuteResultMgr.add(new ExecuteResult(false, sUpdateCount));
        return sUpdatedRowCounts;
    }

    public boolean execute() throws SQLException
    {
        throwErrorForClosed();
        throwErrorForBatchJob("execute");
        checkParameters();

        clearAllResults();

        try
        {
            invokeDeferredRequests();
            // PROJ-2427 cursor�� �ݾƾ� �ϴ� ������ ���� �Ѱ��ش�.
            CmProtocol.preparedExecute((CmProtocolContextPrepExec)mContext.get(mCurrentResultIndex), 
                                       mBindColumns, 
                                       mConnection.isClientSideAutoCommit(), 
                                       mPrepareResult != null && mPrepareResult.isSelectStatement());
        }
        catch (SQLException ex)
        {
            AltibaseFailover.trySTF(mConnection.failoverContext(), ex);
        }
        super.afterExecution();

        if (mLobUpdator != null)
        {
            mLobUpdator.updateLobColumns(); 
            /* PROJ-2190 lob �÷��� ���ԵǾ� ���� ���� updateLobColumns �� ������ commit ���������� �����Ѵ�. */
            CmProtocol.clientCommit((CmProtocolContextPrepExec)mContext.get(mCurrentResultIndex), mConnection.isClientSideAutoCommit());
        }
        
        boolean sResult = processExecutionResult();
        executeForGeneratedKeys();
        return sResult;
    }

    public ResultSet executeQuery() throws SQLException
    {
        throwErrorForClosed();
        throwErrorForBatchJob("executeQuery");
        
        // BUGBUG (2013-02-21) ResultSet�� ������ �ʴ� PSM�� �����ϸ� ���ܴ� ������ ������ �ȴ�.
        // �������� Ȯ������ �����Ƿ� ResultSet�� ���� ���ɼ��� �ִ� SELECT, PSM�� �ƴҶ��� �ٷ� ���ܸ� ������.
        // PSM�� ResultSet�� ������ ���� ��쿡�� processExecutionQueryResult()���� ���ܸ� �ø��µ�,
        // ���� ������ �̹� ���������� �� ���� �� �����̴�.
        // BUG-42424 deferred �����϶��� sql���� Ÿ���� �� �� ���⶧���� �ش� ������ �����Ѵ�.
        if (!mIsDeferred && !mPrepareResult.isSelectStatement() && !mPrepareResult.isStoredProcedureStatement())
        {
            Error.throwSQLException(ErrorDef.NO_RESULTSET, getSql());
        }
        checkParameters();

        super.clearAllResults();

        try
        {
            invokeDeferredRequests();
            // PROJ-2427 cursor�� �ݾƾ� �ϴ� ������ ���� �Ѱ��ش�.
            CmProtocol.preparedExecuteAndFetch((CmProtocolContextPrepExec)mContext.get(mCurrentResultIndex),
                                                mBindColumns, 
                                                mFetchSize, 
                                                mMaxRows, 
                                                mMaxFieldSize,
                                                mPrepareResult != null && mPrepareResult.isSelectStatement());
        }
        catch (SQLException ex)
        {
            AltibaseFailover.trySTF(mConnection.failoverContext(), ex);
        }
        super.afterExecution();

        ResultSet sResult = processExecutionQueryResult(getSql());
        executeForGeneratedKeys();
        return sResult;
    }

    public int executeUpdate() throws SQLException
    {
        boolean sHasResult = execute();
        // BUGBUG (2013-01-31) spec�� ������ ���ܸ� ������ �Ѵ�. �׷���, oracle�� �ȱ׷���. ������ oracle..
//        Error.checkAndThrowSQLException(sHasResult, ErrorDef.SQL_RETURNS_RESULTSET, getSql());
        return mExecuteResultMgr.getFirst().mUpdatedCount;
    }

    public ParameterMetaData getParameterMetaData() throws SQLException
    {
        throwErrorForClosed();
        // BUG-42424 deferred �϶��� ���� �Ķ���� ��Ÿ������ �޾ƿ;� getParameterMetaData�� ������ �� �ִ�.
        if (mIsDeferred)
        {
            receivePrepareResults();
        }
        return new AltibaseParameterMetaData(mBindColumns);
    }

    public void setArray(int aParameterIndex, Array aValue) throws SQLException
    {
        Error.throwSQLException(ErrorDef.UNSUPPORTED_FEATURE, "Array type");
    }

    public void setAsciiStream(int aParameterIndex, InputStream aValue, int aLength) throws SQLException
    {
        ClobLocatorColumn sClobColumn = (ClobLocatorColumn)getColumnForInType(aParameterIndex, Types.CLOB);
        
        if (mLobUpdator == null && aValue != null)
        {
            mLobUpdator = new LobUpdator(this);
        }
        
        if(aValue != null)
        {
            mLobUpdator.addLobColumn(sClobColumn, aValue, aLength);
        }
    }

    public void setBigDecimal(int aParameterIndex, BigDecimal aValue) throws SQLException
    {
        getColumnForInType(aParameterIndex, Types.NUMERIC).setValue(aValue);
    }

    public void setBinaryStream(int aParameterIndex, InputStream aValue) throws SQLException
    {
        setBinaryStream(aParameterIndex, aValue, Long.MAX_VALUE);
    }

    public void setBinaryStream(int aParameterIndex, InputStream aValue, int aLength) throws SQLException
    {
        setBinaryStream(aParameterIndex, aValue, (long)aLength);
    }

    public void setBinaryStream(int aParameterIndex, InputStream aValue, long aLength) throws SQLException
    {
        BlobLocatorColumn sBlobLocatorColumn = (BlobLocatorColumn)getColumnForInType(aParameterIndex, Types.BLOB);
        if (mLobUpdator == null && aValue != null)
        {
            mLobUpdator = new LobUpdator(this);
        }
        
        if(aValue != null)
        {
            mLobUpdator.addLobColumn(sBlobLocatorColumn, aValue, aLength);
        }
    }

    public void setBlob(int aParameterIndex, Blob aValue) throws SQLException
    {
        setObject(aParameterIndex, aValue, Types.BLOB);
    }

    public void setBoolean(int aParameterIndex, boolean aValue) throws SQLException
    {
        // BUG-42424 deferred�϶��� boolean�� precision�� VARCHAR�� Ʋ���� ������ �ϱ⶧���� value���� ���� ������.
        ((CommonCharVarcharColumn)getColumnForInType(aParameterIndex, Types.VARCHAR, 
                                                     Boolean.valueOf(aValue))).setTypedValue(aValue);
    }

    public void setByte(int aParameterIndex, byte aValue) throws SQLException
    {
        ((SmallIntColumn)getColumnForInType(aParameterIndex, Types.SMALLINT)).setTypedValue(aValue);
    }

    public void setBytes(int aParameterIndex, byte[] aValue) throws SQLException
    {
        setObject(aParameterIndex, aValue, Types.BINARY);  // BUG-38043 clientAutoCommit�� Ȱ��ȭ �Ǿ� �ִ� ��� lob locator�� ����ϵ��� �ϱ� ���� setObject�� ȣ���Ѵ�.
    }

    public void setCharacterStream(int aParameterIndex, Reader aValue) throws SQLException
    {
        setCharacterStream(aParameterIndex, aValue, Long.MAX_VALUE);
    }

    public void setCharacterStream(int aParameterIndex, Reader aValue, int aLength) throws SQLException
    {
        setCharacterStream(aParameterIndex, aValue, (long)aLength);
    }

    public void setCharacterStream(int aParameterIndex, Reader aValue, long aLength) throws SQLException
    {
        ClobLocatorColumn sClobColumn = (ClobLocatorColumn)getColumnForInType(aParameterIndex, Types.CLOB);
        
        if (mLobUpdator == null && aValue != null)
        {
            mLobUpdator = new LobUpdator(this);
        }
        
        if(aValue != null)
        {
            mLobUpdator.addLobColumn(sClobColumn, aValue, aLength);
        }
    }

    public void setClob(int aParameterIndex, Clob aValue) throws SQLException
    {
        setObject(aParameterIndex, aValue, Types.CLOB);
    }

    public void setDate(int aParameterIndex, Date aValue, Calendar aCalendar) throws SQLException
    {
        DateColumn sColumn = (DateColumn)getColumnForInType(aParameterIndex, Types.DATE);
        sColumn.setLocalCalendar(aCalendar);
        sColumn.setValue(aValue);
    }

    public void setDate(int aParameterIndex, Date aValue) throws SQLException
    {
        getColumnForInType(aParameterIndex, Types.DATE).setValue(aValue);
    }

    public void setDouble(int aParameterIndex, double aValue) throws SQLException
    {
        ((DoubleColumn)getColumnForInType(aParameterIndex, Types.DOUBLE)).setTypedValue(aValue);
    }

    public void setFloat(int aParameterIndex, float aValue) throws SQLException
    {
        getColumnForInType(aParameterIndex, Types.REAL).setValue(String.valueOf(aValue));
    }

    public void setInt(int aParameterIndex, int aValue) throws SQLException
    {
        ((IntegerColumn)getColumnForInType(aParameterIndex, Types.INTEGER)).setTypedValue(aValue);
    }

    /**
     * deferred�����϶� �÷� ��Ÿ ������ �������� �����Ѵ�. �̶� �����ϴ� ������ setXXX�Ҷ��� sqlType�̴�.
     * @param aIndex �÷� �ε���(1 base)
     * @param aDataType sql type
     * @param aPrecision Precision
     */
    protected void addMetaColumnInfo(int aIndex, int aDataType, int aPrecision)
    {
        CmProtocolContextPrepExec sContext = (CmProtocolContextPrepExec)mContext.get(mCurrentResultIndex);
        CmGetBindParamInfoResult sBindParamInfoResult = (CmGetBindParamInfoResult)sContext.getCmResult(CmGetBindParamInfoResult.MY_OP);
        if (sBindParamInfoResult.getColumnInfoListSize() < aIndex ||
            sBindParamInfoResult.getColumnInfo(aIndex) == null) // BUG-42424 �̹� �÷���Ÿ������ �����Ǿ� �ִٸ� �ǳʶڴ�.
        {
            ColumnInfo sColumnInfo = new ColumnInfo();
            sColumnInfo.makeDefaultValues();
            sColumnInfo.setColumnMetaInfos(aDataType, aPrecision);
            sBindParamInfoResult.addColumnInfo(aIndex, sColumnInfo);
            if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
            {
                mLogger.log(Level.INFO, "created bind param info for deferred : {0}", sColumnInfo);
            }
            /*
             * BUG-42879 mBindColumns�� changeBindColumnInfo�޼ҵ忡�� ��������⶧���� ���⼱ null�� ����Ʈ�� ä���.
             * setXXX(3, 1)�� ���� ����° �׸��� ���� ������ ��쿡�� ù��°�� �ι�°�� null�� ä���� �������� null�� ä���.
             */
            if (mBindColumns.size() < aIndex)
            {
                for (int i = mBindColumns.size(); i < aIndex - 1; i++)
                {
                    mBindColumns.add(null);
                }
                mBindColumns.add(null);
            }
        }
    }

    public void setLong(int aParameterIndex, long aValue) throws SQLException
    {
        ((BigIntColumn)getColumnForInType(aParameterIndex, Types.BIGINT)).setTypedValue(aValue);
    }

    public void setNull(int aParameterIndex, int aSqlType) throws SQLException
    {
        // BUG-38681 batch ó�� ���� sqlType�� �ٲ� �� �ְ� lob �÷��� ��� �ش� ���� null�̴���
        // loblocator�� �����ؾ��ϱ� ������ setObject(,,Types.NULL)�� ȣ���Ѵ�.
        this.setObject(aParameterIndex, null, Types.NULL);
    }

    public void setNull(int aParameterIndex, int aSqlType, String aTypeName) throws SQLException
    {
        // REF, STRUCT, DISTINCT�� �������� �ʱ� ������ aTypeName�� �����Ѵ�.
        setNull(aParameterIndex, aSqlType);
    }

    public void setObject(int aParameterIndex, Object aValue, int aTargetSqlType, int aScale) throws SQLException
    {
        if (aTargetSqlType == Types.NUMERIC || aTargetSqlType == Types.DECIMAL)
        {
            // oracle�� scale�� �����Ѵ�. �츮�� ����;
            BigDecimal sDecimalValue = new BigDecimal(aValue.toString());
            setBigDecimal(aParameterIndex, sDecimalValue);
        }
        else
        {
            setObject(aParameterIndex, aValue, aTargetSqlType);
        }
    }

    public void setObject(int aParameterIndex, Object aValue, int aTargetSqlType) throws SQLException
    {
        // BUG-42424 setObject(int, Object, int)���� ��� �����κ��� ���� prepare����� �޾ƾ��Ѵ�.
        if (mIsDeferred)
        {
            receivePrepareResults();
        }
        Column sColumn = getColumnForInType(aParameterIndex, aTargetSqlType, aValue);

        // BLOB�� CLOB�� �и��ϴ� ������ ���� ��ü ������ ���� ���� ������ ��ü�� �ٸ��� �����̴�.
        // instanceof �������� ��ü�� Ÿ���� ã�� ������ ����� ũ�Ƿ� �̸� ���̱� ���� == �������� ó���� �� �ִ� �κ��� �̸� ó����.
        setObjectInternal(aValue, sColumn);
    }

    // BUG-42424 getMetaData�� PreparedStatement�� �������̽��̱⶧���� AltibaseStatement���� AltibasePreparedStatement�� �̵��Ѵ�.
    public ResultSetMetaData getMetaData() throws SQLException
    {
        throwErrorForClosed();
        throwErrorForStatementNotPrepared();

        if (mIsDeferred)
        {
            // BUG-42424 deferred�����϶��� ���� prepare ����� �޾ƿ;� �Ѵ�.
            receivePrepareResults();
        }
        
        if (mPrepareResult.isSelectStatement())
        {
            int sColumnCount = usingKeySetDriven() ? mPrepareResultColumns.size() - 1 : mPrepareResultColumns.size();
            return new AltibaseResultSetMetaData(mPrepareResultColumns, sColumnCount);
        }
        else
        {
            return null;
        }
    }

    /**
     * deferred �����϶� Ư���� ��쿡�� ���� prepare�� ����� �޾ƿ´�.
     * @throws SQLException
     */
    private void receivePrepareResults() throws SQLException
    {
        invokeDeferredRequests(); // BUG-42712 prepare����� �ޱ��� ���� deferred�� ��û�� ���ۿ� write�Ѵ�.
        CmProtocolContextPrepExec sContext = (CmProtocolContextPrepExec)mContext.get(mCurrentResultIndex);
        CmPrepareResult sPrepareResult = (CmPrepareResult)sContext.getCmResult(CmPrepareResult.MY_OP);
        // BUG-42424 PrepareResult�� ���� ��� defer ��û�� �����ϰ� prepare��û�� ���� ������.
        if (!sPrepareResult.isDataArrived()) 
        {
            if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
            {
                mLogger.log(Level.INFO, "Cancel deferred and receive prepare results first.");
            }
            synchronized (mConnection.channel())
            {
                mConnection.channel().sendAndReceive();
                CmOperation.readProtocolResult(sContext);
                mPrepareResult = sContext.getPrepareResult();
                int sParamCnt = mPrepareResult.getParameterCount();
                int sBindColumnSize = mBindColumns.size();
                // BUG-42879 ���� ���ε�� �÷����� �����κ��� �޾ƿ� �÷������� ���� ��� �������� null�� ä���.
                for (int i = 0; i < sParamCnt - sBindColumnSize; i++)
                {
                    mBindColumns.add(null);
                }
                if (mPrepareResultColumns == null)
                {
                    mPrepareResultColumns = getProtocolContext().getGetColumnInfoResult().getColumns();
                }
            }
        }
    }

    /**
     * sqlType�� �ش��ϴ� �⺻ precision ���� ���Ѵ�.</br> VARCHARŸ���� ��� value�� �̿��� precision�� ����ؾ� �ϱ⶧����
     * argument�� ���� ���޹޴´�.</br> deferred�� ��쿡�� �ش� �޼ҵ带 �̿��Ѵ�.
     * @param aTargetSqlType
     * @param aValue
     * @return
     */
    protected int getDefaultPrecisionForDeferred(int aTargetSqlType, Object aValue)
    {
        int sPrecision = 0;
        
        switch (aTargetSqlType)
        {
            case Types.CLOB:
            case Types.BLOB:
            case Types.BIGINT:
                sPrecision = 19;
                break;
            case Types.BOOLEAN:
                sPrecision = 1;
                break;
            case Types.VARBINARY:
            case AltibaseTypes.BINARY:
                sPrecision = 32000;
                break;
            case Types.DECIMAL:
            case Types.FLOAT:
            case Types.NUMERIC:
                sPrecision = 38;
                break;
            case Types.CHAR:
            case Types.LONGVARCHAR:
                sPrecision = 65534;
                break;
            case Types.VARCHAR:
                if (aValue instanceof Boolean)
                {
                    sPrecision = 5;
                }
                else if (aValue instanceof String)
                {
                    int sBytesOfString = ((String)aValue).length();
                    sBytesOfString *= mConnection.channel().getByteLengthPerChar();
                    sPrecision = sBytesOfString;
                    sPrecision = (sPrecision > 65534) ? 65534 : sPrecision;
                    sPrecision = (sPrecision < 512) ? 512 : sPrecision;
                }
                else
                {
                    sPrecision = 65534;
                }
                break;
            case AltibaseTypes.NCHAR:
            case AltibaseTypes.NVARCHAR:
                sPrecision = 32766;
                break;
            case Types.SMALLINT:
                sPrecision = 5;
                break;
            case Types.DATE:
            case Types.TIME:
            case Types.TIMESTAMP:
                sPrecision = 30;
                break;
            case Types.DOUBLE:
                sPrecision = 15;
                break;
            case Types.REAL:
                sPrecision = 7;
                break;
            case AltibaseTypes.OTHER:
            case Types.INTEGER:
                sPrecision = 10;
                break;
            case Types.NULL:
                sPrecision = 4;
                break;
            case Types.BIT:
                sPrecision = 64 * 1024;
                break;
        }
        
        return sPrecision;
    }

    private void setObjectInternal(Object aValue, Column sColumn) throws SQLException
    {
        if(sColumn.getDBColumnType() == ColumnTypes.BLOB_LOCATOR)
        {
            if (mLobUpdator == null)
            {
                mLobUpdator = new LobUpdator(this);
            }
            
            mLobUpdator.addBlobColumn((BlobLocatorColumn)sColumn, aValue);
        }
        else if(sColumn.getDBColumnType() == ColumnTypes.CLOB_LOCATOR)
        {
            if (mLobUpdator == null)
            {
                mLobUpdator = new LobUpdator(this);
            }
            
            mLobUpdator.addClobColumn((ClobLocatorColumn)sColumn, aValue);
        }
        else
        {
            sColumn.setValue(aValue);
        }
    }

    public void setObject(int aParameterIndex, Object aValue) throws SQLException
    {
        if (aValue instanceof String)
        {
            setString(aParameterIndex, (String)aValue);
        }
        else if (aValue instanceof BigDecimal)
        {
            setBigDecimal(aParameterIndex, (BigDecimal)aValue);
        }
        else if (aValue instanceof Boolean)
        {
            setBoolean(aParameterIndex, ((Boolean)aValue).booleanValue());
        }
        else if (aValue instanceof Integer)
        {
            setInt(aParameterIndex, ((Integer)aValue).intValue());
        }
        else if (aValue instanceof Short)
        {
            setShort(aParameterIndex, ((Short)aValue).shortValue());
        }
        else if (aValue instanceof Long)
        {
            setLong(aParameterIndex, ((Long)aValue).longValue());
        }
        else if (aValue instanceof Float)
        {
            setFloat(aParameterIndex, ((Float)aValue).floatValue());
        }
        else if (aValue instanceof Double)
        {
            setDouble(aParameterIndex, ((Double)aValue).doubleValue());
        }
        else if (aValue instanceof byte[])
        {
            setBytes(aParameterIndex, (byte[])aValue);
        }
        else if (aValue instanceof Date)
        {
            setDate(aParameterIndex, (Date)aValue);
        }
        else if (aValue instanceof Time)
        {
            setTime(aParameterIndex, (Time)aValue);
        }
        else if (aValue instanceof Timestamp)
        {
            setTimestamp(aParameterIndex, (Timestamp)aValue);
        }
        else if (aValue instanceof Blob)
        {
            setBlob(aParameterIndex, (Blob)aValue);
        }
        else if (aValue instanceof Clob)
        {
            setClob(aParameterIndex, (Clob)aValue);
        }
        else if (aValue instanceof InputStream)
        {
            setBinaryStream(aParameterIndex, (InputStream)aValue);
        }
        else if (aValue instanceof Reader)
        {
            setCharacterStream(aParameterIndex, (Reader)aValue);
        }
        else if (aValue instanceof BitSet)
        {
            getColumnForInType(aParameterIndex, Types.BIT).setValue(aValue);
        }
        else if (aValue instanceof char[])
        {
            setString(aParameterIndex, String.valueOf((char[])aValue));
        }
        else if (aValue == null) {
            setNull(aParameterIndex, Types.NULL);
        }
        else
        {
            Error.throwSQLException(ErrorDef.UNSUPPORTED_FEATURE, aValue.getClass().getName());
        }
    }

    public void setRef(int i, Ref aValue) throws SQLException
    {
        Error.throwSQLException(ErrorDef.UNSUPPORTED_FEATURE, "Ref type");
    }

    public void setShort(int aParameterIndex, short aValue) throws SQLException
    {
        ((SmallIntColumn)getColumnForInType(aParameterIndex, Types.SMALLINT)).setTypedValue(aValue);
    }

    public void setString(int aParameterIndex, String aValue) throws SQLException
    {
        // BUG-42424 precision �� ����� ���� value�� ���� �Ѱ��ش�.
        getColumnForInType(aParameterIndex, Types.VARCHAR, aValue).setValue(aValue);
    }

    public void setTime(int aParameterIndex, Time aValue, Calendar aCalendar) throws SQLException
    {
        TimeColumn sColumn = (TimeColumn)getColumnForInType(aParameterIndex, Types.TIME);
        sColumn.setLocalCalendar(aCalendar);
        sColumn.setValue(aValue);
    }

    public void setTime(int aParameterIndex, Time aValue) throws SQLException
    {
        getColumnForInType(aParameterIndex, Types.TIME).setValue(aValue);
    }

    public void setTimestamp(int aParameterIndex, Timestamp aValue, Calendar aCalendar) throws SQLException
    {
        TimestampColumn sColumn = (TimestampColumn)getColumnForInType(aParameterIndex, Types.TIMESTAMP);
        sColumn.setLocalCalendar(aCalendar);
        sColumn.setValue(aValue);
    }

    public void setTimestamp(int aParameterIndex, Timestamp aValue) throws SQLException
    {
        getColumnForInType(aParameterIndex, Types.TIMESTAMP).setValue(aValue);
    }

    public void setURL(int aParameterIndex, URL aValue) throws SQLException
    {
        Error.throwSQLException(ErrorDef.UNSUPPORTED_FEATURE, "URL type");
    }

    public void setUnicodeStream(int aParameterIndex, InputStream aValue, int aLength) throws SQLException
    {
        Error.throwSQLException(ErrorDef.UNSUPPORTED_FEATURE, "Deprecated: setUnicodeStream");
    }

    public void addBatch(String aSql) throws SQLException
    {
        Error.throwSQLException(ErrorDef.CANNOT_USE_PREPARED_STATEMENT_METHOD_WITH_SQL, "addBatch(String sql)");
    }

    public boolean execute(String aSql, int aAutoGeneratedKeys) throws SQLException
    {
        Error.throwSQLException(ErrorDef.CANNOT_USE_PREPARED_STATEMENT_METHOD_WITH_SQL, "execute(String sql, int autoGenKey)");
        return false;
    }

    public boolean execute(String aSql, int[] aColumnIndexes) throws SQLException
    {
        Error.throwSQLException(ErrorDef.CANNOT_USE_PREPARED_STATEMENT_METHOD_WITH_SQL, "execute(String sql, int[] columnIndex)");
        return false;
    }

    public boolean execute(String aSql, String[] aColumnNames) throws SQLException
    {
        Error.throwSQLException(ErrorDef.CANNOT_USE_PREPARED_STATEMENT_METHOD_WITH_SQL, "execute(String sql, String[] columnNames)");
        return false;
    }

    public boolean execute(String aSql) throws SQLException
    {
        Error.throwSQLException(ErrorDef.CANNOT_USE_PREPARED_STATEMENT_METHOD_WITH_SQL, "execute(String sql)");
        return false;
    }

    public ResultSet executeQuery(String aSql) throws SQLException
    {
        Error.throwSQLException(ErrorDef.CANNOT_USE_PREPARED_STATEMENT_METHOD_WITH_SQL, "executeQuery(String sql)");
        return null;
    }

    public int executeUpdate(String aSql, int aAutoGeneratedKeys) throws SQLException
    {
        Error.throwSQLException(ErrorDef.CANNOT_USE_PREPARED_STATEMENT_METHOD_WITH_SQL, "executeUpdate(String sql, int autoGenKey)");
        return 0;
    }

    public int executeUpdate(String aSql, int[] aColumnIndexes) throws SQLException
    {
        Error.throwSQLException(ErrorDef.CANNOT_USE_PREPARED_STATEMENT_METHOD_WITH_SQL, "executeUpdate(String sql, int[] columnIndex)");
        return 0;
    }

    public int executeUpdate(String aSql, String[] aColumnNames) throws SQLException
    {
        Error.throwSQLException(ErrorDef.CANNOT_USE_PREPARED_STATEMENT_METHOD_WITH_SQL, "executeUpdate(String sql, String[] columnNames)");
        return 0;
    }

    public int executeUpdate(String aSql) throws SQLException
    {
        Error.throwSQLException(ErrorDef.CANNOT_USE_PREPARED_STATEMENT_METHOD_WITH_SQL, "executeUpdate(String sql)");
        return 0;
    }
    
    private void checkBindColumnLength(int aIndex) throws SQLException
    {
        if (mIsDeferred) return;    // BUG-42424 deferred�����϶��� ���ε��÷����̸� �������ϱ� ������ �����Ѵ�.
        
        if (mBindColumns.size() == 0 && aIndex > 0)
        {
            Error.throwSQLException(ErrorDef.INVALID_BIND_COLUMN);
        }
        if (aIndex <= 0 || aIndex > mBindColumns.size())
        {
            Error.throwSQLException(ErrorDef.INVALID_COLUMN_INDEX, "1 ~ " + mBindColumns.size(), 
                                    String.valueOf(aIndex));
        }
    }
    
    private void throwErrorForBatchJob(String aCommand) throws SQLException
    {
        if (mBatchAdded)
        {
            Error.throwSQLException(ErrorDef.SOME_BATCH_JOB, aCommand); 
        }
    }
    
    // PROJ-2583 jdbc logging
    public int getStmtId()
    {
        return mPrepareResult.getStatementId();
    }

    /**
     * Trace logging �� statement �� �ĺ��� ���� unique id �� ��ȯ�Ѵ�.
     */
    String getTraceUniqueId()
    {
        return "[StmtId #" + getStmtId() + "] ";
    }
}
