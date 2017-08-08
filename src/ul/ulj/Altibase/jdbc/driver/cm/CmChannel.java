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

import Altibase.jdbc.driver.AltibaseVersion;
import Altibase.jdbc.driver.ClobReader;
import Altibase.jdbc.driver.LobConst;
import Altibase.jdbc.driver.datatype.ColumnFactory;
import Altibase.jdbc.driver.ex.Error;
import Altibase.jdbc.driver.ex.ErrorDef;
import Altibase.jdbc.driver.logging.DumpByteUtil;
import Altibase.jdbc.driver.logging.LoggingProxy;
import Altibase.jdbc.driver.logging.MultipleFileHandler;
import Altibase.jdbc.driver.logging.TraceFlag;
import Altibase.jdbc.driver.util.*;

import javax.net.ssl.*;
import java.io.IOException;
import java.io.InputStream;
import java.io.Reader;
import java.math.BigInteger;
import java.net.*;
import java.nio.ByteBuffer;
import java.nio.CharBuffer;
import java.nio.channels.Channels;
import java.nio.channels.ReadableByteChannel;
import java.nio.channels.WritableByteChannel;
import java.nio.charset.Charset;
import java.nio.charset.CharsetDecoder;
import java.nio.charset.CoderResult;
import java.nio.charset.CodingErrorAction;
import java.security.*;
import java.security.cert.CertificateException;
import java.sql.SQLException;
import java.util.ArrayList;
import java.util.List;
import java.util.logging.*;
import java.util.regex.Pattern;

public class CmChannel extends CmBufferWriter
{
    static final byte NO_OP = -1;

    private static final int CHANNEL_STATE_CLOSED = 0;
    private static final int CHANNEL_STATE_WRITABLE = 1;
    private static final int CHANNEL_STATE_READABLE = 2;
    private static final String[] CHANNEL_STATE_STRING = {"CHANNEL_STATE_CLOSED", "CHANNEL_STATE_WRITABLE", "CHANNEL_STATE_READABLE"};

    public  static final int   CM_BLOCK_SIZE                   = 32 * 1024;
    public  static final int   CM_PACKET_HEADER_SIZE           = 16;
    private static final byte  CM_PACKET_HEADER_VERSION        = AltibaseVersion.CM_MAJOR_VERSION;
    private static final byte  CM_PACKET_HEADER_RESERVED_BYTE  = 0;
    private static final short CM_PACKET_HEADER_RESERVED_SHORT = 0;
    private static final int   CM_PACKET_HEADER_RESERVED_INT   = 0;
    private static final int   CHAR_VARCHAR_COLUMN_SIZE        = 32000;
    public static final int    INVALID_SOCKTFD                 = -1;

    private static final Pattern IP_PATTERN = Pattern.compile("(\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}|\\[[^\\]]+\\])");

    // PROJ-2474 keystore������ �����ϴ� �⺻ ���������� file:�̴�.
    private static final String DEFAULT_URL_FILE_SCHEME = "file:";
    private static final String DEFAULT_KEYSTORE_TYPE = "JKS";

    // PROJ-2583 jdbc logging
    private static final String JRE_DEFAULT_VERSION = "1.4.";

    private short mSessionID = 0;

    private int mState;
    private Socket mSocket;
    private int mSocketFD = INVALID_SOCKTFD; // PROJ-2625
    private CharBuffer mCharBuf;
    private byte[] mLobBuf;
    private WritableByteChannel mWriteChannel;
    private ReadableByteChannel mReadChannel;
    private AltibaseReadableCharChannel mReadChannel4Clob;
    private ColumnFactory mColumnFactory = new ColumnFactory();

    /* packet header */
    private short mPayloadSize = 0;
    private boolean mIsLastPacket = true;
    private boolean mIsRedundantPacket = false;

    private ByteBuffer mPrimitiveTypeBuf;
    private static final int MAX_PRIMITIVE_TYPE_SIZE = 8;

    private int mNextSendSeqNo = 0;
    private int mPrevReceivedSeqNo = -1;

    private CharsetDecoder mDBDecoder = CharsetUtils.newAsciiDecoder();
    private CharsetDecoder mNCharDecoder = CharsetUtils.newAsciiDecoder();

    private boolean mIsPreferIPv6 = false;

    private boolean mRemoveRedundantMode = false;

    private int mPendedDataLength;
    private int CLOB_BUF_SIZE;

    private int mResponseTimeout;

    private int mLobCacheThreshold;

    // PROJ-2474 SSL�� Ȱ��ȭ �Ǿ��ִ��� ��Ÿ���� flag
    private boolean mUseSsl;
    // PROJ-2474 SSL ���õ� �Ӽ����� ������ �ִ� ��ü
    private SSLProperties mSslProps;
    private transient Logger mCmLogger;

    // PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning
    public static final int CM_DEFAULT_SNDBUF_SIZE = CM_BLOCK_SIZE * 2;
    public static final int CM_DEFAULT_RCVBUF_SIZE = CM_BLOCK_SIZE * 2;

    private int mSockRcvBufSize = CM_DEFAULT_RCVBUF_SIZE;

    private CmProtocolContext mAsyncContext;

    // Channel for Target
    private class AltibaseReadableCharChannel implements ReadableCharChannel
    {
        private final CharBuffer mCharBuf  = CharBuffer.allocate(LobConst.LOB_BUFFER_SIZE);
        private boolean          mIsOpen;
        private CoderResult      mResult;
        private Reader           mSrcReader;
        private char[]           mSrcBuf;
        private int              mSrcBufPos;
        private int              mSrcBufLimit;

        private int              mUserReadableLength = -1;
        private int              mTotalActualCharReadLength = 0;

        public void open(Reader aSrc) throws IOException
        {
            close();

            mSrcReader = aSrc;
            mCharBuf.clear();
            mIsOpen = true;
        }

        public void open(Reader aSrc, int aUserReadableLength) throws IOException
        {
            close();

            mSrcReader = aSrc;
            mCharBuf.clear();
            mIsOpen = true;
            mUserReadableLength = aUserReadableLength;
        }

        public void open(char[] aSrc) throws IOException
        {
            open(aSrc, 0, aSrc.length);
        }

        public void open(char[] aSrc, int aOffset, int aLength) throws IOException
        {
            close();

            mSrcBuf = aSrc;
            mSrcBufPos = aOffset;
            mSrcBufLimit = aOffset + aLength;
            mCharBuf.clear();
            mIsOpen = true;
        }

        public int read(ByteBuffer aDst) throws IOException
        {
            if (isClosed())
            {
                Error.throwIOException(ErrorDef.STREAM_ALREADY_CLOSED);
            }

            if (mUserReadableLength > 0)
            {
                // BUG-44553 clob �����͸� ��� �о����� mCharBuf�� compact�� �����Ͱ� ���� ������ �������� �ʴ´�.
                if (mUserReadableLength <= mTotalActualCharReadLength && mCharBuf.position() == 0)
                {
                    return -1;
                }
            }

            mTotalActualCharReadLength += readClobData();

            return copyToSocketBuffer(aDst);
        }

        /**
         * mCharBuf�� encoding�� �� ���� ���۷� �����Ѵ�.
         * @param aDst ���� ����
         * @return ������ ���� ���۷� ���ڵ��Ǿ� ����� ������.
         */
        private int copyToSocketBuffer(ByteBuffer aDst)
        {
            int sReaded = -1;

            if (mCharBuf.flip().remaining() > 0)
            {
                int sBeforeBytePos4Dst = aDst.position();
                mResult = mDBEncoder.encode(mCharBuf, aDst, true);
                sReaded = aDst.position() - sBeforeBytePos4Dst;

                if (mResult.isOverflow())
                {
                    mCharBuf.compact();
                }
                else
                {
                    mCharBuf.clear();
                }
            }

            return sReaded;
        }

