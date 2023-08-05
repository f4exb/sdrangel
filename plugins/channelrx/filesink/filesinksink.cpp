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
#include <QFileInfo>

#include "dsp/dspcommands.h"
#include "dsp/spectrumvis.h"

#include "filesinkmessages.h"
#include "filesinksink.h"
#include "dsp/filerecord.h"
#include "dsp/wavfilerecord.h"

FileSinkSink::FileSinkSink() :
    m_sinkSampleRate(0),
    m_nbCaptures(0),
    m_preRecordBuffer(48000),
    m_preRecordFill(0),
    m_spectrumSink(nullptr),
    m_msgQueueToGUI(nullptr),
    m_recordEnabled(false),
    m_record(false),
    m_squelchOpen(false),
    m_postSquelchCounter(0),
    m_msCount(0),
    m_byteCount(0),
    m_bytesPerSample(sizeof(Sample))
{
    m_fileSink = new FileRecord();
}

FileSinkSink::~FileSinkSink()
{}

void FileSinkSink::startRecording()
{
    if (m_recordEnabled) // File is open for writing and valid
    {
        // set the length of pre record time
        qint64 mSShift = m_sinkSampleRate == 0 ? 0 : (m_preRecordFill * 1000) / m_sinkSampleRate;
        m_fileSink->setMsShift(-mSShift);

        // notify capture start
        if (!m_fileSink->startRecording())
        {
            // qWarning already output in startRecording, just need to send to GUI
            if (m_msgQueueToGUI)
            {
                FileSinkMessages::MsgReportRecordFileError *msg
                    = FileSinkMessages::MsgReportRecordFileError::create(QString("Failed to open %1").arg(m_fileSink->getCurrentFileName()));
                m_msgQueueToGUI->push(msg);
            }
            return;
        }
        m_record = true;
        m_nbCaptures++;

        if (m_msgQueueToGUI)
        {
            FileSinkMessages::MsgReportRecordFileName *msg1
                = FileSinkMessages::MsgReportRecordFileName::create(m_fileSink->getCurrentFileName());
            m_msgQueueToGUI->push(msg1);
            FileSinkMessages::MsgReportRecording *msg2 = FileSinkMessages::MsgReportRecording::create(true);
            m_msgQueueToGUI->push(msg2);
        }

        // copy pre record samples
        SampleVector::iterator p1Begin, p1End, p2Begin, p2End;
        m_preRecordBuffer.readBegin(m_preRecordFill, &p1Begin, &p1End, &p2Begin, &p2End);

        if (p1Begin != p1End) {
            m_fileSink->feed(p1Begin, p1End, false);
        }
        if (p2Begin != p2End) {
            m_fileSink->feed(p2Begin, p2End, false);
        }

        m_byteCount += m_preRecordFill * m_bytesPerSample;

        if (m_sinkSampleRate > 0) {
            m_msCount += (m_preRecordFill * 1000) / m_sinkSampleRate;
        }
    }
}

void FileSinkSink::stopRecording()
{
    if (m_record)
    {
        m_preRecordBuffer.reset();

        if (!m_fileSink->stopRecording())
        {
            // qWarning already output stopRecording, just need to send to GUI
            if (m_msgQueueToGUI)
            {
                FileSinkMessages::MsgReportRecordFileError *msg
                    = FileSinkMessages::MsgReportRecordFileError::create(QString("Error while writing to %1").arg(m_fileSink->getCurrentFileName()));
                m_msgQueueToGUI->push(msg);
            }
        }

        if (m_msgQueueToGUI)
        {
            FileSinkMessages::MsgReportRecording *msg = FileSinkMessages::MsgReportRecording::create(false);
            m_msgQueueToGUI->push(msg);
        }

        m_record = false;
    }
}

void FileSinkSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    SampleVector::const_iterator beginw = begin;
    SampleVector::const_iterator endw = end;

    if (m_decimator.getDecim() != 1)
    {
        for (SampleVector::const_iterator it = begin; it < end; ++it)
        {
            Complex c(it->real(), it->imag());
            c *= m_nco.nextIQ();
            Complex ci;

            if (m_decimator.decimate(c, ci)) {
                m_sampleBuffer.push_back(Sample(ci.real(), ci.imag()));
            }
        }

        beginw = m_sampleBuffer.begin();
        endw = m_sampleBuffer.end();
    }


    if (!m_record && (m_settings.m_preRecordTime != 0)) {
        m_preRecordFill = m_preRecordBuffer.write(beginw, endw);
    }

    if (m_settings.m_squelchRecordingEnable)
    {
        int nbToWrite = endw - beginw;

        if (m_squelchOpen)
        {
            m_fileSink->feed(beginw, endw, true);
        }
        else
        {
            if (nbToWrite < m_postSquelchCounter)
            {
                m_fileSink->feed(beginw, endw, true);
                m_postSquelchCounter -= nbToWrite;
            }
            else
            {
                m_fileSink->feed(beginw, endw + m_postSquelchCounter, true);
                nbToWrite = m_postSquelchCounter;
                m_postSquelchCounter = 0;

                stopRecording();
            }
        }

        m_byteCount += nbToWrite * m_bytesPerSample;

        if (m_sinkSampleRate > 0) {
            m_msCount += (nbToWrite * 1000) / m_sinkSampleRate;
        }
    }
    else if (m_record)
    {
        m_fileSink->feed(beginw, endw, true);
        int nbSamples = endw - beginw;
        m_byteCount += nbSamples * m_bytesPerSample;

        if (m_sinkSampleRate > 0) {
            m_msCount += (nbSamples * 1000) / m_sinkSampleRate;
        }
    }

    if (m_spectrumSink) {
		m_spectrumSink->feed(beginw, endw, false);
	}

    if (m_decimator.getDecim() != 1) {
        m_sampleBuffer.clear();
    }
}

