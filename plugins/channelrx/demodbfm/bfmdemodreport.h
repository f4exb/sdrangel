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

#ifndef INCLUDE_BFMDEMODREPORT_H
#define INCLUDE_BFMDEMODREPORT_H

#include "util/message.h"

class BFMDemodReport
{
public:
    class MsgReportChannelSampleRateChanged : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getSampleRate() const { return m_sampleRate; }

        static MsgReportChannelSampleRateChanged* create(int sampleRate)
        {
            return new MsgReportChannelSampleRateChanged(sampleRate);
        }

    private:
        int m_sampleRate;

        MsgReportChannelSampleRateChanged(int sampleRate) :
            Message(),
            m_sampleRate(sampleRate)
        { }
    };

    BFMDemodReport();
    ~BFMDemodReport();
};

#endif // INCLUDE_BFMDEMODREPORT_H