        /**
         * Src�κ��� clob �����͸� �о mCharBuf ���ۿ� �����Ѵ�.
         * @return ������ �о���� clob ������ ������
         * @throws IOException read ���� I/O ������ �߻����� ��
         */
        private int readClobData() throws IOException
        {
            int sReadableCharLength = getReadableCharLength();
            if (sReadableCharLength <= 0) // BUG-44553 �о���� clob�����Ͱ� ������������ read�� ����
            {
                return 0;
            }

            int sActualCharReadLength = 0;
            if (mSrcBuf == null)
            {
                sActualCharReadLength = mSrcReader.read(mCharBuf.array(), mCharBuf.position(), sReadableCharLength);
                if (sActualCharReadLength > 0)
                {
                    mCharBuf.position(mCharBuf.position() + sActualCharReadLength);
                }
            }
            else
            {
                if (mSrcBufPos < mSrcBufLimit)
                {
                    sActualCharReadLength = Math.min(sReadableCharLength, mSrcBufLimit - mSrcBufPos);
                    mCharBuf.put(mSrcBuf, mSrcBufPos, sActualCharReadLength);
                    mSrcBufPos += sActualCharReadLength;
                }
            }

            return sActualCharReadLength;
        }

        /**
         * clob �����͸� �о���� �� �ִ� �ִ� ����� ���Ѵ�.
         * @return ���� �� �ִ� �ִ� ������
         */
        private int getReadableCharLength()
        {
            int sReadableCharLength;

            if (mUserReadableLength > 0)
            {
                // BUG-44553 ���� ���� �� ��ŭ�� ���������� ���� ����
                sReadableCharLength = Math.min(mUserReadableLength - mTotalActualCharReadLength, CLOB_BUF_SIZE);
                sReadableCharLength = Math.min(mCharBuf.remaining(), sReadableCharLength);
            }
            else
            {
                sReadableCharLength = Math.min(mCharBuf.remaining(), CLOB_BUF_SIZE);
            }

            return sReadableCharLength;
        }

        public boolean isOpen()
        {
            return mIsOpen;
        }

        public void close() throws IOException
        {
            if (!isOpen())
            {
                return;
            }

            mUserReadableLength = -1;
            mTotalActualCharReadLength = 0;

            mIsOpen = false;
            mSrcReader = null;
            mSrcBuf = null;
        }

        public Object getSource()
        {
            if (mSrcReader != null)
            {
                return mSrcReader;
            }
            else
            {
                return mSrcBuf;
            }
        }
    }

    public CmChannel()
    {
        mBuffer = ByteBuffer.allocateDirect(CM_BLOCK_SIZE);
        mCharBuf = CharBuffer.allocate(CM_BLOCK_SIZE);
        mCharVarcharColumnBuffer = ByteBuffer.allocate(CHAR_VARCHAR_COLUMN_SIZE);
        mState = CHANNEL_STATE_CLOSED;

        initializeLogger();
    }

    private void initializeLogger()
    {
        if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
        {
            mCmLogger = Logger.getLogger(LoggingProxy.JDBC_LOGGER_CM);
            try
            {
                String sJreVersion = RuntimeEnvironmentVariables.getVariable("java.version", JRE_DEFAULT_VERSION);
                // PROJ-2583 JDK1.4������ �������Ͽ��� handlers�� �����ص� �ҷ����� ���ϱ� ������
                // ���α׷������� handler�� add���ش�.
                if (sJreVersion.startsWith(JRE_DEFAULT_VERSION))
                {
                    String sHandlers = getProperty(MultipleFileHandler.CM_HANDLERS, "");
                    if (sHandlers.contains(MultipleFileHandler.HANDLER_NAME))
                    {
                        mCmLogger.addHandler(new MultipleFileHandler());
                    }
                    else if (sHandlers.contains("java.util.logging.FileHandler"))
                    {
                        String sLocalPattern = getProperty("java.util.logging.FileHandler.pattern", "%h/jdbc_net.log");
                        int sLocalLimit = Integer.parseInt(getProperty("java.util.logging.FileHandler.limit", "10000000"));
                        int sLocalCount = Integer.parseInt(getProperty("java.util.logging.FileHandler.count", "1"));
                        boolean sLocalAppend = Boolean.getBoolean(getProperty("java.util.logging.FileHandler.append", "false"));
                        String sLevel = getProperty("java.util.logging.FileHandler.level", "FINEST");

                        FileHandler sFileHandler = new FileHandler(sLocalPattern, sLocalLimit, sLocalCount, sLocalAppend);
                        sFileHandler.setLevel(Level.parse(sLevel));
                        sFileHandler.setFormatter(new XMLFormatter());
                        mCmLogger.addHandler(sFileHandler);
                    }
                }
                mCmLogger.setUseParentHandlers(false);
            }
            catch (IOException sIOE)
            {
                mCmLogger.log(Level.SEVERE, "Cannot add handler", sIOE);
            }
        }
    }

    private String getProperty(String aName, String aDefaultValue)
    {
        String property = LogManager.getLogManager().getProperty(aName);
        return property != null ? property : aDefaultValue;
    }

    private boolean isPreferIPv6()
    {
        return mIsPreferIPv6;
    }

    public void setPreferredIPv6()
    {
        mIsPreferIPv6 = true;
    }

    private boolean isHostname(String aConnAddr)
    {
        return !IP_PATTERN.matcher(aConnAddr).matches();
    }

    /**
     * Hostname�� ���� IP ����� ��´�.
     *
     * @param aHost       IP ����� ���� Hostname
     * @param aPreferIPv6 IP ��Ͽ� IPv6�� ���� ������ ����.
     *                    true�� IPv6 ���� IPv4 �ּҰ�, false�� IPv4 ���� IPv6 �ּ� �´�.
     *
     * @return Hostname�� ���� IP ���
     * 
     * @throws SQLException �� �� ���� Host�̰ų�, IP ��� ��ȸ�� ������ ���� ���
     */
    private static List<InetAddress> getAllIpAddressesByName(String aHost, boolean aPreferIPv6) throws SQLException
    {
        InetAddress[] sAddrs = null;

        ArrayList<InetAddress> sIPv4List = new ArrayList<InetAddress>();
        ArrayList<InetAddress> sIPv6List = new ArrayList<InetAddress>();
        ArrayList<InetAddress> sIpAddrList = new ArrayList<InetAddress>();

        try
        {
            sAddrs = InetAddress.getAllByName(aHost);
        }
        catch (Exception sEx)
        {
            Error.throwSQLException(ErrorDef.UNKNOWN_HOST, aHost, sEx);
        }

        for (InetAddress sAddr : sAddrs)
        {
            if (sAddr instanceof Inet4Address)
            {
                sIPv4List.add(sAddr);
            }
            else
            {
                sIPv6List.add(sAddr);
            }
        }

        // To find out in the order of preferred IP address type
        if (aPreferIPv6)
        {
            sIpAddrList.addAll(sIPv6List);
            sIpAddrList.addAll(sIPv4List);
        }
        else
        {
            sIpAddrList.addAll(sIPv4List);
            sIpAddrList.addAll(sIPv6List);
        }

        sIPv6List.clear();
        sIPv4List.clear();

        return sIpAddrList;
    }

