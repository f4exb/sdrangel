///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_AMDEMOD_H
#define INCLUDE_AMDEMOD_H

#include <QMutex>
#include <vector>

#include "dsp/basebandsamplesink.h"
#include "channel/channelsinkapi.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/movingaverage.h"
#include "dsp/agc.h"
#include "dsp/bandpass.h"
#include "audio/audiofifo.h"
#include "util/message.h"
#include "amdemodsettings.h"

class DeviceSourceAPI;
class DownChannelizer;
class ThreadedBasebandSampleSink;

class AMDemod : public BasebandSampleSink, public ChannelSinkAPI {
	Q_OBJECT
public:
    class MsgConfigureAMDemod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const AMDemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureAMDemod* create(const AMDemodSettings& settings, bool force)
        {
            return new MsgConfigureAMDemod(settings, force);
        }

    private:
        AMDemodSettings m_settings;
        bool m_force;

        MsgConfigureAMDemod(const AMDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

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

    AMDemod(DeviceSourceAPI *deviceAPI);
	~AMDemod();

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool po);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& cmd);

    virtual int getDeltaFrequency() const { return m_absoluteFrequencyOffset; }
    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual void getTitle(QString& title) { title = m_settings.m_title; }

	double getMagSq() const { return m_magsq; }
	bool getSquelchOpen() const { return m_squelchOpen; }

	void getMagSqLevels(double& avg, double& peak, int& nbSamples)
	{
	    avg = m_magsqCount == 0 ? 1e-10 : m_magsqSum / m_magsqCount;
	    peak = m_magsqPeak == 0.0 ? 1e-10 : m_magsqPeak;
	    nbSamples = m_magsqCount == 0 ? 1 : m_magsqCount;
	    m_magsqSum = 0.0f;
        m_magsqPeak = 0.0f;
        m_magsqCount = 0;
	}

    static const QString m_channelID;

private:
	enum RateState {
		RSInitialFill,
		RSRunning
	};

	DeviceSourceAPI *m_deviceAPI;
    ThreadedBasebandSampleSink* m_threadedChannelizer;
    DownChannelizer* m_channelizer;

    int m_absoluteFrequencyOffset;
    AMDemodSettings m_settings;

	NCO m_nco;
	Interpolator m_interpolator;
	Real m_interpolatorDistance;
	Real m_interpolatorDistanceRemain;

	Real m_squelchLevel;
	uint32_t m_squelchCount;
	bool m_squelchOpen;
	double m_magsq;
	double m_magsqSum;
	double m_magsqPeak;
	int  m_magsqCount;

	MovingAverage<double> m_movingAverage;
	SimpleAGC m_volumeAGC;
    Bandpass<Real> m_bandpass;

	AudioVector m_audioBuffer;
	uint32_t m_audioBufferFill;
	AudioFifo m_audioFifo;
    UDPSink<qint16> *m_udpBufferAudio;

    static const int m_udpBlockSize;

	QMutex m_settingsMutex;

//	void apply(bool force = false);
    void applySettings(const AMDemodSettings& settings, bool force = false);

	void processOneSample(Complex &ci)
	{
        Real magsq = ci.real() * ci.real() + ci.imag() * ci.imag();
        magsq /= (1<<30);
        m_movingAverage.feed(magsq);
        m_magsq = m_movingAverage.average();
        m_magsqSum += magsq;

        if (magsq > m_magsqPeak)
        {
            m_magsqPeak = magsq;
        }

        m_magsqCount++;

        if (m_magsq >= m_squelchLevel)
        {
            if (m_squelchCount <= m_settings.m_audioSampleRate / 10)
            {
                if (m_squelchCount == m_settings.m_audioSampleRate / 20) {
                    m_volumeAGC.fill(1.0);
                }

                m_squelchCount++;
            }
        }
        else
        {
            if (m_squelchCount > 1)
            {
                m_squelchCount -= 2;
            }
        }

        qint16 sample;

        if ((m_squelchCount >= m_settings.m_audioSampleRate / 20) && !m_settings.m_audioMute)
        {
            Real demod = sqrt(magsq);
            m_volumeAGC.feed(demod);
            demod = (demod - m_volumeAGC.getValue()) / m_volumeAGC.getValue();

            if (m_settings.m_bandpassEnable)
            {
                demod = m_bandpass.filter(demod);
                demod /= 301.0f;
            }

            Real attack = (m_squelchCount - 0.05f * m_settings.m_audioSampleRate) / (0.05f * m_settings.m_audioSampleRate);
            sample = demod * attack * 2048 * m_settings.m_volume;
            if (m_settings.m_copyAudioToUDP) m_udpBufferAudio->write(demod * attack * 32768);

            m_squelchOpen = true;
        }
        else
        {
            sample = 0;
            if (m_settings.m_copyAudioToUDP) m_udpBufferAudio->write(0);
            m_squelchOpen = false;
        }

        m_audioBuffer[m_audioBufferFill].l = sample;
        m_audioBuffer[m_audioBufferFill].r = sample;
        ++m_audioBufferFill;

        if (m_audioBufferFill >= m_audioBuffer.size())
        {
            uint res = m_audioFifo.write((const quint8*)&m_audioBuffer[0], m_audioBufferFill, 10);

            if (res != m_audioBufferFill)
            {
                qDebug("AMDemod::feed: %u/%u audio samples written", res, m_audioBufferFill);
            }

            m_audioBufferFill = 0;
        }
	}

};

#endif // INCLUDE_AMDEMOD_H
