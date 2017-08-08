/***********************************************************************
 * Copyright 1999-2000, RTBase Corporation or its subsidiaries.
 * All rights reserved.
 **********************************************************************/

/***********************************************************************
 * $Id: ideErrorMgr.h 76919 2016-08-30 06:46:19Z jake.jang $
 **********************************************************************/

/***********************************************************************
 *    File Name         :    ideErrorMgr.h
 *
 *  Author            :    Sung-Jin, Kim
 *
 *    Related Files    :
 *
 *    Description        :    ID ���� ó�� ��� �쵢ȭ��
 *                      !! �����޽��� ��ƾ�� ���� (�����ڵ� ������ ���)
 *
 **********************************************************************/

#ifndef _O_IDEERRORMGR_H_
#define _O_IDEERRORMGR_H_ 1

#include <idl.h>
#include <ideFaultMgr.h> /* PROJ-2617 */

/* ------------------------------------------------------------
 *                  ���� �ڵ��� ����
 *
 *      [MODULE]ERR_[ACTION]_[NamingSpace]_[Description]
 *          1           2          3            4
 *
 *  [MODULE] :  sm | qp | cm
 *  [ACTION] :  FATAL | ABORT | IGNORE
 *  [NamingSpace] : qex | qpf | smd | smc ...
 *  [Description] : FileWriteError | ....
 *
 *  Ex) idERR_FATAL_smd_InvalidOidAddress
 * -----------------------------------------------------------*/

#define E_MODULE_MASK    0xF0000000   // CM, QP, SM
#define E_ACTION_MASK    0x0F000000   // FATAL, ABORT, IGNORE

#define E_ERRORCODE_MASK 0xFFFFF000   // real error code mask
#define E_SUBCODE_MASK   0x00FFF000   // subcode mask
#define E_INDEX_MASK     0x00000FFF   // internal index area

#define E_ERROR_CODE(a)  ( ((a) & E_ERRORCODE_MASK) >> 12) // get real error code

// PROJ-1335 user-defined error
// real error code -> server error code
#define E_SERVER_ERROR(a)   ( ( (a) << 12 ) & E_ERRORCODE_MASK )

// PROJ-1477 recover original error code for client error manager
#define E_RECOVER_ERROR(a)   ( ( (a) << 12 ) )

// check errorcode is user-defined ( 990000 ~ 991000 )
#define E_CHECK_UDE(a)      ( ( ( (a) >= 0x000F1B30 ) && ( (a) <= 0x000F1F18 ) ) \
                              ? ID_TRUE : ID_FALSE )

#define E_MESSAGE_MASK   0x00FFFFFF   // real Error message  // useless...

#define E_MODULE_COUNT   16           // ��� ����
#define E_MODULE_ID      0x00000000
#define E_MODULE_SM      0x10000000
#define E_MODULE_MT      0x20000000
#define E_MODULE_QP      0x30000000
#define E_MODULE_MM      0x40000000
#define E_MODULE_RP      0x60000000
#define E_MODULE_CM      0x70000000

// PROJ-1335 user-defined error
#define E_MODULE_USER    0xF0000000   // user-defined error

#define E_ACTION_FATAL   0x00000000
#define E_ACTION_ABORT   0x01000000
#define E_ACTION_IGNORE  0x02000000
#define E_ACTION_RETRY   0x03000000
#define E_ACTION_REBUILD 0x04000000

#define IDE_MAX_ERROR_ACTION  5

// PROJ-2118 BUG Reporting
#define IDE_ERROR_VALIDATION_LEVEL_ABORT  (0)
#define IDE_ERROR_VALIDATION_LEVEL_FATAL  (1)

/* ----------------------------------------------------------------------
 *
 *  ���� �������� ó���� ���� ����Ÿ ����
 *
 * ----------------------------------------------------------------------*/

#define MAX_ARGUMENT  32  // ���� �޽��� ������ �ִ� �������� ����

typedef enum
{
    IDE_ERR_NONE  = 0,
    IDE_ERR_SCHAR,
    IDE_ERR_STRING,
    IDE_ERR_SINT,
    IDE_ERR_UINT,
    IDE_ERR_SLONG,
    IDE_ERR_ULONG,
    IDE_ERR_VSLONG,
    IDE_ERR_VULONG,
    IDE_ERR_HEX32,
    IDE_ERR_HEX64,
    IDE_ERR_HEX_V
} IDE_ERR_DATATYPE;

typedef struct
{
    IDE_ERR_DATATYPE    type;
    const SChar       * tmpSpecStr; // ���� �޽������� ����
    const SChar       * realSpecStr; // ���� printf���� ����
    SChar               len;
} ideErrTypeInfo;

typedef struct ideArgInfo
{
    ideErrTypeInfo *type_info;
    ideArgInfo         *outputOrder; // ����� ���� ���� ����Ŵ
    union
    {
        SChar *string;
        SChar  schar;
        UChar  uchar;
        SInt   sint;
        UInt   uint;
        SLong  slong;
        ULong  ulong;
        vSLong vslong;
        vULong vulong;
        UInt   hex32;
        ULong  hex64;
        vULong hexv;
    } data;
} ideArgInfo;


/* ----------------------------------------------------------------------
 *
 *  SM ���� �ڵ� ����Ÿ ���� �� ��������
 *
 * ----------------------------------------------------------------------*/

#define MAX_ERROR_STACK      64
#define MAX_ERROR_FILE_LEN   256
#define MAX_ERROR_TEST_LEN   1024
// NOTICE!!
// Becuase stored procedure stored error message to a varchar variable,
// MAX_ERROR_MSGLEN should not be larger than varchar size (4096)
#define MAX_ERROR_MSG_LEN    2048
#define MAX_FORMAT_ITEM_LEN   128

/*
 * Project 2514
 * Seperate the data to be used in IDE_PUSH/IDE_POP
 */
struct ideErrorMgrStack
{
    UInt   LastError;
    SInt   LastSystemErrno;
    SChar  LastErrorMsg[MAX_ERROR_MSG_LEN+256];
    SChar  LastErrorFile[MAX_ERROR_FILE_LEN];
    UInt   LastErrorLine;
    idBool HasErrorPosition; // BUG-38883
};

struct ideErrorMgr
{
    ideErrorMgrStack Stack;

    SInt   TestIndex;
    SChar  TestLine[MAX_ERROR_STACK][MAX_ERROR_MSG_LEN * 2];
    SInt   TestLength[MAX_ERROR_STACK];

#ifdef DEBUG
    UInt   ErrorIndex;
    UInt   ErrorTested;
    SChar  ErrorFile[MAX_ERROR_STACK][MAX_ERROR_FILE_LEN];
    UInt   ErrorLine[MAX_ERROR_STACK];
    SChar  ErrorTestLine[MAX_ERROR_STACK][MAX_ERROR_TEST_LEN];
#endif

    /* PROJ-2617 */
    ideFaultMgr mFaultMgr;
};

typedef struct
{
    UInt  AltiVersionId; // MSB���Ͽ� ��Ƽ���̽� ���� ��� (2003/10/31, by kumdory)
    UInt  Section;    // ���� ���� ��ȣ
    ULong ErrorCount; // ���� �޽��� ����, �÷����� ������ align���������� ErrorCount�� Section�ڷ� �ű�
} MsbHeader;

// ���� : genErrMsg�� ���� ������ *.msb ȭ���� ��� ����
typedef struct
{
    union
    {
        UChar dummy[256];
        MsbHeader header;
    } value_;

} idErrorMsbType;

// formatted string ����
typedef struct
{
    idErrorMsbType MsbHeader;
    SChar **MsgBuf;
} ideErrorFactory;


//# endif /* SERVER COMPILE */

/* ----------------------------------------------------------------------
 *  ID ���� �ڵ� Get, Set
 * ----------------------------------------------------------------------*/

/* Project 2514 - Log IDE_TEST failures */
IDL_EXTERN_C void ideLogTest(SChar* aFile, SInt aLine, SChar* aTest);
IDL_EXTERN_C void ideLogError(const SChar* aErrInfo,
                              const idBool aAcceptFaultTolerance, /* PROJ-2617 */
                              const SChar* aFile,
                              const SInt   aLine,
                              const SChar* aMsg, ...);
void   ideTestFlush(void);

/* TASK-6739 Altibase 710 ���� ���� */
IDL_EXTERN_C IDTHREAD ideErrorMgr gIdeErrorMgr;

/* TASK-6739 Altibase 710 ���� ���� */
inline struct ideErrorMgr* ideGetErrorMgr()
{
    return &gIdeErrorMgr;
}

struct  ideFaultMgr* ideGetFaultMgr();

UInt   ideGetErrorCode();
SChar *ideGetErrorMsg();
UInt   ideGetErrorLine();

// BUG-38883
idBool ideHasErrorPosition();
void   ideSetHasErrorPosition();
void   ideInitHasErrorPosition();

SChar *ideGetErrorFile();
SInt   ideGetSystemErrno();
SInt   ideFindErrorCode(UInt ErrorCode);
SInt   ideFindErrorAction(UInt ErrorAction);