    /**
     * ���� ������ �ΰ�, CmChannel�� ����ϱ� ���� �غ� �Ѵ�.
     * 
     * @param aAddr            ������ ������ IP �ּ� �Ǵ� Hostname
     * @param aBindAddr        ���Ͽ� ���ε� �� IP �ּ�. null�̸� ���ε� ���� ����.
     * @param aPort            ������ ������ Port
     * @param aLoginTimeout    Socket.connect()�� ����� Ÿ�Ӿƿ� �� (�� ����)
     * @param aResponseTimeout ���� Ÿ�Ӿƿ� �� (�� ����)
     * 
     * @throws SQLException �̹� ����Ǿ��ְų� ���� ����, �ʱ�ȭ �Ǵ� ���ῡ ������ ���
     */
    public void open(String aAddr, String aBindAddr, int aPort, int aLoginTimeout, int aResponseTimeout)
        throws SQLException
    {
        if (!isClosed())
        {
            Error.throwSQLException(ErrorDef.OPENED_CONNECTION);
        }

        mResponseTimeout = aResponseTimeout;
        
        if (isHostname(aAddr))
        {
            this.openWithName(aAddr, aBindAddr, aPort, aLoginTimeout, aResponseTimeout);
        }
        else
        {
            open(new InetSocketAddress(aAddr, aPort), aBindAddr, aLoginTimeout, aResponseTimeout);
        }
    }

    private void openWithName(String aHostname, String aBindAddr, int aPort, int aLoginTimeout, int aResponseTimeout)
        throws SQLException
    {
        List<InetAddress> sIpAddrList = getAllIpAddressesByName(aHostname, isPreferIPv6());

        int sIdx;
        InetAddress sAddr;
        InetSocketAddress sSockAddr;
        
        // try to connect using All IPs
        for (sIdx = 0; sIdx < sIpAddrList.size();)
        {
            sAddr = sIpAddrList.get(sIdx);
            sSockAddr = new InetSocketAddress(sAddr, aPort);
            if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
            {
                mCmLogger.log(Level.INFO, "openWithName: try to connect to " + sSockAddr);
            }
            try
            {
                open(sSockAddr, aBindAddr, aLoginTimeout, aResponseTimeout);
                break;
            }
            catch (SQLException sEx)
            {
                if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
                {
                    mCmLogger.log(Level.SEVERE, "Connection Failed by Hostname: " + sEx.getMessage());
                }
                sIdx++;
                // If all of the attempting failed, throw a exception.
                if (sIdx == sIpAddrList.size())
                {
                    throw sEx;
                }
            }
        }
    }

    /**
     * ���� ������ �ΰ�, CmChannel�� ����ϱ� ���� �غ� �Ѵ�.
     * 
     * @param aSockAddr        ������ ����
     * @param aBindAddr        ���Ͽ� ���ε� �� IP �ּ�. null�̸� ���ε� ���� ����.
     * @param aLoginTimeout    Socket.connect()�� ����� Ÿ�Ӿƿ� �� (�� ����)
     * @param aResponseTimeout ���� Ÿ�Ӿƿ� �� (�� ����)
     * 
     * @throws SQLException �̹� ����Ǿ��ְų� ���� ����, �ʱ�ȭ �Ǵ� ���ῡ ������ ���
     */
    private void open(SocketAddress aSockAddr, String aBindAddr, int aLoginTimeout, int aResponseTimeout)
        throws SQLException
    {
        try
        {
            if (mUseSsl)
            {
                mSocket = getSslSocketFactory(mSslProps).createSocket();
                if (mSslProps.getCipherSuiteList() != null)
                {
                    ((SSLSocket)mSocket).setEnabledCipherSuites(mSslProps.getCipherSuiteList());
                }
            }
            else
            {
                mSocket = new Socket();
            }
    
            if (aBindAddr != null)
            {
                mSocket.bind(new InetSocketAddress(aBindAddr, 0));
            }
            if (aResponseTimeout > 0)
            {
                mSocket.setSoTimeout(aResponseTimeout * 1000);
            }
            mSocket.setKeepAlive(true);
            mSocket.setReceiveBufferSize(mSockRcvBufSize); // PROJ-2625
            mSocket.setSendBufferSize(CM_DEFAULT_SNDBUF_SIZE);

            mSocket.connect(aSockAddr, aLoginTimeout * 1000);
        
            if (mUseSsl)
            {
                ((SSLSocket)mSocket).setEnabledProtocols(new String[] { "TLSv1" });
                ((SSLSocket)mSocket).startHandshake();
            }
    
            mWriteChannel = Channels.newChannel(mSocket.getOutputStream());
            mReadChannel = Channels.newChannel(mSocket.getInputStream());
            initToWrite();
        }
        catch (SQLException sEx)
        {
            quiteClose();
            throw sEx;
        }
        // connect ���н� �� �� �ִ� ���ܰ� �������� �ƴϹǷ� ��� ���ܸ� ��� ���� ���з� ó���Ѵ�.
        // �������, AIX 6.1������ ClosedSelectorException�� ���µ� �̴� RuntimeException�̴�. (ref. BUG-33341)
        catch (Exception sCauseEx)
        {
            quiteClose();
            Error.throwCommunicationErrorException(sCauseEx);
        }
    }

    /**
     * ������ ������ �̿��� SSLSocketFactory ��ü�� �����Ѵ�.
     * @param aCertiProps ssl���� ���� ������ ������ �ִ� ��ü
     * @return SSLSocketFactory ��ü
     * @throws SQLException keystore���� ������ �߻��� ���
     */
    private SSLSocketFactory getSslSocketFactory(SSLProperties aCertiProps) throws SQLException
    {
        String sKeyStoreUrl = aCertiProps.getKeyStoreUrl();
        String sKeyStorePassword = aCertiProps.getKeyStorePassword();
        String sKeyStoreType = aCertiProps.getKeyStoreType();
        String sTrustStoreUrl = aCertiProps.getTrustStoreUrl();
        String sTrustStorePassword = aCertiProps.getTrustStorePassword();
        String sTrustStoreType = aCertiProps.getTrustStoreType();

        TrustManagerFactory sTmf = null;
        KeyManagerFactory sKmf = null;

        try
        {
            sTmf = TrustManagerFactory.getInstance(TrustManagerFactory.getDefaultAlgorithm());
            sKmf = KeyManagerFactory.getInstance(KeyManagerFactory.getDefaultAlgorithm());
        }
        catch (NoSuchAlgorithmException sNsae)
        {
            Error.throwSQLException(ErrorDef.DEFAULT_ALGORITHM_DEFINITION_INVALID, sNsae);
        }

        loadKeyStore(sKeyStoreUrl, sKeyStorePassword, sKeyStoreType, sKmf);

        // BUG-40165 verify_server_certificate�� true�϶��� TrustManagerFactory�� �ʱ�ȭ ���ش�.
        if (aCertiProps.verifyServerCertificate())
        {
            loadKeyStore(sTrustStoreUrl, sTrustStorePassword, sTrustStoreType, sTmf);
        }
        
        return createAndInitSslContext(aCertiProps, sKeyStoreUrl, sTrustStoreUrl, sTmf, sKmf).getSocketFactory();
    }

