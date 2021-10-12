///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#include <QDebug>

#include "visa.h"

#ifdef _MSC_VER
#include <windows.h>
#else
#include <dlfcn.h>
#endif

VISA::VISA() :
    m_defaultRM(0),
    viOpenDefaultRM(nullptr),
    viOpen(nullptr),
    viClose(nullptr),
    viPrintf(nullptr),
    viScanf(nullptr),
    m_available(false)
{
#ifdef _MSC_VER
    const char *visaName = "visa32.dll";  // Loads visa64.dll on WIN64
#else
    const char *visaName = "libktvisa32.so";  // Keysight library
#endif

    visaLibrary = libraryOpen(visaName);
    if (visaLibrary)
    {
        viOpenDefaultRM = (ViStatus (*)(ViPSession)) libraryFunc(visaLibrary, "viOpenDefaultRM");
        viOpen = (ViStatus (*)(ViSession sesn, ViRsrc name, ViAccessMode mode, ViUInt32 timeout, ViPSession vi)) libraryFunc(visaLibrary, "viOpen");
        viClose = (ViStatus (*)(ViObject vi)) libraryFunc(visaLibrary, "viClose");
        viPrintf = (ViStatus (*) (ViSession vi, ViString writeFmt, ...)) libraryFunc(visaLibrary, "viPrintf");
        viScanf = (ViStatus (*) (ViSession vi, ViString writeFmt, ...)) libraryFunc(visaLibrary, "viScanf");

        if (viOpenDefaultRM && viOpen && viClose && viPrintf) {
            m_available = true;
        }
    }
    else
    {
        qDebug() << "VISA::VISA: Unable to load " << visaName;
    }
}

ViSession VISA::openDefault()
{
    if (isAvailable() && (m_defaultRM == 0))
    {
        viOpenDefaultRM(&m_defaultRM);
        return m_defaultRM;
    }
    else
    {
        return m_defaultRM;
    }
}

void VISA::closeDefault()
{
    if (isAvailable())
    {
        viClose(m_defaultRM);
        m_defaultRM = 0;
    }
}

ViSession VISA::open(const QString& device)
{
    ViSession session;
    if (isAvailable())
    {
        if (VI_SUCCESS == viOpen(m_defaultRM, device.toLatin1().data(), VI_NULL, VI_NULL, &session))
        {
            qDebug() << "VISA::open: Opened VISA device: " << device;
            return session;
        }
        else
        {
            qDebug() << "VISA::open: Failed to open VISA device: " << device;
        }
    }
    return 0;
}

void VISA::close(ViSession session)
{
    if (isAvailable()) {
        viClose(session);
    }
}

QStringList VISA::processCommands(ViSession session, const QString& commands)
{
    QStringList list = commands.split("\n");
    QStringList results;
    for (int i = 0; i < list.size(); i++)
    {
        QString command = list[i].trimmed();
        if (!command.isEmpty() && !command.startsWith("#"))   // Allow # to comment out lines
        {
            qDebug() << "VISA ->: " << command;
            QByteArray bytes = QString("%1\n").arg(command).toLatin1();
            char *cmd = bytes.data();
            viPrintf(session, cmd);
            if (command.endsWith("?"))
            {
                char buf[1024] = "";
                char format[] = "%t";
                viScanf(session, format, buf);
                results.append(buf);
                qDebug() << "VISA <-: " << QString(buf).trimmed();
            }
        }
    }
    return results;
}

#ifdef _MSC_VER

void *VISA::libraryOpen(const char *filename)
{
    HMODULE module;
    module = LoadLibrary ((LPCSTR)filename);
    return module;
}

void *VISA::libraryFunc(void *library, const char *function)
{
    return GetProcAddress ((HMODULE)library, function);
}

#else

void *VISA::libraryOpen(const char *filename)
{
    return dlopen (filename, RTLD_LAZY);
}

void *VISA::libraryFunc(void *library, const char *function)
{
    return dlsym (library, function);
}

#endif
