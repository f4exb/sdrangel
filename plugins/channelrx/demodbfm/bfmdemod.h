///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
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

#ifndef INCLUDE_BFMDEMOD_H
#define INCLUDE_BFMDEMOD_H

#include <dsp/basebandsamplesink.h>
#include <QMutex>
#include <vector>
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/lowpass.h"
#include "dsp/movingaverage.h"
#include "dsp/fftfilt.h"
#include "dsp/phaselock.h"
#include "dsp/filterrc.h"
#include "dsp/phasediscri.h"
#include "audio/audiofifo.h"
#include "util/message.h"
#include "util/udpsink.h"

#include "rdsparser.h"
#include "rdsdecoder.h"
#include "rdsdemod.h"

class DeviceSourceAPI;
class ThreadedBasebandSampleSink;
class DownChannelizer;

class BFMDemod : public BasebandSampleSink {
public:
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

    class MsgReportChannelSampleRateChanged : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getSampleRate() const { return m_sampleRate; }

        static MsgReportChannelSampleRateChanged* create(int sampleRate)
        {
            return new MsgReportChannelSampleRateChanged(sampleRate);
        }

    private:
        int m_sampleRate;

        MsgReportChannelSampleRateChanged(int sampleRate) :
            Message(),
            m_sampleRate(sampleRate)
        { }
    };

	BFMDemod(DeviceSourceAPI *deviceAPI);
	virtual ~BFMDemod();
	void setSampleSink(BasebandSampleSink* sampleSink) { m_sampleSink = sampleSink; }

	void configure(MessageQueue* messageQueue,
			Real rfBandwidth,
			Real afBandwidth,
			Real volume,
			Real squelch,
			bool audioStereo,
			bool lsbStereo,
			bool showPilot,
			bool rdsActive,
			bool copyAudioUDP,
			const QString& m_udpAddress,
			quint16 udpPort,
			bool force = false);

	int getSampleRate() const { return m_config.m_inputSampleRate; }
	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool po);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& cmd);

	double getMagSq() const { return m_magsq; }

	bool getPilotLock() const { return m_pilotPLL.locked(); }
	Real getPilotLevel() const { return m_pilotPLL.get_pilot_level(); }

	Real getDecoderQua() const { return m_rdsDecoder.m_qua; }
	bool getDecoderSynced() const { return m_rdsDecoder.synced(); }
	Real getDemodAcc() const { return m_rdsDemod.m_report.acc; }
	Real getDemodQua() const { return m_rdsDemod.m_report.qua; }
	Real getDemodFclk() const { return m_rdsDemod.m_report.fclk; }

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

    RDSParser& getRDSParser() { return m_rdsParser; }

private slots:
    void channelSampleRateChanged();

