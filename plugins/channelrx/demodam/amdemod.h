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
#include "util/movingaverage.h"
#include "dsp/agc.h"
#include "dsp/bandpass.h"
#include "dsp/lowpass.h"
#include "dsp/phaselockcomplex.h"
#include "dsp/fftfilt.h"
#include "audio/audiofifo.h"
#include "util/message.h"
#include "util/doublebufferfifo.h"
#include "util/stepfunctions.h"

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
	virtual void destroy() { delete this; }

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool po);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& cmd);

    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual void getTitle(QString& title) { title = m_settings.m_title; }
    virtual qint64 getCenterFrequency() const { return m_settings.m_inputFrequencyOffset; }

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual int webapiSettingsGet(
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

    virtual int webapiSettingsPutPatch(
            bool force,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

    virtual int webapiReportGet(
            SWGSDRangel::SWGChannelReport& response,
            QString& errorMessage);

    uint32_t getAudioSampleRate() const { return m_audioSampleRate; }
	double getMagSq() const { return m_magsq; }
	bool getSquelchOpen() const { return m_squelchOpen; }
	bool getPllLocked() const { return m_settings.m_pll && m_pll.locked(); }

	void getMagSqLevels(double& avg, double& peak, int& nbSamples)
	{
	    avg = m_magsqCount == 0 ? 1e-10 : m_magsqSum / m_magsqCount;
	    peak = m_magsqPeak == 0.0 ? 1e-10 : m_magsqPeak;
	    nbSamples = m_magsqCount == 0 ? 1 : m_magsqCount;
	    m_magsqSum = 0.0f;
        m_magsqPeak = 0.0f;
        m_magsqCount = 0;
	}

    static const QString m_channelIdURI;
    static const QString m_channelId;

private:
	enum RateState {
		RSInitialFill,
		RSRunning
	};

	DeviceSourceAPI *m_deviceAPI;
    ThreadedBasebandSampleSink* m_threadedChannelizer;
    DownChannelizer* m_channelizer;

    int m_inputSampleRate;
    int m_inputFrequencyOffset;
    AMDemodSettings m_settings;
    uint32_t m_audioSampleRate;
    bool m_running;

	NCO m_nco;
	Interpolator m_interpolator;
	Real m_interpolatorDistance;
	Real m_interpolatorDistanceRemain;

	Real m_squelchLevel;
	uint32_t m_squelchCount;
	bool m_squelchOpen;
	DoubleBufferFIFO<Real> m_squelchDelayLine;
	double m_magsq;
	double m_magsqSum;
	double m_magsqPeak;
	int  m_magsqCount;

	MovingAverageUtil<Real, double, 16> m_movingAverage;
	SimpleAGC<4800> m_volumeAGC;
    Bandpass<Real> m_bandpass;
    Lowpass<std::complex<float> > m_pllFilt;
    PhaseLockComplex m_pll;
    fftfilt* DSBFilter;
    fftfilt* SSBFilter;
    Real m_syncAMBuff[2*1024];
    uint32_t m_syncAMBuffIndex;
    MagAGC m_syncAMAGC;

	AudioVector m_audioBuffer;
	uint32_t m_audioBufferFill;
	AudioFifo m_audioFifo;

    static const int m_udpBlockSize;

	QMutex m_settingsMutex;

	void applyChannelSettings(int inputSampleRate, int inputFrequencyOffset, bool force = false);
    void applySettings(const AMDemodSettings& settings, bool force = false);
    void applyAudioSampleRate(int sampleRate);
    void webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const AMDemodSettings& settings);
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);

	void processOneSample(Complex &ci)
	{
	    Real re = ci.real() / SDR_RX_SCALEF;
	    Real im = ci.imag() / SDR_RX_SCALEF;
        Real magsq = re*re + im*im;
        m_movingAverage(magsq);
        m_magsq = m_movingAverage.asDouble();
        m_magsqSum += magsq;

        if (magsq > m_magsqPeak)
        {
            m_magsqPeak = magsq;
        }

        m_magsqCount++;

        m_squelchDelayLine.write(magsq);

        if (m_magsq < m_squelchLevel)
        {
            if (m_squelchCount > 0) {
                m_squelchCount--;
            }
        }
        else
        {
            if (m_squelchCount < m_audioSampleRate / 10) {
                m_squelchCount++;
            }
        }

        qint16 sample;

        m_squelchOpen = (m_squelchCount >= m_audioSampleRate / 20);

        if (m_squelchOpen && !m_settings.m_audioMute)
        {
            Real demod;

            if (m_settings.m_pll)
            {
                std::complex<float> s(re, im);
                s = m_pllFilt.filter(s);
                m_pll.feed(s.real(), s.imag());
                float yr = re * m_pll.getImag() - im * m_pll.getReal();
                float yi = re * m_pll.getReal() + im * m_pll.getImag();

                fftfilt::cmplx *sideband;
                std::complex<float> cs(yr, yi);
                int n_out;

                if (m_settings.m_syncAMOperation == AMDemodSettings::SyncAMDSB) {
                    n_out = DSBFilter->runDSB(cs, &sideband, false);
                } else {
                    n_out = SSBFilter->runSSB(cs, &sideband, m_settings.m_syncAMOperation == AMDemodSettings::SyncAMUSB, false);
                }

                for (int i = 0; i < n_out; i++)
                {
                    float agcVal = m_syncAMAGC.feedAndGetValue(sideband[i]);
                    fftfilt::cmplx z = sideband[i] * agcVal; // * m_syncAMAGC.getStepValue();

                    if (m_settings.m_syncAMOperation == AMDemodSettings::SyncAMDSB) {
                        m_syncAMBuff[i] = (z.real() + z.imag())/2.0f;
                    } else if (m_settings.m_syncAMOperation == AMDemodSettings::SyncAMUSB) {
                        m_syncAMBuff[i] = (z.real() + z.imag());
                    } else {
                        m_syncAMBuff[i] = (z.real() + z.imag());
                    }

//                    if (m_settings.m_syncAMOperation == AMDemodSettings::SyncAMDSB) {
//                        m_syncAMBuff[i] = (sideband[i].real() + sideband[i].imag())/2.0f;
//                    } else if (m_settings.m_syncAMOperation == AMDemodSettings::SyncAMUSB) {
//                        m_syncAMBuff[i] = (sideband[i].real() + sideband[i].imag());
//                    } else {
//                        m_syncAMBuff[i] = (sideband[i].real() + sideband[i].imag());
//                    }

                    m_syncAMBuffIndex = 0;
                }

                m_syncAMBuffIndex = m_syncAMBuffIndex < 2*1024 ? m_syncAMBuffIndex : 0;
                demod = m_syncAMBuff[m_syncAMBuffIndex++]*4.0f; // mos pifometrico
//                demod = m_syncAMBuff[m_syncAMBuffIndex++]*(SDR_RX_SCALEF/602.0f);
//                m_volumeAGC.feed(demod);
//                demod /= (10.0*m_volumeAGC.getValue());
            }
            else
            {
                demod = sqrt(m_squelchDelayLine.readBack(m_audioSampleRate/20));
                m_volumeAGC.feed(demod);
                demod = (demod - m_volumeAGC.getValue()) / m_volumeAGC.getValue();
            }

            if (m_settings.m_bandpassEnable)
            {
                demod = m_bandpass.filter(demod);
                demod /= 301.0f;
            }

            Real attack = (m_squelchCount - 0.05f * m_audioSampleRate) / (0.05f * m_audioSampleRate);
            sample = demod * StepFunctions::smootherstep(attack) * (m_audioSampleRate/24) * m_settings.m_volume;
        }
        else
        {
            sample = 0;
        }

        m_audioBuffer[m_audioBufferFill].l = sample;
        m_audioBuffer[m_audioBufferFill].r = sample;
        ++m_audioBufferFill;

        if (m_audioBufferFill >= m_audioBuffer.size())
        {
            uint res = m_audioFifo.write((const quint8*)&m_audioBuffer[0], m_audioBufferFill, 10);

            if (res != m_audioBufferFill)
            {
                qDebug("AMDemod::processOneSample: %u/%u audio samples written", res, m_audioBufferFill);
                m_audioFifo.clear();
            }

            m_audioBufferFill = 0;
        }
	}

};

#endif // INCLUDE_AMDEMOD_H
