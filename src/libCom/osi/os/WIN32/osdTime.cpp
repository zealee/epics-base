
//
// $Id$
//
// Author: Jeff Hill
//
//

//
// ANSI C
//
#include <math.h>
#include <time.h>
#include <limits.h>

//
// WIN32
//
#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN 
#include <winsock2.h>
#include <process.h>

//
// EPICS
//
#define epicsExportSharedSymbols
#include "epicsTime.h"
#include "epicsTimer.h"
#include "errlog.h"
#include "epicsAssert.h"
#include "epicsThread.h"

#if defined ( DEBUG ) 
#   define debugPrintf(argsInParen) ::printf argsInParen
#else
#   define debugPrintf(argsInParen)
#endif

static const SYSTEMTIME epicsEpochST = {
	1990, // year
	1, // month
	1, // day of the week (Monday)
	1, // day of the month
	0, // hour
	0, // min
	0, // sec
	0 // milli sec
};

class currentTime : public epicsTimerNotify {
public:
    currentTime ();
    ~currentTime ();
    void getCurrentTime ( epicsTimeStamp & dest );
    void startPLL ();
private:
    CRITICAL_SECTION mutex;
    LONGLONG epicsEpochInFileTime;
    LONGLONG lastPerfCounter;
    LONGLONG perfCounterFreq;
    LONGLONG epicsTimeLast; // nano-sec since the EPICS epoch
    LONGLONG perfCounterFreqPLL;
    LONGLONG lastPerfCounterPLL;
    LONGLONG lastFileTimePLL;
    epicsTimerQueueActive * pTimerQueue;
    epicsTimer * pTimer;
    bool perfCtrPresent;

    epicsTimerNotify::expireStatus expire ( const epicsTime & );
};

static currentTime * pCurrentTime = 0;
static bool osdTimeInitSuccess = false;
static epicsThreadOnceId osdTimeOnceFlag = EPICS_THREAD_ONCE_INIT;
static const LONGLONG FILE_TIME_TICKS_PER_SEC = 10000000;
static const LONGLONG EPICS_TIME_TICKS_PER_SEC = 1000000000;

//
// osdTimeInit ()
//
static void osdTimeInit ( void * )
{
    pCurrentTime = new currentTime ();

    // set here to avoid recursion problems
    osdTimeInitSuccess = true;

    pCurrentTime->startPLL ();
}

//
// epicsTimeGetCurrent ()
//
extern "C" epicsShareFunc int epicsShareAPI epicsTimeGetCurrent ( epicsTimeStamp *pDest )
{
    // double test avoids recursion problems
    if ( ! osdTimeInitSuccess ) {
        epicsThreadOnce ( & osdTimeOnceFlag, osdTimeInit, 0 );
        if ( ! ( osdTimeInitSuccess && pCurrentTime ) ) {
            return epicsTimeERROR;
        }
    }

    pCurrentTime->getCurrentTime ( *pDest );

	return epicsTimeOK;
}

//
// epicsTimeGetEvent ()
//
extern "C" epicsShareFunc int epicsShareAPI epicsTimeGetEvent 
            ( epicsTimeStamp *pDest, int eventNumber )
{
    if ( eventNumber == epicsTimeEventCurrentTime ) {
        return epicsTimeGetCurrent ( pDest );
    }
    else {
        return epicsTimeERROR;
    }
}

inline void UnixTimeToFileTime ( const time_t * pAnsiTime, LPFILETIME pft )
{
     // Note that LONGLONG is a 64-bit value
     LARGE_INTEGER ll;

     ll.QuadPart = Int32x32To64 ( *pAnsiTime, 10000000 ) + 116444736000000000;
     pft->dwLowDateTime = ll.LowPart;
     pft->dwHighDateTime = ( DWORD ) ll.HighPart;
}

static int daysInMonth[] = { 31, 28, 31, 30, 31, 30, 31,
    31, 30, 31, 30, 31 }; 

static bool isLeapYear ( DWORD year ) 
{
    if ( (year % 4) == 0 ) {
        if ( ( year % 100 ) != 0 || ( year % 400 ) == 0 ) {
            return true;         
        }
        else {
            return false;         
        }
    } else {                 
        return false;         
    } 
} 

