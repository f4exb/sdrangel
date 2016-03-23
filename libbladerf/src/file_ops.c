/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2013-2014 Nuand LLC
 * Copyright (C) 2013 Daniel Gröber <dxld ÄT darkboxed DOT org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <libbladeRF.h>
#include "host_config.h"
#include "file_ops.h"
#include "minmax.h"
#include "log.h"
#include "rel_assert.h"

#define LOCAL_BLADERF_OS_LINUX 1

/* Paths to search for bladeRF files */
struct search_path_entries {
    bool prepend_home;
    const char *path;
};

int file_write(FILE *f, uint8_t *buf, size_t len)
{
    size_t rv;

    rv = fwrite(buf, 1, len, f);
    if(rv < len) {
        log_debug("File write failed: %s\n", strerror(errno));
        return BLADERF_ERR_IO;
    }

    return 0;
}

int file_read(FILE *f, char *buf, size_t len)
{
    size_t rv;

    rv = fread(buf, 1, len, f);
    if(rv < len) {
        if(feof(f))
            log_debug("Unexpected end of file: %s\n", strerror(errno));
        else
            log_debug("Error reading file: %s\n", strerror(errno));

        return BLADERF_ERR_IO;
    }

    return 0;
}

ssize_t file_size(FILE *f)
{
    ssize_t rv = BLADERF_ERR_IO;
    long int fpos = ftell(f);
    long len;

    if(fpos < 0) {
        log_verbose("ftell failed: %s\n", strerror(errno));
        goto out;
    }

    if(fseek(f, 0, SEEK_END)) {
        log_verbose("fseek failed: %s\n", strerror(errno));
        goto out;
    }

    len = ftell(f);
    if(len < 0) {
        log_verbose("ftell failed: %s\n", strerror(errno));
        goto out;
    } else if (len == LONG_MAX) {
        log_debug("ftell called with a directory?\n");
        goto out;
    }

    if(fseek(f, fpos, SEEK_SET)) {
        log_debug("fseek failed: %s\n", strerror(errno));
        goto out;
    }

    rv = (ssize_t) len;
    assert(rv == len);

out:
    return rv;
}

int file_read_buffer(const char *filename, uint8_t **buf_ret, size_t *size_ret)
{
    int status = BLADERF_ERR_UNEXPECTED;
    FILE *f;
    uint8_t *buf = NULL;
    ssize_t len;

    f = fopen(filename, "rb");
    if (!f) {
        switch (errno) {
            case ENOENT:
                return BLADERF_ERR_NO_FILE;

            case EACCES:
                return BLADERF_ERR_PERMISSION;

            default:
                return BLADERF_ERR_IO;
        }
    }

    len = file_size(f);
    if(len < 0) {
        status = BLADERF_ERR_IO;
        goto out;
    }

    buf = (uint8_t*) malloc(len);
    if (!buf) {
        status = BLADERF_ERR_MEM;
        goto out;
    }

    status = file_read(f, (char*)buf, len);
    if (status < 0) {
        goto out;
    }

    *buf_ret = buf;
    *size_ret = len;
    fclose(f);
    return 0;

out:
    free(buf);
    if (f) {
        fclose(f);
    }

    return status;
}

/* Remove the last entry in a path. This is used to strip the executable name
* from a path to get the directory that the executable resides in. */
static size_t strip_last_path_entry(char *buf, char dir_delim)
{
    size_t len = strlen(buf);

    while (len > 0 && buf[len - 1] != dir_delim) {
        buf[len - 1] = '\0';
        len--;
    }

    return len;
}

#if LOCAL_BLADERF_OS_LINUX || LOCAL_BLADERF_OS_OSX || LOCAL_BLADERF_OS_FREEBSD
#define ACCESS_FILE_EXISTS F_OK
#define DIR_DELIMETER '/'

static const struct search_path_entries search_paths[] = {
    { false, "" },
    { true,  "/.config/Nuand/bladeRF/" },
    { true,  "/.Nuand/bladeRF/" },

    /* LIBBLADERF_SEARCH_PATH_PREFIX is defined in the libbladeRF
     * CMakeLists.txt file. It defaults to ${CMAKE_INSTALL_PREFIX}, but
     * can be overridden via -DLIBBLADERF_SEARCH_PATH_OVERRIDE
     */
    // { false, LIBBLADERF_SEARCH_PREFIX "/etc/Nuand/bladeRF/" },
    // { false, LIBBLADERF_SEARCH_PREFIX "/share/Nuand/bladeRF/" },

    /* These two entries are here for reverse compatibility.
     *
     * We failed to prefix ${CMAKE_INSTALL_PREFIX} on these from the beginning,
     * forcing package maintainers to hard-code one of these locations,
     * despite having a different ${CMAKE_INSTALL_PREFIX}.
     *
     * We'll keep these around for some time as fall-backs, as not to break
     * existing packaging scripts.
     */
    { false, "/etc/Nuand/bladeRF/" },
    { false, "/usr/share/Nuand/bladeRF/" },
};

