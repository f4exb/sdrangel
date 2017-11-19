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

#ifndef INCLUDE_SSBDEMOD_H
#define INCLUDE_SSBDEMOD_H

#include <QMutex>
#include <vector>

#include "dsp/basebandsamplesink.h"
#include "channel/channelsinkapi.h"
#include "dsp/ncof.h"
#include "dsp/interpolator.h"
#include "dsp/fftfilt.h"
#include "dsp/agc.h"
#include "audio/audiofifo.h"
#include "util/message.h"

#include "ssbdemodsettings.h"

#define ssbFftLen 1024
#define agcTarget 3276.8 // -10 dB amplitude => -20 dB power: center of normal signal

class DeviceSourceAPI;
class ThreadedBasebandSampleSink;
class DownChannelizer;

class SSBDemod : public BasebandSampleSink, public ChannelSinkAPI {
public:
    class MsgConfigureSSBDemod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const SSBDemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureSSBDemod* create(const SSBDemodSettings& settings, bool force)
        {
            return new MsgConfigureSSBDemod(settings, force);
        }

    private:
        SSBDemodSettings m_settings;
        bool m_force;

        MsgConfigureSSBDemod(const SSBDemodSettings& settings, bool force) :
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

	SSBDemod(DeviceSourceAPI *deviceAPI);
	virtual ~SSBDemod();
	void setSampleSink(BasebandSampleSink* sampleSink) { m_sampleSink = sampleSink; }

	void configure(MessageQueue* messageQueue,
			Real Bandwidth,
			Real LowCutoff,
			Real volume,
			int spanLog2,
			bool audioBinaural,
			bool audioFlipChannels,
			bool dsb,
			bool audioMute,
			bool agc,
			bool agcClamping,
			int agcTimeLog2,
			int agcPowerThreshold,
			int agcThresholdGate);

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& cmd);

    virtual int getDeltaFrequency() const { return m_absoluteFrequencyOffset; }
    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual void getTitle(QString& title) { title = m_settings.m_title; }

    double getMagSq() const { return m_magsq; }
	bool getAudioActive() const { return m_audioActive; }

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
	class MsgConfigureSSBDemodPrivate : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		Real getBandwidth() const { return m_Bandwidth; }
		Real getLoCutoff() const { return m_LowCutoff; }
		Real getVolume() const { return m_volume; }
		int  getSpanLog2() const { return m_spanLog2; }
		bool getAudioBinaural() const { return m_audioBinaural; }
		bool getAudioFlipChannels() const { return m_audioFlipChannels; }
		bool getDSB() const { return m_dsb; }
		bool getAudioMute() const { return m_audioMute; }
		bool getAGC() const { return m_agc; }
		bool getAGCClamping() const { return m_agcClamping; }
		int  getAGCTimeLog2() const { return m_agcTimeLog2; }
		int  getAGCPowerThershold() const { return m_agcPowerThreshold; }
        int  getAGCThersholdGate() const { return m_agcThresholdGate; }

		static MsgConfigureSSBDemodPrivate* create(Real Bandwidth,
				Real LowCutoff,
				Real volume,
				int spanLog2,
				bool audioBinaural,
				bool audioFlipChannels,
				bool dsb,
				bool audioMute,
                bool agc,
                bool agcClamping,
                int  agcTimeLog2,
                int  agcPowerThreshold,
                int  agcThresholdGate)
		{
			return new MsgConfigureSSBDemodPrivate(
			        Bandwidth,
			        LowCutoff,
			        volume,
			        spanLog2,
			        audioBinaural,
			        audioFlipChannels,
			        dsb,
			        audioMute,
			        agc,
			        agcClamping,
			        agcTimeLog2,
			        agcPowerThreshold,
			        agcThresholdGate);
		}

	private:
		Real m_Bandwidth;
		Real m_LowCutoff;
		Real m_volume;
		int  m_spanLog2;
		bool m_audioBinaural;
		bool m_audioFlipChannels;
		bool m_dsb;
		bool m_audioMute;
		bool m_agc;
		bool m_agcClamping;
		int  m_agcTimeLog2;
		int  m_agcPowerThreshold;
		int  m_agcThresholdGate;

		MsgConfigureSSBDemodPrivate(Real Bandwidth,
				Real LowCutoff,
				Real volume,
				int spanLog2,
				bool audioBinaural,
				bool audioFlipChannels,
				bool dsb,
				bool audioMute,
				bool agc,
				bool agcClamping,
				int  agcTimeLog2,
				int  agcPowerThreshold,
				int  agcThresholdGate) :
			Message(),
			m_Bandwidth(Bandwidth),
			m_LowCutoff(LowCutoff),
			m_volume(volume),
			m_spanLog2(spanLog2),
			m_audioBinaural(audioBinaural),
			m_audioFlipChannels(audioFlipChannels),
			m_dsb(dsb),
			m_audioMute(audioMute),
			m_agc(agc),
			m_agcClamping(agcClamping),
			m_agcTimeLog2(agcTimeLog2),
			m_agcPowerThreshold(agcPowerThreshold),
			m_agcThresholdGate(agcThresholdGate)
		{ }
	};

	DeviceSourceAPI *m_deviceAPI;
    ThreadedBasebandSampleSink* m_threadedChannelizer;
    DownChannelizer* m_channelizer;
    SSBDemodSettings m_settings;

	Real m_Bandwidth;
	Real m_LowCutoff;
	Real m_volume;
	int m_spanLog2;
	fftfilt::cmplx m_sum;
	int m_undersampleCount;
	int m_sampleRate;
	int m_absoluteFrequencyOffset;
	bool m_audioBinaual;
	bool m_audioFlipChannels;
	bool m_usb;
	bool m_dsb;
	bool m_audioMute;
	double m_magsq;
	double m_magsqSum;
	double m_magsqPeak;
    int  m_magsqCount;
    MagAGC m_agc;
    bool m_agcActive;
    bool m_agcClamping;
    int m_agcNbSamples;         //!< number of audio (48 kHz) samples for AGC averaging
    double m_agcPowerThreshold; //!< AGC power threshold (linear)
    int m_agcThresholdGate;     //!< Gate length in number of samples befor threshold triggers
    bool m_audioActive;         //!< True if an audio signal is produced (no AGC or AGC and above threshold)

	NCOF m_nco;
	Interpolator m_interpolator;
	Real m_sampleDistanceRemain;
	fftfilt* SSBFilter;
	fftfilt* DSBFilter;

	BasebandSampleSink* m_sampleSink;
	SampleVector m_sampleBuffer;

	AudioVector m_audioBuffer;
	uint m_audioBufferFill;
	AudioFifo m_audioFifo;
	quint32 m_audioSampleRate;

	QMutex m_settingsMutex;

	void applySettings(const SSBDemodSettings& settings, bool force = false);
};

#endif // INCLUDE_SSBDEMOD_H
