/**
@file SystemResources.h
@author Lime Microsystems
@brief APIs for locating system resources.
*/

#include "SystemResources.h"
#include "Logger.h"

#include <cstdlib> //getenv, system
#include <vector>
#include <sstream>
#include <iostream>

#ifdef _MSC_VER
#include <windows.h>
#include <shlobj.h>
#include <io.h>

//access mode constants
#define F_OK 0
#define R_OK 2
#define W_OK 4
#endif

#ifdef __MINGW32__
#include <unistd.h>
#elif __unix__
#include <unistd.h>
#include <pwd.h>
#endif

#include <sys/types.h>
#include <sys/stat.h> //stat

std::string lime::getLimeSuiteRoot(void)
{
    //first check the environment variable
    const char *limeSuiteRoot = std::getenv("LIME_SUITE_ROOT");
    if (limeSuiteRoot != nullptr) return limeSuiteRoot;

    // Get the path to the current dynamic linked library.
    // The path to this library can be used to determine
    // the installation root without prior knowledge.
    #if defined(_MSC_VER) && defined(LIME_DLL)
    char path[MAX_PATH];
    HMODULE hm = NULL;
    if (GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR) &lime::getLimeSuiteRoot, &hm))
    {
        const DWORD size = GetModuleFileNameA(hm, path, sizeof(path));
        if (size != 0)
        {
            const std::string libPath(path, size);
            const size_t slash0Pos = libPath.find_last_of("/\\");
            const size_t slash1Pos = libPath.substr(0, slash0Pos).find_last_of("/\\");
            if (slash0Pos != std::string::npos && slash1Pos != std::string::npos)
                return libPath.substr(0, slash1Pos);
        }
    }
    #endif //_MSC_VER && LIME_DLL

    return "/opt/install/LimeSuite";
}

std::string lime::getHomeDirectory(void)
{
#ifndef __MINGW32__
    //first check the HOME environment variable
    const char *userHome = std::getenv("HOME");
    if (userHome != nullptr) return userHome;

    //use unix user id lookup to get the home directory
    #ifdef __unix__
    const char *pwDir = getpwuid(getuid())->pw_dir;
    if (pwDir != nullptr) return pwDir;
    #endif
#endif
    return "";
}

/*!
 * The generic location for data storage with user permission level.
 */
static std::string getBareAppDataDirectory(void)
{
    //always check APPDATA (usually windows, but can be set for linux)
    const char *appDataDir = std::getenv("APPDATA");
    if (appDataDir != nullptr) return appDataDir;

    //use windows API to query for roaming app data directory
    #ifdef _MSC_VER
    char csidlAppDataDir[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, csidlAppDataDir)))
    {
        return csidlAppDataDir;
    }
    #endif

    //xdg freedesktop standard location environment variable
    #ifdef __unix__
    const char *xdgDataHome = std::getenv("XDG_DATA_HOME");
    if (xdgDataHome != nullptr) return xdgDataHome;
    #endif

    //xdg freedesktop standard location for data in home directory
    return lime::getHomeDirectory() + "/.local/share";
}

std::string lime::getAppDataDirectory(void)
{
    return getBareAppDataDirectory() + "/LimeSuite";
}

std::string lime::getConfigDirectory(void)
{
    //xdg standard is XDG_CONFIG_HOME or $HOME/.config
    //but historically we have used $HOME/.limesuite
    return lime::getHomeDirectory() + "/.limesuite";
}

std::vector<std::string> lime::listImageSearchPaths(void)
{
    std::vector<std::string> imageSearchPaths;

    //separator for search paths in the environment variable
    #ifdef _MSC_VER
    static const char sep = ';';
    #else
    static const char sep = ':';
    #endif

    //check the environment's search path
    const char *imagePathEnv = std::getenv("LIME_IMAGE_PATH");
    if (imagePathEnv != nullptr)
    {
        std::stringstream imagePaths(imagePathEnv);
        std::string imagePath;
        while (std::getline(imagePaths, imagePath, sep))
        {
            if (imagePath.empty()) continue;
            imageSearchPaths.push_back(imagePath);
        }
    }

    //search directories in the user's home directory
    imageSearchPaths.push_back(lime::getAppDataDirectory() + "/images");

    //search global installation directories
    imageSearchPaths.push_back(lime::getLimeSuiteRoot() + "/share/LimeSuite/images");

    return imageSearchPaths;
}

std::string lime::locateImageResource(const std::string &name)
{
    for (const auto &searchPath : lime::listImageSearchPaths())
    {
        const std::string fullPath(searchPath + "/18.02/" + name);
        if (access(fullPath.c_str(), R_OK) == 0) return fullPath;
    }
    return "";
}

int lime::downloadImageResource(const std::string &name)
{
    const std::string destDir(lime::getAppDataDirectory() + "/images/18.02");
    const std::string destFile(destDir + "/" + name);
    const std::string sourceUrl("http://downloads.myriadrf.org/project/limesuite/18.02/" + name);

    //check if the directory already exists
    struct stat s;
    if (stat(destDir.c_str(), &s) == 0)
    {
        if ((s.st_mode & S_IFDIR) == 0)
        {
            return lime::ReportError("Not a directory: %s", destDir.c_str());
        }
    }

    //create images directory
    else
    {
        #ifdef __unix__
        const std::string mkdirCmd("mkdir -p \""+destDir+"\"");
        #else
        const std::string mkdirCmd("md.exe \""+destDir+"\"");
        #endif
        int result = std::system(mkdirCmd.c_str());
        if (result != 0) return lime::ReportError(result, "Failed: %s", mkdirCmd.c_str());
    }

    //check for write access
    if (access(destDir.c_str(), W_OK) != 0) lime::ReportError("Cannot write: %s", destDir.c_str());

    //download the file
    #ifdef __unix__
    const std::string dnloadCmd("wget --output-document=\""+destFile+"\" \""+sourceUrl+"\"");
    #else
    const std::string dnloadCmd("powershell.exe -Command \"(new-object System.Net.WebClient).DownloadFile('"+sourceUrl+"', '"+destFile+"')\"");
    #endif
    int result = std::system(dnloadCmd.c_str());
    if (result != 0) return lime::ReportError(result, "Failed: %s", dnloadCmd.c_str());

    return 0;
}