static inline size_t get_home_dir(char *buf, size_t max_len)
{
    // const char *home;
    //
    // home = getenv("HOME");
    // if (home != NULL && strlen(home) > 0 && strlen(home) < max_len) {
    //     strncat(buf, home, max_len);
    // } else {
    //     const struct passwd *passwd;
    //     const uid_t uid = getuid();
    //     passwd = getpwuid(uid);
    //     strncat(buf, passwd->pw_dir, max_len);
    // }
    strcpy(buf, "/");

    return strlen(buf);
}

static inline size_t get_install_dir(char *buf, size_t max_len)
{
	return 0;
}

#if LOCAL_BLADERF_OS_LINUX
static inline size_t get_binary_dir(char *buf, size_t max_len)
{
    return 0;

    // ssize_t result = readlink("/proc/self/exe", buf, max_len);
    //
    // if (result > 0) {
    //     return strip_last_path_entry(buf, DIR_DELIMETER);
    // } else {
    //     return 0;
    // }
}
#elif LOCAL_BLADERF_OS_FREEBSD
static inline size_t get_binary_dir(char *buf, size_t max_len)
{
    int mib[4];
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PATHNAME;
    mib[3] = -1;
    ssize_t result = sysctl(mib, 4, buf, &max_len, NULL, 0);

    if (result > 0) {
        return strip_last_path_entry(buf, DIR_DELIMETER);
    } else {
        return 0;
    }
}
#elif LOCAL_BLADERF_OS_OSX
#include <mach-o/dyld.h>
static inline size_t get_binary_dir(char *buf, size_t max_len)
{
    uint32_t buf_size = max_len;
    int status = _NSGetExecutablePath(buf, &buf_size);

    if (status == 0) {
        return strip_last_path_entry(buf, DIR_DELIMETER);
    } else {
        return 0;
    }

}
#endif

#elif LOCAL_BLADERF_OS_WINDOWS
#define ACCESS_FILE_EXISTS 0
#define DIR_DELIMETER '\\'
#include <shlobj.h>

static const struct search_path_entries search_paths[] = {
    { false, "" },
    { true,  "/Nuand/bladeRF/" },
};

static inline size_t get_home_dir(char *buf, size_t max_len)
{
    /* Explicitly link to a runtime DLL to get SHGetKnownFolderPath.
     * This deals with the case where we might not be able to staticly
     * link it at build time, e.g. mingw32.
     *
     * http://msdn.microsoft.com/en-us/library/784bt7z7.aspx
     */
    typedef HRESULT (CALLBACK* LPFNSHGKFP_T)(REFKNOWNFOLDERID, DWORD, HANDLE, PWSTR*);
    HINSTANCE hDLL;                         // Handle to DLL
    LPFNSHGKFP_T lpfnSHGetKnownFolderPath;  // Function pointer

    const KNOWNFOLDERID folder_id = FOLDERID_RoamingAppData;
    PWSTR path;
    HRESULT status;

    assert(max_len < INT_MAX);

    hDLL = LoadLibrary("Shell32");
    if (hDLL == NULL) {
        // DLL couldn't be loaded, bail out.
        return 0;
    }

    lpfnSHGetKnownFolderPath = (LPFNSHGKFP_T)GetProcAddress(hDLL, "SHGetKnownFolderPath");

    if (!lpfnSHGetKnownFolderPath) {
        // Can't find the procedure we want.  Free and bail.
        FreeLibrary(hDLL);
        return 0;
    }

    status = lpfnSHGetKnownFolderPath(&folder_id, 0, NULL, &path);
    if (status == S_OK) {
        WideCharToMultiByte(CP_ACP, 0, path, -1, buf, (int)max_len, NULL, NULL);
        CoTaskMemFree(path);
    }

    FreeLibrary(hDLL);

    return strlen(buf);
}

static inline size_t get_binary_dir(char *buf, size_t max_len)
{
    DWORD status;

    assert(max_len <= MAXDWORD);
    status = GetModuleFileName(NULL, buf, (DWORD) max_len);

    if (status > 0) {
        return strip_last_path_entry(buf, DIR_DELIMETER);
    } else {
        return 0;
    }
}