    private SSLContext createAndInitSslContext(SSLProperties aCertiInfo, String aKeyStoreUrl, String aTrustStoreUrl,
                                               TrustManagerFactory aTmf, KeyManagerFactory aKmf) throws SQLException
    {
        SSLContext sSslContext = null;

        try 
        {
            sSslContext = SSLContext.getInstance("TLS");
            
            KeyManager[] sKeyManagers = (StringUtils.isEmpty(aKeyStoreUrl)) ?  null : aKmf.getKeyManagers();
            TrustManager[] sTrustManagers;
            if (aCertiInfo.verifyServerCertificate())
            {
                sTrustManagers = (StringUtils.isEmpty(aTrustStoreUrl)) ? null : aTmf.getTrustManagers();
            }
            else
            {
                sTrustManagers = new X509TrustManager[] { BlindTrustManager.getInstance() };
            }
            sSslContext.init(sKeyManagers, sTrustManagers, null);
            
        }
        catch (NoSuchAlgorithmException sNsae) 
        {
            Error.throwSQLException(ErrorDef.UNSUPPORTED_KEYSTORE_ALGORITHM, sNsae.getMessage(), sNsae);
        } 
        catch (KeyManagementException sKme) 
        {
            Error.throwSQLException(ErrorDef.KEY_MANAGEMENT_EXCEPTION_OCCURRED, sKme.getMessage(), sKme);
        }
        return sSslContext;
    }

    /**
     * PROJ-2474 KeyStore �� TrustStore ������ �о���δ�.
     */
    private void loadKeyStore(String aKeyStoreUrl, String aKeyStorePassword, String aKeyStoreType, 
                                    Object aKeystoreFactory) throws SQLException
    {
        if (StringUtils.isEmpty(aKeyStoreUrl)) return;
        InputStream keyStoreStream = null;
        
        try 
        {
            aKeyStoreType = (StringUtils.isEmpty(aKeyStoreType)) ? DEFAULT_KEYSTORE_TYPE : aKeyStoreType;
            KeyStore sClientKeyStore = KeyStore.getInstance(aKeyStoreType);
            URL sKsURL = new URL(DEFAULT_URL_FILE_SCHEME + aKeyStoreUrl);
            char[] sPassword = (aKeyStorePassword == null) ? new char[0] : aKeyStorePassword.toCharArray();
            keyStoreStream = sKsURL.openStream();
            sClientKeyStore.load(keyStoreStream, sPassword);
            keyStoreStream.close();
            if (aKeystoreFactory instanceof KeyManagerFactory)
            {
                ((KeyManagerFactory)aKeystoreFactory).init(sClientKeyStore, sPassword);
            }
            else if (aKeystoreFactory instanceof TrustManagerFactory)
            {
                ((TrustManagerFactory)aKeystoreFactory).init(sClientKeyStore);
            }
        }
        catch (UnrecoverableKeyException sUke) 
        {
            Error.throwSQLException(ErrorDef.CAN_NOT_RETREIVE_KEY_FROM_KEYSTORE, sUke);
        } 
        catch (NoSuchAlgorithmException sNsae) 
        {
            Error.throwSQLException(ErrorDef.UNSUPPORTED_KEYSTORE_ALGORITHM, sNsae.getMessage(), sNsae);   
        } 
        catch (KeyStoreException sKse) 
        {
            Error.throwSQLException(ErrorDef.CAN_NOT_CREATE_KEYSTORE_INSTANCE, sKse.getMessage(), sKse);
        } 
        catch (CertificateException sNsae) 
        {
            Error.throwSQLException(ErrorDef.CAN_NOT_LOAD_KEYSTORE, aKeyStoreType, sNsae);
        } 
        catch (MalformedURLException sMue) 
        {
            Error.throwSQLException(ErrorDef.INVALID_KEYSTORE_URL, sMue);   
        } 
        catch (IOException sIoe) 
        {
            Error.throwSQLException(ErrorDef.CAN_NOT_OPEN_KEYSTORE, sIoe);
        }
        finally
        {
            try
            {
                if (keyStoreStream != null)
                {
                    keyStoreStream.close();
                }
            }
            catch (IOException sIOe)
            {
                Error.throwSQLExceptionForIOException(sIOe.getCause());
            }
        }
    }
    
    public boolean isClosed()
    {
        return (mState == CHANNEL_STATE_CLOSED);
    }

    public void close() throws IOException
    {
        if (mState != CHANNEL_STATE_CLOSED)
        {
            if (mWriteChannel != null)
            {
                mWriteChannel.close();
                mWriteChannel = null;
            }
            if (mReadChannel != null)
            {
                mReadChannel.close();
                mReadChannel = null;
            }
            if (mSocket != null)
            {
                mSocket.close();
                mSocket = null;
            }
            mSocketFD = INVALID_SOCKTFD;
            mState = CHANNEL_STATE_CLOSED;
        }
    }

    /**
     * ä���� �ݴ´�.
     * �� ��, ���ܰ� �߻��ϴ��� ������ �Ѿ��.
     */
    public void quiteClose()
    {
        try
        {
            close();
        }
        catch (IOException ex)
        {
            // quite
        }
    }

    public void setCharset(Charset aCharset, Charset aNCharset)
    {
        super.setCharset(aCharset, aNCharset); // BUG-45156 encoder�� ���� �κ��� CmBufferWrite���� ó��

        mDBDecoder = aCharset.newDecoder();
        mNCharDecoder = aNCharset.newDecoder();

        mDBDecoder.onMalformedInput(CodingErrorAction.REPORT);
        mDBDecoder.onUnmappableCharacter(CodingErrorAction.REPORT);
        mNCharDecoder.onMalformedInput(CodingErrorAction.REPORT);
        mNCharDecoder.onUnmappableCharacter(CodingErrorAction.REPORT);

        CLOB_BUF_SIZE = LobConst.LOB_BUFFER_SIZE  / getByteLengthPerChar();

        // PROJ-2427 getBytes�� ����� Encoder�� ColumnFactory�� ����
        mColumnFactory.setCharSetEncoder(mDBEncoder);
        mColumnFactory.setNCharSetEncoder(mNCharEncoder);
    }
    
    public Charset getCharset()
    {
        return mDBEncoder.charset();
    }
    
    public Charset getNCharset()
    {
        return mNCharEncoder.charset();
    }
    
    public int getByteLengthPerChar()
    {
        return (int)Math.ceil(mDBEncoder.maxBytesPerChar());
    }
    
    void writeOp(byte aOpCode) throws SQLException
    {
        checkWritable(1);
        mBuffer.put(aOpCode);
    }

    public void writeBytes(byte[] aValue, int aOffset, int aLength) throws SQLException
    {
        checkWritable(aLength);
        mBuffer.put(aValue, aOffset, aLength);
    }
    
    int writeBytes4Clob(ReadableCharChannel aChannelFromSource, long aLocatorId, boolean isCopyMode) throws SQLException
    {
        throwErrorForClosed();

        int sByteReadSize = -1;
        try
        {
            checkWritable4Lob(LobConst.LOB_BUFFER_SIZE);

            int sOrgPos4Op = mBuffer.position();
            mBuffer.position(sOrgPos4Op + LobConst.FIXED_LOB_OP_LENGTH_FOR_PUTLOB);
            sByteReadSize = aChannelFromSource.read(mBuffer);
            if (sByteReadSize == -1)
            {
                mBuffer.position(sOrgPos4Op);
            }
            else
            {
                mBuffer.position(sOrgPos4Op);
                this.writeOp(CmOperationDef.DB_OP_LOB_PUT);
                this.writeLong(aLocatorId);
                this.writeUnsignedInt(sByteReadSize);
                mBuffer.position(mBuffer.position() + sByteReadSize);

                if (isCopyMode)
                {
                    sendPacket(false);
                    initToWrite();

                    Object sSrc = aChannelFromSource.getSource();
                    if (sSrc instanceof ClobReader)
                    {
                        ((ClobReader)sSrc).readyToCopy();
                    }
                }
            }
        }
        catch (IOException sException)
        {
            Error.throwSQLExceptionForIOException(sException.getCause());
        }
        return sByteReadSize;
    }

