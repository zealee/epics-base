/* epicsTime.h */
/* Author Jeffrey O. Hill */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_Combined file included as part
of this distribution.
****************************************************************************/
#ifndef epicsTimehInclude
#define epicsTimehInclude

#include <time.h>

#include "shareLib.h"
#include "epicsTypes.h"
#include "epicsAssert.h"
#include "osdTime.h"
#ifdef __cplusplus
#include "locationException.h"
#endif /* __cplusplus */

/* epics time stamp for C interface*/
typedef struct epicsTimeStamp {
    epicsUInt32    secPastEpoch;   /* seconds since 0000 Jan 1, 1990 */
    epicsUInt32    nsec;           /* nanoseconds within second */
} epicsTimeStamp;

/*TS_STAMP is deprecated */
#define TS_STAMP epicsTimeStamp


struct timespec; /* POSIX real time */
struct timeval; /* BSD */

/* Network Time Protocal Timestamp */
struct ntpTimeStamp {
    epicsUInt32 l_ui; /* sec past NTP epoch */
    epicsUInt32 l_uf; /* fractional seconds */
};

#ifdef __cplusplus

class aitTimeStamp; /* GDD*/

// extend ANSI C RTL "struct tm" to include nano seconds within a second
// and a struct tm that is adjusted for the local timezone
struct local_tm_nano_sec {
    struct tm ansi_tm; /* ANSI C time details */
    unsigned long nSec; /* nano seconds extension */
};

// extend ANSI C RTL "struct tm" to includes nano seconds within a second
// and a struct tm that is adjusted for GMT (UTC)
struct gm_tm_nano_sec {
    struct tm ansi_tm; /* ANSI C time details */
    unsigned long nSec; /* nano seconds extension */
};

// wrapping this in a struct allows conversion to and
// from ANSI time_t but does not allow unexpected
// conversions to occur
struct time_t_wrapper {
    time_t ts;
};

class epicsTime;

class epicsShareClass epicsTimeEvent
{
    friend class epicsTime;
public:
    epicsTimeEvent (const int &eventName);
private:
    int eventNumber;
};

class epicsShareClass epicsTime
{
public:
    // exceptions
    class unableToFetchCurrentTime {};
    class formatProblemWithStructTM {};

    epicsTime ();
    epicsTime (const epicsTime &t);

    static epicsTime getEvent (const epicsTimeEvent &event);
    static epicsTime getCurrent ();

    // convert to and from EPICS epicsTimeStamp format
    operator epicsTimeStamp () const;
    epicsTime (const epicsTimeStamp &ts);
    epicsTime operator = (const epicsTimeStamp &rhs);

    // convert to and from ANSI time_t 
    operator time_t_wrapper () const;
    epicsTime (const time_t_wrapper &tv);
    epicsTime operator = (const time_t_wrapper &rhs);

    // convert to and from ANSI Cs "struct tm" (with nano seconds)
    // adjusted for the local time zone
    operator local_tm_nano_sec () const;
    epicsTime (const local_tm_nano_sec &ts);
    epicsTime operator = (const local_tm_nano_sec &rhs);

    // convert to ANSI Cs "struct tm" (with nano seconds)
    // adjusted for GM time (UTC)
    operator gm_tm_nano_sec () const;

    // convert to and from POSIX RTs "struct timespec"
    operator struct timespec () const;
    epicsTime (const struct timespec &ts);
    epicsTime operator = (const struct timespec &rhs);

    // convert to and from BSDs "struct timeval"
    operator struct timeval () const;
    epicsTime (const struct timeval &ts);
    epicsTime operator = (const struct timeval &rhs);

    // convert to and from NTP timestamp format
    operator ntpTimeStamp () const;
    epicsTime (const ntpTimeStamp &ts);
    epicsTime operator = (const ntpTimeStamp &rhs);

    // convert to and from GDDs aitTimeStamp format
    operator aitTimeStamp () const;
    epicsTime (const aitTimeStamp &ts);
    epicsTime operator = (const aitTimeStamp &rhs);

