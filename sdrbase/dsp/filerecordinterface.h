///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// File recorder in SigMF format single channel for SI plugins                   //
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

#ifndef INCLUDE_FILERECORD_INTERFACE_H
#define INCLUDE_FILERECORD_INTERFACE_H

#include <QString>

#include "export.h"

class SDRBASE_API FileRecordInterface {
public:
    virtual void setFileName(const QString &filename) = 0;
    virtual void startRecording() = 0;
    virtual void stopRecording() = 0;
    virtual bool isRecording() const = 0;
    static QString genUniqueFileName(unsigned int deviceUID, int istream = -1);
};


#endif // INCLUDE_FILERECORD_INTERFACE_H