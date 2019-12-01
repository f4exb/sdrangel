///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_ATVDEMODREPORT_H
#define INCLUDE_ATVDEMODREPORT_H

#include <QObject>

#include "util/message.h"

class ATVDemodReport : public QObject
{
    Q_OBJECT
public:
    class MsgReportEffectiveSampleRate : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        int getSampleRate() const { return m_sampleRate; }
        int getNbPointsPerLine() const { return m_nbPointsPerLine; }

        static MsgReportEffectiveSampleRate* create(int sampleRate, int nbPointsPerLine)
        {
            return new MsgReportEffectiveSampleRate(sampleRate, nbPointsPerLine);
        }

    protected:
        int m_sampleRate;
        int m_nbPointsPerLine;

        MsgReportEffectiveSampleRate(int sampleRate, int nbPointsPerLine) :
            Message(),
            m_sampleRate(sampleRate),
            m_nbPointsPerLine(nbPointsPerLine)
        { }
    };

    ATVDemodReport();
    ~ATVDemodReport();
};

#endif // INCLUDE_ATVDEMODREPORT_H
