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

import java.sql.SQLException;
import java.sql.SQLWarning;
import java.util.ArrayList;
import java.util.logging.Level;
import java.util.logging.Logger;

import Altibase.jdbc.driver.cm.CmChannel;
import Altibase.jdbc.driver.ex.Error;
import Altibase.jdbc.driver.ex.ErrorDef;
import Altibase.jdbc.driver.logging.LoggingProxy;
import Altibase.jdbc.driver.logging.TraceFlag;
import Altibase.jdbc.driver.util.AltibaseProperties;
import Altibase.jdbc.driver.util.StringUtils;

final class AltibaseFailover
{
    enum CallbackState
    {
        STOPPED,
        IN_PROGRESS,
    }

    /* BUG-31390 Failover info for v$session */
    enum FailoverType
    {
        CTF,
        STF,
    }

    private static Logger    mLogger;

    static 
    {
        if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
        {
            mLogger = Logger.getLogger(LoggingProxy.JDBC_LOGGER_FAILOVER);
        }
    }

    private AltibaseFailover()
    {
    }

    /**
     * CTF�� �õ��ϰ�, ����� ���� ��� ����ų� �������� ���ܸ� �ٽ� ������.
     *
     * @param aContext Failover context
     * @param aWarning ��� ���� ��ü. null�̸� �� ��ü�� �����.
     * @param aException ���� ���� ����
     * @return ��� ���� ��ü
     * @throws SQLException CTF�� �� ��Ȳ�� �ƴϰų� CTF�� ������ ���
     */
    public static SQLWarning tryCTF(AltibaseFailoverContext aContext, SQLWarning aWarning, SQLException aException) throws SQLException
    {
        if (!AltibaseFailover.checkNeedCTF(aContext, aException))
        {
            throw aException;
        }

        boolean sCTFSuccess = AltibaseFailover.doCTF(aContext);
        if (sCTFSuccess == false)
        {
            throw aException;
        }

        /* BUG-32917 Connection Time Failover(CTF) information should be logged */
        aWarning = Error.createWarning(aWarning, ErrorDef.FAILOVER_SUCCESS, aException.getSQLState(), aException.getMessage(), aException);
        if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
        {
            mLogger.log(Level.WARNING, "Failover completed");
        }

        return aWarning;
    }

    /**
     * STF�� �õ��ϰ�, ����� ���� ���ܸ� ������.
     * ������������ ���ܸ� ���� STF ������ �˸���.
     * <p>
     * ���� failover context�� �ʱ�ȭ���� �ʾҴٸ� ���� ���� ���ܸ� �����ų� ����ڰ� �ѱ� ��� ��ü�� �״�� ��ȯ�Ѵ�.
     *
     * @param aContext Failover context
     * @param aException ���� ���� ����
     * @throws SQLException STF�� �����ߴٸ� ���� ���� ����, �ƴϸ� STF ������ �˸��� ����
     */
    public static void trySTF(AltibaseFailoverContext aContext, SQLException aException) throws SQLException
    {
        if (!AltibaseFailover.checkNeedSTF(aContext, aException))
        {
            throw aException;
        }

        boolean sSTFSuccess = AltibaseFailover.doSTF(aContext);
        if (sSTFSuccess)
        {
            Error.throwSQLExceptionForFailover(aException);
        }
        else 
        {
            throw aException;
        }           
    }

    /**
     * CTF�� ���� alternateservers���� ��� ������ ������ ������ �����Ѵ�.
     *
     * @param aContext Failover context
     * @return CTF ���� ����
     */
    private static boolean doCTF(AltibaseFailoverContext aContext)
    {
        return doCTF(aContext, FailoverType.CTF);
    }

