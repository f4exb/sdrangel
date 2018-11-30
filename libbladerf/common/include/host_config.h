/**
 * @file host_config.h.in
 *
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (c) 2013-2018 Nuand LLC.
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

#define  BLADERF_OS_LINUX 1
#define  BLADERF_OS_FREEBSD 0
#define  BLADERF_OS_OSX 0
#define  BLADERF_OS_WINDOWS 0
#define  BLADERF_BIG_ENDIAN 0
#define  LIBBLADERF_SEARCH_PREFIX "/opt/install/libbladeRF"

#if !(BLADERF_OS_LINUX || BLADERF_OS_OSX || BLADERF_OS_WINDOWS || BLADERF_OS_FREEBSD)
#   error "Build not configured for any supported operating systems"
#endif

#if 1 < (BLADERF_OS_LINUX + BLADERF_OS_OSX + BLADERF_OS_WINDOWS + BLADERF_OS_FREEBSD)
#error "Build configured for multiple operating systems"
#endif

#define  HAVE_CLOCK_GETTIME 1

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
 * Endianness conversions for constants
 *
 * We shouldn't be using the above items for assigning constants because the
 * implementations may be functions [1].
 *
 * [1] https://sourceware.org/bugzilla/show_bug.cgi?id=17679#c7
 ******************************************************************************/

#define BLADERF_BYTESWAP16(x) ((((x) & 0xff00) >> 8) | (((x) & 0x00ff) << 8))

#define BLADERF_BYTESWAP32(x) ((((x) & 0xff000000) >> 24) | \
                               (((x) & 0x00ff0000) >>  8) | \
                               (((x) & 0x0000ff00) <<  8) | \
                               (((x) & 0x000000ff) << 24) )

#define BLADERF_BYTESWAP64(x) ((((x) & 0xff00000000000000llu) >> 56) | \
                               (((x) & 0x00ff000000000000llu) >> 40) | \
                               (((x) & 0x0000ff0000000000llu) >> 24) | \
                               (((x) & 0x000000ff00000000llu) >>  8) | \
                               (((x) & 0x00000000ff000000llu) <<  8) | \
                               (((x) & 0x0000000000ff0000llu) << 24) | \
                               (((x) & 0x000000000000ff00llu) << 40) | \
                               (((x) & 0x00000000000000ffllu) << 56) | \

#if BLADERF_BIG_ENDIAN
#   define HOST_TO_LE16_CONST(x) (BLADERF_BYTESWAP16(x))
#   define LE16_TO_HOST_CONST(x) (BLADERF_BYTESWAP16(x))
#   define HOST_TO_BE16_CONST(x) (x)
#   define BE16_TO_HOST_CONST(x) (x)

#   define HOST_TO_LE32_CONST(x) (BLADERF_BYTESWAP32(x))
#   define LE32_TO_HOST_CONST(x) (BLADERF_BYTESWAP32(x))
#   define HOST_TO_BE32_CONST(x) (x)
#   define BE32_TO_HOST_CONST(x) (x)

#   define HOST_TO_LE64_CONST(x) (BLADERF_BYTESWAP64(x))
#   define LE64_TO_HOST_CONST(x) (BLADERF_BYTESWAP64(x))
#   define HOST_TO_BE64_CONST(x) (x)
#   define BE64_TO_HOST_CONST(x) (x)
#else
#   define HOST_TO_LE16_CONST(x) (x)
#   define LE16_TO_HOST_CONST(x) (x)
#   define HOST_TO_BE16_CONST(x) (BLADERF_BYTESWAP16(x))
#   define BE16_TO_HOST_CONST(x) (BLADERF_BYTESWAP16(x))

#   define HOST_TO_LE32_CONST(x) (x)
#   define LE32_TO_HOST_CONST(x) (x)
#   define HOST_TO_BE32_CONST(x) (BLADERF_BYTESWAP32(x))
#   define BE32_TO_HOST_CONST(x) (BLADERF_BYTESWAP32(x))

#   define HOST_TO_LE64_CONST(x) (x)
#   define LE64_TO_HOST_CONST(x) (x)
#   define HOST_TO_BE64_CONST(x) (BLADERF_BYTESWAP64(x))
#   define BE64_TO_HOST_CONST(x) (BLADERF_BYTESWAP64(x))
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
#include <direct.h>
#define usleep(x)      Sleep((x+999)/1000)

/* Changes specific to Microsoft Visual Studio and its C89-compliant ways */
#if _MSC_VER
#   define strtok_r    strtok_s
#   define strtoull    _strtoui64
#if _MSC_VER < 1900
#   define snprintf    _snprintf
#   define vsnprintf   _vsnprintf
#else
#define STDC99
#endif
#   define strcasecmp  _stricmp
#   define strncasecmp _strnicmp
#   define fileno      _fileno
#   define strdup      _strdup
#   define access      _access
#   define chdir       _chdir
#   define getcwd      _getcwd
#   define rmdir       _rmdir
#   define unlink      _unlink
#   define mkdir(n, m) _mkdir(n)

/* ssize_t lives elsewhere */
#   include <BaseTsd.h>
#   define ssize_t SSIZE_T

/* With msvc, inline is only available for C++. Otherwise we need to use __inline:
 *  http://msdn.microsoft.com/en-us/library/z8y1yy88.aspx */
#   if !defined(__cplusplus)
#       define inline __inline
#   endif

/* Visual studio 2012 compiler (v17.00.XX) doesn't have round functions */
#   if _MSC_VER <= 1700
#       include <math.h>
        static inline float roundf(float x)
        {
            return x >= 0.0f ? floorf(x + 0.5f) : ceilf(x - 0.5f);
        }
        static inline double round(double x)
        {
            return x >= 0.0 ? floor(x + 0.5) : ceil(x - 0.5);
        }
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

/**
 * GCC 7+ warns on implicit fallthroughs in switch statements
 * (-Wimplicit-fallthrough), which we use in a couple places for simplicity.
 * The statement attribute syntax used to suppress this warning is not
 * supported by anything else.
 */
#if __GNUC__ >= 7
#define EXPLICIT_FALLTHROUGH __attribute__((fallthrough))
#else
#define EXPLICIT_FALLTHROUGH
#endif  // __GNUC__ >= 7

#endif