static int dayOfYear ( DWORD day, DWORD month, DWORD year ) 
{
    DWORD nDays = 0;
    for ( unsigned m = 1; m < month; m++ ) {
        nDays += daysInMonth[m-1];
        if ( m == 2 && isLeapYear(year) ) {
            nDays++;
        }
    }
    return nDays + day;
}

// synthesize a reentrant gmtime on WIN32
int epicsTime_gmtime ( const time_t *pAnsiTime, struct tm *pTM )
{
    FILETIME ft;
    UnixTimeToFileTime ( pAnsiTime, &ft );

    SYSTEMTIME st;
    BOOL status = FileTimeToSystemTime ( &ft, &st );
    if ( ! status ) {
		return epicsTimeERROR;
    }

    pTM->tm_sec = st.wSecond; // seconds after the minute - [0,59]
    pTM->tm_min = st.wMinute; // minutes after the hour - [0,59]
    pTM->tm_hour = st.wHour; // hours since midnight - [0,23]
    assert ( st.wDay >= 1 && st.wDay <= 31 );
    pTM->tm_mday = st.wDay; // day of the month - [1,31]
    assert ( st.wMonth >= 1 && st.wMonth <= 12 );
    pTM->tm_mon = st.wMonth - 1; // months since January - [0,11]
    assert ( st.wYear >= 1900 );
    pTM->tm_year = st.wYear - 1900; // years since 1900
    pTM->tm_wday = st.wDayOfWeek; // days since Sunday - [0,6] 
    pTM->tm_yday = dayOfYear ( st.wDay, st.wMonth, st.wYear ) - 1; 
    pTM->tm_isdst = 0;

	return epicsTimeOK;
}

// synthesize a reentrant localtime on WIN32
int epicsTime_localtime ( const time_t *pAnsiTime, struct tm *pTM )
{
    FILETIME ft;
    UnixTimeToFileTime ( pAnsiTime, &ft );

    FILETIME lft;
    BOOL status = FileTimeToLocalFileTime ( &ft, &lft );
    if ( ! status ) {
		return epicsTimeERROR;
    }

    SYSTEMTIME st;
    status = FileTimeToSystemTime ( &lft, &st );
    if ( ! status ) {
		return epicsTimeERROR;
    }
    pTM->tm_sec = st.wSecond; // seconds after the minute - [0,59]
    pTM->tm_min = st.wMinute; // minutes after the hour - [0,59]
    pTM->tm_hour = st.wHour; // hours since midnight - [0,23]
    assert ( st.wDay >= 1 && st.wDay <= 31 );
    pTM->tm_mday = st.wDay; // day of the month - [1,31]
    assert ( st.wMonth >= 1 && st.wMonth <= 12 );
    pTM->tm_mon = st.wMonth - 1; // months since January - [0,11]
    assert ( st.wYear >= 1900 );
    pTM->tm_year = st.wYear - 1900; // years since 1900
    pTM->tm_wday = st.wDayOfWeek; // days since Sunday - [0,6] 
    pTM->tm_yday = dayOfYear ( st.wDay, st.wMonth, st.wYear ) - 1;

    TIME_ZONE_INFORMATION tzInfo;
    DWORD tzStatus = GetTimeZoneInformation ( &tzInfo );
    switch ( tzStatus ) {
    case TIME_ZONE_ID_UNKNOWN:
        pTM->tm_isdst = -1;
        break;
    case TIME_ZONE_ID_STANDARD:
        pTM->tm_isdst = 0;
        break;
    case TIME_ZONE_ID_DAYLIGHT:
        pTM->tm_isdst = 1;
        break;
    default:
        pTM->tm_isdst = -1;
        break;
    }

	return epicsTimeOK;
}