SInt   ideIsAbort();
SInt   ideIsFatal();
SInt   ideIsIgnore();
SInt   ideIsRetry();
SInt   ideIsRebuild();

// ���� �ڵ� ����
ideErrorMgr* ideSetErrorCode(UInt ErrorCode, ...);

#ifdef ALTIBASE_FIT_CHECK
ideErrorMgr* ideFitSetErrorCode( UInt aErrorCode, va_list aArgs );
#endif

#ifdef DEBUG
SInt   ideSetDebugInfo(SChar *File, UInt Line, SChar *testline);
#endif

// PROJ-1335 errorcode, errmsg ���� ����
ideErrorMgr* ideSetErrorCodeAndMsg( UInt   ErrorCode,
                                    SChar* ErrorMsg );

/* PROJ-2617 */
void ideCopyErrorInfo(ideErrorMgr       *aDestErrorMgr,
                      const ideErrorMgr *aSrcErrorMgr);

IDL_EXTERN_C void ideClearError(); // �ε��� �ʱ�ȭ
SInt idePopErrorCode();
void ideDump();

/* ------------------------------------------------------------------
 * ���� �޽��� ó�� (MSB)
 * ----------------------------------------------------------------*/

SInt    ideRegistErrorMsb(SChar *fn);
SInt    ideFreeErrorMsb(SChar *fn);
SChar  *ideGetErrorMsg(UInt ErrorCode);

// BUG-15166
UInt   ideGetErrorArgCount(UInt ErrorCode);

/* ----------------------------------------------------------------------
 *
 *  IDE ���� Handling ��ũ��
 *
 * ----------------------------------------------------------------------*/

#define IDE_CLEAR() ideClearError()

/******************************************************************************
 * PROJ-2514
 * New features of error manager
 * With property turned on, every IDE_TEST failures will be recorded
 * PROJ-2617
 * FATAL�� �غ��ϱ� ���� IDE_FT_BEGIN(), IDE_FT_END(), IDE_FT_END_FAILURE(), ��
 * ��ũ�� ���. �ڼ��� ������ 'http://nok.altibase.com/x/LGEUAg' ������ �ּ���.
 *****************************************************************************/

/**
 * FATAL �غ� ������ �����ϱ� ���� IDE_FT_BEGIN() ��ũ�� ���.
 * �� ���� �������� Ư�� UNIX signal �߻��� FATAL �غ� �����ϴ�.
 * Signal handler���� setjmp() �Լ��� non-local jump �ǰ� ��ȯ ���� 1��
 * ��ȯ�ȴ�. ����, IDE_EXCEPTION_SIGNAL_LABEL label�� local jump �Ǿ�
 * ����ó�� �ڵ带 �����ϰ� �ȴ�. ����, FT call-stack�� overflow �߻��Ѵٸ�,
 * IDE_FT_ROOT_BEGIN()�� �����ϱ� ������ FATAL �غ� �Ұ����ϴ�.
 */
#ifdef DEBUG

#define IDE_FT_BEGIN()                                                  \
    do {                                                                \
        ideErrorMgr *sErrorMgr = ideGetErrorMgr();                      \
        if ((sErrorMgr->mFaultMgr.mIsTransientDisable == ID_FALSE) &&   \
            (sErrorMgr->mFaultMgr.mIsCallStackException == ID_FALSE)) { \
            if ( IDL_LIKELY_TRUE( sErrorMgr->mFaultMgr.mCallStackNext < IDE_FT_MAX_CALLSTACK_SIZE )) { \
                IDE_FT_TRACE_MACRO(sErrorMgr->mFaultMgr.mCallStackNext + 1, \
                                   "IDE_FT_BEGIN()");                   \
                sErrorMgr->mFaultMgr.mCallStack[sErrorMgr->mFaultMgr.mCallStackNext].mEntryFuncName = ID_FUNCTION; \
                if ( IDL_LIKELY_FALSE( setjmp(sErrorMgr->mFaultMgr.mCallStack[sErrorMgr->mFaultMgr.mCallStackNext++].mJmpBuf) != 0)) { \
                    (void)idlOS::pthread_sigmask(SIG_SETMASK, &sErrorMgr->mFaultMgr.mSavedSigMask, NULL); \
                    sErrorMgr->mFaultMgr.mIsTransientDisable = ID_TRUE; \
                    sErrorMgr->mFaultMgr.mDisabledEntryFuncName = NULL; \
                    sErrorMgr->mFaultMgr.mDisabledCallCount = 0;        \
                    goto IDE_EXCEPTION_SIGNAL_LABEL;                    \
                }                                                       \
            }                                                           \
            else {                                                      \
                IDE_FT_TRACE_MACRO(sErrorMgr->mFaultMgr.mCallStackNext + 1, \
                                   "CallStack Exception!");             \
                sErrorMgr->mFaultMgr.mIsCallStackException = ID_TRUE;   \
                sErrorMgr->mFaultMgr.mCallStackNext = 0;                \
            }                                                           \
        }                                                               \
    } while (0)

#else /* DEBUG */

#define IDE_FT_BEGIN()                                                  \
    do {                                                                \
        ideErrorMgr *sErrorMgr = ideGetErrorMgr();                      \
        if ((sErrorMgr->mFaultMgr.mIsTransientDisable == ID_FALSE) &&   \
            (sErrorMgr->mFaultMgr.mIsCallStackException == ID_FALSE)) { \
            if ( IDL_LIKELY_TRUE( sErrorMgr->mFaultMgr.mCallStackNext < IDE_FT_MAX_CALLSTACK_SIZE )) { \
                sErrorMgr->mFaultMgr.mCallStack[sErrorMgr->mFaultMgr.mCallStackNext].mEntryFuncName = ID_FUNCTION; \
                if ( IDL_LIKELY_FALSE( setjmp(sErrorMgr->mFaultMgr.mCallStack[sErrorMgr->mFaultMgr.mCallStackNext++].mJmpBuf )) != 0) { \
                    (void)idlOS::pthread_sigmask(SIG_SETMASK, &sErrorMgr->mFaultMgr.mSavedSigMask, NULL); \
                    sErrorMgr->mFaultMgr.mIsTransientDisable = ID_TRUE; \
                    sErrorMgr->mFaultMgr.mDisabledEntryFuncName = NULL; \
                    sErrorMgr->mFaultMgr.mDisabledCallCount = 0;        \
                    goto IDE_EXCEPTION_SIGNAL_LABEL;                    \
                }                                                       \
            }                                                           \
            else {                                                      \
                sErrorMgr->mFaultMgr.mIsCallStackException = ID_TRUE;   \
                sErrorMgr->mFaultMgr.mCallStackNext = 0;                \
            }                                                           \
        }                                                               \
    } while (0)

#endif /* DEBUG */

/**
 * FATAL �غ� ������ �����ϱ� ���� IDE_FT_END() ��ũ�� ���.
 * �� ��ũ�� ������ FATAL �غ� ���δ� caller�� �����Ѵ�. ����, FT call-stack��
 * underflow �߻��Ѵٸ�, IDE_FT_ROOT_BEGIN()�� �����ϱ� ������ FATAL �غ�
 * �Ұ����ϴ�.
 */
#ifdef DEBUG

#define IDE_FT_END()                                                    \
    do {                                                                \
        ideErrorMgr *sErrorMgr = ideGetErrorMgr();                      \
        if ( IDL_LIKELY_FALSE( sErrorMgr->mFaultMgr.mDummyFlag == 1 )) { /* prevent code optimization */ \
            goto IDE_EXCEPTION_SIGNAL_LABEL;    /* never jump */        \
        }                                                               \
        if ((sErrorMgr->mFaultMgr.mIsTransientDisable == ID_FALSE) &&   \
            (sErrorMgr->mFaultMgr.mIsCallStackException == ID_FALSE)) { \
            IDE_FT_TRACE_MACRO(sErrorMgr->mFaultMgr.mCallStackNext,     \
                               "IDE_FT_END()");                         \
            sErrorMgr->mFaultMgr.mDisabledEntryFuncName = NULL;         \
            sErrorMgr->mFaultMgr.mDisabledCallCount = 0;                \
            sErrorMgr->mFaultMgr.mCallStackNext--;                      \
            if (sErrorMgr->mFaultMgr.mCallStackNext < 0) {              \
                sErrorMgr->mFaultMgr.mIsCallStackException = ID_TRUE;   \
                sErrorMgr->mFaultMgr.mCallStackNext = 0;                \
            }                                                           \
        }                                                               \
    } while (0)

#else /* DEBUG */

