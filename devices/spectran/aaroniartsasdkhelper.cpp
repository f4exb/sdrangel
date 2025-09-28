///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2025 Frank Zosso, HB9FXQ                                        //
// Copyright (C) 2025 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#include <thread>
#include <iostream>
#include <string>
#include <cmath>

#include "aaroniartsaapi.h"

#include <iostream>
#include <string>

#ifndef HELPER_H
#define HELPER_H
#ifdef _WIN32
#include <windows.h>
#endif
#endif

// This function loads the Aaronia RTSA API library with the search path set to the install directory.
// It handles the loading of the library on Windows, including adding the install directory to the DLL search path.
// It returns 0 on success or -1 on failure.
// The function uses the Aaronia RTSA API configuration defined in the CFG_AARONIA_RTSA_INSTALL_DIRECTORY and CFG_AARONIA_SDK_DIRECTORY macros.
// It also handles error reporting by formatting the error messages using FormatMessageW.
// The function is designed to be used in a C++ project that requires the Aaronia RTSA API for signal processing tasks.
int LoadRTSAAPI_with_searchpath() {
    #if !defined(RTSA_BUILDAPP_INTERNAL_SDK)

    #ifdef _WIN32
        HMODULE mod = NULL;
        DLL_DIRECTORY_COOKIE cookie_install = NULL;

        cookie_install = ::AddDllDirectory(CFG_AARONIA_RTSA_INSTALL_DIRECTORY);
        if (!cookie_install) {
            DWORD error = GetLastError();
            wchar_t errBuf[256];
            FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                           NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                           errBuf, (sizeof(errBuf) / sizeof(wchar_t)), NULL);

            std::wcerr << L"AddDllDirectory failed for install directory: " << CFG_AARONIA_RTSA_INSTALL_DIRECTORY
                       << L". Error " << error << L": " << errBuf << std::endl;
        }

        std::wstring fullDllPath = CFG_AARONIA_SDK_DIRECTORY;
        if (!fullDllPath.empty() && fullDllPath.back() != L'\\') {
            fullDllPath += L'\\';
        }
        fullDllPath += CFG_CPP_RTSAAPI_DLIB;

        DWORD loadFlags = LOAD_LIBRARY_SEARCH_DEFAULT_DIRS;
        if (cookie_install) {
            loadFlags |= LOAD_LIBRARY_SEARCH_USER_DIRS;
        } else {
            std::wcerr << L"Warning: Install directory was not added to dependency search path (AddDllDirectory failed)." << std::endl;
        }

        mod = ::LoadLibraryExW(fullDllPath.c_str(), NULL, loadFlags);

        if (!mod) {
            DWORD error = GetLastError();
            wchar_t errBuf[256];
            FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                           NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                           errBuf, (sizeof(errBuf) / sizeof(wchar_t)), NULL);
            std::wcerr << L"LoadLibraryExW (full path) failed for " << fullDllPath
                       << L". Error " << error << L": " << errBuf << std::endl;
        } else {
            //std::wcout << L"LoadLibraryExW (full path) SUCCEEDED." << std::endl;
        }

        if (cookie_install) {
            ::RemoveDllDirectory(cookie_install);
        }

        if (!mod) {
            return -1;
        }

        return 0;
    #else
        return 0;
    #endif // windows



    #else
        return 0;
    #endif //RTSA_BUILDAPP_INTERNAL_SDK
    }
