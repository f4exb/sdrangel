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

#ifndef INCLUDE_FREQTRACKERREPORT_H
#define INCLUDE_FREQTRACKERREPORT_H

#include "util/message.h"

class FreqTrackerReport
{
public:
    class MsgConfigureChannelizer : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getSampleRate() const { return m_sampleRate; }
        int getCenterFrequency() const { return m_centerFrequency; }

        static MsgConfigureChannelizer* create(int sampleRate, int centerFrequency)
        {
            return new MsgConfigureChannelizer(sampleRate, centerFrequency);
        }

    private:
        int m_sampleRate;
        int  m_centerFrequency;

        MsgConfigureChannelizer(int sampleRate, int centerFrequency) :
            Message(),
            m_sampleRate(sampleRate),
            m_centerFrequency(centerFrequency)
        { }
    };

    class MsgSinkFrequencyOffsetNotification : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgSinkFrequencyOffsetNotification* create(int frequencyOffset) {
            return new MsgSinkFrequencyOffsetNotification(frequencyOffset);
        }

        int getFrequencyOffset() const { return m_frequencyOffset; }

    private:
        MsgSinkFrequencyOffsetNotification(int frequencyOffset) :
            Message(),
            m_frequencyOffset(frequencyOffset)
        { }

        int m_frequencyOffset;
    };

    FreqTrackerReport();
    ~FreqTrackerReport();
};

#endif // INCLUDE_FREQTRACKERREPORT_H
