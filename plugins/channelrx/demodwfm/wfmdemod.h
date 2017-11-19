///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
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

#ifndef INCLUDE_WFMDEMOD_H
#define INCLUDE_WFMDEMOD_H

#include <QMutex>
#include <vector>

#include "dsp/basebandsamplesink.h"
#include "channel/channelsinkapi.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/lowpass.h"
#include "dsp/movingaverage.h"
#include "dsp/fftfilt.h"
#include "dsp/phasediscri.h"
#include "audio/audiofifo.h"
#include "util/message.h"

#include "wfmdemodsettings.h"

#define rfFilterFftLength 1024

class ThreadedBasebandSampleSink;
class DownChannelizer;
class DeviceSourceAPI;

class WFMDemod : public BasebandSampleSink, public ChannelSinkAPI {
public:
    class MsgConfigureWFMDemod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const WFMDemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureWFMDemod* create(const WFMDemodSettings& settings, bool force)
        {
            return new MsgConfigureWFMDemod(settings, force);
        }

    private:
        WFMDemodSettings m_settings;
        bool m_force;

        MsgConfigureWFMDemod(const WFMDemodSettings& settings, bool force) :
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
        int m_centerFrequency;

        MsgConfigureChannelizer(int sampleRate, int centerFrequency) :
            Message(),
            m_sampleRate(sampleRate),
            m_centerFrequency(centerFrequency)
        { }
    };

	WFMDemod(DeviceSourceAPI *deviceAPI);
	virtual ~WFMDemod();
	void setSampleSink(BasebandSampleSink* sampleSink) { m_sampleSink = sampleSink; }

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool po);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& cmd);

    virtual int getDeltaFrequency() const { return m_absoluteFrequencyOffset; }
    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual void getTitle(QString& title) { title = m_settings.m_title; }

	double getMagSq() const { return m_movingAverage.average(); }
    bool getSquelchOpen() const { return m_squelchOpen; }

    void getMagSqLevels(double& avg, double& peak, int& nbSamples)
    {
        avg = m_magsqCount == 0 ? 1e-10 : m_magsqSum / m_magsqCount;
        m_magsq = avg;
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

    DeviceSourceAPI* m_deviceAPI;
    ThreadedBasebandSampleSink* m_threadedChannelizer;
    DownChannelizer* m_channelizer;

    WFMDemodSettings m_settings;
    int m_absoluteFrequencyOffset;

	NCO m_nco;
	Interpolator m_interpolator; //!< Interpolator between sample rate sent from DSP engine and requested RF bandwidth (rational)
	Real m_interpolatorDistance;
	Real m_interpolatorDistanceRemain;
	fftfilt* m_rfFilter;

	Real m_squelchLevel;
	int m_squelchState;
    bool m_squelchOpen;
    double m_magsq; //!< displayed averaged value
    double m_magsqSum;
    double m_magsqPeak;
    int  m_magsqCount;

	Real m_lastArgument;
	MovingAverage<double> m_movingAverage;
	Real m_fmExcursion;

	AudioVector m_audioBuffer;
	uint m_audioBufferFill;

	BasebandSampleSink* m_sampleSink;
	AudioFifo m_audioFifo;
	SampleVector m_sampleBuffer;
	QMutex m_settingsMutex;

	PhaseDiscriminators m_phaseDiscri;

	void applySettings(const WFMDemodSettings& settings, bool force = false);
};

#endif // INCLUDE_WFMDEMOD_H