#define IDE_FT_END()                                                    \
    do {                                                                \
        ideErrorMgr *sErrorMgr = ideGetErrorMgr();                      \
        if ( IDL_LIKELY_FALSE( sErrorMgr->mFaultMgr.mDummyFlag == 1 )) { /* prevent code optimization */ \
            goto IDE_EXCEPTION_SIGNAL_LABEL;    /* never jump */        \
        }                                                               \
        if ((sErrorMgr->mFaultMgr.mIsTransientDisable == ID_FALSE) &&   \
            (sErrorMgr->mFaultMgr.mIsCallStackException == ID_FALSE)) { \
            sErrorMgr->mFaultMgr.mDisabledEntryFuncName = NULL;         \
            sErrorMgr->mFaultMgr.mDisabledCallCount = 0;                \
            sErrorMgr->mFaultMgr.mCallStackNext--;                      \
            if (sErrorMgr->mFaultMgr.mCallStackNext < 0) {              \
                sErrorMgr->mFaultMgr.mIsCallStackException = ID_TRUE;   \
                sErrorMgr->mFaultMgr.mCallStackNext = 0;                \
            }                                                           \
        }                                                               \
    } while (0)

#endif /* DEBUG */

/**
 * Exception section���� IDE_FT_BEGIN()�� ���� ����� IDE_FT_END_FAILURE()
 * ��ũ�� ���. FATAL �غ� �������� exception section���� jump�� ���� �ǵ����
 * FATAL �غ���Ű�� �ʴµ� �̸� �����Ѵ�. IDE_FT_END()�� ���������� ����
 * FATAL �غ����δ� caller�� �����Ѵ�.
 */
#ifdef DEBUG

#define IDE_FT_END_FAILURE()                                            \
    do {                                                                \
        ideErrorMgr *sErrorMgr = ideGetErrorMgr();                      \
        if ( IDL_LIKELY_FALSE( sErrorMgr->mFaultMgr.mDummyFlag == 1 ) ) {  /* prevent code optimization */ \
            goto IDE_EXCEPTION_SIGNAL_LABEL;    /* never jump */        \
        }                                                               \
        if ((sErrorMgr->mFaultMgr.mIsTransientDisable == ID_TRUE) &&    \
            (sErrorMgr->mFaultMgr.mDisabledEntryFuncName == NULL)) {    \
            if (sErrorMgr->mFaultMgr.mIsCallStackException == ID_FALSE) { \
                IDE_FT_TRACE_MACRO(sErrorMgr->mFaultMgr.mCallStackNext, \
                                   "IDE_FT_END_FAILURE()");             \
                sErrorMgr->mFaultMgr.mCallStackNext--;                  \
                if (sErrorMgr->mFaultMgr.mCallStackNext < 0) {          \
                    sErrorMgr->mFaultMgr.mIsCallStackException = ID_TRUE; \
                    sErrorMgr->mFaultMgr.mCallStackNext = 0;            \
                }                                                       \
            }                                                           \
            sErrorMgr->mFaultMgr.mIsTransientDisable = ID_FALSE;        \
            sErrorMgr->mFaultMgr.mDisabledEntryFuncName = NULL;         \
            sErrorMgr->mFaultMgr.mDisabledCallCount = 0;                \
        }                                                               \
    } while (0)

#else /* DEBUG */

#define IDE_FT_END_FAILURE()                                            \
    do {                                                                \
        ideErrorMgr *sErrorMgr = ideGetErrorMgr();                      \
        if ( IDL_LIKELY_FALSE( sErrorMgr->mFaultMgr.mDummyFlag == 1 ) ) {  /* prevent code optimization */ \
            goto IDE_EXCEPTION_SIGNAL_LABEL;    /* never jump */        \
        }                                                               \
        if ((sErrorMgr->mFaultMgr.mIsTransientDisable == ID_TRUE) &&    \
            (sErrorMgr->mFaultMgr.mDisabledEntryFuncName == NULL)) {    \
            if (sErrorMgr->mFaultMgr.mIsCallStackException == ID_FALSE) { \
                sErrorMgr->mFaultMgr.mCallStackNext--;                  \
                if (sErrorMgr->mFaultMgr.mCallStackNext < 0) {          \
                    sErrorMgr->mFaultMgr.mIsCallStackException = ID_TRUE; \
                    sErrorMgr->mFaultMgr.mCallStackNext = 0;            \
                }                                                       \
            }                                                           \
            sErrorMgr->mFaultMgr.mIsTransientDisable = ID_FALSE;        \
            sErrorMgr->mFaultMgr.mDisabledEntryFuncName = NULL;         \
            sErrorMgr->mFaultMgr.mDisabledCallCount = 0;                \
        }                                                               \
    } while (0)

#endif /* DEBUG */

/**
 * FATAL �غ� ������ �����ϱ� ��, call root���� IDE_FT_ROOT_BEGIN() ��ũ�� ���.
 * struct ideFaultMgr �ڷᱸ���� �ʱ�ȭ�Ѵ�. ����, FT call-stack�� overflow �Ǵ�
 * underflow �߻��� ���ο� call path�� ���� FATAL �غ��� �� �ֵ��� �ʱ�ȭ���ش�.
 */
#ifdef DEBUG

#define IDE_FT_ROOT_BEGIN()                                             \
    do {                                                                \
        ideErrorMgr *sErrorMgr = ideGetErrorMgr();                      \
        IDE_FT_TRACE_MACRO(sErrorMgr->mFaultMgr.mCallStackNext,         \
                           "IDE_FT_ROOT_BEGIN()");                      \
        ideClearFTCallStack();                                          \
    } while (0)

#else /* DEBUG */

#define IDE_FT_ROOT_BEGIN()                                             \
    do {                                                                \
        ideClearFTCallStack();                                          \
    } while (0)

#endif /* DEBUG */

/**
 * Call root���� IDE_FT_ROOT_BEGIN() ����Ͽ� IDE_FT_ROOT_END() ��ũ�� ���.
 * ����, FT call-stack�� overflow �Ǵ� underflow �߻��� debug ��忡��
 * assertion �߻���Ų��.
 */
#ifdef DEBUG

#define IDE_FT_ROOT_END()                                               \
    do {                                                                \
        ideErrorMgr *sErrorMgr = ideGetErrorMgr();                      \
        IDE_FT_TRACE_MACRO(sErrorMgr->mFaultMgr.mCallStackNext,         \
                           "IDE_FT_ROOT_END()");                        \
        IDE_DASSERT(sErrorMgr->mFaultMgr.mCallStackNext == 0);          \
        IDE_DASSERT(sErrorMgr->mFaultMgr.mIsTransientDisable == ID_FALSE); \
        IDE_DASSERT(sErrorMgr->mFaultMgr.mDisabledEntryFuncName == NULL); \
        IDE_DASSERT(sErrorMgr->mFaultMgr.mDisabledCallCount == 0);      \
    } while (0)

#else /* DEBUG */

#define IDE_FT_ROOT_END() do { } while (0)

#endif /* DEBUG */

/**
 * FT macro�� �Բ� trace logging ���� IDE_FT_TRACE() ��ũ�� ���.
 * Runtime �󿡼� FT macro ���࿡ ���� �������� �м��� �� �ִ� ������ �����Ѵ�.
 * ��, __FAULT_TOLERANCE_TRACE property ���� 1 �̰� debug mode������ �����Ѵ�.
 */
#ifdef DEBUG

#define IDE_FT_TRACE(format, ...)                                       \
    do {                                                                \
        if (gIdeFTTrace != 0) {                                         \
            ideLogFT(ID_FUNCTION,                                       \
                     __FILE__, __LINE__,                                \
                     format, ##__VA_ARGS__);                            \
        }                                                               \
    } while (0)

#else /* DEBUG */

#define IDE_FT_TRACE(format, ...) do { } while (0)

#endif /* DEBUG */

/**
 * FT macro ���ο��� ���Ǵ� IDE_FT_TRACE_MACRO() ��ũ��.
 */
#ifdef DEBUG

#define IDE_FT_TRACE_MACRO(aCallLevel, aFTMacro)                        \
    do {                                                                \
        if (gIdeFTTrace != 0) {                                         \
            ideLogFTMacro(ID_FUNCTION,                                  \
                          __FILE__, __LINE__,                           \
                          aCallLevel, aFTMacro);                        \
        }                                                               \
    } while (0)

#else /* DEBUG */

#define IDE_FT_TRACE_MACRO(aCallLevel, aFTMacro) do { } while (0)

#endif /* DEBUG */

#ifdef DEBUG
#define IDE_RAISE( b ) {                                                \
    ideErrorMgr *sErrorMgr = ideGetErrorMgr();                          \
    if( sErrorMgr->ErrorIndex < MAX_ERROR_STACK ){                      \
        idlOS::snprintf( sErrorMgr->ErrorTestLine[sErrorMgr->ErrorIndex], MAX_ERROR_TEST_LEN, "IDE_RAISE( %s );", #b ); \
            idlOS::snprintf( sErrorMgr->ErrorFile[sErrorMgr->ErrorIndex], MAX_ERROR_FILE_LEN, "%s", idlVA::basename(__FILE__) ); \
            sErrorMgr->ErrorLine[sErrorMgr->ErrorIndex] = __LINE__;     \
            sErrorMgr->ErrorTested++;                                   \
    }                                                                   \
    goto b;                                                             \
}
#else
#define IDE_RAISE( b ) {                                                \
    goto b;                                                             \
}
#endif

