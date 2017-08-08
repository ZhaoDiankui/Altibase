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

package Altibase.jdbc.driver.datatype;

import java.nio.ByteBuffer;
import java.sql.SQLException;
import java.util.List;

import Altibase.jdbc.driver.cm.CmBufferWriter;

//PROJ-2368
/**
 * @author pss4you
 * Batch Operation - List Protocol ���ÿ� ������ Data �� ��Ƶ� ByteBuffer Handler 
 */

public class ListBufferHandle extends CmBufferWriter implements BatchDataHandle
{
    // Buffer �ʱ�  ũ��    
    private static final int BUFFER_INIT_SIZE  = 524288;
    // Buffer ������ ����
    private static final int BUFFER_ALLOC_UNIT = 524288;
    private List<Column> mColumns;
    private int mBatchedRowCount;

    public void setColumns(List<Column> aColumns)
    {
        mColumns = aColumns;
    }

    public int size()
    {
        return mBatchedRowCount;
    }

    public void flipBuffer()
    {
        mBuffer.flip();
    }

    private void allocateBuffer(int aDataBufferSize)
    {
        if ((mBuffer == null) || (mBuffer.capacity() < aDataBufferSize))
        {
            mBuffer = ByteBuffer.allocateDirect(aDataBufferSize);
        }
    }

    public ByteBuffer getBuffer()
    {
        return mBuffer;
    }

    public int getBufferPosition()
    {
        return mBuffer.position();
    }

    public void initToStore()
    {
        if (mBuffer == null)
        {
            allocateBuffer(BUFFER_INIT_SIZE);
        } 
        else 
        {
            mBuffer.clear();
        }

        mBatchedRowCount = 0;
    }

    public void store() throws SQLException
    {
        for (Column sColumn : mColumns)
        {
            sColumn.storeTo(this);
        }

        ++mBatchedRowCount;
    }

    public void checkWritable(int aNeedToWrite)
    {
        // ����, Data �� ����� ������ ������� �ʴٸ�, BUFFER_ALLOC_UNIT ��ŭ ������ ũ���� Buffer �� ���� �Ҵ� 
        if (mBuffer.remaining() < aNeedToWrite)
        {
            int sPosition = mBuffer.position();
            ByteBuffer sNewBuf = ByteBuffer.allocateDirect(mBuffer.limit() + BUFFER_ALLOC_UNIT);
            // ���ο� Buffer �� ���� Buffer ���� ����
            mBuffer.position(0);
            sNewBuf.put(mBuffer);
            // Position Recovery
            sNewBuf.position(sPosition);
            mBuffer = sNewBuf;
        }
    }

    public void writeBytes(ByteBuffer aValue) throws SQLException
    {
        checkWritable(aValue.remaining());
        mBuffer.put(aValue);
    }

    public void changeBindColumnType(int aIndex, Column aColumn, ColumnInfo aColumnInfo, byte aInOutType)
    {
        mColumns.set(aIndex, aColumn);
        ColumnInfo sColumnInfo = (ColumnInfo)aColumnInfo.clone();
        sColumnInfo.modifyInOutType(aInOutType);
        mColumns.get(aIndex).getDefaultColumnInfo(sColumnInfo);
        sColumnInfo.addInType();
        mColumns.get(aIndex).setColumnInfo(sColumnInfo);
    }
}
