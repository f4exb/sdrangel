///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB.                                  //
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

#include "dsp/dspcommands.h"
#include "dsp/sigmffilerecord.h"

#include "sigmffilesinksink.h"

SigMFFileSinkSink::SigMFFileSinkSink() :
    m_recordEnabled(false),
    m_record(false)
{}

SigMFFileSinkSink::~SigMFFileSinkSink()
{}

void SigMFFileSinkSink::startRecording()
{
    if (m_recordEnabled)
    {
        m_fileSink.startRecording();
        m_record = true;
    }
}

void SigMFFileSinkSink::stopRecording()
{
    m_record = false;
    m_fileSink.stopRecording();
}

void SigMFFileSinkSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
	for (SampleVector::const_iterator it = begin; it < end; ++it)
	{
		Complex c(it->real(), it->imag());
		c *= m_nco.nextIQ();

		if (m_interpolatorDistance == 1)
		{
	        m_sampleBuffer.push_back(Sample(c.real(), c.imag()));
		}
		else
		{
            Complex ci;

            if (m_interpolator.decimate(&m_interpolatorDistanceRemain, c, &ci))
            {
                m_sampleBuffer.push_back(Sample(ci.real(), ci.imag()));
                m_interpolatorDistanceRemain += m_interpolatorDistance;
            }
		}
	}

    if (m_record) {
        m_fileSink.feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), true);
    }

    m_sampleBuffer.clear();
}

void SigMFFileSinkSink::applyChannelSettings(
    int channelSampleRate,
    int sinkSampleRate,
    int channelFrequencyOffset,
    int64_t centerFrequency,
    bool force)
{
    qDebug() << "SigMFFileSinkSink::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " sinkSampleRate: " << sinkSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset
            << " centerFrequency: " << centerFrequency
            << " force: " << force;

    if ((m_channelFrequencyOffset != channelFrequencyOffset) ||
        (m_channelSampleRate != channelSampleRate) || force)
    {
        m_nco.setFreq(-channelFrequencyOffset, channelSampleRate);
    }

    if ((m_channelSampleRate != channelSampleRate)
     || (m_sinkSampleRate != sinkSampleRate) || force)
    {
        m_interpolator.create(16, channelSampleRate, channelSampleRate / 2.2f);
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance = (Real) channelSampleRate / (Real) sinkSampleRate;
    }

    if ((m_centerFrequency != centerFrequency)
     || (m_channelFrequencyOffset != channelFrequencyOffset)
     || (m_sinkSampleRate != sinkSampleRate) || force)
    {
        DSPSignalNotification *notif = new DSPSignalNotification(sinkSampleRate, centerFrequency + m_settings.m_inputFrequencyOffset);
        m_fileSink.getInputMessageQueue()->push(notif);
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
    m_sinkSampleRate = sinkSampleRate;
    m_centerFrequency = centerFrequency;
}

void SigMFFileSinkSink::applySettings(const SigMFFileSinkSettings& settings, bool force)
{
    qDebug() << "SigMFFileSinkSink::applySettings:"
        << "m_log2Decim:" << settings.m_log2Decim
        << "m_inputFrequencyOffset:" << settings.m_inputFrequencyOffset
        << "m_fileRecordName: " << settings.m_fileRecordName
        << "force: " << force;

    if ((settings.m_fileRecordName != m_settings.m_fileRecordName) || force)
    {
        QString fileBase;
        FileRecordInterface::RecordType recordType = FileRecordInterface::guessTypeFromFileName(settings.m_fileRecordName, fileBase);

        if (recordType == FileRecordInterface::RecordTypeSigMF)
        {
            m_fileSink.setFileName(fileBase);
            m_recordEnabled = true;
        }
        else
        {
            m_recordEnabled = false;
        }
    }

    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || force)
    {
        DSPSignalNotification *notif = new DSPSignalNotification(m_sinkSampleRate, m_centerFrequency + m_settings.m_inputFrequencyOffset);
        m_fileSink.getInputMessageQueue()->push(notif);
    }

    m_settings = settings;
}