currentTime::currentTime () :
    epicsEpochInFileTime ( 0 ),
    lastPerfCounter ( 0 ),
    perfCounterFreq ( 0 ),
    epicsTimeLast ( 0 ),
    perfCounterFreqPLL ( 0 ),
    lastPerfCounterPLL ( 0 ),
    lastFileTimePLL ( 0 ),
    pTimerQueue ( 0 ),
    pTimer ( 0 ),
    perfCtrPresent ( false )
{
    FILETIME ft;
    {
	    SystemTimeToFileTime ( & epicsEpochST, & ft );
        LARGE_INTEGER tmp;
	    tmp.LowPart = ft.dwLowDateTime;
	    tmp.HighPart = ft.dwHighDateTime;
        this->epicsEpochInFileTime = tmp.QuadPart;
    }

    InitializeCriticalSection ( & this->mutex );

    // avoid interruptions by briefly becoming a time critical thread
    int originalPriority = GetThreadPriority ( GetCurrentThread () );
    SetThreadPriority ( GetCurrentThread (), THREAD_PRIORITY_TIME_CRITICAL );

    GetSystemTimeAsFileTime ( & ft );
    LARGE_INTEGER tmp;
	QueryPerformanceCounter ( & tmp );
    this->lastPerfCounter = tmp.QuadPart;
    // if no high resolution counters then default to low res file time
	if ( QueryPerformanceFrequency ( & tmp ) ) {
        this->perfCounterFreq = tmp.QuadPart;
        this->perfCtrPresent = true;
    }
    SetThreadPriority ( GetCurrentThread (), originalPriority );

    LARGE_INTEGER liFileTime;
    liFileTime.LowPart = ft.dwLowDateTime;
    liFileTime.HighPart = ft.dwHighDateTime;

    if ( liFileTime.QuadPart >= this->epicsEpochInFileTime ) {
        // the windows file time has a maximum resolution of 100 nS
        // and a nominal resolution of 10 mS - 16 mS
	    this->epicsTimeLast = 
            ( liFileTime.QuadPart - this->epicsEpochInFileTime ) * 
            ( EPICS_TIME_TICKS_PER_SEC / FILE_TIME_TICKS_PER_SEC );
    }
    else {
        errlogPrintf ( 
            "win32 osdTime.cpp detected questionable "
            "system date prior to EPICS epoch\n" );
        this->epicsTimeLast = 0;
    }

    this->perfCounterFreqPLL = this->perfCounterFreq;
    this->lastPerfCounterPLL = this->lastPerfCounter;
    this->lastFileTimePLL = liFileTime.QuadPart;

    // create frequency estimation timer when needed
    if ( this->perfCtrPresent ) {
        this->pTimerQueue =  
            & epicsTimerQueueActive::allocate ( true );
        this->pTimer = & this->pTimerQueue->createTimer ();
    }
}

currentTime::~currentTime ()
{
    DeleteCriticalSection ( & this->mutex );
    if ( this->pTimer ) {
        this->pTimer->destroy ();
    }
    if ( this->pTimerQueue ) {
        this->pTimerQueue->release ();
    }
}

void currentTime::getCurrentTime ( epicsTimeStamp & dest ) 
{
    if ( this->perfCtrPresent ) {
        EnterCriticalSection ( & this->mutex );

        LARGE_INTEGER curPerfCounter;
	    QueryPerformanceCounter ( & curPerfCounter );

        LONGLONG offset;
	    if ( curPerfCounter.QuadPart >= this->lastPerfCounter ) {	
            offset = curPerfCounter.QuadPart - this->lastPerfCounter;
        }
        else {
		    //
		    // must have been a timer roll-over event
            //
            // It takes 9.223372036855e+18/perf_freq sec to roll over this 
            // counter. This is currently about 245118 years using the perf
            // counter freq value on my system (1193182). Nevertheless, I
            // have code for this situation because the performance 
            // counter resolution will more than likely improve over time.
		    //
            offset = ( MAXLONGLONG - this->lastPerfCounter )
                                + ( curPerfCounter.QuadPart + MAXLONGLONG );
	    }
        if ( offset < MAXLONGLONG / EPICS_TIME_TICKS_PER_SEC ) {
            offset *= EPICS_TIME_TICKS_PER_SEC;
            offset /= this->perfCounterFreq;
        }
        else {
            double fpOffset = static_cast < double > ( offset );
            fpOffset *= EPICS_TIME_TICKS_PER_SEC;
            fpOffset /= static_cast < double > ( this->perfCounterFreq );
            offset = static_cast < LONGLONG > ( fpOffset );
        }
        LONGLONG epicsTimeCurrent = this->epicsTimeLast + offset;
        if ( this->epicsTimeLast > epicsTimeCurrent ) {
            double diff = this->epicsTimeLast - epicsTimeCurrent;
            errlogPrintf ( 
                "currentTime::getCurrentTime(): %f sec "
                "time discontinuity detected\n",
                diff );
        }
        this->epicsTimeLast = epicsTimeCurrent;
        this->lastPerfCounter = curPerfCounter.QuadPart;

        LeaveCriticalSection ( & this->mutex );

	    dest.secPastEpoch = static_cast < epicsUInt32 > 
            ( epicsTimeCurrent / EPICS_TIME_TICKS_PER_SEC );
        dest.nsec = static_cast < epicsUInt32 >
            ( epicsTimeCurrent % EPICS_TIME_TICKS_PER_SEC );
    }
    else {
        // if high resolution performance counters are not supported then 
        // fall back to low res file time
        FILETIME ft;
    	GetSystemTimeAsFileTime ( & ft );
        LARGE_INTEGER lift;
	    lift.LowPart = ft.dwLowDateTime;
	    lift.HighPart = ft.dwHighDateTime;

        if ( lift.QuadPart > this->epicsEpochInFileTime ) {
            LONGLONG fileTimeTicksSinceEpochEPICS = 
                lift.QuadPart - this->epicsEpochInFileTime;
	        dest.secPastEpoch = static_cast < epicsUInt32 > 
                ( fileTimeTicksSinceEpochEPICS / FILE_TIME_TICKS_PER_SEC );
            dest.nsec = static_cast < epicsUInt32 >
                ( fileTimeTicksSinceEpochEPICS * 100 );
        }
        else {
	        dest.secPastEpoch = 0;
            dest.nsec = 0;
        }
    }
}