/* TASK-6739 Altibase 710 ���� ���� */
#ifdef DEBUG
#define IDE_FT_RAISE( b ) {                                             \
    ideErrorMgr *sErrorMgr = ideGetErrorMgr();                          \
    if( sErrorMgr->ErrorIndex < MAX_ERROR_STACK ){                      \
        idlOS::snprintf( sErrorMgr->ErrorTestLine[sErrorMgr->ErrorIndex], MAX_ERROR_TEST_LEN, "IDE_FT_RAISE( %s );", #b ); \
            idlOS::snprintf( sErrorMgr->ErrorFile[sErrorMgr->ErrorIndex], MAX_ERROR_FILE_LEN, "%s", idlVA::basename(__FILE__) ); \
            sErrorMgr->ErrorLine[sErrorMgr->ErrorIndex] = __LINE__;     \
            sErrorMgr->ErrorTested++;                                   \
    }                                                                   \
    if ((sErrorMgr->mFaultMgr.mCallStackNext > 0) &&                    \
        (sErrorMgr->mFaultMgr.mCallStack[sErrorMgr->mFaultMgr.mCallStackNext - 1].mEntryFuncName == ID_FUNCTION)) { \
        /* in FT range on same function */                              \
        IDE_FT_TRACE_MACRO(sErrorMgr->mFaultMgr.mCallStackNext,         \
                           "IDE_FT_RAISE()");                           \
        sErrorMgr->mFaultMgr.mIsTransientDisable = ID_TRUE;             \
        sErrorMgr->mFaultMgr.mDisabledEntryFuncName = NULL;             \
        sErrorMgr->mFaultMgr.mDisabledCallCount = 0;                    \
    }                                                                   \
    else if ((sErrorMgr->mFaultMgr.mIsTransientDisable == ID_TRUE) &&   \
             (sErrorMgr->mFaultMgr.mDisabledEntryFuncName == ID_FUNCTION)) { \
        /* in NOFT range on same function */                            \
        IDE_FT_TRACE_MACRO(sErrorMgr->mFaultMgr.mCallStackNext,         \
                           "IDE_FT_RAISE()");                           \
        sErrorMgr->mFaultMgr.mDisabledCallCount--;                      \
        if (sErrorMgr->mFaultMgr.mDisabledCallCount == 0) {             \
            sErrorMgr->mFaultMgr.mIsTransientDisable = ID_FALSE;        \
            sErrorMgr->mFaultMgr.mDisabledEntryFuncName = NULL;         \
        }                                                               \
    }                                                                   \
    goto b;                                                             \
}
#else
#define IDE_FT_RAISE( b ) {                                             \
    ideErrorMgr *sErrorMgr = ideGetErrorMgr();                          \
    if ((sErrorMgr->mFaultMgr.mCallStackNext > 0) &&                    \
        (sErrorMgr->mFaultMgr.mCallStack[sErrorMgr->mFaultMgr.mCallStackNext - 1].mEntryFuncName == ID_FUNCTION)) { \
        /* in FT range on same function */                              \
        sErrorMgr->mFaultMgr.mIsTransientDisable = ID_TRUE;             \
        sErrorMgr->mFaultMgr.mDisabledEntryFuncName = NULL;             \
        sErrorMgr->mFaultMgr.mDisabledCallCount = 0;                    \
    }                                                                   \
    else if ((sErrorMgr->mFaultMgr.mIsTransientDisable == ID_TRUE) &&   \
             (sErrorMgr->mFaultMgr.mDisabledEntryFuncName == ID_FUNCTION)) { \
        /* in NOFT range on same function */                            \
        sErrorMgr->mFaultMgr.mDisabledCallCount--;                      \
        if (sErrorMgr->mFaultMgr.mDisabledCallCount == 0) {             \
            sErrorMgr->mFaultMgr.mIsTransientDisable = ID_FALSE;        \
            sErrorMgr->mFaultMgr.mDisabledEntryFuncName = NULL;         \
        }                                                               \
    }                                                                   \
    goto b;                                                             \
}
#endif

#ifdef DEBUG
#define IDE_TEST_RAISE( a, b ) { if ( IDL_LIKELY_FALSE( a ) ) {         \
    ideErrorMgr *sErrorMgr = ideGetErrorMgr();                          \
    if( sErrorMgr->ErrorIndex < MAX_ERROR_STACK ){                      \
        idlOS::snprintf( sErrorMgr->ErrorTestLine[sErrorMgr->ErrorIndex], MAX_ERROR_TEST_LEN, "IDE_TEST_RAISE( %s, %s );", #a, #b ); \
            idlOS::snprintf( sErrorMgr->ErrorFile[sErrorMgr->ErrorIndex], MAX_ERROR_FILE_LEN, "%s", idlVA::basename(__FILE__) ); \
            sErrorMgr->ErrorLine[sErrorMgr->ErrorIndex] = __LINE__;     \
            sErrorMgr->ErrorTested++;                                   \
    }                                                                   \
    goto b;                                                             \
}}
#else
#define IDE_TEST_RAISE( a, b ) { if ( IDL_LIKELY_FALSE( a ) ) {         \
    goto b;                                                             \
}}
#endif

/* TASK-6739 Altibase 710 ���� ���� */
#ifdef DEBUG
#define IDE_FT_TEST_RAISE( a, b ) { if ( IDL_LIKELY_FALSE( a ) ) {      \
    ideErrorMgr *sErrorMgr = ideGetErrorMgr();                          \
    if( sErrorMgr->ErrorIndex < MAX_ERROR_STACK ){                      \
        idlOS::snprintf( sErrorMgr->ErrorTestLine[sErrorMgr->ErrorIndex], MAX_ERROR_TEST_LEN, "IDE_FT_TEST_RAISE( %s, %s );", #a, #b ); \
            idlOS::snprintf( sErrorMgr->ErrorFile[sErrorMgr->ErrorIndex], MAX_ERROR_FILE_LEN, "%s", idlVA::basename(__FILE__) ); \
            sErrorMgr->ErrorLine[sErrorMgr->ErrorIndex] = __LINE__;     \
            sErrorMgr->ErrorTested++;                                   \
    }                                                                   \
    if ((sErrorMgr->mFaultMgr.mCallStackNext > 0) &&                    \
        (sErrorMgr->mFaultMgr.mCallStack[sErrorMgr->mFaultMgr.mCallStackNext - 1].mEntryFuncName == ID_FUNCTION)) { \
        /* in FT range on same function */                              \
        IDE_FT_TRACE_MACRO(sErrorMgr->mFaultMgr.mCallStackNext,         \
                           "IDE_FT_TEST_RAISE()");                      \
        sErrorMgr->mFaultMgr.mIsTransientDisable = ID_TRUE;             \
        sErrorMgr->mFaultMgr.mDisabledEntryFuncName = NULL;             \
        sErrorMgr->mFaultMgr.mDisabledCallCount = 0;                    \
    }                                                                   \
    else if ((sErrorMgr->mFaultMgr.mIsTransientDisable == ID_TRUE) &&   \
             (sErrorMgr->mFaultMgr.mDisabledEntryFuncName == ID_FUNCTION)) { \
        /* in NOFT range on same function */                            \
        IDE_FT_TRACE_MACRO(sErrorMgr->mFaultMgr.mCallStackNext,         \
                           "IDE_FT_TEST_RAISE()");                      \
        sErrorMgr->mFaultMgr.mDisabledCallCount--;                      \
        if (sErrorMgr->mFaultMgr.mDisabledCallCount == 0) {             \
            sErrorMgr->mFaultMgr.mIsTransientDisable = ID_FALSE;        \
            sErrorMgr->mFaultMgr.mDisabledEntryFuncName = NULL;         \
        }                                                               \
    }                                                                   \
    goto b;                                                             \
}}
#else
#define IDE_FT_TEST_RAISE( a, b ) { if ( IDL_LIKELY_FALSE( a ) ) {      \
    ideErrorMgr *sErrorMgr = ideGetErrorMgr();                          \
    if ((sErrorMgr->mFaultMgr.mCallStackNext > 0) &&                    \
        (sErrorMgr->mFaultMgr.mCallStack[sErrorMgr->mFaultMgr.mCallStackNext - 1].mEntryFuncName == ID_FUNCTION)) { \
        /* in FT range on same function */                              \
        sErrorMgr->mFaultMgr.mIsTransientDisable = ID_TRUE;             \
        sErrorMgr->mFaultMgr.mDisabledEntryFuncName = NULL;             \
        sErrorMgr->mFaultMgr.mDisabledCallCount = 0;                    \
    }                                                                   \
    else if ((sErrorMgr->mFaultMgr.mIsTransientDisable == ID_TRUE) &&   \
             (sErrorMgr->mFaultMgr.mDisabledEntryFuncName == ID_FUNCTION)) { \
        /* in NOFT range on same function */                            \
        sErrorMgr->mFaultMgr.mDisabledCallCount--;                      \
        if (sErrorMgr->mFaultMgr.mDisabledCallCount == 0) {             \
            sErrorMgr->mFaultMgr.mIsTransientDisable = ID_FALSE;        \
            sErrorMgr->mFaultMgr.mDisabledEntryFuncName = NULL;         \
        }                                                               \
    }                                                                   \
    goto b;                                                             \
}}
#endif