    /**
     * CTF �Ǵ� STF�� ���� alternateservers���� ��� ������ ������ ������ �����Ѵ�.
     *
     * @param aContext Failover context
     * @param aFailoverType Failover ����
     * @return CTF ���� ����
     */
    private static boolean doCTF(AltibaseFailoverContext aContext, FailoverType aFailoverType)
    {
        final AltibaseProperties sProp = aContext.connectionProperties();
        final AltibaseFailoverServerInfo sOldServerInfo = aContext.getCurrentServer();
        final int sMaxConnectionRetry = sProp.getConnectionRetryCount();
        final int sConnectionRetryDelay = sProp.getConnectionRetryDelay() * 1000; // millisecond

        ArrayList<AltibaseFailoverServerInfo> sTryList = new ArrayList<AltibaseFailoverServerInfo>(aContext.getFailoverServerList());
        if (sTryList.get(0) != sOldServerInfo)
        {
            sTryList.remove(sOldServerInfo);
            sTryList.add(0, sOldServerInfo);
        }
        for (AltibaseFailoverServerInfo sNewServerInfo : sTryList)
        {
            for (int sRemainRetryCnt = Math.max(1, sMaxConnectionRetry); sRemainRetryCnt > 0; sRemainRetryCnt--)
            {
                /* BUG-43219 �� ���ӽø��� ä���� ������ؾ� �Ѵ�. �׷��� ������ ù��° ���� ������ �ڿ����� ���ӿ� ������ ��ģ��. */
                CmChannel sNewChannel = new CmChannel();
                try
                {
                    sNewChannel.open(sNewServerInfo.getServer(),
                                     sProp.getSockBindAddr(),
                                     sNewServerInfo.getPort(),                                         
                                     sProp.getLoginTimeout(),
                                     sProp.getResponseTimeout());
                    setFailoverSource(aContext, aFailoverType, sOldServerInfo);
                    aContext.setCurrentServer(sNewServerInfo);
                    AltibaseFailover.changeChannelAndConnect(aContext, sNewChannel, sNewServerInfo);
                    if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
                    {
                        mLogger.log(Level.INFO, "Success to connect to (" + sNewServerInfo + ")");
                    }
                    return true;
                }
                catch (Exception sEx)
                {
                    if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
                    {
                        mLogger.log(Level.INFO, "Failure to connect to (" + sNewServerInfo + ")", sEx);
                        mLogger.log(Level.INFO, "Sleep "+ sConnectionRetryDelay);
                    }
                    try
                    {
                        Thread.sleep(sConnectionRetryDelay);
                    }
                    catch (InterruptedException sExSleep)
                    {
                        // ignore
                    }
                }
            }
        }

        return false;
    }

    /**
     * <tt>CmChannel</tt>�� �ٲٰ�, �׸� �̿��� <tt>connect</tt>�� �õ��Ѵ�.
     *
     * @param aContext Failover contest
     * @param aChannel �ٲ� <tt>CmChannel</tt>
     * @param aServerInfo AltibaseFailoverServerInfo ����
     * @throws SQLException �� <tt>CmChannel</tt>�� �̿��� <tt>connect</tt>�� ������ ���
     */
    private static void changeChannelAndConnect(AltibaseFailoverContext aContext, CmChannel aChannel, AltibaseFailoverServerInfo aServerInfo) throws SQLException
    {
        AltibaseConnection sConn = aContext.getConnection();

        sConn.setChannel(aChannel);
        sConn.handshake();
        /* BUG-43219 set dbname property */
        AltibaseProperties sProps = aContext.connectionProperties();
        sProps.setDatabase(aServerInfo.getDatabase());
        sConn.connect(sProps);
    }

    /**
     * CTF�� Failover callback�� �����Ѵ�.
     * callback ����� ���� STF�� ��� �������� �׸������� �����Ѵ�.
     *
     * @param aContext Failover context
     * @return STF ���� ����
     * @throws SQLException STF�� ���� <tt>Statement</tt>�� �����ϴٰ� ������ ���
     */
    private static boolean doSTF(AltibaseFailoverContext aContext) throws SQLException
    {

        int sFailoverIntention;
        boolean sFOSuccess;
        int sFailoverEvent;
        AltibaseFailoverCallback sFailoverCallback = aContext.getCallback();

        aContext.getConnection().clearStatements4STF();

        aContext.setCallbackState(CallbackState.IN_PROGRESS);
        sFailoverIntention = sFailoverCallback.failoverCallback(aContext.getConnection(), aContext.getAppContext(), AltibaseFailoverCallback.Event.BEGIN);
        aContext.setCallbackState(CallbackState.STOPPED);

        if (sFailoverIntention == AltibaseFailoverCallback.Result.QUIT)
        {
            sFOSuccess = false;
        }
        else
        {
            sFOSuccess = doCTF(aContext, FailoverType.STF);

            if (sFOSuccess == true)
            {
                aContext.setCallbackState(CallbackState.IN_PROGRESS);
                try
                {
                    AltibaseXAResource sXAResource = aContext.getRelatedXAResource();
                    if ((sXAResource != null) && (sXAResource.isOpen() == true))
                    {
                        sXAResource.xaOpen();
                    }
                    sFailoverEvent = AltibaseFailoverCallback.Event.COMPLETED;
                }
                catch (SQLException e)
                {
                    sFailoverEvent = AltibaseFailoverCallback.Event.ABORT;
                }
                sFailoverIntention = sFailoverCallback.failoverCallback(aContext.getConnection(), aContext.getAppContext(), sFailoverEvent);
                aContext.setCallbackState(CallbackState.STOPPED);

                if ((sFailoverEvent == AltibaseFailoverCallback.Event.ABORT) ||
                    (sFailoverIntention == AltibaseFailoverCallback.Result.QUIT))
                {
                    sFOSuccess = false;
                    aContext.getConnection().quiteClose();
                }
            }
            else
            {
                aContext.setCallbackState(CallbackState.IN_PROGRESS);
                sFailoverIntention = sFailoverCallback.failoverCallback(aContext.getConnection(), aContext.getAppContext(), AltibaseFailoverCallback.Event.ABORT);
                aContext.setCallbackState(CallbackState.STOPPED);
            }
        }
        return sFOSuccess;

    }

