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
 * $Id: iloLoadInsert.h 80545 2017-07-19 08:05:23Z daramix $
 **********************************************************************/

/***********************************************************************
 * FILE DESCRIPTION :
 *
 * This file includes the inline functions that break off from
 * the iloLoad::InsertFromBuff function.
 **********************************************************************/

#define PRINT_RECORD(i)                                           \
    m_LogFile.PrintOneRecord(sHandle,                             \
            &m_pTableInfo[sConnIndex],                            \
            aData->mRecordNumber[i],                              \
            i);                                                   \
    m_BadFile.PrintOneRecord(sHandle,                             \
            &m_pTableInfo[sConnIndex],                            \
            i,                                                    \
            m_DataFile.mLOBFile,                                  \
            aData->mOptionUsed);

#define ECODE (uteGetErrorCODE(sErrorMgr))
#define FAIL_BREAK                                                \
    (ECODE == 0x3b032 ||                                          \
     ECODE == 0x5003b || /* buffer full */                        \
     ECODE == 0x51043 || /* ��� ���    */                       \
     ECODE == 0x91044 || /* Data file IO error */                 \
     ECODE == 0x91045)   /* LOB file IO error */

#define ADD_ERROR_COUNT(cnt)                                      \
    aData->mErrorCount += (cnt); /* Fail whole array records. */  \
    mErrorCount += (cnt);        /* BUG-28608 */

