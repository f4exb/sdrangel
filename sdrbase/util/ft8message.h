///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB.                                  //
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
#ifndef INCLUDE_FT8MESSAGE_H
#define INCLUDE_FT8MESSAGE_H

#include <QString>
#include <QDateTime>
#include <QList>

#include "export.h"
#include "message.h"

struct SDRBASE_API FT8Message
{
    QDateTime ts;
    int pass;
    int snr;
    int nbCorrectBits;
    float dt;
    float df;
    QString call1;
    QString call2;
    QString loc;
    QString decoderInfo;
};

class SDRBASE_API MsgReportFT8Messages : public Message {
    MESSAGE_CLASS_DECLARATION
public:
    QList<FT8Message>& getFT8Messages() { return m_ft8Messages; }
    void setBaseFrequency(qint64 baseFrequency) { m_baseFrequency = baseFrequency; }

    static MsgReportFT8Messages* create() {
        return new MsgReportFT8Messages();
    }

private:
    QList<FT8Message> m_ft8Messages;
    qint64 m_baseFrequency;

    MsgReportFT8Messages() :
        Message(),
        m_baseFrequency(0)
    { }
};

#endif // INCLUDE_FT8MESSAGE_H