//
// Maintain corrected version of the performance counter's frequency using
// a phase locked loop. This approach is similar to NTP's.
//
epicsTimerNotify::expireStatus currentTime::expire ( const epicsTime & )
{

    // avoid interruptions by briefly becoming a time critical thread
    LARGE_INTEGER curFileTime;
    LARGE_INTEGER curPerfCounter;
    {
        int originalPriority = GetThreadPriority ( GetCurrentThread () );
        SetThreadPriority ( GetCurrentThread (), THREAD_PRIORITY_TIME_CRITICAL );
        FILETIME ft;
        GetSystemTimeAsFileTime ( & ft );
	    QueryPerformanceCounter ( & curPerfCounter );
        SetThreadPriority ( GetCurrentThread (), originalPriority );

        curFileTime.LowPart = ft.dwLowDateTime;
	    curFileTime.HighPart = ft.dwHighDateTime;
    }

    EnterCriticalSection ( & this->mutex );

    LONGLONG perfCounterDiff = curPerfCounter.QuadPart - this->lastPerfCounterPLL;
	if ( curPerfCounter.QuadPart >= this->lastPerfCounter ) {	
        perfCounterDiff = curPerfCounter.QuadPart - this->lastPerfCounterPLL;
    }
    else {
		//
		// must have been a timer roll-over event
        //
        // It takes 9.223372036855e+18/perf_freq sec to roll over this 
        // counter. This is currently about 245118 years using the perf
        // counter freq value on my system (1193182). Nevertheless, I
        // have code for this situation because the performance 
        // counter resolution will more than likely improve over time.
		//
        perfCounterDiff = ( MAXLONGLONG - this->lastPerfCounterPLL )
                            + ( curPerfCounter.QuadPart + MAXLONGLONG );
	}
    this->lastPerfCounterPLL = curPerfCounter.QuadPart;

    LONGLONG fileTimeDiff = curFileTime.QuadPart - this->lastFileTimePLL;
    this->lastFileTimePLL = curFileTime.QuadPart;

    // discard glitches
    if ( fileTimeDiff == 0 ) {
        LeaveCriticalSection( & this->mutex );
        debugPrintf ( ( "currentTime: file time difference in PLL was zero\n" ) );
        return expireStatus ( restart, 1.0 /* sec */ );
    }

    LONGLONG freq = ( FILE_TIME_TICKS_PER_SEC * perfCounterDiff ) / fileTimeDiff; 
    LONGLONG delta = freq - this->perfCounterFreqPLL;

    // discard glitches
    LONGLONG bound = this->perfCounterFreqPLL >> 10;
    if ( delta < -bound || delta > bound ) {
        LeaveCriticalSection( & this->mutex );
        debugPrintf ( ( "freq est out of bounds l=%d e=%d h=%d\n",
            static_cast < int > ( -bound ),
            static_cast < int > ( delta ),
            static_cast < int > ( bound ) ) );
        return expireStatus ( restart, 1.0 /* sec */ );
    }

    // update feedback loop estimating the performance counter's frequency
    LONGLONG feedback = delta >> 8;
    this->perfCounterFreqPLL += feedback;

    LONGLONG perfCounterDiffSinceLastFetch;
	if ( curPerfCounter.QuadPart >= this->lastPerfCounter ) {	
        perfCounterDiffSinceLastFetch = 
            curPerfCounter.QuadPart - this->lastPerfCounter;
    }
    else {
		//
		// must have been a timer roll-over event
        //
        // It takes 9.223372036855e+18/perf_freq sec to roll over this 
        // counter. This is currently about 245118 years using the perf
        // counter freq value on my system (1193182). Nevertheless, I
        // have code for this situation because the performance 
        // counter resolution will more than likely improve over time.
		//
        perfCounterDiffSinceLastFetch = 
            ( MAXLONGLONG - this->lastPerfCounter )
                            + ( curPerfCounter.QuadPart + MAXLONGLONG );
	}

    // Update the current estimated time.
    this->epicsTimeLast += 
        ( perfCounterDiffSinceLastFetch * EPICS_TIME_TICKS_PER_SEC ) 
            / this->perfCounterFreq;
    this->lastPerfCounter = curPerfCounter.QuadPart;

    LONGLONG epicsTimeFromCurrentFileTime = 
        ( curFileTime.QuadPart - this->epicsEpochInFileTime )*
        ( EPICS_TIME_TICKS_PER_SEC / FILE_TIME_TICKS_PER_SEC );

    delta = epicsTimeFromCurrentFileTime - this->epicsTimeLast;
    if ( delta > EPICS_TIME_TICKS_PER_SEC || delta < -EPICS_TIME_TICKS_PER_SEC ) {
        // When there is an abrupt shift in the current computed time vs 
        // the time derived from the current file time then someone has 
        // probabably adjusted the real time clock and the best reaction 
        // is to just assume the new time base
  	    this->epicsTimeLast = epicsTimeFromCurrentFileTime;
 	    this->perfCounterFreq = this->perfCounterFreqPLL;
        debugPrintf ( ( "currentTime: did someone adjust the date?\n" ) );
    } 
    else {
        // update the effective performance counter frequency that will bring 
        // our calculated time base in syncy with file time one second from now.
    	this->perfCounterFreq = 
            ( EPICS_TIME_TICKS_PER_SEC * this->perfCounterFreqPLL )
                / ( delta + EPICS_TIME_TICKS_PER_SEC );

        // limit effective performance counter frequency rate of change
        LONGLONG lowLimit = this->perfCounterFreqPLL - bound;
        if ( this->perfCounterFreq < lowLimit ) {
            debugPrintf ( ( "currentTime: out of bounds low freq excursion %d\n",
                static_cast <int> ( lowLimit - this->perfCounterFreq ) ) );
            this->perfCounterFreq = lowLimit;
        } 
        else {
            LONGLONG highLimit = this->perfCounterFreqPLL + bound;
            if ( this->perfCounterFreq > highLimit ) {
                debugPrintf ( ( "currentTime: out of bounds high freq excursion %d\n",
                    static_cast <int> ( this->perfCounterFreq - highLimit ) ) );
                this->perfCounterFreq = highLimit;
            }
        }

#       if defined ( DEBUG ) 
            LARGE_INTEGER sysFreq;
	        QueryPerformanceFrequency ( & sysFreq );
            double freqDiff = static_cast <int> 
                ( this->perfCounterFreq - sysFreq.QuadPart );
            freqDiff /= sysFreq.QuadPart;
            freqDiff *= 100.0;
            double freqEstDiff = static_cast <int> 
                ( this->perfCounterFreqPLL - sysFreq.QuadPart );
            freqEstDiff /= sysFreq.QuadPart;
            freqEstDiff *= 100.0;
            debugPrintf ( ( "currentTime: freq delta %f %% freq est delta %f %% time delta %f sec\n", 
                freqDiff, freqEstDiff, static_cast < double > ( delta ) / EPICS_TIME_TICKS_PER_SEC ) );
#       endif
    }

    LeaveCriticalSection ( & this->mutex );

    return expireStatus ( restart, 1.0 /* sec */ );
}

void currentTime::startPLL ()
{
    this->pTimer->start ( *this, 1.0 );
}