    public void writeBytes(ByteBuffer aValue) throws SQLException
    {
        throwErrorForClosed();

        while (aValue.remaining() > 0)
        {
            if (aValue.remaining() <= mBuffer.remaining())
            {
                mBuffer.put(aValue);
            }
            else
            {
                int sOrgLimit = aValue.limit();
                aValue.limit(aValue.position() + mBuffer.remaining());
                mBuffer.put(aValue);
                sendPacket(false); // ���� �� = �������� �ƴ�

                aValue.limit(sOrgLimit);
                initToWrite();
            }
        }
    }

    public void sendAndReceive() throws SQLException
    {
        throwErrorForClosed();
        sendPacket(true);
        initToRead();
        receivePacket();
    }
    
    public void send() throws SQLException
    {
        throwErrorForClosed();
        sendPacket(true);
        initToWrite();
    }

    public void receive() throws SQLException
    {
        throwErrorForClosed();
        initToRead();
        receivePacket();
    }

    /**
     * LOB I/O�� ����� �ӽ� ���۸� ��´�.
     * <p>
     * ���ü� ������ �߻��� �� �����Ƿ� �ݵ�� synchronozed ���������� ����ؾ��Ѵ�.
     * 
     * @return LOB I/O�� ����� �ӽ� ����
     */
    byte[] getLobBuffer()
    {
        if (mLobBuf == null)
        {
            mLobBuf = new byte[LobConst.LOB_BUFFER_SIZE];
        }
        return mLobBuf;
    }
    
    int checkWritable4Lob(long aLengthToWrite) throws SQLException
    {
        checkWritable(LobConst.FIXED_LOB_OP_LENGTH_FOR_PUTLOB);

        if (mBuffer.remaining() < (aLengthToWrite + LobConst.FIXED_LOB_OP_LENGTH_FOR_PUTLOB))
        {
            return mBuffer.remaining() - LobConst.FIXED_LOB_OP_LENGTH_FOR_PUTLOB; 
        }
        else
        {
            return (int)aLengthToWrite;
        }
    }
    
    public void checkWritable(int aNeedToWrite) throws SQLException
    {
        throwErrorForClosed();

        if (mState == CHANNEL_STATE_READABLE)
        {
            initToWrite();
        }
        else
        {
            if (mBuffer.remaining() < aNeedToWrite)
            {
                sendPacket(false);
                initToWrite();
            }
        }
    }

    private void writePacketHeader(int aPayloadLength, boolean aIsLast)
    {
        mBuffer.put(CM_PACKET_HEADER_VERSION);
        mBuffer.put(CM_PACKET_HEADER_RESERVED_BYTE);
        mBuffer.putShort((short) aPayloadLength);
        
        if(aIsLast) 
        {
            mBuffer.putInt(setLast(mNextSendSeqNo));
        }
        else
        {
            mBuffer.putInt(mNextSendSeqNo);
        }
        
        mBuffer.putShort(CM_PACKET_HEADER_RESERVED_SHORT);
        mBuffer.putShort(mSessionID);
        mBuffer.putInt(CM_PACKET_HEADER_RESERVED_INT);
        
        mNextSendSeqNo = incAsUnsignedInt(mNextSendSeqNo);
    }

    private void initToWrite()
    {
        mBuffer.clear();
        mBuffer.position(CM_PACKET_HEADER_SIZE);
        mState = CHANNEL_STATE_WRITABLE;
    }