#ifdef DEBUG
#define IDE_TEST( a ) { if ( IDL_LIKELY_FALSE( a ) ) {                  \
    ideErrorMgr *sErrorMgr = ideGetErrorMgr();                          \
    if( sErrorMgr->ErrorIndex < MAX_ERROR_STACK ){                      \
        idlOS::snprintf( sErrorMgr->ErrorTestLine[sErrorMgr->ErrorIndex], MAX_ERROR_TEST_LEN, "IDE_TEST( %s );", #a ); \
            idlOS::snprintf( sErrorMgr->ErrorFile[sErrorMgr->ErrorIndex], MAX_ERROR_FILE_LEN, "%s", idlVA::basename(__FILE__) ); \
            sErrorMgr->ErrorLine[sErrorMgr->ErrorIndex] = __LINE__;     \
            sErrorMgr->ErrorTested++;                                   \
    }                                                                   \
    goto IDE_EXCEPTION_END_LABEL;                                       \
}}
#else
#define IDE_TEST( a ) { if ( IDL_LIKELY_FALSE( a ) ) {                  \
    goto IDE_EXCEPTION_END_LABEL;                                       \
}}
#endif

/* TASK-6739 Altibase 710 ���� ���� */
#ifdef DEBUG
#define IDE_FT_TEST( a ) { if ( IDL_LIKELY_FALSE( a ) ) {               \
    ideErrorMgr *sErrorMgr = ideGetErrorMgr();                          \
    if( sErrorMgr->ErrorIndex < MAX_ERROR_STACK ){                      \
        idlOS::snprintf( sErrorMgr->ErrorTestLine[sErrorMgr->ErrorIndex], MAX_ERROR_TEST_LEN, "IDE_FT_TEST( %s );", #a ); \
            idlOS::snprintf( sErrorMgr->ErrorFile[sErrorMgr->ErrorIndex], MAX_ERROR_FILE_LEN, "%s", idlVA::basename(__FILE__) ); \
            sErrorMgr->ErrorLine[sErrorMgr->ErrorIndex] = __LINE__;     \
            sErrorMgr->ErrorTested++;                                   \
    }                                                                   \
    if ((sErrorMgr->mFaultMgr.mCallStackNext > 0) &&                    \
        (sErrorMgr->mFaultMgr.mCallStack[sErrorMgr->mFaultMgr.mCallStackNext - 1].mEntryFuncName == ID_FUNCTION)) { \
        /* in FT range on same function */                              \
        IDE_FT_TRACE_MACRO(sErrorMgr->mFaultMgr.mCallStackNext,         \
                           "IDE_FT_TEST()");                            \
        sErrorMgr->mFaultMgr.mIsTransientDisable = ID_TRUE;             \
        sErrorMgr->mFaultMgr.mDisabledEntryFuncName = NULL;             \
        sErrorMgr->mFaultMgr.mDisabledCallCount = 0;                    \
    }                                                                   \
    else if ((sErrorMgr->mFaultMgr.mIsTransientDisable == ID_TRUE) &&   \
             (sErrorMgr->mFaultMgr.mDisabledEntryFuncName == ID_FUNCTION)) { \
        /* in NOFT range on same function */                            \
        IDE_FT_TRACE_MACRO(sErrorMgr->mFaultMgr.mCallStackNext,         \
                           "IDE_FT_TEST()");                            \
        sErrorMgr->mFaultMgr.mDisabledCallCount--;                      \
        if (sErrorMgr->mFaultMgr.mDisabledCallCount == 0) {             \
            sErrorMgr->mFaultMgr.mIsTransientDisable = ID_FALSE;        \
            sErrorMgr->mFaultMgr.mDisabledEntryFuncName = NULL;         \
        }                                                               \
    }                                                                   \
    goto IDE_EXCEPTION_END_LABEL;                                       \
}}
#else
#define IDE_FT_TEST( a ) { if ( IDL_LIKELY_FALSE( a ) ) {               \
    ideErrorMgr *sErrorMgr = ideGetErrorMgr();                          \
    if ((sErrorMgr->mFaultMgr.mCallStackNext > 0) &&                    \
        (sErrorMgr->mFaultMgr.mCallStack[sErrorMgr->mFaultMgr.mCallStackNext - 1].mEntryFuncName == ID_FUNCTION)) { \
        /* in FT range on same function */                              \
        sErrorMgr->mFaultMgr.mIsTransientDisable = ID_TRUE;             \
        sErrorMgr->mFaultMgr.mDisabledEntryFuncName = NULL;             \
        sErrorMgr->mFaultMgr.mDisabledCallCount = 0;                    \
    }                                                                   \
    else if ((sErrorMgr->mFaultMgr.mIsTransientDisable == ID_TRUE) &&   \
             (sErrorMgr->mFaultMgr.mDisabledEntryFuncName == ID_FUNCTION)) { \
        /* in NOFT range on same function */                            \
        sErrorMgr->mFaultMgr.mDisabledCallCount--;                      \
        if (sErrorMgr->mFaultMgr.mDisabledCallCount == 0) {             \
            sErrorMgr->mFaultMgr.mIsTransientDisable = ID_FALSE;        \
            sErrorMgr->mFaultMgr.mDisabledEntryFuncName = NULL;         \
        }                                                               \
    }                                                                   \
    goto IDE_EXCEPTION_END_LABEL;                                       \
}}
#endif

/**
 * FATAL �غ� ���� ������ �Ͻ������� ��Ȱ��ȭ�ϱ� ���� IDE_TEST_RAISE()
 * ���Ǵ� IDE_NOFT_TEST_RAISE() ��ũ�� ���.
 */
#ifdef DEBUG

