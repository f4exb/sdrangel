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
#include "dsp/spectrumvis.h"

#include "sigmffilesinkmessages.h"
#include "sigmffilesinksink.h"

SigMFFileSinkSink::SigMFFileSinkSink() :
    m_spectrumSink(nullptr),
    m_msgQueueToGUI(nullptr),
    m_preRecordBuffer(48000),
    m_preRecordFill(0),
    m_recordEnabled(false),
    m_record(false),
    m_squelchOpen(false),
    m_postSquelchCounter(0),
    m_msCount(0),
    m_byteCount(0)
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

    if (m_settings.m_squelchRecordingEnable && (m_settings.m_squelchPreRecordTime != 0)) {
        m_preRecordFill = m_preRecordBuffer.write(m_sampleBuffer.begin(), m_sampleBuffer.end());
    }

    if (m_settings.m_squelchRecordingEnable)
    {
        int nbToWrite = m_sampleBuffer.end() - m_sampleBuffer.begin();

        if (m_squelchOpen)
        {
            m_fileSink.feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), true);
        }
        else
        {
            if (nbToWrite < m_postSquelchCounter)
            {
                m_fileSink.feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), true);
                m_postSquelchCounter -= nbToWrite;
            }
            else
            {
                if (m_msgQueueToGUI)
                {
                    SigMFFileSinkMessages::MsgReportRecording *msg = SigMFFileSinkMessages::MsgReportRecording::create(false);
                    m_msgQueueToGUI->push(msg);
                }

                m_fileSink.feed(m_sampleBuffer.begin(), m_sampleBuffer.begin() + m_postSquelchCounter, true);
                nbToWrite = m_postSquelchCounter;
                m_postSquelchCounter = 0;
                m_record = false;
                m_fileSink.stopRecording();
            }
        }

        m_byteCount += nbToWrite * sizeof(Sample);

        if (m_sinkSampleRate > 0) {
            m_msCount += (nbToWrite * 1000) / m_sinkSampleRate;
        }
    }
    else if (m_record)
    {
        m_fileSink.feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), true);
        int nbSamples = m_sampleBuffer.end() - m_sampleBuffer.begin();
        m_byteCount += nbSamples * sizeof(Sample);

        if (m_sinkSampleRate > 0) {
            m_msCount += (nbSamples * 1000) / m_sinkSampleRate;
        }
    }

    if (m_spectrumSink) {
		m_spectrumSink->feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), false);
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
        m_interpolator.create(16, channelSampleRate, channelSampleRate / 2.0f);
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance = (Real) channelSampleRate / (Real) sinkSampleRate;
    }

    if ((m_centerFrequency != centerFrequency)
     || (m_channelFrequencyOffset != channelFrequencyOffset)
     || (m_sinkSampleRate != sinkSampleRate) || force)
    {
        DSPSignalNotification *notif = new DSPSignalNotification(sinkSampleRate, centerFrequency);
        DSPSignalNotification *notifToSpectrum = new DSPSignalNotification(*notif);
        m_fileSink.getInputMessageQueue()->push(notif);
        m_spectrumSink->getInputMessageQueue()->push(notifToSpectrum);

        if (m_msgQueueToGUI)
        {
            SigMFFileSinkMessages::MsgConfigureSpectrum *msg = SigMFFileSinkMessages::MsgConfigureSpectrum::create(
                centerFrequency, sinkSampleRate);
            m_msgQueueToGUI->push(msg);
        }
    }

    if ((m_sinkSampleRate != sinkSampleRate) || force) {
        m_preRecordBuffer.setSize(m_settings.m_squelchPreRecordTime * sinkSampleRate);
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
    m_sinkSampleRate = sinkSampleRate;
    m_centerFrequency = centerFrequency;
    m_preRecordBuffer.reset();
}

void SigMFFileSinkSink::applySettings(const SigMFFileSinkSettings& settings, bool force)
{
    qDebug() << "SigMFFileSinkSink::applySettings:"
        << "m_fileRecordName: " << settings.m_fileRecordName
        << "force: " << force;

    if ((settings.m_fileRecordName != m_settings.m_fileRecordName) || force)
    {
        QString fileBase;
        FileRecordInterface::RecordType recordType = FileRecordInterface::guessTypeFromFileName(settings.m_fileRecordName, fileBase);

        if (recordType == FileRecordInterface::RecordTypeSigMF)
        {
            m_fileSink.setFileName(fileBase);
            m_msCount = 0;
            m_byteCount = 0;
            m_recordEnabled = true;
        }
        else
        {
            m_recordEnabled = false;
        }
    }

    if ((settings.m_squelchPreRecordTime != m_settings.m_squelchPostRecordTime) || force)
    {
        m_preRecordBuffer.setSize(settings.m_squelchPreRecordTime * m_sinkSampleRate);
    }

    m_settings = settings;
}

void SigMFFileSinkSink::squelchRecording(bool squelchOpen)
{
    if (!m_recordEnabled || !m_settings.m_squelchRecordingEnable) {
        return;
    }

    if (squelchOpen)
    {
        if (!m_record)
        {
            m_fileSink.startRecording();
            m_record = true;
            SampleVector::iterator p1Begin, p1End, p2Begin, p2End;
            m_preRecordBuffer.readBegin(m_preRecordFill, &p1Begin, &p1End, &p2Begin, &p2End);

            if (p1Begin != p1End) {
                m_fileSink.feed(p1Begin, p1End, false);
            }
            if (p2Begin != p2End) {
                m_fileSink.feed(p2Begin, p2End, false);
            }

            m_byteCount += m_preRecordFill * sizeof(Sample);

            if (m_sinkSampleRate > 0) {
                m_msCount += (m_preRecordFill * 1000) / m_sinkSampleRate;
            }

            if (m_msgQueueToGUI)
            {
                SigMFFileSinkMessages::MsgReportRecording *msg = SigMFFileSinkMessages::MsgReportRecording::create(true);
                m_msgQueueToGUI->push(msg);
            }
        }
    }
    else
    {
        m_postSquelchCounter = m_settings.m_squelchPostRecordTime * m_sinkSampleRate;
    }

    m_squelchOpen = squelchOpen;
}