    // arithmetic operators
    double operator- (const epicsTime &rhs) const; // returns seconds
    epicsTime operator+ (const double &rhs) const; // add rhs seconds
    epicsTime operator- (const double &rhs) const; // subtract rhs seconds
    epicsTime operator+= (const double &rhs); // add rhs seconds
    epicsTime operator-= (const double &rhs); // subtract rhs seconds

    // comparison operators
    bool operator == (const epicsTime &rhs) const;
    bool operator != (const epicsTime &rhs) const;
    bool operator <= (const epicsTime &rhs) const;
    bool operator < (const epicsTime &rhs) const;
    bool operator >= (const epicsTime &rhs) const;
    bool operator > (const epicsTime &rhs) const;

    // convert current state to user-specified string
    size_t strftime (char *pBuff, size_t bufLength, const char *pFormat) const;

    // dump current state to standard out
    void show (unsigned interestLevel) const;

    // useful public constants
    static const unsigned secPerMin;
    static const unsigned mSecPerSec;
    static const unsigned uSecPerSec;
    static const unsigned nSecPerSec;
    static const unsigned nSecPerUSec;

    // depricated
    static void synchronize ();
private:
    // private because:
    // a) application does not break when EPICS epoch is changed
    // b) no assumptions about internal storage or internal precision
    // in the application
    // c) it would be easy to forget which argument is nanoseconds
    // and which argument is seconds (no help from compiler)
    //
    epicsTime (const unsigned long secPastEpoch, const unsigned long nSec);
    void addNanoSec (long nanoSecAdjust);

    unsigned long secPastEpoch; // seconds since O000 Jan 1, 1990 
    unsigned long nSec; // nanoseconds within second 
};

extern "C" {
#endif /* __cplusplus */

/* All epicsTime routines return (-1,0) for (failure,success) */
#define epicsTimeOK 0
#define epicsTimeERROR (-1)

/*Some special values for eventNumber*/
#define epicsTimeEventCurrentTime 0
#define epicsTimeEventBestTime -1
#define epicsTimeEventDeviceTime -2

/* convert to and from ANSI C's "time_t" */
epicsShareFunc int epicsShareAPI epicsTimeGetCurrent (epicsTimeStamp *pDest);
epicsShareFunc int epicsShareAPI epicsTimeGetEvent (
    epicsTimeStamp *pDest, int eventNumber);

/* convert to and from ANSI C's "struct tm" with nano second */
epicsShareFunc int epicsShareAPI epicsTimeToTime_t (
    time_t *pDest, const epicsTimeStamp *pSrc);
epicsShareFunc int epicsShareAPI epicsTimeFromTime_t (
    epicsTimeStamp *pDest, time_t src);

/*convert to and from ANSI C's "struct tm" with nano seconds */
epicsShareFunc int epicsShareAPI epicsTimeToTM (
    struct tm *pDest, unsigned long *pNSecDest, const epicsTimeStamp *pSrc);
epicsShareFunc int epicsShareAPI epicsTimeToGMTM (
    struct tm *pDest, unsigned long *pNSecDest, const epicsTimeStamp *pSrc);
epicsShareFunc int epicsShareAPI epicsTimeFromTM (
    epicsTimeStamp *pDest, const struct tm *pSrc, unsigned long nSecSrc);

/* convert to and from POSIX RT's "struct timespec" */
epicsShareFunc int epicsShareAPI epicsTimeToTimespec (
    struct timespec *pDest, const epicsTimeStamp *pSrc);
epicsShareFunc int epicsShareAPI epicsTimeFromTimespec (
    epicsTimeStamp *pDest, const struct timespec *pSrc);

/* convert to and from BSD's "struct timeval" */
epicsShareFunc int epicsShareAPI epicsTimeToTimeval (
    struct timeval *pDest, const epicsTimeStamp *pSrc);
epicsShareFunc int epicsShareAPI epicsTimeFromTimeval (
    epicsTimeStamp *pDest, const struct timeval *pSrc);

/*arithmetic operations */
epicsShareFunc double epicsShareAPI epicsTimeDiffInSeconds (
    const epicsTimeStamp *pLeft, const epicsTimeStamp *pRight);/* left - right */
epicsShareFunc void epicsShareAPI epicsTimeAddSeconds (
    epicsTimeStamp *pDest, double secondsToAdd); /* adds seconds to *pDest */

/*comparison operations: returns (0,1) if (false,true) */
epicsShareFunc int epicsShareAPI epicsTimeEqual (
    const epicsTimeStamp *pLeft, const epicsTimeStamp *pRight);
epicsShareFunc int epicsShareAPI epicsTimeNotEqual (
    const epicsTimeStamp *pLeft, const epicsTimeStamp *pRight);
epicsShareFunc int epicsShareAPI epicsTimeLessThan (
    const epicsTimeStamp *pLeft, const epicsTimeStamp *pRight); /*true if left < right */
epicsShareFunc int epicsShareAPI epicsTimeLessThanEqual (
    const epicsTimeStamp *pLeft, const epicsTimeStamp *pRight); /*true if left <= right) */
epicsShareFunc int epicsShareAPI epicsTimeGreaterThan (
    const epicsTimeStamp *pLeft, const epicsTimeStamp *pRight); /*true if left < right */
epicsShareFunc int epicsShareAPI epicsTimeGreaterThanEqual (
    const epicsTimeStamp *pLeft, const epicsTimeStamp *pRight); /*true if left < right */

/*convert to ASCII string */
epicsShareFunc size_t epicsShareAPI epicsTimeToStrftime (
    char *pBuff, size_t bufLength, const char *pFormat, const epicsTimeStamp *pTS);

/* dump current state to standard out */
epicsShareFunc void epicsShareAPI epicsTimeShow (
    const epicsTimeStamp *, unsigned interestLevel);

/* OS dependent reentrant versions of the ANSI C interface because */
/* vxWorks gmtime_r interface does not match POSIX standards */
int epicsTime_localtime ( const time_t *clock, struct tm *result );
int epicsTime_gmtime ( const time_t *clock, struct tm *result );

#ifdef __cplusplus
}
#endif /* __cplusplus */

