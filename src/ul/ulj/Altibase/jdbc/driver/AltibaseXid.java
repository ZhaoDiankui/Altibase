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

import java.util.Arrays;

import javax.transaction.xa.Xid;

import Altibase.jdbc.driver.util.ByteUtils;

public class AltibaseXid implements Xid
{
    public static final short MAXDATASIZE = Xid.MAXGTRIDSIZE + Xid.MAXBQUALSIZE;

    private volatile int      hashCode    = 0;
    private long              mFormatId;
    private byte[]            mGlobalTransactionId;
    private byte[]            mBranchQualifier;

    public AltibaseXid(long aFormatId, byte[] aGlobalTransactionId, byte[] aBranchQualifier)
    {
        mFormatId = aFormatId;
        mGlobalTransactionId = aGlobalTransactionId;
        mBranchQualifier = aBranchQualifier;
    }

    public byte[] getGlobalTransactionId()
    {
        return mGlobalTransactionId;
    }

    public int getFormatId()
    {
        return (int) mFormatId;
    }

    public byte[] getBranchQualifier()
    {
        return mBranchQualifier;
    }

    public boolean equals(Object aObj)
    {
        if (aObj == this)
        {
            return true;
        }
        if (aObj == null)
        {
            return false;
        }
        if (!(aObj instanceof Xid))
        {
            return false;
        }

        Xid sOther = (Xid) aObj;
        if (mFormatId != sOther.getFormatId())
        {
            return false;
        }
        byte[] sOtherGlobalID = sOther.getGlobalTransactionId();
        byte[] sOtherBranchID = sOther.getBranchQualifier();
        if (mGlobalTransactionId.length != sOtherGlobalID.length || mBranchQualifier.length != sOtherBranchID.length)
        {
            return false;
        }
        
        for (int i = 0; i < mGlobalTransactionId.length; ++i)
        {
            if (mGlobalTransactionId[i] != sOtherGlobalID[i])
            {
                return false;
            }
        }
        for (int i = 0; i < mBranchQualifier.length; ++i)
        {
            if (mBranchQualifier[i] != sOtherBranchID[i])
            {
                return false;
            }
        }
        return true;
    }

    public int hashCode()
    {
        if(hashCode == 0)
        {
            int result = 17;
            
            result = 13 * result + (int)(mFormatId & 0x7FFFFFFF);
            
            for(int i=0; i < mGlobalTransactionId.length; i++)
            {
                result = 13 * result + (int) mGlobalTransactionId[i];
            }
            
            for(int i=0; i < mBranchQualifier.length; i++)
            {
                result = 13 * result + (int) mBranchQualifier[i];
            }
            hashCode = result;
        }
        return hashCode;
    }
    
    public String toString()
    {
        return toString(this);
    }

    public static String toString(Xid aXid)
    {
        if (aXid == null)
        {
            return "null";
        }
        String sName = aXid.getClass().getName();
        return sName.substring(sName.lastIndexOf('.') + 1)
                + " [FormatId=" + aXid.getFormatId()
                + ", GlobalTransactionId=" + ByteUtils.toHexString(aXid.getGlobalTransactionId())
                + ", BranchQualifier=" + ByteUtils.toHexString(aXid.getBranchQualifier())
                + "]";
    }
}
