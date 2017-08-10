/**
@file ErrorReporting.cpp
@author Lime Microsystems
@brief API for reporting error codes and error messages.
*/

#include "ErrorReporting.h"
#include <cstring> //strerror
#include <cstdio>

#ifdef _MSC_VER
    #define thread_local __declspec( thread )
    #include <Windows.h>
#endif

#ifdef __APPLE__
    #define thread_local __thread
#endif

#define MAX_MSG_LEN 1024
thread_local int _reportedErrorCode;
thread_local char _reportedErrorMessage[MAX_MSG_LEN];

static const char *errToStr(const int errnum)
{
    thread_local static char buff[MAX_MSG_LEN];
    #ifdef _MSC_VER
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errnum, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&buff, sizeof(buff), NULL);
    return buff;
    #else
    //http://linux.die.net/man/3/strerror_r
    #if ((_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && ! _GNU_SOURCE) || __APPLE__
    strerror_r(errnum, buff, sizeof(buff));
    #else
    //this version may decide to use its own internal string
    //return strerror_r(errnum, buff, sizeof(buff));
    return buff;
    #endif
    return buff;
    #endif
}

int lime::GetLastError(void)
{
    return _reportedErrorCode;
}

const char *lime::GetLastErrorMessage(void)
{
    return _reportedErrorMessage;
}

int lime::ReportError(const int errnum)
{
    return lime::ReportError(errnum, errToStr(errnum));
}

int lime::ReportError(const int errnum, const char *format, va_list argList)
{
    _reportedErrorCode = errnum;
    vsnprintf(_reportedErrorMessage, MAX_MSG_LEN, format, argList);
    return errnum;
}