inline IDE_RC iloLoad::InitContextData(iloLoadInsertContext *aData)
{
    iloaderHandle *sHandle = aData->mHandle;

    aData->mArrayCount = (m_pProgOption->m_ArrayCount == 0) ?
                         1 : m_pProgOption->m_ArrayCount;
    aData->mLoad       = 1;
    aData->mOptionUsed = ILO_FALSE;       // BUG-24583

    iloMutexLock( sHandle, &(sHandle->mParallel.mLoadInsMutex) );
    /* PROJ-1714
     * sConnIndex�� ���� Thread���� ����ϰԵ� Identifier ���� ������ �Ѵ�.
     * Connection�� TableInfo�� ������ �� �ֵ��� �Ѵ�.
     */
    aData->mConnIndex = mConnIndex++;          
    aData->mLOBColExist = IsLOBColExist( sHandle, &m_pTableInfo[aData->mConnIndex] );
    
    iloMutexUnLock( sHandle, &(sHandle->mParallel.mLoadInsMutex) );

    // BUG-28605
    aData->mErrorMgr = (uteErrorMgr *)idlOS::malloc(
                           aData->mArrayCount * ID_SIZEOF(uteErrorMgr));
    IDE_TEST_RAISE(aData->mErrorMgr == NULL, MAllocError);
    idlOS::memset(aData->mErrorMgr, 0, aData->mArrayCount * ID_SIZEOF(uteErrorMgr));
    m_pISPApiUp[aData->mConnIndex].SetErrorMgr(aData->mArrayCount, aData->mErrorMgr);
  
    // BUG-28675
    aData->mRecordNumber = (SInt *)idlOS::calloc(aData->mArrayCount, ID_SIZEOF(SInt));
    IDE_TEST_RAISE(aData->mRecordNumber == NULL, MAllocError);
    
    /* BUG-24583
     * -lob 'use_separate_files=yes' �ɼ��� ����� ��쿡 
     * Bad File�� FilePath + FileName�� �����Ѵ�.
     */
    if ( aData->mLOBColExist == ILO_TRUE &&
         m_pProgOption->mUseSeparateFiles == ILO_TRUE )
    {
        aData->mOptionUsed = ILO_TRUE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(MAllocError);
    {
        iloMutexLock( sHandle, &(sHandle->mParallel.mLoadFIOMutex) );
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_memory_error,
                    __FILE__, __LINE__);
        
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }
        
        aData->mTotal = 0;
        iloMutexUnLock( sHandle, &(sHandle->mParallel.mLoadFIOMutex) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

inline IDE_RC iloLoad::FiniContextData(iloLoadInsertContext *aData)
{
    SInt           sConnIndex = aData->mConnIndex;
    SInt           sTotal     = aData->mTotal;
    iloaderHandle *sHandle    = aData->mHandle;

    /* BUG-30413 */
    if ( sHandle->mProgOption->mGetTotalRowCount == ILO_TRUE )
    {
        sHandle->mStatisticLog.totalCount = aData->mRecordNumber[aData->mRealCount];
    }

    iloMutexLock( sHandle, &(sHandle->mParallel.mLoadInsMutex) );
    mTotalCount += sTotal;
    mParallelLoad[sConnIndex] = sTotal;
    sHandle->mParallel.mLoadThrNum--;   //BUG-24211
    iloMutexUnLock( sHandle, &(sHandle->mParallel.mLoadInsMutex) );

    // BUG-28605
    if (aData->mErrorMgr != NULL)
    {
        m_pISPApiUp[sConnIndex].SetErrorMgr(sHandle->mNErrorMgr,
                                            sHandle->mErrorMgr);
        idlOS::free(aData->mErrorMgr);
    }

    // BUG-28675
    if (aData->mRecordNumber != NULL)
    {
        idlOS::free(aData->mRecordNumber);
    }

    return IDE_SUCCESS;
}

inline SInt iloLoad::ReadOneRecord(iloLoadInsertContext *aData)
{
    SInt           sRet;
    SInt           sConnIndex = aData->mConnIndex;
    SInt           sRealCount = aData->mRealCount;
    iloaderHandle *sHandle    = aData->mHandle;
    uteErrorMgr   *sErrorMgr  = aData->mErrorMgr;

    aData->mNextAction = NONE;

    iloMutexLock( sHandle, &(sHandle->mParallel.mLoadInsMutex) );

    // BUG-24898 iloader �Ľ̿��� ��ȭ
    // �����޽����� clear �Ѵ�.
    if (uteGetErrorCODE(sHandle->mErrorMgr) != 0x00000)
    {
        uteClearError(sHandle->mErrorMgr);
    } 

    sRet = m_DataFile.ReadOneRecordFromCBuff(sHandle, 
            &m_pTableInfo[sConnIndex],
            sRealCount);

    if (sRet != READ_SUCCESS)
    {
        idlOS::memcpy(sErrorMgr, sHandle->mErrorMgr, ID_SIZEOF(uteErrorMgr));
    }

    // BUG-24879 errors �ɼ� ����
    // recordcount �� ��Ȯ�� ����ϱ� ���ؼ� eof �϶��� count ���� �ʴ´�.
    if (sRet != END_OF_FILE)
    {
        aData->mRecordNumber[sRealCount] = mReadRecCount += 1; //���� Record ��
    }

    iloMutexUnLock( sHandle, &(sHandle->mParallel.mLoadInsMutex) );

    // BUG-28208: malloc � �������� �� iloader �ٷ� ���� 
    if ( (sRet == SYS_ERROR) ||
         (m_pProgOption->m_bExist_L &&
          (aData->mRecordNumber[sRealCount] > m_pProgOption->m_LastRow)) )
    {
        aData->mNextAction = BREAK;
    }

    if (sRet == END_OF_FILE && sRealCount == 0)
    {
        aData->mNextAction = CONTINUE;
    }

    if ( (sRet != END_OF_FILE) &&
         (aData->mRecordNumber[sRealCount] < m_pProgOption->m_FirstRow) )
    {
        aData->mRealCount = 0;
        aData->mNextAction = CONTINUE;
    }
    return sRet;
}

inline IDE_RC iloLoad::LogError(iloLoadInsertContext *aData,
                                SInt                  aArrayIndex)
{
    SInt           sConnIndex = aData->mConnIndex;
    iloaderHandle *sHandle    = aData->mHandle;
    uteErrorMgr   *sErrorMgr  = aData->mErrorMgr;

    iloMutexLock( sHandle, &(sHandle->mParallel.mLoadFIOMutex) );

    ADD_ERROR_COUNT(1);

    // BUG-21640 iloader���� �����޽����� �˾ƺ��� ���ϰ� ����ϱ�
    // log ���Ͽ� rowcount �� ����Ѵ�.
    PRINT_RECORD(aArrayIndex);

    // BUG-21640 iloader���� �����޽����� �˾ƺ��� ���ϰ� ����ϱ�
    // ���� �����޽����� ������ �������� ����ϴ� �Լ��߰�   
    m_LogFile.PrintLogErr(sErrorMgr);

    ILO_CALLBACK;

    iloMutexUnLock( sHandle, &(sHandle->mParallel.mLoadFIOMutex) );

    return IDE_SUCCESS;
}

inline void iloLoad::Sleep(SInt aLoad)
{
    PDL_Time_Value sSleepTime;

    if ((m_pProgOption->mExistWaitTime == SQL_TRUE) &&
            (m_pProgOption->mExistWaitCycle == SQL_TRUE))
    {
        if ( (aLoad % m_pProgOption->mWaitCycle) == 0)
        {
            sSleepTime.initialize(0, m_pProgOption->mWaitTime * 1000);
            idlOS::sleep( sSleepTime );
        }
    }
}

inline IDE_RC iloLoad::Callback4Api(iloLoadInsertContext *aData)
{
    SInt           sRealCount = aData->mRealCount;
    iloaderHandle *sHandle    = aData->mHandle;

    iloMutexLock( sHandle, &(sHandle->mParallel.mLoadFIOMutex) );

    /*
     * BUG-32910
     * mStopIloader flag should be checked
     * before invoking callback in iloader
     */
    if ( sHandle->mProgOption->mStopIloader == ILO_TRUE )
    {
        IDE_RAISE(fail_callback);
    }

    /* BUG-30413
     * mCBFrequencyCnt ���� arrayCount �� ���� �� �Ͽ� �ݹ��� ȣ�� �Ѵ�.  
     */
    if ( sRealCount == aData->mArrayCount ) 
    {
        mSetCallbackLoadCnt = mSetCallbackLoadCnt + sRealCount;
    }
    if (( sHandle->mProgOption->mSetRowFrequency != 0 ) &&
            ( mCBFrequencyCnt <= mSetCallbackLoadCnt )) 
    {
        mCBFrequencyCnt +=  sHandle->mProgOption->mSetRowFrequency;

        sHandle->mStatisticLog.loadCount  = mSetCallbackLoadCnt; 
        sHandle->mStatisticLog.errorCount = mErrorCount;

        if ( sHandle->mLogCallback( ILO_STATISTIC_LOG, &sHandle->mStatisticLog )
                != ALTIBASE_ILO_SUCCESS )
        { 
            sHandle->mProgOption->mStopIloader = ILO_TRUE;
        }
    }

    iloMutexUnLock( sHandle, &(sHandle->mParallel.mLoadFIOMutex) );

    return IDE_SUCCESS;

    IDE_EXCEPTION(fail_callback);
    {
        iloMutexUnLock( sHandle, &(sHandle->mParallel.mLoadFIOMutex) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

inline IDE_RC iloLoad::SetParamInfo(iloLoadInsertContext *aData)
{
    SInt           sConnIndex = aData->mConnIndex;
    iloaderHandle *sHandle    = aData->mHandle;
    uteErrorMgr   *sErrorMgr  = aData->mErrorMgr;

    aData->mNextAction = NONE;

    IDE_TEST(m_pISPApiUp[sConnIndex].SetColwiseParamBind( sHandle,
                (UInt)aData->mRealCount,
                m_pTableInfo[sConnIndex].mStatusPtr)
             != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    iloMutexLock( sHandle, &(sHandle->mParallel.mLoadFIOMutex) );

    ADD_ERROR_COUNT(aData->mRealCount);

    if (FAIL_BREAK)
    {
        PRINT_ERROR_CODE(sErrorMgr);
        aData->mNextAction = BREAK;
    }
    else
    {
        aData->mNextAction = CONTINUE;
    }

    ILO_CALLBACK;

    m_LogFile.PrintLogErr(sErrorMgr);
    iloMutexUnLock( sHandle, &(sHandle->mParallel.mLoadFIOMutex) );

    return IDE_FAILURE;
}

//INSERT SQL EXECUTE
inline IDE_RC iloLoad::ExecuteInsertSQL(iloLoadInsertContext *aData)
{
    SInt           i;
    SInt           j;
    SInt           sConnIndex = aData->mConnIndex;
    SInt           sRealCount = aData->mRealCount;
    iloaderHandle *sHandle    = aData->mHandle;
    uteErrorMgr   *sErrorMgr  = aData->mErrorMgr;
    SQLUSMALLINT  *sStatusPtr = NULL;
    SChar          szMsg[4096];

    aData->mNextAction = NONE;

    IDE_TEST(m_pISPApiUp[sConnIndex].Execute() == SQL_FALSE);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    iloMutexLock( sHandle, &(sHandle->mParallel.mLoadFIOMutex) );

    if ( sRealCount == 1 )
    {
        ADD_ERROR_COUNT(sRealCount);

        if (FAIL_BREAK)
        {
            PRINT_ERROR_CODE(sErrorMgr);
            aData->mNextAction = BREAK;
        }
        else
        {
            PRINT_RECORD(0);
            aData->mNextAction = CONTINUE;
        }

        ILO_CALLBACK;

        m_LogFile.PrintLogErr(sErrorMgr);
    }
    else if ( sRealCount > 1 )
    {
        if (FAIL_BREAK)
        {
            ADD_ERROR_COUNT(sRealCount);

            PRINT_ERROR_CODE(sErrorMgr);
            aData->mNextAction = BREAK;
        }
        else
        {
            sStatusPtr = m_pTableInfo[sConnIndex].mStatusPtr;
            for (i = j = 0; i < sRealCount; i++)
            {
                if (sStatusPtr[i] != SQL_PARAM_SUCCESS)
                {
                    ADD_ERROR_COUNT(1);

                    // array ������ ����Ҷ� ��Ȯ�� rowcount �� ����ϱ����ؼ�
                    // array �� ������ ���� +1 �Ѵ�.
                    PRINT_RECORD(i);

                    if (sStatusPtr[i] == SQL_PARAM_ERROR)
                    {
                        if (uteGetErrorCODE(&sErrorMgr[j]) != 0x00000)
                        {
                            m_LogFile.PrintLogErr(&sErrorMgr[j]);
                        }
                        else
                        {
                            m_LogFile.PrintLogErr(&sErrorMgr[0]);
                        }
                        j++;
                    }
                    else if (sStatusPtr[i] == SQL_PARAM_UNUSED)
                    {
                        // BUGBUG : when happen?
                        idlOS::sprintf(szMsg, "Insert Error : unused\n");
                        m_LogFile.PrintLogMsg(szMsg);
                    }
                }
            }
            aData->mNextAction = CONTINUE;
        }
    }
    iloMutexUnLock( sHandle, &(sHandle->mParallel.mLoadFIOMutex) );

    return IDE_FAILURE;
}

inline IDE_RC iloLoad::ExecuteInsertLOB(iloLoadInsertContext *aData)
{
    IDE_RC         sRC;
    SInt           i;
    SInt           sConnIndex = aData->mConnIndex;
    SInt           sRealCount = aData->mRealCount;
    iloaderHandle *sHandle    = aData->mHandle;
    uteErrorMgr   *sErrorMgr  = aData->mErrorMgr;

    aData->mNextAction = NONE;

    if (aData->mLOBColExist == ILO_TRUE)
    {
        for (i = 0; i < sRealCount; i++)
        {
            /* LOB �����͸� ���Ϸκ��� �о� ������ ����. */
            iloMutexLock( sHandle, &(sHandle->mParallel.mLoadLOBMutex) );
            sRC = m_DataFile.LoadOneRecordLOBCols( sHandle,
                    aData->mRecordNumber[i],
                    &m_pTableInfo[sConnIndex], 
                    i,
                    &m_pISPApiUp[sConnIndex]);
            iloMutexUnLock( sHandle, &(sHandle->mParallel.mLoadLOBMutex) );

            if (sRC != IDE_SUCCESS)
            {
                /* ������ �߻��� ���ڵ��� ������ ����Ѵ�.
                 * m_ArrayCount�� m_CommitUnit�� 1�̶��
                 * ������ ��� �ִ�. */

                (void)m_pISPApiUp[sConnIndex].EndTran(ILO_FALSE);

                iloMutexLock( sHandle, &(sHandle->mParallel.mLoadFIOMutex) );

                if (FAIL_BREAK)
                {
                    aData->mNextAction = BREAK;

                    ADD_ERROR_COUNT(sRealCount - i);

                    ILO_CALLBACK;

                    m_LogFile.PrintLogErr(sErrorMgr);
                    //goto AFTER_MAIN_LOOP_LABEL;       //!!! ó���ؾ���
                    iloMutexUnLock( sHandle, &(sHandle->mParallel.mLoadFIOMutex) );

                    break;
                }
                else
                {
                    ADD_ERROR_COUNT(1);
                    PRINT_RECORD(i);

                    ILO_CALLBACK;

                    m_LogFile.PrintLogErr(sErrorMgr);
                    iloMutexUnLock( sHandle, &(sHandle->mParallel.mLoadFIOMutex) );
                }
            }
        }
    }
    return IDE_SUCCESS;
}

inline IDE_RC iloLoad::Commit4Dryrun(iloLoadInsertContext *aData)
{
    aData->mPrevCommitRecCnt = aData->mTotal - aData->mErrorCount;

    return IDE_SUCCESS;
}

inline IDE_RC iloLoad::Commit(iloLoadInsertContext *aData)
{
    SInt           sConnIndex = aData->mConnIndex;
    SInt           sDiffErrorCount = 0; // BUG-28608
    iloaderHandle *sHandle    = aData->mHandle;
    uteErrorMgr   *sErrorMgr  = aData->mErrorMgr;

    IDE_TEST_RAISE( m_pISPApiUp[sConnIndex].EndTran(ILO_TRUE) != IDE_SUCCESS,
                    fail_commit );

    aData->mPrevCommitRecCnt = aData->mTotal - aData->mErrorCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION(fail_commit);
    {
        iloMutexLock( sHandle, &(sHandle->mParallel.mLoadFIOMutex) );

        ILO_CALLBACK;

        m_LogFile.PrintLogErr(aData->mErrorMgr);
        (void)m_pISPApiUp[sConnIndex].EndTran(ILO_FALSE);

        // BUG-28608
        sDiffErrorCount = aData->mTotal - aData->mErrorCount - aData->mPrevCommitRecCnt;
        aData->mErrorCount += sDiffErrorCount;
        mErrorCount += sDiffErrorCount;

        iloMutexUnLock( sHandle, &(sHandle->mParallel.mLoadFIOMutex) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
