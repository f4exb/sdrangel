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

#include <dsp/basebandsamplesink.h>
#include <dsp/phasediscri.h>
#include <QMutex>
#include <vector>
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/lowpass.h"
#include "dsp/bandpass.h"
#include "dsp/afsquelch.h"
#include "dsp/movingaverage.h"
#include "dsp/afsquelch.h"
#include "audio/audiofifo.h"
#include "util/message.h"

#include "../../channelrx/demoddsd/dsddecoder.h"

class DSDDemodGUI;

class DSDDemod : public BasebandSampleSink {
public:
    DSDDemod(BasebandSampleSink* sampleSink);
	~DSDDemod();

	void configure(MessageQueue* messageQueue,
			int  rfBandwidth,
			int  demodGain,
            int  volume,
            int  baudRate,
			int  fmDeviation,
			int  squelchGate,
			Real squelch,
			bool audioMute,
			bool enableCosineFiltering,
			bool syncOrConstellation,
			bool slot1On,
			bool slot2On,
			bool tdmaStereo);

	void configureMyPosition(MessageQueue* messageQueue, float myLatitude, float myLongitude);

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool po);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& cmd);

	void registerGUI(DSDDemodGUI *dsdDemodGUI) {
		m_dsdDemodGUI = dsdDemodGUI;
	}

	double getMagSq() { return m_magsq; }
	bool getSquelchOpen() const { return m_squelchOpen; }

	const DSDDecoder& getDecoder() const { return m_dsdDecoder; }

    void getMagSqLevels(Real& avg, Real& peak, int& nbSamples)
    {
        avg = m_magsqSum / m_magsqCount;
        m_magsq = avg;
        peak = m_magsqPeak;
        nbSamples = m_magsqCount;
        m_magsqSum = 0.0f;
        m_magsqPeak = 0.0f;
        m_magsqCount = 0;
    }

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

	class MsgConfigureDSDDemod : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		int  getRFBandwidth() const { return m_rfBandwidth; }
		int  getDemodGain() const { return m_demodGain; }
		int  getFMDeviation() const { return m_fmDeviation; }
        int  getVolume() const { return m_volume; }
        int  getBaudRate() const { return m_baudRate; }
		int  getSquelchGate() const { return m_squelchGate; }
		Real getSquelch() const { return m_squelch; }
		bool getAudioMute() const { return m_audioMute; }
		bool getEnableCosineFiltering() const { return m_enableCosineFiltering; }
		bool getSyncOrConstellation() const { return m_syncOrConstellation; }
		bool getSlot1On() const { return m_slot1On; }
		bool getSlot2On() const { return m_slot2On; }
		bool getTDMAStereo() const { return m_tdmaStereo; }

		static MsgConfigureDSDDemod* create(int rfBandwidth,
				int  demodGain,
				int  fmDeviation,
				int  volume,
				int  baudRate,
				int  squelchGate,
				Real squelch,
				bool audioMute,
				bool enableCosineFiltering,
				bool syncOrConstellation,
				bool slot1On,
				bool slot2On,
				bool tdmaStereo)
		{
			return new MsgConfigureDSDDemod(rfBandwidth,
			        demodGain,
			        fmDeviation,
			        volume,
			        baudRate,
			        squelchGate,
			        squelch,
			        audioMute,
			        enableCosineFiltering,
			        syncOrConstellation,
			        slot1On,
			        slot2On,
			        tdmaStereo);
		}

	private:
		Real m_rfBandwidth;
		Real m_demodGain;
		int  m_fmDeviation;
		int  m_volume;
		int  m_baudRate;
		int  m_squelchGate;
		Real m_squelch;
		bool m_audioMute;
		bool m_enableCosineFiltering;
		bool m_syncOrConstellation;
        bool m_slot1On;
        bool m_slot2On;
        bool m_tdmaStereo;

		MsgConfigureDSDDemod(int rfBandwidth,
				int  demodGain,
				int  fmDeviation,
				int  volume,
				int  baudRate,
				int  squelchGate,
				Real squelch,
				bool audioMute,
				bool enableCosineFiltering,
				bool syncOrConstellation,
				bool slot1On,
				bool slot2On,
				bool tdmaStereo) :
			Message(),
			m_rfBandwidth(rfBandwidth),
			m_demodGain(demodGain),
			m_fmDeviation(fmDeviation),
			m_volume(volume),
			m_baudRate(baudRate),
			m_squelchGate(squelchGate),
			m_squelch(squelch),
			m_audioMute(audioMute),
			m_enableCosineFiltering(enableCosineFiltering),
			m_syncOrConstellation(syncOrConstellation),
			m_slot1On(slot1On),
			m_slot2On(slot2On),
			m_tdmaStereo(tdmaStereo)
		{ }
	};

	struct AudioSample {
		qint16 l;
		qint16 r;
	};
	typedef std::vector<AudioSample> AudioVector;

	enum RateState {
		RSInitialFill,
		RSRunning
	};

	struct Config {
		int m_inputSampleRate;
		qint64 m_inputFrequencyOffset;
		int  m_rfBandwidth;
		int  m_demodGain;
		int  m_volume;
		int  m_baudRate;
		int  m_fmDeviation;
		int  m_squelchGate;
		Real m_squelch;
		bool m_audioMute;
		quint32 m_audioSampleRate;
		bool m_enableCosineFiltering;
		bool m_syncOrConstellation;
		bool m_slot1On;
		bool m_slot2On;
		bool m_tdmaStereo;

		Config() :
			m_inputSampleRate(-1),
			m_inputFrequencyOffset(0),
			m_rfBandwidth(-1),
			m_demodGain(-1),
			m_volume(-1),
			m_baudRate(4800),
			m_fmDeviation(1),
			m_squelchGate(1),
			m_squelch(0),
			m_audioMute(false),
			m_audioSampleRate(0),
			m_enableCosineFiltering(false),
			m_syncOrConstellation(false),
			m_slot1On(false),
			m_slot2On(false),
			m_tdmaStereo(false)
		{ }
	};

	Config m_config;
	Config m_running;

	NCO m_nco;
	Interpolator m_interpolator;
	Real m_interpolatorDistance;
	Real m_interpolatorDistanceRemain;
	int m_sampleCount;
	int m_squelchCount;
	int m_squelchGate;

	double m_squelchLevel;
	bool m_squelchOpen;

	Real m_lastArgument;
    MovingAverage<double> m_movingAverage;
    Real m_magsq;
    Real m_magsqSum;
    Real m_magsqPeak;
    int  m_magsqCount;

	Real m_fmExcursion;

	SampleVector m_scopeSampleBuffer;
	AudioVector m_audioBuffer;
	uint m_audioBufferFill;
	qint16 *m_sampleBuffer; //!< samples ring buffer
	int m_sampleBufferIndex;

	AudioFifo m_audioFifo1;
    AudioFifo m_audioFifo2;
	BasebandSampleSink* m_scope;
	bool m_scopeEnabled;

	DSDDecoder m_dsdDecoder;
	DSDDemodGUI *m_dsdDemodGUI;
	QMutex m_settingsMutex;

    PhaseDiscriminators m_phaseDiscri;

	void apply();
};

#endif // INCLUDE_DSDDEMOD_H