private:
	class MsgConfigureBFMDemod : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		Real getRFBandwidth() const { return m_rfBandwidth; }
		Real getAFBandwidth() const { return m_afBandwidth; }
		Real getVolume() const { return m_volume; }
		Real getSquelch() const { return m_squelch; }
		bool getAudioStereo() const { return m_audioStereo; }
		bool getLsbStereo() const { return m_lsbStereo; }
		bool getShowPilot() const { return m_showPilot; }
		bool getRDSActive() const { return m_rdsActive; }
		bool getCopyAudioToUDP() const { return m_copyAudioToUDP; }
		const QString& getUDPAddress() const { return m_udpAddress; }
		quint16 getUDPPort() const { return m_udpPort; }
		bool getForce() const { return m_force; }

		static MsgConfigureBFMDemod* create(Real rfBandwidth,
				Real afBandwidth,
				Real volume,
				Real squelch,
				bool audioStereo,
				bool lsbStereo,
				bool showPilot,
				bool rdsActive,
				bool copyAudioToUDP,
				const QString& udpAddress,
				quint16 udpPort,
				bool force)
		{
			return new MsgConfigureBFMDemod(rfBandwidth,
					afBandwidth,
					volume,
					squelch,
					audioStereo,
					lsbStereo,
					showPilot,
					rdsActive,
					copyAudioToUDP,
					udpAddress,
					udpPort,
					force);
		}

	private:
		Real m_rfBandwidth;
		Real m_afBandwidth;
		Real m_volume;
		Real m_squelch;
		bool m_audioStereo;
		bool m_lsbStereo;
		bool m_showPilot;
		bool m_rdsActive;
		bool m_copyAudioToUDP;
		QString m_udpAddress;
		quint16 m_udpPort;
		bool m_force;

		MsgConfigureBFMDemod(Real rfBandwidth,
				Real afBandwidth,
				Real volume,
				Real squelch,
				bool audioStereo,
				bool lsbStereo,
				bool showPilot,
				bool rdsActive,
				bool copyAudioToUDP,
		        const QString& udpAddress,
		        quint16 udpPort,
		        bool force) :
			Message(),
			m_rfBandwidth(rfBandwidth),
			m_afBandwidth(afBandwidth),
			m_volume(volume),
			m_squelch(squelch),
			m_audioStereo(audioStereo),
			m_lsbStereo(lsbStereo),
			m_showPilot(showPilot),
			m_rdsActive(rdsActive),
			m_copyAudioToUDP(copyAudioToUDP),
			m_udpAddress(udpAddress),
			m_udpPort(udpPort),
			m_force(force)
		{ }
	};

	enum RateState {
		RSInitialFill,
		RSRunning
	};

	struct Config {
		int m_inputSampleRate;
		qint64 m_inputFrequencyOffset;
		Real m_rfBandwidth;
		Real m_afBandwidth;
		Real m_squelch;
		Real m_volume;
		quint32 m_audioSampleRate;
		bool m_audioStereo;
		bool m_lsbStereo;
		bool m_showPilot;
		bool m_rdsActive;
		bool m_copyAudioToUDP;
		QString m_udpAddress;
		quint16 m_udpPort;

		Config() :
			m_inputSampleRate(-1),
			m_inputFrequencyOffset(0),
			m_rfBandwidth(-1),
			m_afBandwidth(-1),
			m_squelch(0),
			m_volume(0),
			m_audioSampleRate(0),
			m_audioStereo(false),
			m_lsbStereo(false),
			m_showPilot(false),
			m_rdsActive(false),
			m_copyAudioToUDP(false),
			m_udpAddress("127.0.0.1"),
			m_udpPort(9999)
		{ }
	};

	Config m_config;
	Config m_running;

	DeviceSourceAPI *m_deviceAPI;
    ThreadedBasebandSampleSink* m_threadedChannelizer;
    DownChannelizer* m_channelizer;

	NCO m_nco;
	Interpolator m_interpolator; //!< Interpolator between fixed demod bandwidth and audio bandwidth (rational)
	Real m_interpolatorDistance;
	Real m_interpolatorDistanceRemain;

	Interpolator m_interpolatorStereo; //!< Twin Interpolator for stereo subcarrier
	Real m_interpolatorStereoDistance;
	Real m_interpolatorStereoDistanceRemain;

	Interpolator m_interpolatorRDS; //!< Twin Interpolator for stereo subcarrier
	Real m_interpolatorRDSDistance;
	Real m_interpolatorRDSDistanceRemain;

	Lowpass<Real> m_lowpass;
	fftfilt* m_rfFilter;
	static const int filtFftLen = 1024;

	Real m_squelchLevel;
	int m_squelchState;

	Real m_m1Arg; //!> x^-1 real sample

    double m_magsq;
    double m_magsqSum;
    double m_magsqPeak;
    int    m_magsqCount;

	AudioVector m_audioBuffer;
	uint m_audioBufferFill;

	BasebandSampleSink* m_sampleSink;
	AudioFifo m_audioFifo;
	SampleVector m_sampleBuffer;
	QMutex m_settingsMutex;

	RDSPhaseLock m_pilotPLL;
	Real m_pilotPLLSamples[4];

	RDSDemod m_rdsDemod;
	RDSDecoder m_rdsDecoder;
	RDSParser m_rdsParser;

	LowPassFilterRC m_deemphasisFilterX;
	LowPassFilterRC m_deemphasisFilterY;
    static const Real default_deemphasis;

	Real m_fmExcursion;
	static const int default_excursion = 750000; // +/- 75 kHz

	PhaseDiscriminators m_phaseDiscri;
    UDPSink<AudioSample> *m_udpBufferAudio;

    static const int m_udpBlockSize;

	void apply(bool force = false);
};

#endif // INCLUDE_BFMDEMOD_H