static inline size_t get_install_dir(char *buf, size_t max_len)
{
    typedef LONG (CALLBACK* LPFNREGOPEN_T)(HKEY, LPCTSTR, DWORD, REGSAM, PHKEY);
    typedef LONG (CALLBACK* LPFNREGQUERY_T)(HKEY, LPCTSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
    typedef LONG (CALLBACK* LPFNREGCLOSE_T)(HKEY);

    HINSTANCE hDLL;                         // Handle to DLL
    LPFNREGOPEN_T  lpfnRegOpenKeyEx;        // Function pointer
    LPFNREGQUERY_T lpfnRegQueryValueEx;     // Function pointer
    LPFNREGCLOSE_T lpfnRegCloseKey;         // Function pointer
    HKEY hk;
    DWORD len;

    assert(max_len < INT_MAX);

    memset(buf, 0, max_len);
    hDLL = LoadLibrary("Advapi32");
    if (hDLL == NULL) {
        // DLL couldn't be loaded, bail out.
        return 0;
    }

    lpfnRegOpenKeyEx = (LPFNREGOPEN_T)GetProcAddress(hDLL, "RegOpenKeyExA");
    if (!lpfnRegOpenKeyEx) {
        // Can't find the procedure we want.  Free and bail.
        FreeLibrary(hDLL);
        return 0;
    }

    lpfnRegQueryValueEx = (LPFNREGQUERY_T)GetProcAddress(hDLL, "RegQueryValueExA");
    if (!lpfnRegQueryValueEx) {
        // Can't find the procedure we want.  Free and bail.
        FreeLibrary(hDLL);
        return 0;
    }

    lpfnRegCloseKey = (LPFNREGCLOSE_T)GetProcAddress(hDLL, "RegCloseKey");
    if (!lpfnRegCloseKey) {
        // Can't find the procedure we want.  Free and bail.
        FreeLibrary(hDLL);
        return 0;
    }

    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\Nuand LLC", 0, KEY_READ, &hk)) {
        FreeLibrary(hDLL);
        return 0;
    }

    len = (DWORD)max_len;
    if (RegQueryValueEx(hk, "Path", 0, NULL, (LPBYTE) buf, &len) == ERROR_SUCCESS) {
        if (len > 0 && len < max_len && buf[len - 1] != '\\')
            strcat(buf, "\\");
    } else
        len = 0;

    if (len) {
        lpfnRegCloseKey(hk);
    }

    FreeLibrary(hDLL);

    return len;
}

#else
#error "Unknown OS or missing BLADERF_OS_* definition"
#endif

/* We're not using functions that use the *nix PATH_MAX (which is known to be
 * problematic), or the WIN32 MAX_PATH.  Therefore,  we'll just use this
 * arbitrary, but "sufficiently" large max buffer size for paths */
#define PATH_MAX_LEN    4096

char *file_find(const char *filename)
{
    size_t i, max_len;
    char *full_path = (char*) calloc(PATH_MAX_LEN + 1, 1);
    const char *env_var = getenv("BLADERF_SEARCH_DIR");

    /* Check directory specified by environment variable */
    if (env_var != NULL) {
        strncat(full_path, env_var, PATH_MAX_LEN - 1);
        full_path[strlen(full_path)] = DIR_DELIMETER;

        max_len = PATH_MAX_LEN - strlen(full_path);

        if (max_len >= strlen(filename)) {
            strncat(full_path, filename, max_len);
            if (access(full_path, ACCESS_FILE_EXISTS) != -1) {
                return full_path;
            }
        }
    }

    /* Check the directory containing the currently running binary */
    memset(full_path, 0, PATH_MAX_LEN);
    max_len = PATH_MAX_LEN - 1;

    if (get_binary_dir(full_path, max_len) != 0) {
        max_len -= strlen(full_path);

        if (max_len >= strlen(filename)) {
            strncat(full_path, filename, max_len);
            if (access(full_path, ACCESS_FILE_EXISTS) != -1) {
                return full_path;
            }
        } else {
            log_debug("Skipping search for %s in %s. "
                      "Path would be truncated.\n",
                      filename, full_path);
        }
    }


    /* Search our list of pre-determined paths */
    for (i = 0; i < ARRAY_SIZE(search_paths); i++) {
        memset(full_path, 0, PATH_MAX_LEN);
        max_len = PATH_MAX_LEN;

        if (search_paths[i].prepend_home) {
            const size_t len = get_home_dir(full_path, max_len);

            if (len != 0)  {
                max_len -= len;
            } else {
                continue;
            }

        }

        strncat(full_path, search_paths[i].path, max_len);
        max_len = PATH_MAX_LEN - strlen(full_path);

        if (max_len >= strlen(filename)) {
            strncat(full_path, filename, max_len);

            if (access(full_path, ACCESS_FILE_EXISTS) != -1) {
                return full_path;
            }
        } else {
            log_debug("Skipping search for %s in %s. "
                      "Path would be truncated.\n",
                      filename, full_path);
        }
    }

    /* Search the installation directory, if applicable */
    if (get_install_dir(full_path, PATH_MAX_LEN)) {
        max_len = PATH_MAX_LEN - strlen(full_path);

        if (max_len >= strlen(filename)) {
            strncat(full_path, filename, max_len);
            if (access(full_path, ACCESS_FILE_EXISTS) != -1) {
                return full_path;
            }
        } else {
            log_debug("Skipping search for %s in %s. "
                      "Path would be truncated.\n",
                      filename, full_path);
        }
    }

    free(full_path);
    return NULL;
}

int file_find_and_read(const char *filename, uint8_t **buf, size_t *size)
{
    int status;
    char *full_path = file_find(filename);
    *buf = NULL;
    *size = 0;

    if (full_path != NULL) {
        status = file_read_buffer(full_path, buf, size);
        free(full_path);
        return status;
    } else {
        return BLADERF_ERR_NO_FILE;
    }
}

#undef LOCAL_BLADERF_OS_LINUX