#define IDE_NOFT_TEST_RAISE(a, b)                                       \
    do {                                                                \
        ideErrorMgr *sErrorMgr = ideGetErrorMgr();                      \
        if( sErrorMgr->ErrorIndex < MAX_ERROR_STACK ){                  \
            idlOS::snprintf( sErrorMgr->ErrorTestLine[sErrorMgr->ErrorIndex], MAX_ERROR_TEST_LEN, "IDE_TEST_RAISE_NOFT( %s, %s );", #a, #b ); \
                idlOS::snprintf( sErrorMgr->ErrorFile[sErrorMgr->ErrorIndex], MAX_ERROR_FILE_LEN, "%s", idlVA::basename(__FILE__) ); \
                sErrorMgr->ErrorLine[sErrorMgr->ErrorIndex] = __LINE__; \
                sErrorMgr->ErrorTested++;                               \
        }                                                               \
        if ((sErrorMgr->mFaultMgr.mCallStackNext > 0) &&                \
            (sErrorMgr->mFaultMgr.mCallStack[sErrorMgr->mFaultMgr.mCallStackNext - 1].mEntryFuncName == ID_FUNCTION)) { \
            /* in FT range on same function */                          \
            if (sErrorMgr->mFaultMgr.mIsTransientDisable == ID_FALSE) { \
                sErrorMgr->mFaultMgr.mIsTransientDisable = ID_TRUE;     \
                sErrorMgr->mFaultMgr.mDisabledEntryFuncName = ID_FUNCTION; \
                sErrorMgr->mFaultMgr.mDisabledCallCount = 1;            \
                if (a) {                                                \
                    IDE_FT_TRACE_MACRO(sErrorMgr->mFaultMgr.mCallStackNext, \
                                       "IDE_TEST_RAISE_NOFT()");        \
                    sErrorMgr->mFaultMgr.mIsTransientDisable = ID_TRUE; \
                    sErrorMgr->mFaultMgr.mDisabledEntryFuncName = NULL; \
                    sErrorMgr->mFaultMgr.mDisabledCallCount = 0;        \
                    goto b;                                             \
                }                                                       \
                else {                                                  \
                    sErrorMgr->mFaultMgr.mIsTransientDisable = ID_FALSE; \
                    sErrorMgr->mFaultMgr.mDisabledEntryFuncName = NULL; \
                    sErrorMgr->mFaultMgr.mDisabledCallCount = 0;        \
                }                                                       \
            }                                                           \
            else {                                                      \
                /* in NOFT range after FT begin on same function */     \
                if (a) {                                                \
                    IDE_FT_TRACE_MACRO(sErrorMgr->mFaultMgr.mCallStackNext, \
                                       "IDE_TEST_RAISE_NOFT()");        \
                    sErrorMgr->mFaultMgr.mIsTransientDisable = ID_TRUE; \
                    sErrorMgr->mFaultMgr.mDisabledEntryFuncName = NULL; \
                    sErrorMgr->mFaultMgr.mDisabledCallCount = 0;        \
                    goto b;                                             \
                }                                                       \
            }                                                           \
        }                                                               \
        else {                                                          \
            if (a) {                                                    \
                if (sErrorMgr->mFaultMgr.mDisabledEntryFuncName == ID_FUNCTION) { \
                    /* in NOFT range on same function */                \
                    IDE_FT_TRACE_MACRO(sErrorMgr->mFaultMgr.mCallStackNext, \
                                       "IDE_TEST_RAISE_NOFT()");        \
                    sErrorMgr->mFaultMgr.mDisabledCallCount--;          \
                    if (sErrorMgr->mFaultMgr.mDisabledCallCount == 0) { \
                        sErrorMgr->mFaultMgr.mIsTransientDisable = ID_FALSE;\
                        sErrorMgr->mFaultMgr.mDisabledEntryFuncName = NULL; \
                    }                                                   \
                }                                                       \
                goto b;                                                 \
            }                                                           \
        }                                                               \
    } while (0)

#else /* DEBUG */

#define IDE_NOFT_TEST_RAISE(a, b)                                       \
    do {                                                                \
        ideErrorMgr *sErrorMgr = ideGetErrorMgr();                      \
        if ((sErrorMgr->mFaultMgr.mCallStackNext > 0) &&                \
            (sErrorMgr->mFaultMgr.mCallStack[sErrorMgr->mFaultMgr.mCallStackNext - 1].mEntryFuncName == ID_FUNCTION)) { \
            /* in FT range on same function */                          \
            if (sErrorMgr->mFaultMgr.mIsTransientDisable == ID_FALSE) { \
                sErrorMgr->mFaultMgr.mIsTransientDisable = ID_TRUE;     \
                sErrorMgr->mFaultMgr.mDisabledEntryFuncName = ID_FUNCTION; \
                sErrorMgr->mFaultMgr.mDisabledCallCount = 1;            \
                if (a) {                                                \
                    sErrorMgr->mFaultMgr.mIsTransientDisable = ID_TRUE; \
                    sErrorMgr->mFaultMgr.mDisabledEntryFuncName = NULL; \
                    sErrorMgr->mFaultMgr.mDisabledCallCount = 0;        \
                    goto b;                                             \
                }                                                       \
                else {                                                  \
                    sErrorMgr->mFaultMgr.mIsTransientDisable = ID_FALSE; \
                    sErrorMgr->mFaultMgr.mDisabledEntryFuncName = NULL; \
                    sErrorMgr->mFaultMgr.mDisabledCallCount = 0;        \
                }                                                       \
            }                                                           \
            else {                                                      \
                /* in NOFT range after FT begin on same function */     \
                if (a) {                                                \
                    sErrorMgr->mFaultMgr.mIsTransientDisable = ID_TRUE; \
                    sErrorMgr->mFaultMgr.mDisabledEntryFuncName = NULL; \
                    sErrorMgr->mFaultMgr.mDisabledCallCount = 0;        \
                    goto b;                                             \
                }                                                       \
            }                                                           \
        }                                                               \
        else {                                                          \
            if (a) {                                                    \
                if (sErrorMgr->mFaultMgr.mDisabledEntryFuncName == ID_FUNCTION) { \
                    /* in NOFT range on same function */                \
                    sErrorMgr->mFaultMgr.mDisabledCallCount--;          \
                    if (sErrorMgr->mFaultMgr.mDisabledCallCount == 0) { \
                        sErrorMgr->mFaultMgr.mIsTransientDisable = ID_FALSE; \
                        sErrorMgr->mFaultMgr.mDisabledEntryFuncName = NULL; \
                    }                                                   \
                }                                                       \
                goto b;                                                 \
            }                                                           \
        }                                                               \
    } while (0)

#endif /* DEBUG */

/**
 * FATAL �غ� ���� ������ �Ͻ������� ��Ȱ��ȭ�ϱ� ���� IDE_TEST()
 * ���Ǵ� IDE_NOFT_TEST() ��ũ�� ���.
 */
#ifdef DEBUG

#define IDE_NOFT_TEST(a)                                                \
    do {                                                                \
        ideErrorMgr *sErrorMgr = ideGetErrorMgr();                      \
        if( sErrorMgr->ErrorIndex < MAX_ERROR_STACK ){                  \
            idlOS::snprintf( sErrorMgr->ErrorTestLine[sErrorMgr->ErrorIndex], MAX_ERROR_TEST_LEN, "IDE_NOFT_TEST( %s );", #a ); \
                idlOS::snprintf( sErrorMgr->ErrorFile[sErrorMgr->ErrorIndex], MAX_ERROR_FILE_LEN, "%s", idlVA::basename(__FILE__) ); \
                sErrorMgr->ErrorLine[sErrorMgr->ErrorIndex] = __LINE__; \
                sErrorMgr->ErrorTested++;                               \
        }                                                               \
        if ((sErrorMgr->mFaultMgr.mCallStackNext > 0) &&                \
            (sErrorMgr->mFaultMgr.mCallStack[sErrorMgr->mFaultMgr.mCallStackNext - 1].mEntryFuncName == ID_FUNCTION)) { \
            /* in FT range on same function */                          \
            if (sErrorMgr->mFaultMgr.mIsTransientDisable == ID_FALSE) { \
                sErrorMgr->mFaultMgr.mIsTransientDisable = ID_TRUE;     \
                sErrorMgr->mFaultMgr.mDisabledEntryFuncName = ID_FUNCTION; \
                sErrorMgr->mFaultMgr.mDisabledCallCount = 1;            \
                if (a) {                                                \
                    IDE_FT_TRACE_MACRO(sErrorMgr->mFaultMgr.mCallStackNext, \
                                       "IDE_NOFT_TEST()");              \
                    sErrorMgr->mFaultMgr.mIsTransientDisable = ID_TRUE; \
                    sErrorMgr->mFaultMgr.mDisabledEntryFuncName = NULL; \
                    sErrorMgr->mFaultMgr.mDisabledCallCount = 0;        \
                    goto IDE_EXCEPTION_END_LABEL;                       \
                }                                                       \
                else {                                                  \
                    sErrorMgr->mFaultMgr.mIsTransientDisable = ID_FALSE; \
                    sErrorMgr->mFaultMgr.mDisabledEntryFuncName = NULL; \
                    sErrorMgr->mFaultMgr.mDisabledCallCount = 0;        \
                }                                                       \
            }                                                           \
            else {                                                      \
                /* in NOFT range after FT begin on same function */     \
                if (a) {                                                \
                    IDE_FT_TRACE_MACRO(sErrorMgr->mFaultMgr.mCallStackNext, \
                                       "IDE_NOFT_TEST()");              \
                    sErrorMgr->mFaultMgr.mIsTransientDisable = ID_TRUE; \
                    sErrorMgr->mFaultMgr.mDisabledEntryFuncName = NULL; \
                    sErrorMgr->mFaultMgr.mDisabledCallCount = 0;        \
                    goto IDE_EXCEPTION_END_LABEL;                       \
                }                                                       \
            }                                                           \
        }                                                               \
        else {                                                          \
            if (a) {                                                    \
                if (sErrorMgr->mFaultMgr.mDisabledEntryFuncName == ID_FUNCTION) { \
                    /* in NOFT range on same function */                \
                    IDE_FT_TRACE_MACRO(sErrorMgr->mFaultMgr.mCallStackNext, \
                                       "IDE_NOFT_TEST()");              \
                    sErrorMgr->mFaultMgr.mDisabledCallCount--;          \
                    if (sErrorMgr->mFaultMgr.mDisabledCallCount == 0) { \
                        sErrorMgr->mFaultMgr.mIsTransientDisable = ID_FALSE; \
                        sErrorMgr->mFaultMgr.mDisabledEntryFuncName = NULL; \
                    }                                                   \
                }                                                       \
                goto IDE_EXCEPTION_END_LABEL;                           \
            }                                                           \
        }                                                               \
    } while (0)

#else /* DEBUG */

#define IDE_NOFT_TEST(a)                                                \
    do {                                                                \
        ideErrorMgr *sErrorMgr = ideGetErrorMgr();                      \
        if ((sErrorMgr->mFaultMgr.mCallStackNext > 0) &&                \
            (sErrorMgr->mFaultMgr.mCallStack[sErrorMgr->mFaultMgr.mCallStackNext - 1].mEntryFuncName == ID_FUNCTION)) { \
            /* in FT range on same function */                          \
            if (sErrorMgr->mFaultMgr.mIsTransientDisable == ID_FALSE) { \
                sErrorMgr->mFaultMgr.mIsTransientDisable = ID_TRUE;     \
                sErrorMgr->mFaultMgr.mDisabledEntryFuncName = ID_FUNCTION; \
                sErrorMgr->mFaultMgr.mDisabledCallCount = 1;            \
                if (a) {                                                \
                    sErrorMgr->mFaultMgr.mIsTransientDisable = ID_TRUE; \
                    sErrorMgr->mFaultMgr.mDisabledEntryFuncName = NULL; \
                    sErrorMgr->mFaultMgr.mDisabledCallCount = 0;        \
                    goto IDE_EXCEPTION_END_LABEL;                       \
                }                                                       \
                else {                                                  \
                    sErrorMgr->mFaultMgr.mIsTransientDisable = ID_FALSE; \
                    sErrorMgr->mFaultMgr.mDisabledEntryFuncName = NULL; \
                    sErrorMgr->mFaultMgr.mDisabledCallCount = 0;        \
                }                                                       \
            }                                                           \
            else {                                                      \
                /* in NOFT range after FT begin on same function */     \
                if (a) {                                                \
                    sErrorMgr->mFaultMgr.mIsTransientDisable = ID_TRUE; \
                    sErrorMgr->mFaultMgr.mDisabledEntryFuncName = NULL; \
                    sErrorMgr->mFaultMgr.mDisabledCallCount = 0;        \
                    goto IDE_EXCEPTION_END_LABEL;                       \
                }                                                       \
            }                                                           \
        }                                                               \
        else {                                                          \
            if (a) {                                                    \
                if (sErrorMgr->mFaultMgr.mDisabledEntryFuncName == ID_FUNCTION) { \
                    /* in NOFT range on same function */                \
                    sErrorMgr->mFaultMgr.mDisabledCallCount--;          \
                    if (sErrorMgr->mFaultMgr.mDisabledCallCount == 0) { \
                        sErrorMgr->mFaultMgr.mIsTransientDisable = ID_FALSE; \
                        sErrorMgr->mFaultMgr.mDisabledEntryFuncName = NULL; \
                    }                                                   \
                }                                                       \
                goto IDE_EXCEPTION_END_LABEL;                           \
            }                                                           \
        }                                                               \
    } while (0)

#endif /* DEBUG */

/**
 * FATAL �غ� ���� ������ �Ͻ������� ��Ȱ��ȭ�ϱ� ���� IDE_NOFT() ��ũ�� ���.
 * ���� �Ͽ� exception section���� jump �Ǿ�� �Ѵٸ�, IDE_NOFT_TEST_RAISE()
 * �Ǵ� IDE_NOFT_TEST() ��ũ�θ� ����ؾ� �Ѵ�.
 */

/* To prevent re-ordering instruction in IDE_NOFT() macro under AIX/PowerPC or Linux environment */
#if defined(ALTI_CFG_CPU_POWERPC) || defined(ALTI_CFG_OS_LINUX)
#define IDE_FT_VOLATILE_TRANSIENT_DISABLE(aIsTransientDisable)          \
    *(volatile idBool *)(&sErrorMgr->mFaultMgr.mIsTransientDisable) = aIsTransientDisable
#else /* ALTI_CFG_CPU_POWERPC */
#define IDE_FT_VOLATILE_TRANSIENT_DISABLE(aIsTransientDisable)          \
    sErrorMgr->mFaultMgr.mIsTransientDisable = aIsTransientDisable
#endif /* ALTI_CFG_CPU_POWERPC */

#ifdef DEBUG

#define IDE_NOFT(a)                                                     \
    do {                                                                \
        ideErrorMgr *sErrorMgr = ideGetErrorMgr();                      \
        if (sErrorMgr->mFaultMgr.mIsTransientDisable == ID_FALSE) {     \
            IDE_FT_TRACE_MACRO(sErrorMgr->mFaultMgr.mCallStackNext,     \
                               "IDE_NOFT() - BEGIN");                   \
            IDE_FT_VOLATILE_TRANSIENT_DISABLE(ID_TRUE);                 \
            sErrorMgr->mFaultMgr.mDisabledEntryFuncName = ID_FUNCTION;  \
            sErrorMgr->mFaultMgr.mDisabledCallCount = 1;                \
            a;                                                          \
            IDE_FT_TRACE_MACRO(sErrorMgr->mFaultMgr.mCallStackNext,     \
                               "IDE_NOFT() - END");                     \
            sErrorMgr->mFaultMgr.mIsTransientDisable = ID_FALSE;        \
            sErrorMgr->mFaultMgr.mDisabledEntryFuncName = NULL;         \
            sErrorMgr->mFaultMgr.mDisabledCallCount--;                  \
        }                                                               \
        else {                                                          \
            a;                                                          \
        } \
    } while (0)

#else /* DEBUG */

#define IDE_NOFT(a)                                                     \
    do {                                                                \
        ideErrorMgr *sErrorMgr = ideGetErrorMgr();                      \
        if (sErrorMgr->mFaultMgr.mIsTransientDisable == ID_FALSE) {     \
            IDE_FT_VOLATILE_TRANSIENT_DISABLE(ID_TRUE);                 \
            sErrorMgr->mFaultMgr.mDisabledEntryFuncName = ID_FUNCTION;  \
            sErrorMgr->mFaultMgr.mDisabledCallCount = 1;                \
            a;                                                          \
            sErrorMgr->mFaultMgr.mIsTransientDisable = ID_FALSE;        \
            sErrorMgr->mFaultMgr.mDisabledEntryFuncName = NULL;         \
            sErrorMgr->mFaultMgr.mDisabledCallCount--;                  \
        }                                                               \
        else {                                                          \
            a;                                                          \
        } \
    } while (0)

#endif /* DEBUG */

/**
 * FATAL �غ� ���� ������ �Ͻ������� ��Ȱ��ȭ ������ �����ϱ� ����
 * IDE_NOFT_BEGIN() ��ũ�� ���.
 */
#ifdef DEBUG

#define IDE_NOFT_BEGIN()                                                \
    do {                                                                \
        ideErrorMgr *sErrorMgr = ideGetErrorMgr();                      \
        if (sErrorMgr->mFaultMgr.mIsTransientDisable == ID_FALSE) {     \
            IDE_FT_TRACE_MACRO(sErrorMgr->mFaultMgr.mCallStackNext,     \
                               "IDE_NOFT_BEGIN()");                     \
            sErrorMgr->mFaultMgr.mIsTransientDisable = ID_TRUE;         \
            sErrorMgr->mFaultMgr.mDisabledEntryFuncName = ID_FUNCTION;  \
            sErrorMgr->mFaultMgr.mDisabledCallCount = 1;                \
        }                                                               \
        else if (sErrorMgr->mFaultMgr.mDisabledEntryFuncName == ID_FUNCTION) { \
            /* for recursive function */                                \
            sErrorMgr->mFaultMgr.mDisabledCallCount++;                  \
        }                                                               \
    } while (0)

#else /* DEBUG */

#define IDE_NOFT_BEGIN()                                                \
    do {                                                                \
        ideErrorMgr *sErrorMgr = ideGetErrorMgr();                      \
        if (sErrorMgr->mFaultMgr.mIsTransientDisable == ID_FALSE) {     \
            sErrorMgr->mFaultMgr.mIsTransientDisable = ID_TRUE;         \
            sErrorMgr->mFaultMgr.mDisabledEntryFuncName = ID_FUNCTION;  \
            sErrorMgr->mFaultMgr.mDisabledCallCount = 1;                \
        }                                                               \
        else if (sErrorMgr->mFaultMgr.mDisabledEntryFuncName == ID_FUNCTION) { \
            /* for recursive function */                                \
            sErrorMgr->mFaultMgr.mDisabledCallCount++;                  \
        }                                                               \
    } while (0)

#endif /* DEBUG */

/**
 * FATAL �غ� ���� ������ �Ͻ������� ��Ȱ��ȭ ������ �����ϱ� ����
 * IDE_NOFT_END() ��ũ�� ���. IDE_FT_END_FAILURE()�� �ٸ���
 * exception section���� �� ��ũ�θ� �������� �ʾƵ� �ȴ�.
 */
#ifdef DEBUG

#define IDE_NOFT_END()                                                  \
    do {                                                                \
        ideErrorMgr *sErrorMgr = ideGetErrorMgr();                      \
        if ((sErrorMgr->mFaultMgr.mIsTransientDisable == ID_TRUE) &&    \
            (sErrorMgr->mFaultMgr.mDisabledEntryFuncName == ID_FUNCTION)) { \
            sErrorMgr->mFaultMgr.mDisabledCallCount--;                  \
            if (sErrorMgr->mFaultMgr.mDisabledCallCount == 0) {         \
                IDE_FT_TRACE_MACRO(sErrorMgr->mFaultMgr.mCallStackNext, \
                                   "IDE_NOFT_END()");                   \
                sErrorMgr->mFaultMgr.mIsTransientDisable = ID_FALSE;    \
                sErrorMgr->mFaultMgr.mDisabledEntryFuncName = NULL;     \
            }                                                           \
        }                                                               \
    } while (0)

#else /* DEBUG */

#define IDE_NOFT_END()                                                  \
    do {                                                                \
        ideErrorMgr *sErrorMgr = ideGetErrorMgr();                      \
        if ((sErrorMgr->mFaultMgr.mIsTransientDisable == ID_TRUE) &&    \
            (sErrorMgr->mFaultMgr.mDisabledEntryFuncName == ID_FUNCTION)) { \
            sErrorMgr->mFaultMgr.mDisabledCallCount--;                  \
            if (sErrorMgr->mFaultMgr.mDisabledCallCount == 0) {         \
                sErrorMgr->mFaultMgr.mIsTransientDisable = ID_FALSE;    \
                sErrorMgr->mFaultMgr.mDisabledEntryFuncName = NULL;     \
            }                                                           \
        }                                                               \
    } while (0)

#endif /* DEBUG */

#define IDE_TEST_CONT( a, b ) { if( a ) { goto b; } }
#define IDE_CONT( b ) { goto b;}

/******************************************************************************
 * PROJ-2514 Error Manager
 * ERROR Message ��ũ�� �߰�.
 *****************************************************************************/
#define IDE_ERROR( a )                                          \
    if( !(a) )                                                  \
    {                                                           \
        ideLogError("IDE_ERROR( "#a" )",                        \
                    ID_FALSE, __FILE__, __LINE__, NULL );       \
        goto IDE_EXCEPTION_END_LABEL;                           \
    }                                                           \
    else { /* do nothing */ }

#define IDE_ERROR_MSG( a, format, ... )                         \
    if( !(a) )                                                  \
    {                                                           \
        ideLogError("IDE_ERROR_MSG( "#a" )",                    \
                    ID_FALSE, __FILE__, __LINE__,               \
                    format, ##__VA_ARGS__ );                    \
        goto IDE_EXCEPTION_END_LABEL;                           \
    }                                                           \
    else { /* do nothing */ }

#define IDE_ERROR_RAISE( a, b )                                 \
    if( !(a) )                                                  \
    {                                                           \
        ideLogError("IDE_ERROR_RAISE( "#a" )",                  \
                    ID_FALSE, __FILE__, __LINE__, NULL );       \
        goto b;                                                 \
    }                                                           \
    else { /* do nothing */ }

#define IDE_ERROR_RAISE_MSG( a, b, format, ... )                \
    if( !(a) )                                                  \
    {                                                           \
        ideLogError("IDE_ERROR_RAISE_MSG( "#a" )",              \
                    ID_FALSE, __FILE__, __LINE__,               \
                    format, ##__VA_ARGS__ );                    \
        goto b;                                                 \
    }                                                           \
    else { /* do nothing */ }

#define IDE_FT_ERROR( a )                                       \
    if ( !(a) )                                                 \
    {                                                           \
        ideLogError("IDE_FT_ERROR( "#a" )",                     \
                    ID_TRUE, __FILE__, __LINE__, NULL );        \
        goto IDE_EXCEPTION_END_LABEL;                           \
    }                                                           \
    else { /* do nothing */ }

#define IDE_FT_ERROR_MSG( a, format, ... )                      \
    if ( !(a) )                                                 \
    {                                                           \
        ideLogError("IDE_FT_ERROR_MSG( "#a" )",                 \
                    ID_TRUE, __FILE__, __LINE__,                \
                    format, ##__VA_ARGS__ );                    \
        goto IDE_EXCEPTION_END_LABEL;                           \
    }                                                           \
    else { /* do nothing */ }

#define IDE_FT_ERROR_RAISE( a, b )                              \
    if ( !(a) )                                                 \
    {                                                           \
        ideLogError("IDE_FT_ERROR_RAISE( "#a" )",               \
                    ID_TRUE, __FILE__, __LINE__, NULL );        \
        goto b;                                                 \
    }                                                           \
    else { /* do nothing */ }

#define IDE_FT_ERROR_RAISE_MSG( a, b, format, ... )             \
    if ( !(a) )                                                 \
    {                                                           \
        ideLogError("IDE_FT_ERROR_RAISE_MSG( "#a" )",           \
                    ID_TRUE, __FILE__, __LINE__,                \
                    format, ##__VA_ARGS__ );                    \
        goto b;                                                 \
    }                                                           \
    else { /* do nothing */ }

#define IDE_EXCEPTION(a) goto IDE_EXCEPTION_END_LABEL; a:

/* PROJ-2617 */
#define IDE_EXCEPTION_SIGNAL() goto IDE_EXCEPTION_END_LABEL; IDE_EXCEPTION_SIGNAL_LABEL:

#define IDE_EXCEPTION_CONT(a) a:

#ifdef DEBUG
#define IDE_EXCEPTION_END IDE_EXCEPTION_END_LABEL: {  \
    ideErrorMgr *sErrorMgr = ideGetErrorMgr();        \
    if( sErrorMgr->ErrorTested ){                     \
      sErrorMgr->ErrorIndex++;                        \
      sErrorMgr->ErrorTested = 0;                     \
    }                                                 \
}
#else
#define IDE_EXCEPTION_END IDE_EXCEPTION_END_LABEL:;
#endif

#ifdef DEBUG
#define IDE_SET(setFunc)                                                    \
{                                                                           \
     ideErrorMgr* sErrMgr = setFunc;                                        \
     sErrMgr->Stack.LastErrorLine = __LINE__;                               \
     idlOS::strncpy(sErrMgr->Stack.LastErrorFile,                           \
                    idlVA::basename(__FILE__), MAX_ERROR_FILE_LEN);         \
     sErrMgr->Stack.LastErrorFile[MAX_ERROR_FILE_LEN - 1] = 0;              \
     ideSetDebugInfo((SChar *)idlVA::basename(__FILE__), __LINE__, (SChar *)#setFunc);\
}
#else
#define IDE_SET(setFunc)                                                    \
{                                                                           \
     ideErrorMgr* sErrMgr = setFunc;                                        \
     sErrMgr->Stack.LastErrorLine = __LINE__;                               \
     idlOS::strncpy(sErrMgr->Stack.LastErrorFile,                           \
                    idlVA::basename(__FILE__), MAX_ERROR_FILE_LEN);         \
     sErrMgr->Stack.LastErrorFile[MAX_ERROR_FILE_LEN - 1] = 0;              \
}
#endif

/* BUG-18657: ID�������� FATAL�߻��� ������ �ٷ� �׿��� �մϴ�. */
#define IDE_SET_AND_DIE(setFunc) \
{\
     IDE_SET( setFunc ); \
     ideLog::logCallStack(IDE_SERVER_0); \
     IDE_CALLBACK_FATAL("Failure of Function Call "); \
}


/* ------------------------------------------------------------------
 *          ���� �ڵ� Conversion Matrix
 *       (�����ڵ尡 ����Ǿ��� ��� ����)
 *       (X => �����ʿ� ���� O=> ������)
 *   --------------+-------------------------------------------------------------
 *                 |                         NEW CODE
 *     OLD CODE    +-------------------------------------------------------------
 *                 |   IGNORE  |   RETRY   | REBUILD   |   ABORT   |    FATAL
 *   --------------+-----------+-----------+-----------+-----------+-------------
 *     IGNORE      |     X     |     O     |     O     |     O     |      O
 *   --------------+-----------+-----------+-----------+-----------+-------------
 *     RETRY       |     X     |     X     |     O     |     O     |      O
 *   --------------+-----------+-----------+-----------+-----------+-------------
 *     REBUILD     |     X     |     X     |     X     |     O     |      O
 *   --------------+-----------+-----------+-----------+-----------+-------------
 *     ABORT       |     X     |     X     |     X     |     X     |      O
 *   --------------+-----------+-----------+-----------+-----------+-------------
 *     FATAL       |     X     |     X     |     X     |     X     |      X
 *   --------------+-----------+-----------+-----------+-----------+-------------
 *
 *   IDE_PUSH() �� IDE_POP()�� pair�� ����ؾ� ��.
 * ----------------------------------------------*/

extern const idBool ideErrorConversionMatrix[IDE_MAX_ERROR_ACTION][IDE_MAX_ERROR_ACTION];


/******************************************************************************
 * PROJ-2514 Error Manager New Features
 * IDE_PUSH�� IDE_POP ����ȭ
 *****************************************************************************/
#define IDE_PUSH() {                                                        \
    ideErrorMgrStack sStack;                                                \
    idlOS::memcpy(&sStack, &(ideGetErrorMgr()->Stack), sizeof(ideErrorMgrStack));

#define IDE_POP()                                                           \
    idlOS::memcpy(&(ideGetErrorMgr()->Stack), &sStack, sizeof(ideErrorMgrStack));     \
}

typedef struct ideClientErrorFactory
{
    const SChar *mErrorState;
    const SChar *mErrorMsg;
}ideClientErrorFactory;

typedef struct ideClientErrorMgr
{
    UInt   mErrorCode;     /* CODE  */
    SChar  mErrorState[6]; /* STATE */
    SChar  mErrorMessage[MAX_ERROR_MSG_LEN+256];
} ideClientErrorMgr;

void ideSetClientErrorCode(ideClientErrorMgr     *aErrorMgr,
                           ideClientErrorFactory *aFactory,
                           UInt                   aErrorCode,
                           va_list                aArgs);

#endif /* _O_IDEERRORMGR_H_ */

#if defined(ITRON) || defined(WIN32_ODBC_CLI2)
extern struct ideErrorMgr *toppers_error;
#endif
