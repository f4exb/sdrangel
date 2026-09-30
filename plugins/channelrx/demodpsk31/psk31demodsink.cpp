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

#include "dsp/scopevis.h"

#include "psk31demod.h"
#include "psk31demodsink.h"

PSK31DemodSink::PSK31DemodSink() :
    m_scopeSink(nullptr),
    m_messageQueueToChannel(nullptr),
    m_channelSampleRate(PSK31DemodSettings::PSK31DEMOD_CHANNEL_SAMPLE_RATE),
    m_channelFrequencyOffset(0),
    m_scopeBufferIndex(0)
{
    m_scopeBuffer.resize(m_scopeBufferSize);
    m_demodulator.setCharacterCallback([this](QChar character) { characterReceived(character); });
    m_demodulator.setReportCallback([this]() { report(); });
    m_demodulator.setScopeCallback([this](const Complex& sample) { sampleToScope(sample); });
    applySettings(QStringList(), m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

void PSK31DemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    m_demodulator.feed(begin, end);
}

void PSK31DemodSink::characterReceived(QChar character)
{
    if (m_messageQueueToChannel) {
        m_messageQueueToChannel->push(PSK31Demod::MsgCharacter::create(QString(character)));
    }
}

void PSK31DemodSink::report()
{
    if (m_messageQueueToChannel)
    {
        m_messageQueueToChannel->push(PSK31Demod::MsgDemodReport::create(
            m_demodulator.getFrequencyOffset(),
            m_demodulator.getSNR(),
            m_demodulator.isLocked()));
    }
}

void PSK31DemodSink::sampleToScope(const Complex& sample)
{
    if (!m_scopeSink) {
        return;
    }

    m_scopeBuffer[m_scopeBufferIndex++] = Sample(
        sample.real() * SDR_RX_SCALEF,
        sample.imag() * SDR_RX_SCALEF);

    if (m_scopeBufferIndex == m_scopeBufferSize)
    {
        std::vector<SampleVector::const_iterator> begin;
        begin.push_back(m_scopeBuffer.begin());
        m_scopeSink->feed(begin, m_scopeBufferSize);
        m_scopeBufferIndex = 0;
    }
}

void PSK31DemodSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    if ((channelSampleRate != m_channelSampleRate) ||
        (channelFrequencyOffset != m_channelFrequencyOffset) || force)
    {
        m_demodulator.setChannel(channelSampleRate, channelFrequencyOffset);
        m_channelSampleRate = channelSampleRate;
        m_channelFrequencyOffset = channelFrequencyOffset;
    }
}

void PSK31DemodSink::applySettings(const QStringList& settingsKeys, const PSK31DemodSettings& settings, bool force)
{
    PSK31DemodSettings validatedSettings = settings;
    validatedSettings.m_rfBandwidth = PSK31DemodSettings::validateRFBandwidth(settings.m_rfBandwidth);

    if (settingsKeys.contains("rfBandwidth") || force) {
        m_demodulator.setRFBandwidth(validatedSettings.m_rfBandwidth);
    }

    if (force) {
        m_settings = validatedSettings;
    } else {
        m_settings.applySettings(settingsKeys, validatedSettings);
    }
}
