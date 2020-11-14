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

#ifndef PLUGINS_CHANNELTX_FILESOURCE_FILESOURCESOURCE_H_
#define PLUGINS_CHANNELTX_FILESOURCE_FILESOURCESOURCE_H_

#include <ctime>
#include <iostream>
#include <fstream>

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QTimer>

#include "dsp/channelsamplesource.h"
#include "util/movingaverage.h"
#include "filesourcesettings.h"

class MessageQueue;

class FileSourceSource : public ChannelSampleSource {
public:
    FileSourceSource();
    ~FileSourceSource();

    virtual void pull(SampleVector::iterator begin, unsigned int nbSamples);
    virtual void pullOne(Sample& sample);
    virtual void prefetch(unsigned int nbSamples) { (void) nbSamples; }

    /** Set center frequency given in Hz */
    void setCenterFrequency(uint64_t centerFrequency) { m_centerFrequency = centerFrequency; }

    /** Set sample rate given in Hz */
    void setSampleRate(uint32_t sampleRate) { m_sampleRate = sampleRate; }

    quint64 getSamplesCount() const { return m_samplesCount; }
    double getMagSq() const { return m_magsq; }

    void getMagSqLevels(double& avg, double& peak, int& nbSamples)
    {
        if (m_magsqCount > 0)
        {
            m_magsq = m_magsqSum / m_magsqCount;
            m_magSqLevelStore.m_magsq = m_magsq;
            m_magSqLevelStore.m_magsqPeak = m_magsqPeak;
        }

        avg = m_magSqLevelStore.m_magsq;
        peak = m_magSqLevelStore.m_magsqPeak;
        nbSamples = m_magsqCount == 0 ? 1 : m_magsqCount;

        m_magsqSum = 0.0f;
        m_magsqPeak = 0.0f;
        m_magsqCount = 0;
    }

    void applySettings(const FileSourceSettings& settings, bool force = false);
    void setMessageQueueToGUI(MessageQueue *messageQueue) { m_guiMessageQueue = messageQueue; }

	void openFileStream(const QString& fileName);
	void seekFileStream(int seekMillis);
    void setRunning(bool running) { m_running = running; }

    uint32_t getFileSampleRate() const { return m_fileSampleRate; }
    quint64 getStartingTimeStamp() const { return m_startingTimeStamp; }
    quint64 getRecordLengthMuSec() const { return m_recordLengthMuSec; }
    quint32 getFileSampleSize() const { return m_sampleSize; }

private:
    struct MagSqLevelsStore
    {
        MagSqLevelsStore() :
            m_magsq(1e-12),
            m_magsqPeak(1e-12)
        {}
        double m_magsq;
        double m_magsqPeak;
    };

    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    FileSourceSettings m_settings;

	std::ifstream m_ifstream;
	QString m_fileName;
	quint32 m_sampleSize;
	quint64 m_centerFrequency;
    int64_t m_frequencyOffset;
    uint32_t m_fileSampleRate;
    quint64 m_samplesCount;
    uint32_t m_sampleRate;
    uint32_t m_deviceSampleRate;
    quint64 m_recordLengthMuSec; //!< record length in microseconds computed from file size
    quint64 m_startingTimeStamp;
	QTimer m_masterTimer;
    bool m_running;

    double m_linearGain;
	double m_magsq;
	double m_magsqSum;
	double m_magsqPeak;
    int  m_magsqCount;
    MagSqLevelsStore m_magSqLevelStore;
	MovingAverageUtil<Real, double, 16> m_movingAverage;

    MessageQueue *m_guiMessageQueue;

    void handleEOF();
    MessageQueue *getMessageQueueToGUI() { return m_guiMessageQueue; }
};

#endif // PLUGINS_CHANNELTX_FILESOURCE_FILESOURCE_H_
