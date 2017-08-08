/** 
 *  Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License, version 3,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
 
/***********************************************************************
 * $Id$
 **********************************************************************/

#include <utISPApi.h>
#include <utString.h>
#include <isqlTypes.h>

void isqlChar::Reformat()
{
    if (mInd == SQL_NULL_DATA)
    {
        mValue[0] = '\0';
    }
    else
    {
        utString::removeLastSpace(mValue);
    }
    mCurr = mValue;
    mCurrLen = idlOS::strlen(mCurr);
}