/* epicsTime inline member functions ,*/
#ifdef __cplusplus

inline epicsTime epicsTime::operator - (const double &rhs) const
{
    return epicsTime::operator + (-rhs);
}

inline epicsTime epicsTime::operator += (const double &rhs)
{
    *this = epicsTime::operator + (rhs);
    return *this;
}

inline epicsTime epicsTime::operator -= (const double &rhs)
{
    *this = epicsTime::operator + (-rhs);
    return *this;
}

inline bool epicsTime::operator == (const epicsTime &rhs) const
{
    if (this->secPastEpoch == rhs.secPastEpoch && this->nSec == rhs.nSec) {
    	return true;
    }
    return false;
}

inline bool epicsTime::operator != (const epicsTime &rhs) const
{
    return !epicsTime::operator == (rhs);
}

inline bool epicsTime::operator >= (const epicsTime &rhs) const
{
    return ! ( *this < rhs );
}

inline bool epicsTime::operator > (const epicsTime &rhs) const
{
    return ! ( *this <= rhs );
}

inline epicsTime epicsTime::operator = (const local_tm_nano_sec &rhs)
{
    return *this = epicsTime (rhs);
}

inline epicsTime epicsTime::operator = (const struct timespec &rhs)
{
    *this = epicsTime (rhs);
    return *this;
}

inline epicsTime epicsTime::operator = (const aitTimeStamp &rhs)
{
    *this = epicsTime (rhs);
    return *this;
}

inline epicsTime epicsTime::operator = (const epicsTimeStamp &rhs)
{
    *this = epicsTime (rhs);
    return *this;
}

inline epicsTime::operator epicsTimeStamp () const
{
    epicsTimeStamp ts;
    ts.secPastEpoch = this->secPastEpoch;
    ts.nsec = this->nSec;
    return ts;
}

#ifdef NTP_SUPPORT
inline epicsTime epicsTime::operator = (const ntpTimeStamp &rhs)
{
    *this = epicsTime (rhs);
    return *this;
}
#endif

inline epicsTime epicsTime::operator = (const time_t_wrapper &rhs)
{
    *this = epicsTime (rhs);
    return *this;
}
#endif /* __cplusplus */

#endif /* epicsTimehInclude */

