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

import java.sql.SQLWarning;

public class CmProtocolContext
{
    private CmChannel mChannel;
    private CmErrorResult mError = null;
    private SQLWarning mWarning = null;
    private CmResult[] mResults;
    
    public CmProtocolContext(CmChannel aChannel)
    {
        mChannel = aChannel;
        mResults = new CmResult[CmOperation.DB_OP_COUNT];
    }
    
    public void addCmResult(CmResult aResult)
    {
        mResults[aResult.getResultOp()] = aResult;
    }
    
    public void clearError()
    {
        mError = null;
    }
    
    public CmErrorResult getError()
    {
        return mError;
    }

    public SQLWarning getWarning()
    {
        return mWarning;
    }

    public CmChannel channel()
    {
        return mChannel;
    }
    
    // BUG-42424 AltibasePreparedStatement���� �ش�޼ҵ带 �̿��ϱ� ������ public���� ����
    public CmResult getCmResult(byte aResultOp)
    {
        CmResult sResult = mResults[aResultOp];
        if (sResult == null)
        {
            sResult = CmResultFactory.getInstance(aResultOp);
            mResults[aResultOp] = sResult;
        }
        return sResult;
    }
    
    void addError(CmErrorResult aError)
    {
        if (mError == null)
        {
            mError = aError;
        }
        else
        {
            mError.addError(aError);
        }
    }

    void addWarning(String aMessage)
    {
        SQLWarning sWarning = new SQLWarning(aMessage);
        if (mWarning == null)
        {
            mWarning = sWarning;
        }
        else
        {
            mWarning.setNextWarning(sWarning);
        }
    }
}
