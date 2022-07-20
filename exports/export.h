///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef __SDRANGEL_EXPORT_H
#define __SDRANGEL_EXPORT_H

#if defined (__GNUC__) && (__GNUC__ >= 4)
#  define __SDR_EXPORT   __attribute__((visibility("default")))
#  define __SDR_IMPORT   __attribute__((visibility("default")))

#elif defined (_MSC_VER)
#  define __SDR_EXPORT   __declspec(dllexport)
#  define __SDR_IMPORT   __declspec(dllimport)

#else
#  define __SDR_EXPORT
#  define __SDR_IMPORT
#endif

/* The 'SDRBASE_API' controls the import/export of 'sdrbase' symbols and classes.
 */
#if !defined(sdrangel_STATIC)
#  if defined sdrbase_EXPORTS
#    define SDRBASE_API __SDR_EXPORT
#  else
#    define SDRBASE_API __SDR_IMPORT
#  endif
#else
#  define SDRBASE_API
#endif

/* the 'SDRGUI_API' controls the import/export of 'sdrgui' symbols
 */
#if !defined(sdrangel_STATIC)
#  ifdef sdrgui_EXPORTS
#    define SDRGUI_API __SDR_EXPORT
#  else
#    define SDRGUI_API __SDR_IMPORT
#  endif
#else
#   define SDRGUI_API
#endif

/* the 'SDRSRV_API' controls the import/export of 'sdrsrv' symbols
 */
#if !defined(sdrangel_STATIC)
#  ifdef sdrsrv_EXPORTS
#    define SDRSRV_API __SDR_EXPORT
#  else
#    define SDRSRV_API __SDR_IMPORT
#  endif
#else
#   define SDRSRV_API
#endif

/* the 'DEVICES_API' controls the import/export of 'devices' symbols
 */
#if !defined(sdrangel_STATIC)
#  ifdef devices_EXPORTS
#    define DEVICES_API __SDR_EXPORT
#  else
#    define DEVICES_API __SDR_IMPORT
#  endif
#else
#  define DEVICES_API
#endif

/* the 'HTTPSERVER_API' controls the import/export of 'httpserver' symbols
 */
#if !defined(sdrangel_STATIC)
#  ifdef httpserver_EXPORTS
#    define HTTPSERVER_API __SDR_EXPORT
#  else
#    define HTTPSERVER_API __SDR_IMPORT
#  endif
#else
#  define HTTPSERVER_API
#endif

/* the 'LOGGING_API' controls the import/export of 'logging' symbols
 */
#if !defined(sdrangel_STATIC)
#  ifdef logging_EXPORTS
#    define LOGGING_API __SDR_EXPORT
#  else
#    define LOGGING_API __SDR_IMPORT
#  endif
#else
#  define LOGGING_API
#endif

/* the 'QRTPLIB_API' controls the import/export of 'qrtplib' symbols
 */
#if !defined(sdrangel_STATIC)
#  ifdef qrtplib_EXPORTS
#    define QRTPLIB_API __SDR_EXPORT
#  else
#    define QRTPLIB_API __SDR_IMPORT
#  endif
#else
#  define QRTPLIB_API
#endif

/* the 'SWG_API' controls the import/export of 'swagger' symbols
 */
#if !defined(sdrangel_STATIC)
#  ifdef swagger_EXPORTS
#    define SWG_API __SDR_EXPORT
#  else
#    define SWG_API __SDR_IMPORT
#  endif
#else
#  define SWG_API
#endif

/* the 'SDRBENCH_API' controls the import/export of 'sdrbench' symbols
 */
#if !defined(sdrangel_STATIC)
#  ifdef sdrbench_EXPORTS
#    define SDRBENCH_API __SDR_EXPORT
#  else
#    define SDRBENCH_API __SDR_IMPORT
#  endif
#else
#  define SDRBENCH_API
#endif

/* the 'MODEMM17_API' controls the import/export of 'modemm17' symbols
 */
#if !defined(sdrangel_STATIC)
#  ifdef modemm17_EXPORTS
#    define MODEMM17_API __SDR_EXPORT
#  else
#    define MODEMM17_API __SDR_IMPORT
#  endif
#else
#  define MODEMM17_API
#endif

#endif /* __SDRANGEL_EXPORT_H */