    private void sendPacket(boolean aIsLast) throws SQLException
    {
        int sTotalWrittenLength = mBuffer.position();
        mBuffer.flip();
        writePacketHeader(sTotalWrittenLength - CM_PACKET_HEADER_SIZE, aIsLast);
        mBuffer.rewind();
        try
        {
            // PROJ-2583 jdbc packet logging
            if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED && mCmLogger.isLoggable(Level.FINEST))
            {
                byte[] sByteArry = new byte[mBuffer.remaining()];
                mBuffer.get(sByteArry);
                mBuffer.rewind();
                String sDumpedBytes = DumpByteUtil.dumpBytes(sByteArry, 0, sByteArry.length);
                mCmLogger.log(Level.FINEST, sDumpedBytes, Integer.toHexString(mSessionID & 0xffff));
            }
            while (mBuffer.hasRemaining())
            {
                mWriteChannel.write(mBuffer);
            }
        }
        catch (IOException sException)
        {
            Error.throwCommunicationErrorException(sException);
        }        
    }

    private void initToRead()
    {
        // �񵿱�� ��û�� ���¿��� ����� �ۼ��� �ϰ��� �Ѵٸ�, �ϳ��� CM block ���� �� �̻��� packet �� ��� ���� �� ����.
        // �� ���, mPendedDataLength ���� 0 �� �ƴϱ� ������ ������ mBuffer.clear() ���� �ʵ��� �ϰ�,
        // ���� receivePacket() �Լ����� mBuffer �� �����ϰ� �� ������ packet data �� ���� ������. 
        if (mPendedDataLength == 0)
        {
            mBuffer.clear();
        }
        mState = CHANNEL_STATE_READABLE;
    }

    /**
     * ����Ÿ�� aSkipLen ��ŭ �ǳ� �ڴ�.
     * <p>
     * ����, aSkipLen�� 0�̸� ������ �����Ѵ�.
     *
     * @param aSkipLen �ǳ� �� byte ��
     * @throws InternalError aSkipLen�� 0���� ���� ���
     */
    public void skip(int aSkipLen)
    {
        if (aSkipLen < 0)
        {
            Error.throwInternalError(ErrorDef.INVALID_ARGUMENT,
                                     "Skip length",
                                     "0 ~ Integer.MAX_VALUE",
                                     String.valueOf(aSkipLen));
        }

        if (aSkipLen > 0)
        {
            mBuffer.position(mBuffer.position() + aSkipLen);
        }
    }

    private int initPendedData()
    {
        int pend = mPendedDataLength;
        if(mPendedDataLength > 0)
        {
            /* move for compact it, keep position */
            mBuffer.limit(mBuffer.limit() + mPendedDataLength);
            mBuffer.compact();
        }
        else
        {
            mBuffer.clear();
        }
        mPendedDataLength = 0;
        return pend;
    }
    
    private void readyToRead()
    {
        // Expected Size
        int size = mPayloadSize + CM_PACKET_HEADER_SIZE;
        
        // The exceeded amount of data more than expected data size
        mPendedDataLength = mBuffer.limit() - size;

        // BUGBUG Data loss occured
        if(mPendedDataLength < 0)
        {
            mPendedDataLength = 0;
        }    
        
        // Skip Header Size
        mBuffer.position(CM_PACKET_HEADER_SIZE);
        
        // To definite limitation to read one packet exactly 
        mBuffer.limit(size);
    }
    
    private void receivePacket() throws SQLException
    {
        long sLeftPacket, sReadLength;

        /* 1.0 Read At Least Header, include pending */
        for (sLeftPacket = CM_PACKET_HEADER_SIZE - initPendedData(); sLeftPacket > 0; sLeftPacket -= sReadLength)
        {
            throwErrorForThreadInterrupted();
            sReadLength = readFromSocket();
        }

        /*
         * 2.0 gets a left, left could be <0 - it mean that read more then
         * header bytes, so do not read it.
         */
        for (sLeftPacket += parseHeader(); sLeftPacket > 0; sLeftPacket -= sReadLength)
        {
            throwErrorForThreadInterrupted();
            // bb.limit(bb.capacity()); - this is Ok too but a lot of panding
            // bytes
            mBuffer.limit(CM_PACKET_HEADER_SIZE + mPayloadSize);
            sReadLength = readFromSocket();
        }
        
        // PROJ-2583 jdbc packet logging
        if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED && mCmLogger.isLoggable(Level.FINEST))
        {
            mBuffer.rewind();  // BUG-43694 limit�� ���� �ʰ� �ٷ� rewind�Ѵ�.
            byte[] sByteArry = new byte[mBuffer.remaining()];
            mBuffer.get(sByteArry);
            String sDumpedBytes = DumpByteUtil.dumpBytes(sByteArry, 0, sByteArry.length);
            mCmLogger.log(Level.FINEST, sDumpedBytes, String.valueOf(mSessionID));
        }
        
        readyToRead();
    }

    private int readFromSocket() throws SQLException
    {
        int sRead = 0;
        
        while (true)
        {
            try
            {
                sRead = mReadChannel.read(mBuffer);
            }
            catch (SocketTimeoutException sTimeoutException)
            {
                quiteClose(); // RESPONSE_TIMEOUT�� ���� ������ ���´�.
                Error.throwSQLException(ErrorDef.RESPONSE_TIMEOUT, String.valueOf(mResponseTimeout), sTimeoutException);
            }
            catch (IOException sException)
            {
                Error.throwCommunicationErrorException(sException);
            }
            
            if (sRead == -1)
            {
                Error.throwSQLException(ErrorDef.COMMUNICATION_ERROR, "There was no response from the server, and the channel has reached end-of-stream.");
            }

            if (sRead > 0)
            {
                break;
            }
        }
        
        return sRead;
    }
    
    byte readOp() throws SQLException
    {
        if (mBuffer.remaining() == 0)
        {
            if (mIsLastPacket)
            {
                return NO_OP;
            }
            receivePacket();
        }
        return mBuffer.get();
    }

    public byte readByte() throws SQLException
    {
        checkReadable(1);
        readyToReadAfterCheck();
        return mBuffer.get();
    }

    public short readUnsignedByte() throws SQLException
    {
        checkReadable(1);
        readyToReadAfterCheck();
        byte sByte = mBuffer.get();
        return (short) (sByte & 0xFF);
    }

    public short readShort() throws SQLException
    {
        checkReadable(2);
        readyToReadAfterCheck();
        return mBuffer.getShort();
    }

    public int readUnsignedShort() throws SQLException
    {
        checkReadable(2);
        readyToReadAfterCheck();
        short sShort = mBuffer.getShort();
        return sShort & 0xFFFF;
    }
    
    private void readyToReadAfterCheck()
    {
        mBuffer.limit(CM_PACKET_HEADER_SIZE + mPayloadSize);
    }

    public int readInt() throws SQLException
    {
        checkReadable(4);
        readyToReadAfterCheck();
        return mBuffer.getInt();
    }
    
    long readUnsignedInt() throws SQLException
    {
        checkReadable(4);        
        readyToReadAfterCheck();
        return mBuffer.getInt() & 0xFFFFFFFFL;
    }

    public BigInteger readUnsignedLong() throws SQLException
    {
        checkReadable(8);
        readyToReadAfterCheck();
        byte[] uLongMagnitude = new byte[8];
        mBuffer.get(uLongMagnitude);
        return new BigInteger(1, uLongMagnitude);
    }
    
    public long readLong() throws SQLException
    {
        checkReadable(8);
        readyToReadAfterCheck();
        return mBuffer.getLong();
    }

    public float readFloat() throws SQLException
    {
        checkReadable(4);
        readyToReadAfterCheck();
        return mBuffer.getFloat();
    }

    public double readDouble() throws SQLException
    {
        checkReadable(8);
        readyToReadAfterCheck();
        return mBuffer.getDouble();
    }

    public void readBytes(byte[] aDest) throws SQLException
    {
        readBytes(aDest, aDest.length);
    }
    
    public void readBytes(byte[] aDest, int aOffset, int aLength) throws SQLException
    {
        for (int sBufPos = aOffset, sNeededBytesSize = aLength;;)
        {
            int sReadableBytesSize = mBuffer.remaining();
            if (sNeededBytesSize <= sReadableBytesSize)
            {
                int sOrgLimit = mBuffer.limit();
                mBuffer.limit(mBuffer.position() + sNeededBytesSize);
                mBuffer.get(aDest, sBufPos, sNeededBytesSize);
                mBuffer.limit(sOrgLimit);
                throwErrorForPacketTwisted(sBufPos + sNeededBytesSize - aOffset != aLength);
                break;
            }
            else /* if (sNeededBytesSize > mBuf.remaining()) */
            {
                mBuffer.get(aDest, sBufPos, sReadableBytesSize);
                sBufPos += sReadableBytesSize;
                sNeededBytesSize -= sReadableBytesSize;
                receivePacket();
            }
        }
    }
    
    public void readBytes(byte[] aDest, int aLength) throws SQLException
    {
        readBytes(aDest, 0, aLength);
    }

    public void readBytes(ByteBuffer aBuf) throws SQLException
    {
        for (int sNeededBytesSize = aBuf.remaining();;)
        {
            int sReadableBytesSize = mBuffer.remaining();
            if (sNeededBytesSize <= sReadableBytesSize)
            {
                int sOrgLimit = mBuffer.limit();
                mBuffer.limit(mBuffer.position() + sNeededBytesSize);
                aBuf.put(mBuffer);
                mBuffer.limit(sOrgLimit);
                throwErrorForPacketTwisted(aBuf.remaining() > 0);
                break;
            }
            else /* if (sNeededBytesSize > mBuf.remaining()) */
            {
                aBuf.put(mBuffer);
                sNeededBytesSize -= sReadableBytesSize;
                receivePacket();
            }
        }
    }

    String readString(int aByteSize, int aSkipLength) throws SQLException
    {
        StringBuffer sStrBuf = readStringBy(aByteSize, aSkipLength, mDBDecoder);
        return sStrBuf.toString();
    }
    
    void readCharArrayTo(char[] aDst, int aDstOffset, int aByteLength, int aCharLength) throws SQLException
    {
        StringBuffer sStrBuf = readStringBy(aByteLength, 0, mDBDecoder);
        sStrBuf.getChars(0, aCharLength, aDst, aDstOffset);
    }
    
    public String readString(byte[] aBytesValue)
    {
        return decodeString(ByteBuffer.wrap(aBytesValue), mDBDecoder);
    }

    /**
     * Char, Varchar �÷� �����͸� ���� �� Charset�� �°� decode�� ���ڿ��� �����Ѵ�.
     * @param aColumnSize �÷������� ������
     * @param aSkipSize  ��ŵ������
     * @param aIsNationalCharset �ٱ���ĳ���ͼ����� ����
     * @return ���ڵ��� char, varchar�÷� ��Ʈ��
     * @throws SQLException ���������� �÷������͸� �޾ƿ��� ���� ���
     */
    public String readCharVarcharColumnString(int aColumnSize, int aSkipSize,
                                              boolean aIsNationalCharset) throws SQLException
    {
        mCharVarcharColumnBuffer.clear();
        mCharVarcharColumnBuffer.limit(aColumnSize);
        this.readBytes(mCharVarcharColumnBuffer);
        this.skip(aSkipSize);
        mCharVarcharColumnBuffer.flip();

        return  aIsNationalCharset ? decodeString(mCharVarcharColumnBuffer, mNCharDecoder) :
                                     decodeString(mCharVarcharColumnBuffer, mDBDecoder);
    }

    private String decodeString(ByteBuffer aByteBuf, CharsetDecoder aDecoder)
    {
        mCharBuf.clear();
        while (aByteBuf.remaining() > 0)
        {
            CoderResult sResult = aDecoder.decode(aByteBuf, mCharBuf, true);
            if (sResult == CoderResult.OVERFLOW)
            {
                Error.throwInternalError(ErrorDef.INTERNAL_BUFFER_OVERFLOW);
            }
            if (sResult.isError())
            {
                char data = (char)aByteBuf.get();
                if (sResult.isUnmappable())
                {
                    // fix BUG-27782 ��ȯ�� �� ���� ���ڴ� '?'�� ���
                    mCharBuf.put('?');
                }
                else if (sResult.isMalformed())
                {
                    mCharBuf.put(data);
                }
            }
        }
        mCharBuf.flip();
        return mCharBuf.toString();
    }

    String readStringForErrorResult() throws SQLException
    {
        int sSize = readUnsignedShort();
        byte[] sBuf = new byte[sSize];
        readBytes(sBuf);
        return new String(sBuf);
    }

    String readStringForMessage() throws SQLException
    {
        long sSize = readUnsignedInt();
        byte[] sBuf = new byte[(int)sSize];
        readBytes(sBuf);

        long sRestLength = sSize - Integer.MAX_VALUE;
        while (sRestLength > 0)
        {
            if (sRestLength >= Integer.MAX_VALUE)
            {
                skip(Integer.MAX_VALUE);
                sRestLength -= Integer.MAX_VALUE;
            }
            else
            {
                skip((int)sRestLength);
                sRestLength = 0;
            }
        }

        return new String(sBuf);
    }

    String readStringForColumnInfo() throws SQLException
    {
        int sStrLen = (int)readByte();
        if (sStrLen == 0)
        {
            return null;
        }

        byte[] sBuf = new byte[sStrLen];
        readBytes(sBuf);
        return new String(sBuf);
    }
    
    private void checkReadable(int aLengthToRead) throws SQLException
    {
        if (mState != CHANNEL_STATE_READABLE)
        {
            Error.throwInternalError(ErrorDef.INVALID_STATE, "CHANNEL_STATE_READABLE", CHANNEL_STATE_STRING[mState]);
        }

        if (mBuffer.remaining() < aLengthToRead)
        {
            if (mIsLastPacket) 
            {
                Error.throwInternalError(ErrorDef.MORE_DATA_NEEDED);
            }

            if (mPrimitiveTypeBuf == null)
            {
                mPrimitiveTypeBuf = ByteBuffer.allocateDirect(MAX_PRIMITIVE_TYPE_SIZE);
            }

            int sRemain = mBuffer.remaining();

            if (sRemain > MAX_PRIMITIVE_TYPE_SIZE)
            {
                Error.throwInternalError(ErrorDef.EXCEED_PRIMITIVE_DATA_SIZE);
            }

            if (sRemain > 0)
            {
                mPrimitiveTypeBuf.clear();
                mPrimitiveTypeBuf.put(mBuffer);
                mPrimitiveTypeBuf.flip();
            }

            receivePacket();

            if (sRemain > 0)
            {
                mBuffer.position(mBuffer.position() - sRemain);
                int sPos = mBuffer.position();
                mBuffer.put(mPrimitiveTypeBuf);
                mBuffer.position(sPos);
            }
        }
    }

    private int parseHeader() throws SQLException
    {
        int sOrgPos = mBuffer.position();
        mBuffer.flip();

        byte sByte = mBuffer.get();
        if (sByte != CM_PACKET_HEADER_VERSION)
        {
            Error.throwInternalError(ErrorDef.INVALID_PACKET_HEADER_VERSION,
                                     String.valueOf(CM_PACKET_HEADER_VERSION),
                                     String.valueOf(sByte));
        }

        mBuffer.get();  // Reserved 1
        mPayloadSize = mBuffer.getShort();        
        int sSequenceNo = mBuffer.getInt();               
        mBuffer.getShort(); // Reserved 2

        mIsLastPacket = !isFragmented(sSequenceNo);

        mSessionID = mBuffer.getShort();
        mBuffer.getInt(); // Reserved 3
        
        mBuffer.position(sOrgPos);
        
        checkValidSeqNo(removeIntegerMSB(sSequenceNo));
        
        return mPayloadSize;
    }

    private StringBuffer readStringBy(int aByteSize, int aSkipLength, CharsetDecoder aDecoder) throws SQLException
    {
        StringBuffer sStrBuf = new StringBuffer();
        int sNeededBytesSize = aByteSize;
        int sReadableBytesSize;
        while (sNeededBytesSize > (sReadableBytesSize = mBuffer.remaining()))
        {
            mCharBuf.clear();
            CoderResult sCoderResult = aDecoder.decode(mBuffer, mCharBuf, true);
            if (sCoderResult == CoderResult.OVERFLOW)
            {
                Error.throwInternalError(ErrorDef.INTERNAL_BUFFER_OVERFLOW);
            }
            mCharBuf.flip();
            sStrBuf.append(mCharBuf.array(), mCharBuf.arrayOffset(), mCharBuf.limit());
            if (sCoderResult.isError()) // BUG-27782
            {
                char data = (char)mBuffer.get();
                if (sCoderResult.isUnmappable())
                {
                    sStrBuf.append("?");
                }
                else if (sCoderResult.isMalformed())
                {
                    sStrBuf.append(data);
                }
            }

            // ���ڰ� ©�� encode�� ���� ���ۿ� ���ܵ� ����Ÿ�� ���� �� �����Ƿ�
            // ���� ���̸� ���� �ٽ� ó���� ���̿� ������� �Ѵ�.
            int sRemainBytesSize = mBuffer.remaining();
            sNeededBytesSize -= sReadableBytesSize;
            checkReadable(sNeededBytesSize);
            sNeededBytesSize += sRemainBytesSize;
        }
        if (sNeededBytesSize > 0)
        {
            int sOrgLimit = mBuffer.limit();
            mBuffer.limit(mBuffer.position() + sNeededBytesSize);
            CoderResult sCoderResult;
            do
            {
                mCharBuf.clear();
                sCoderResult = aDecoder.decode(mBuffer, mCharBuf, true);
                mCharBuf.flip();
                sStrBuf.append(mCharBuf.array(), mCharBuf.arrayOffset(), mCharBuf.limit());
                if (sCoderResult.isError())
                {
                    // �ش� byte �鿡 ���� mapping �� �� �ִ� char �� ���� ��� :
                    // ByteBuffer �� �Һ� �����־�� ���� ���ڿ� ���� ó���� �����ϹǷ�,
                    // �ϴ� get���� �����͸� ���� ��, �̸� ������ string buffer�� ����.
                    // ���� encoding table ���� �ٸ� ���̹Ƿ� ���� ���ڷ� ǥ����.
                    // ('?' �� ǥ���Ǹ�, �ڵ尪�� 63)
                    char data = (char)mBuffer.get();
                    if (sCoderResult.isUnmappable())
                    {
                        // fix BUG-27782 ��ȯ�� �� ���� ���ڴ� '?'�� ����Ѵ�.
                        sStrBuf.append("?");
                    }
                    else if (sCoderResult.isMalformed())
                    {
                        sStrBuf.append(data);
                    }
                }
            } while (sCoderResult != CoderResult.OVERFLOW && sCoderResult != CoderResult.UNDERFLOW);

            if (sCoderResult == CoderResult.OVERFLOW) 
            {
                Error.throwInternalError(ErrorDef.INTERNAL_BUFFER_OVERFLOW);
            }
            aSkipLength += mBuffer.remaining();
            mBuffer.limit(sOrgLimit);
            if (aSkipLength > 0)
            {
                mBuffer.position(mBuffer.position() + aSkipLength);
            }
        }
        return sStrBuf;
    }

    private void checkValidSeqNo(int aNumber) 
    {
        if (mPrevReceivedSeqNo + 1 == aNumber)
        {
            mPrevReceivedSeqNo = aNumber;
        }
        else
        {
            Error.throwInternalError(ErrorDef.INVALID_PACKET_SEQUENCE_NUMBER, String.valueOf(mPrevReceivedSeqNo + 1), String.valueOf(aNumber));
        }
    }
    
    private static int incAsUnsignedInt(int aNumber)
    {
        if (aNumber == 0x7FFFFFFF)
        {
            return 0;
        }

        return ++aNumber;
    }
    
    private static int removeIntegerMSB(int aNumber) 
    {
        return aNumber & 0x7FFFFFFF;
    }
    
    private static boolean isFragmented(int aSequenceNo)
    {
        return (aSequenceNo & 0x80000000) == 0;
    }

    private static int setLast(int aSequenceNo)
    {
        return aSequenceNo | 0x80000000;
    }

    public int getLobCacheThreshold()
    {
        return mLobCacheThreshold;
    }

    public void setLobCacheThreshold(int aLobCacheThreshold)
    {
        this.mLobCacheThreshold = aLobCacheThreshold;
    }

    ReadableCharChannel getReadChannel4Clob(Reader aSource) throws IOException
    {
        if(mReadChannel4Clob == null)
        {
            mReadChannel4Clob = new AltibaseReadableCharChannel();
        }
        
        mReadChannel4Clob.open(aSource);
        return mReadChannel4Clob;
    }
    
    ReadableCharChannel getReadChannel4Clob(Reader aSource, int aSourceLength) throws IOException
    {
        if(mReadChannel4Clob == null)
        {
            mReadChannel4Clob = new AltibaseReadableCharChannel();
        }
        
        mReadChannel4Clob.open(aSource, aSourceLength);
        return mReadChannel4Clob;
    }

    ReadableCharChannel getReadChannel4Clob(char[] aSource, int aOffset, int aLength) throws IOException
    {
        if(mReadChannel4Clob == null)
        {
            mReadChannel4Clob = new AltibaseReadableCharChannel();
        }
        
        mReadChannel4Clob.open(aSource, aOffset, aLength);
        return mReadChannel4Clob;
    }

    public void setRemoveRedundantMode(boolean aMode, AltibaseProperties aProps)
    {
        this.mRemoveRedundantMode = aMode;

        mColumnFactory.setProperties(aProps);
    }
    
    public ColumnFactory getColumnFactory()
    {
        return mColumnFactory;
    }
    
    private void throwErrorForClosed() throws SQLException
    {
        if (isClosed())
        {
            Error.throwSQLException(ErrorDef.CLOSED_CONNECTION);
        }
    }
    
    private void throwErrorForThreadInterrupted() throws SQLException
    {
        if (Thread.interrupted())
        {
            Error.throwSQLException(ErrorDef.THREAD_INTERRUPTED);
        }
    }
    
    private void throwErrorForPacketTwisted(boolean aTest) throws SQLException
    {
        if (aTest)
        {
            Error.throwSQLException(ErrorDef.PACKET_TWISTED);
        }
    }

    /**
     * PROJ-2474 ssl_enable �� Ȱ��ȭ �Ǿ� �ִ� ��� mUseSsl flag�� true�� �������ְ�, ������ ���õ� ������ �о��
     * mSslProps ��ü�� �����Ѵ�.
     * @param aProps  AltibaseProperties��ü
     */
    public void setSslProps(AltibaseProperties aProps)
    {
        this.mSslProps = new SSLProperties(aProps);
        this.mUseSsl = mSslProps.isSslEnabled();
    }

    /**
     * ���� SSLSocket���� ������ CipherSuites ����Ʈ�� �����ش�.
     * @return CipherSuite ����Ʈ
     */
    public String[] getCipherSuitList()
    {
        return ((SSLSocket)mSocket).getEnabledCipherSuites();
    }

    /**
     * Socket receive buffer ũ�⸦ ��ȯ�Ѵ�.
     */
    public int getSockRcvBufSize()
    {
        return mSockRcvBufSize;
    }

    /**
     * Socket receive buffer ũ�⸦ �����Ѵ�.
     */
    public void setSockRcvBufSize(int aSockRcvBufSize) throws IOException
    {
        int sSockRcvBufSize = aSockRcvBufSize;

        if (!isClosed())
        {
            mSocket.setReceiveBufferSize(sSockRcvBufSize);
        }

        mSockRcvBufSize = sSockRcvBufSize;
    }

    /**
     * Socket receive buffer ũ�⸦ CM block ������ �����Ѵ�.
     */
    public void setSockRcvBufBlockRatio(int aSockRcvBufBlockRatio) throws IOException
    {
        int sSockRcvBufSize;
        
        if (aSockRcvBufBlockRatio <= 0)
        {
            sSockRcvBufSize = CM_DEFAULT_RCVBUF_SIZE;
        }
        else
        {
            sSockRcvBufSize = aSockRcvBufBlockRatio * CM_BLOCK_SIZE;
        }

        if (!isClosed())
        {
            mSocket.setReceiveBufferSize(sSockRcvBufSize);
        }

        mSockRcvBufSize = sSockRcvBufSize;
    }

    /**
     * �񵿱������� fetch ���������� �۽��� protocol context �� �����Ѵ�.
     */
    public void setAsyncContext(CmProtocolContext aContext)
    {
        mAsyncContext = aContext;
    }

    /**
     * �񵿱������� fetch ���������� �۽��� protocol context �� ��´�.
     */
    public boolean isAsyncSent()
    {
        return (mAsyncContext != null);
    }

    /**
     * �񵿱������� fetch ���������� �۽��� protocol context �� ��´�.
     */
    public CmProtocolContext getAsyncContext()
    {
        return mAsyncContext;
    }

    /**
     * Socket �� file descriptor �� ��ȯ�Ѵ�.
     */
    public int getSocketFD() throws SQLException
    {
        throwErrorForClosed();

        if (mSocketFD == INVALID_SOCKTFD)
        {
            try
            {
                mSocketFD = SocketUtils.getFileDescriptor(mSocket);
            }
            catch (Exception e)
            {
                Error.throwSQLException(ErrorDef.COMMUNICATION_ERROR, "Failed to get a file descriptor of the socket.", e);
            }
        }

        return mSocketFD;
    }
}
