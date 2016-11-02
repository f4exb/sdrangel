/**
 * @file host_config.h.in
 *
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (c) 2013 Nuand LLC.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef HOST_CONFIG_H__
#define HOST_CONFIG_H__

//#define  BLADERF_OS_LINUX 0
//#define  BLADERF_OS_FREEBSD 0
//#define  BLADERF_OS_OSX 0
//#define  BLADERF_OS_WINDOWS 1
//#define  BLADERF_BIG_ENDIAN 0

#if !(BLADERF_OS_LINUX || BLADERF_OS_OSX || BLADERF_OS_WINDOWS || BLADERF_OS_FREEBSD)
#   error "Build not configured for any supported operating systems"
#endif

#if 1 < (BLADERF_OS_LINUX + BLADERF_OS_OSX + BLADERF_OS_WINDOWS + BLADERF_OS_FREEBSD)
#error "Build configured for multiple operating systems"
#endif


/*******************************************************************************
 * Endianness conversions
 *
 * HOST_TO_LE16         16-bit host endianness to little endian conversion
 * LE16_TO_HOST         16-bit little endian to host endianness conversion
 * HOST_TO_BE16         16-bit host endianness to big endian conversion
 * BE16_TO_HOST         16-bit big endian to host endianness conversion
 * HOST_TO_LE32         32-bit host endianness to little endian conversion
 * LE32_TO_HOST         32-bit little endian to host endianness conversion
 * HOST_TO_BE32         32-bit host endianness to big endian conversion
 * BE32_TO_HOST         32-bit big endian to host endianness conversion
 * HOST_TO_BE64         64-bit host endianness to big endian conversion
 * BE64_TO_HOST         64-bit big endian to host endianness conversion
 ******************************************************************************/

/*-----------------------
 * Linux
 *---------------------*/
#if BLADERF_OS_LINUX
#include <endian.h>

#define HOST_TO_LE16(val) htole16(val)
#define LE16_TO_HOST(val) le16toh(val)
#define HOST_TO_BE16(val) htobe16(val)
#define BE16_TO_HOST(val) be16toh(val)

#define HOST_TO_LE32(val) htole32(val)
#define LE32_TO_HOST(val) le32toh(val)
#define HOST_TO_BE32(val) htobe32(val)
#define BE32_TO_HOST(val) be32toh(val)

#define HOST_TO_LE64(val) htole64(val)
#define LE64_TO_HOST(val) le64toh(val)
#define HOST_TO_BE64(val) be64toh(val)
#define BE64_TO_HOST(val) be64toh(val)

/*-----------------------
 * FREEBSD
 *---------------------*/
#elif BLADERF_OS_FREEBSD
#include <sys/endian.h>

#define HOST_TO_LE16(val) htole16(val)
#define LE16_TO_HOST(val) le16toh(val)
#define HOST_TO_BE16(val) htobe16(val)
#define BE16_TO_HOST(val) be16toh(val)

#define HOST_TO_LE32(val) htole32(val)
#define LE32_TO_HOST(val) le32toh(val)
#define HOST_TO_BE32(val) htobe32(val)
#define BE32_TO_HOST(val) be32toh(val)

#define HOST_TO_LE64(val) htole64(val)
#define LE64_TO_HOST(val) le64toh(val)
#define HOST_TO_BE64(val) be64toh(val)
#define BE64_TO_HOST(val) be64toh(val)

/*-----------------------
 * Apple OSX
 *---------------------*/
#elif BLADERF_OS_OSX
#include <libkern/OSByteOrder.h>

#define HOST_TO_LE16(val) OSSwapHostToLittleInt16(val)
#define LE16_TO_HOST(val) OSSwapLittleToHostInt16(val)
#define HOST_TO_BE16(val) OSSwapHostToBigInt16(val)
#define BE16_TO_HOST(val) OSSwapBigToHostInt16(val)

#define HOST_TO_LE32(val) OSSwapHostToLittleInt32(val)
#define LE32_TO_HOST(val) OSSwapLittleToHostInt32(val)
#define HOST_TO_BE32(val) OSSwapHostToBigInt32(val)
#define BE32_TO_HOST(val) OSSwapBigToHostInt32(val)

