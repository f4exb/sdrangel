///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                    //
// Some code by AI                                                               //
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

#ifndef INCLUDE_PSK31DEMODSINK_H
#define INCLUDE_PSK31DEMODSINK_H

#include "dsp/channelsamplesink.h"
#include "util/messagequeue.h"

#include "psk31demodsettings.h"
#include "psk31demodulator.h"

class ScopeVis;

class PSK31DemodSink : public ChannelSampleSink
{
public:
    PSK31DemodSink();

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void applySettings(const QStringList& settingsKeys, const PSK31DemodSettings& settings, bool force = false);
    void setScopeSink(ScopeVis *scopeSink) { m_scopeSink = scopeSink; }
    void setMessageQueueToChannel(MessageQueue *queue) { m_messageQueueToChannel = queue; }
    double getMagSq() const { return m_demodulator.getMagSq(); }
    float getFrequencyOffset() const { return m_demodulator.getFrequencyOffset(); }
    float getSNR() const { return m_demodulator.getSNR(); }
    bool isLocked() const { return m_demodulator.isLocked(); }

    void getMagSqLevels(double& avg, double& peak, int& nbSamples) {
        m_demodulator.getMagSqLevels(avg, peak, nbSamples);
    }

private:
    static const int m_scopeBufferSize = 50;

    ScopeVis *m_scopeSink;
    MessageQueue *m_messageQueueToChannel;
    PSK31DemodSettings m_settings;
    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    PSK31Demodulator m_demodulator;

    SampleVector m_scopeBuffer;
    int m_scopeBufferIndex;

    void characterReceived(QChar character);
    void report();
    void sampleToScope(const Complex& sample);
};

#endif // INCLUDE_PSK31DEMODSINK_H