    /* BUG-31390 Failover info for v$session */
    /**
     * V$SESSION.FAILOVER_SOURCE�� ������ ���� ������ Connection property�� �����Ѵ�.
     *
     * @param aContext Failover Context
     * @param aFailoverType Failover ����
     * @param aServerInfo ������ ���� ����
     */
    private static void setFailoverSource(AltibaseFailoverContext aContext, FailoverType aFailoverType, AltibaseFailoverServerInfo aServerInfo)
    {
        StringBuilder sStrBdr = new StringBuilder();

        switch (aFailoverType)
        {
            case CTF:
                sStrBdr.append("CTF ");
                break;

            case STF:
                sStrBdr.append("STF ");
                break;

            default:
                // ���ο����� ���Ƿ�, �̷����� ���� �Ͼ�� �ȵȴ�.
                Error.throwInternalError(ErrorDef.INTERNAL_ASSERTION);
                break;
        }

        sStrBdr.append(aServerInfo.getServer());
        sStrBdr.append(':');
        sStrBdr.append(aServerInfo.getPort());
        if (!StringUtils.isEmpty(aServerInfo.getDatabase()))
        {
            sStrBdr.append('/');
            sStrBdr.append(aServerInfo.getDatabase());
        }
        aContext.setFailoverSource(sStrBdr.toString());
    }

    /**
     * CTF�� �ʿ����� Ȯ���Ѵ�.
     *
     * @param aContext Failover�� ���� ������ ���� ��ü
     * @param aException CTF�� �ʿ����� �� ����
     * @return CTF�� �ʿ��ϸ� true, �ƴϸ� false
     */
    private static boolean checkNeedCTF(AltibaseFailoverContext aContext, SQLException aException) throws SQLException
    {
        if ((aContext == null) ||
            (aContext.getFailoverServerList().size() <= 0) ||
            (isNeedToFailover(aException) == false))
        {
            return false;
        }
        return true;
    }

    /**
     * STF�� �ʿ����� Ȯ���Ѵ�.
     *
     * @param aContext Failover�� ���� ������ ���� ��ü
     * @param aException STF�� �ʿ����� �� ����
     * @return STF�� �ʿ��ϸ� true, �ƴϸ� false
     */
    private static boolean checkNeedSTF(AltibaseFailoverContext aContext, SQLException aException) throws SQLException
    {
        if ((aContext == null) ||
            (aContext.useSessionFailover() == false) ||
            (aContext.getFailoverServerList().size() <= 0) ||
            (isNeedToFailover(aException) == false) ||
            (aContext.getCallbackState() != CallbackState.STOPPED))
        {
            return false;
        }
        return true;
    }

    /**
     * Failover�� �ʿ��� �������� Ȯ���Ѵ�.
     *
     * @param aException Failover�� �ʿ����� �� ����
     * @return Failover�� �ʿ��ϸ� true, �ƴϸ� false
     */
    private static boolean isNeedToFailover(SQLException aException)
    {
        switch (aException.getErrorCode())
        {
            case ErrorDef.COMMUNICATION_ERROR:
            case ErrorDef.UNKNOWN_HOST:
            case ErrorDef.CLIENT_UNABLE_ESTABLISH_CONNECTION:
            case ErrorDef.CLOSED_CONNECTION:
            case ErrorDef.RESPONSE_TIMEOUT:
            case ErrorDef.ADMIN_MODE_ERROR:
                return true;
            default:
                return false;
        }
    }
}