#define HOST_TO_LE64(val) OSSwapHostToLittleInt64(val)
#define LE64_TO_HOST(val) OSSwapLittleToHostInt64(val)
#define HOST_TO_BE64(val) OSSwapHostToBigInt64(val)
#define BE64_TO_HOST(val) OSSwapBigToHostInt64(val)

/*-----------------------
 * Windows
 *---------------------*/
#elif BLADERF_OS_WINDOWS
#include <intrin.h>

/* We'll assume little endian for Windows platforms:
 * http://blogs.msdn.com/b/larryosterman/archive/2005/06/07/426334.aspx
 */
#define HOST_TO_LE16(val) (val)
#define LE16_TO_HOST(val) (val)
#define HOST_TO_BE16(val) _byteswap_ushort(val)
#define BE16_TO_HOST(val) _byteswap_ushort(val)

#define HOST_TO_LE32(val) (val)
#define LE32_TO_HOST(val) (val)
#define HOST_TO_BE32(val) _byteswap_ulong(val)
#define BE32_TO_HOST(val) _byteswap_ulong(val)


#define HOST_TO_LE64(val) (val)
#define LE64_TO_HOST(val) (val)
#define HOST_TO_BE64(val) _byteswap_uint64(val)
#define BE64_TO_HOST(val) _byteswap_uint64(val)

/*-----------------------
 * Unsupported or bug
 *---------------------*/
#else
#error "Encountered an OS that we do not have endian wrappers for?"
#endif

/*******************************************************************************
 * Miscellaneous Linux fixups
 ******************************************************************************/
#if BLADERF_OS_LINUX

/* Added here just to keep this out of the other source files. We'll have
 * a few Windows replacements for unistd.h items. */
#include <unistd.h>
#include <pwd.h>

/*******************************************************************************
 * Miscellaneous FREEBSD fixups
 ******************************************************************************/
#elif BLADERF_OS_FREEBSD

#include <unistd.h>
#include <pwd.h>
#include <sys/sysctl.h>

/*******************************************************************************
 * Miscellaneous OSX fixups
 ******************************************************************************/
#elif BLADERF_OS_OSX

#include <unistd.h>
#include <pwd.h>

/*******************************************************************************
 * Miscellaneous Windows fixups
 ******************************************************************************/
#elif BLADERF_OS_WINDOWS

/* Rename a few functions and types */
#include <windows.h>
#include <io.h>
#define usleep(x)      Sleep((x+999)/1000)

/* Changes specific to Microsoft Visual Studio and its C89-compliant ways */
#if _MSC_VER
#   define strtok_r    strtok_s
#   define strtoull    _strtoui64
#   define snprintf    _snprintf
#   define vsnprintf   _vsnprintf
#   define strcasecmp  _stricmp
#   define strncasecmp _strnicmp
#   define fileno      _fileno
#   define strdup      _strdup
#   define access      _access

/* ssize_t lives elsewhere */
#   include <BaseTsd.h>
#   define ssize_t SSIZE_T

/* With msvc, inline is only available for C++. Otherwise we need to use __inline:
 *  http://msdn.microsoft.com/en-us/library/z8y1yy88.aspx */
#   if !defined(__cplusplus)
#       define inline __inline
#   endif

#endif // _MSC_VER

#endif

/* Windows (msvc) does not support C99 designated initializers.
 *
 * Therefore, the following macro should be used. However, note that you'll
 * need to be sure to keep your elements in order to avoid breaking Windows
 * portability!
 *
 * http://stackoverflow.com/questions/5440611/how-to-rewrite-c-struct-designated-initializers-to-c89-resp-msvc-c-compiler
 *
 * Here's a sample regexep you could use in vim to clean these up in your code:
 *  @\(\s\+\)\(\.[a-zA-Z0-9_]\+\)\s*=\s*\([a-zA-Z0-9_]\+\)\(,\)\?@\1FIELD_INIT(\2,\3)\4@gc
 */
#if _MSC_VER
#   define FIELD_INIT(field, ...) __VA_ARGS__
#else
#   define FIELD_INIT(field, ...) field = __VA_ARGS__
#endif // _MSC_VER

/*******************************************************************************
 * Miscellaneous utility macros
 ******************************************************************************/
#define ARRAY_SIZE(n) (sizeof(n) / sizeof(n[0]))

#endif
