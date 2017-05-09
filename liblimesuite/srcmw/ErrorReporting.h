/**
@file ErrorReporting.h
@author Lime Microsystems
@brief API for reporting error codes and error messages.
All calls are thread-safe using thread-local storage.
Code returning with an error should use:
return lime::ReportError(code, message, ...);
*/

#ifndef LIMESUITE_ERROR_REPORTING_H
#define LIMESUITE_ERROR_REPORTING_H

#include <LimeSuiteConfig.h>
#include <cerrno>
#include <string>
#include <stdexcept>
#include <cstdarg>

namespace lime
{

/*!
 * Get the error code reported.
 */
LIME_API int GetLastError(void);

/*!
 * Get the error code to string + any optional message reported.
 */
LIME_API const char *GetLastErrorMessage(void);

/*!
 * Report a typical errno style error.
 * The resulting error message comes from strerror().
 * \param errnum a recognized error code
 * \return a non-zero status code to return
 */
LIME_API int ReportError(const int errnum);

/*!
 * Report an error as an integer code and a formatted message string.
 * \param errnum a recognized error code
 * \param format a format string followed by args
 * \return a non-zero status code to return
 */
inline int ReportError(const int errnum, const char *format, ...);

/*!
 * Report an error as a formatted message string.
 * The reported errnum is 0 - no relevant error code.
 * \param format a format string followed by args
 * \return a non-zero status code to return
 */
inline int ReportError(const char *format, ...);

/*!
 * Report an error as an integer code and message format arguments
 * \param errnum a recognized error code
 * \param format a printf-style format string
 * \param argList the format string args as a va_list
 * \return a non-zero status code to return
 */
LIME_API int ReportError(const int errnum, const char *format, va_list argList);

}

inline int lime::ReportError(const int errnum, const char *format, ...)
{
    va_list argList;
    va_start(argList, format);
    int status = lime::ReportError(errnum, format, argList);
    va_end(argList);
    return status;
}

inline int lime::ReportError(const char *format, ...)
{
    va_list argList;
    va_start(argList, format);
    int status = lime::ReportError(-1, format, argList);
    va_end(argList);
    return status;
}

#endif //LIMESUITE_ERROR_REPORTING_H
