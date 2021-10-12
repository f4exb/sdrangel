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

#ifndef INCLUDE_VISA_H
#define INCLUDE_VISA_H

// Minimal implementation of VISA specification (just the bits we need so far)
// https://www.ivifoundation.org/docs/vpp432_2016-02-26.pdf

#include "export.h"

typedef char ViChar;
typedef ViChar * ViPChar;
typedef signed long ViInt32;
typedef unsigned long ViUInt32;
typedef ViPChar ViString;

typedef ViInt32 ViStatus;
typedef ViUInt32 ViObject;
typedef ViObject ViSession;
typedef ViSession * ViPSession;
typedef ViString ViRsrc;
typedef ViUInt32 ViAccessMode;

#define VI_SUCCESS 0
#define VI_TRUE 1
#define VI_FALSE 0
#define VI_NULL 0

// We dynamically load the visa dll, as most users probably do not have it
// Note: Can't seem to call viOpenDefaultRM/viClose in constructor / destructor of global instance
class SDRBASE_API VISA {
public:

    // Default session
    ViSession m_defaultRM;
    // Function pointers to VISA API for direct calls
    ViStatus (*viOpenDefaultRM) (ViPSession vi);
    ViStatus (*viOpen) (ViSession sesn, ViRsrc name, ViAccessMode mode, ViUInt32 timeout, ViPSession vi);
    ViStatus (*viClose) (ViObject vi);
    ViStatus (*viPrintf) (ViSession vi, ViString writeFmt, ...);
    ViStatus (*viScanf) (ViSession vi, ViString readFmt, ...);

    VISA();

    ViSession openDefault();
    void closeDefault();
    ViSession open(const QString& device);
    void close(ViSession session);
    QStringList processCommands(ViSession session, const QString& commands);

    // Is the VISA library available
    bool isAvailable() const
    {
        return m_available;
    }

private:
    bool m_available;

protected:
    void *visaLibrary;

    void *libraryOpen(const char *filename);
    void *libraryFunc(void *library, const char *function);
};

#endif // INCLUDE_VISA_H
