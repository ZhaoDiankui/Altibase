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

/**
 * PROJ-2190 ClientSide Autocommit ��尡 �߰��Ǿ��� ������ autoCommit ���� boolean�� �ƴ� enum type���� ǥ���Ѵ�.
 * 
 * @author yjpark
 */
public class AutoCommitMode
{
    private final String               mName;

    public static final AutoCommitMode SERVER_SIDE_AUTOCOMMIT_ON  = new AutoCommitMode("server_side_autocommit_on");
    public static final AutoCommitMode SERVER_SIDE_AUTOCOMMIT_OFF = new AutoCommitMode("server_side_autocommit_off");
    public static final AutoCommitMode CLIENT_SIDE_AUTOCOMMIT_ON  = new AutoCommitMode("client_side_autocommit_on");

    private AutoCommitMode(String aName)
    {
        this.mName = aName;
    }

    public String toString()
    {
        return mName;
    }
}
