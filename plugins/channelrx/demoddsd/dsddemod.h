///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef INCLUDE_DSDDEMOD_H
#define INCLUDE_DSDDEMOD_H

#include <QMutex>
#include <vector>

#include "dsp/basebandsamplesink.h"
#include "channel/channelsinkapi.h"
#include "dsp/phasediscri.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/lowpass.h"
#include "dsp/bandpass.h"
#include "dsp/afsquelch.h"
#include "dsp/movingaverage.h"
#include "dsp/afsquelch.h"
#include "audio/audiofifo.h"
#include "util/message.h"
#include "util/udpsink.h"

#include "dsddemodsettings.h"
#include "dsddecoder.h"

class DeviceSourceAPI;
class ThreadedBasebandSampleSink;
class DownChannelizer;

class DSDDemod : public BasebandSampleSink, public ChannelSinkAPI {
public:
    class MsgConfigureDSDDemod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const DSDDemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureDSDDemod* create(const DSDDemodSettings& settings, bool force)
        {
            return new MsgConfigureDSDDemod(settings, force);
        }

    private:
        DSDDemodSettings m_settings;
        bool m_force;

        MsgConfigureDSDDemod(const DSDDemodSettings& settings, bool force) :
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

    DSDDemod(DeviceSourceAPI *deviceAPI);
	~DSDDemod();
	virtual void destroy() { delete this; }
	void setScopeSink(BasebandSampleSink* sampleSink) { m_scope = sampleSink; }

	void configureMyPosition(MessageQueue* messageQueue, float myLatitude, float myLongitude);

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool po);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& cmd);

    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual void getTitle(QString& title) { title = m_settings.m_title; }
    virtual qint64 getCenterFrequency() const { return m_settings.m_inputFrequencyOffset; }

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

	double getMagSq() { return m_magsq; }
	bool getSquelchOpen() const { return m_squelchOpen; }

	const DSDDecoder& getDecoder() const { return m_dsdDecoder; }

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

    static const QString m_channelIdURI;
    static const QString m_channelId;

private:
	class MsgConfigureMyPosition : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		float getMyLatitude() const { return m_myLatitude; }
		float getMyLongitude() const { return m_myLongitude; }

		static MsgConfigureMyPosition* create(float myLatitude, float myLongitude)
		{
			return new MsgConfigureMyPosition(myLatitude, myLongitude);
		}

	private:
		float m_myLatitude;
		float m_myLongitude;

		MsgConfigureMyPosition(float myLatitude, float myLongitude) :
			m_myLatitude(myLatitude),
			m_myLongitude(myLongitude)
		{}
	};

	enum RateState {
		RSInitialFill,
		RSRunning
	};

	DeviceSourceAPI *m_deviceAPI;
    ThreadedBasebandSampleSink* m_threadedChannelizer;
    DownChannelizer* m_channelizer;

    int m_inputSampleRate;
	int m_inputFrequencyOffset;
	DSDDemodSettings m_settings;

	NCO m_nco;
	Interpolator m_interpolator;
	Real m_interpolatorDistance;
	Real m_interpolatorDistanceRemain;
	int m_sampleCount;
	int m_squelchCount;
	int m_squelchGate;

	double m_squelchLevel;
	bool m_squelchOpen;

    MovingAverage<double> m_movingAverage;
    double m_magsq;
    double m_magsqSum;
    double m_magsqPeak;
    int  m_magsqCount;

	Real m_fmExcursion;

	SampleVector m_scopeSampleBuffer;
	AudioVector m_audioBuffer;
	uint m_audioBufferFill;
	FixReal *m_sampleBuffer; //!< samples ring buffer
	int m_sampleBufferIndex;
	int m_scaleFromShort;

	AudioFifo m_audioFifo1;
    AudioFifo m_audioFifo2;
	BasebandSampleSink* m_scope;
	bool m_scopeEnabled;

	DSDDecoder m_dsdDecoder;
	QMutex m_settingsMutex;

    PhaseDiscriminators m_phaseDiscri;
    UDPSink<AudioSample> *m_udpBufferAudio;

    static const int m_udpBlockSize;

    void applyChannelSettings(int inputSampleRate, int inputFrequencyOffset, bool force = false);
	void applySettings(const DSDDemodSettings& settings, bool force = false);
};

#endif // INCLUDE_DSDDEMOD_H
