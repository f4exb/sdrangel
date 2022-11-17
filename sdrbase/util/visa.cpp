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
#include <QRegularExpression>

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
        viFindRsrc = (ViStatus (*) (ViSession vi, ViString expr, ViPFindList li, ViPUInt32 retCnt, ViChar desc[])) libraryFunc(visaLibrary, "viFindRsrc");
        viFindNext = (ViStatus (*) (ViSession vi, ViChar desc[])) libraryFunc(visaLibrary, "viFindNext");

        if (viOpenDefaultRM && viOpen && viClose && viPrintf && viFindRsrc && viFindNext) {
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

QStringList VISA::processCommands(ViSession session, const QString& commands, bool *error)
{
    QStringList results;

    if (isAvailable())
    {
        QStringList list = commands.split("\n");
        ViStatus status;

        if (error) {
            *error = false;
        }
        for (int i = 0; i < list.size(); i++)
        {
            QString command = list[i].trimmed();
            if (!command.isEmpty() && !command.startsWith("#"))   // Allow # to comment out lines
            {
                if (m_debugIO) {
                    qDebug() << "VISA ->: " << command;
                }
                QByteArray bytes = QString("%1\n").arg(command).toLatin1();
                char *cmd = bytes.data();
                status = viPrintf(session, cmd);
                if (error && status) {
                    *error = true;
                }
                if (command.contains("?"))
                {
                    char buf[1024] = "";
                    char format[] = "%t";
                    status = viScanf(session, format, buf);
                    if (error && status) {
                        *error = true;
                    }
                    results.append(buf);
                    if (m_debugIO) {
                        qDebug() << "VISA <-: " << QString(buf).trimmed();
                    }
                }
            }
        }
    }
    else if (error)
    {
        *error = true;
    }
    return results;
}

QStringList VISA::findResources()
{
    QStringList resources;

    if (isAvailable())
    {
        ViChar rsrc[VI_FIND_BUFLEN];
        ViFindList list;
        ViRsrc matches = rsrc;
        ViUInt32 nMatches = 0;
        ViChar expr[] = "?*INSTR";

        if (VI_SUCCESS == viFindRsrc(m_defaultRM, expr, &list, &nMatches, matches))
        {
            if (nMatches > 0)
            {
                resources.append(QString(rsrc));
                while (VI_SUCCESS == viFindNext(list, matches))
                {
                    resources.append(QString(rsrc));
                }
            }
        }
    }
    return resources;
}

bool VISA::identification(ViSession session, QString &manufacturer, QString &model, QString &serialNumber, QString &revision)
{
    if (isAvailable())
    {
        QStringList result = processCommands(session, "*IDN?");
        if ((result.size() == 1) && (!result[0].isEmpty()))
        {
            QStringList details = result[0].trimmed().split(',');
            manufacturer = details[0];
            // Some serial devices (ASRLn) loop back the the command if not connected
            if (manufacturer == "*IDN?") {
                return false;
            }
            if (details.size() >= 2) {
                model = details[1];
            }
            if (details.size() >= 3) {
                serialNumber = details[2];
            }
            if (details.size() >= 4) {
                revision = details[3];
            }
            qDebug() << "VISA::identification: "
                     << "manufacturer: " << manufacturer
                     << "model: " << model
                     << "serialNumber: " << serialNumber
                     << "revision: " << revision;
            return true;
        }
    }
    return false;
}

// Filter is a list of resources not to try to open
QList<VISA::Instrument> VISA::instruments(QRegularExpression *filter)
{
    QList<VISA::Instrument> instruments;

    if (isAvailable())
    {
        QStringList resourceList = findResources();

        for (auto const &resource : resourceList)
        {
            if (filter)
            {
                if (filter->match(resource).hasMatch()) {
                    continue;
                }
            }
            ViSession session = open(resource);
            if (session)
            {
                Instrument instrument;
                QString manufacturer, model, serialNumber, revision;
                if (identification(session, instrument.m_manufacturer, instrument.m_model, instrument.m_serial, instrument.m_revision))
                {
                    instrument.m_resource = resource;
                    instruments.append(instrument);
                }
                close(session);
            }
        }
    }
    return instruments;
}

#ifdef _MSC_VER

void *VISA::libraryOpen(const char *filename)
{
    HMODULE module;
    module = LoadLibraryA ((LPCSTR)filename);
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
