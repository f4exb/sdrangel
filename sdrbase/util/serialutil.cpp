///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#include <string.h>
#include <regex>

#if defined(_WIN32)
#include <Windows.h>
#include <winbase.h>
#include <tchar.h>
#elif defined(__linux__)
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "serialutil.h"

#if defined(_WIN32)
void SerialUtil::getComPorts(std::vector<std::string>& comPorts, const std::string& regexStr)
{
    (void) regexStr;
	TCHAR lpTargetPath[5000]; // buffer to store the path of the COMPORTS
	DWORD test;
	bool gotPort = 0; // in case the port is not found

	char portName[100];

	for (int i = 0; i<255; i++) // checking ports from COM0 to COM255
	{
		sprintf(portName, "COM%d", i);

		test = QueryDosDeviceA((LPCSTR)portName, (LPSTR)lpTargetPath, 5000);

		// Test the return value and error if any
		if (test != 0) //QueryDosDevice returns zero if it didn't find an object
		{
			comPorts.push_back(std::string(portName));
		}

		if (::GetLastError() == ERROR_INSUFFICIENT_BUFFER)
		{
			lpTargetPath[10000]; // in case the buffer got filled, increase size of the buffer.
			continue;
		}
	}
}
#elif defined(__linux__)
void SerialUtil::getComPorts(std::vector<std::string>& comPorts, const std::string& regexStr)
{
    int n;
    struct dirent **namelist;
    comPorts.clear();
    const char* sysdir = "/sys/class/tty/";
    struct stat fileStat;
    std::regex devRegex(regexStr);
    std::smatch devMatch;
    std::string devString = "/dev/";

    // Scan through /sys/class/tty - it contains all tty-devices in the system
    n = scandir(sysdir, &namelist, NULL, alphasort);
    if (n < 0)
    {
        perror("scandir");
    }
    else
    {
        while (n--)
        {
            if (strcmp(namelist[n]->d_name, "..") && strcmp(namelist[n]->d_name, "."))
            {
                // Construct full absolute file path
                std::string fullpath = sysdir;
                std::string devName = std::string(namelist[n]->d_name);
                fullpath += devName;
                fullpath += std::string("/device");

                if (lstat(fullpath.c_str(), &fileStat) == 0)
                {
                    if (regexStr.size() != 0)
                    {
                        std::regex_search(devName, devMatch, devRegex);

                        if (devMatch.size() != 0) {
                            comPorts.push_back(devString + devName);
                        }
                    }
                    else
                    {
                        comPorts.push_back(devString + devName);
                    }
                }
            }

            free(namelist[n]);
        }

        free(namelist);
    }
}
#else // not supported
void SerialUtil::getComPorts(std::vector<std::string>& comPorts, const std::string& regexStr)
{
    (void) comPorts;
    (void) regexStr;
}
#endif