void FileSinkSink::applyChannelSettings(
    int channelSampleRate,
    int sinkSampleRate,
    int channelFrequencyOffset,
    int64_t centerFrequency,
    bool force)
{
    qDebug() << "FileSinkSink::applyChannelSettings:"
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
        int decim = channelSampleRate / sinkSampleRate;

        for (int i = 0; i < 7; i++) // find log2 between 0 and 6
        {
            if ((decim & 1) == 1)
            {
                qDebug() << "FileSinkSink::applyChannelSettings: log2decim: " << i;
                m_decimator.setLog2Decim(i);
                break;
            }

            decim >>= 1;
        }
    }

    if ((m_centerFrequency != centerFrequency)
     || (m_channelFrequencyOffset != channelFrequencyOffset)
     || (m_sinkSampleRate != sinkSampleRate) || force)
    {
        DSPSignalNotification *notif = new DSPSignalNotification(sinkSampleRate, centerFrequency);
        DSPSignalNotification *notifToSpectrum = new DSPSignalNotification(*notif);
        m_fileSink->getInputMessageQueue()->push(notif);
        m_spectrumSink->getInputMessageQueue()->push(notifToSpectrum);

        if (m_msgQueueToGUI)
        {
            FileSinkMessages::MsgConfigureSpectrum *msg = FileSinkMessages::MsgConfigureSpectrum::create(
                centerFrequency, sinkSampleRate);
            m_msgQueueToGUI->push(msg);
        }
    }

    if ((m_sinkSampleRate != sinkSampleRate) || force) {
        m_preRecordBuffer.setSize(m_settings.m_preRecordTime * sinkSampleRate);
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
    m_sinkSampleRate = sinkSampleRate;
    m_centerFrequency = centerFrequency;
    m_preRecordBuffer.reset();
}

void FileSinkSink::applySettings(const FileSinkSettings& settings, bool force)
{
    qDebug() << "FileSinkSink::applySettings:"
        << "m_fileRecordName: " << settings.m_fileRecordName
        << "force: " << force;

    QString fileRecordName = settings.m_fileRecordName;

    if ((settings.m_fileRecordName != m_settings.m_fileRecordName) || force)
    {
        QFileInfo fileInfo(settings.m_fileRecordName);
        QString extension = fileInfo.suffix();
        if (extension.isEmpty()) {
            fileRecordName.append(".sdriq");
        } else if ((extension != "sdriq") && (extension != "wav")) {
            fileRecordName.chop(extension.size());
            fileRecordName.append("sdriq");
        }

        QString fileBase;
        FileRecordInterface::RecordType recordType = FileRecordInterface::guessTypeFromFileName(fileRecordName, fileBase);

        if ((recordType == FileRecordInterface::RecordTypeSdrIQ) || (recordType == FileRecordInterface::RecordTypeWav))
        {
            delete m_fileSink;
            if (recordType == FileRecordInterface::RecordTypeSdrIQ) {
                m_fileSink = new FileRecord(m_sinkSampleRate, m_centerFrequency);
            } else {
                m_fileSink = new WavFileRecord(m_sinkSampleRate, m_centerFrequency);
            }
            m_fileSink->setFileName(fileBase);
            m_msCount = 0;
            m_byteCount = 0;
            m_bytesPerSample = m_fileSink->getBytesPerSample();
            m_nbCaptures = 0;
            m_recordEnabled = true;
        }
        else
        {
            m_recordEnabled = false;
        }
    }

    if ((settings.m_preRecordTime != m_settings.m_squelchPostRecordTime) || force)
    {
        m_preRecordBuffer.setSize(settings.m_preRecordTime * m_sinkSampleRate);

        if (settings.m_preRecordTime  == 0) {
            m_preRecordFill = 0;
        }
    }

    m_settings = settings;
    m_settings.m_fileRecordName = fileRecordName;
}

void FileSinkSink::squelchRecording(bool squelchOpen)
{
    if (!m_recordEnabled || !m_settings.m_squelchRecordingEnable) {
        return;
    }

    if (squelchOpen)
    {
        if (!m_record) {
            startRecording();
        }
    }
    else
    {
        m_postSquelchCounter = m_settings.m_squelchPostRecordTime * m_sinkSampleRate;
    }

    m_squelchOpen = squelchOpen;
}
