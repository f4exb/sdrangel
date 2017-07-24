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

#ifndef INCLUDE_NFMDEMOD_H
#define INCLUDE_NFMDEMOD_H

#include <dsp/basebandsamplesink.h>
#include <dsp/phasediscri.h>
#include <QMutex>
#include <vector>
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/lowpass.h"
#include "dsp/bandpass.h"
#include "dsp/afsquelch.h"
#include "dsp/agc.h"
#include "dsp/ctcssdetector.h"
#include "dsp/afsquelch.h"
#include "audio/audiofifo.h"
#include "util/message.h"

class NFMDemodGUI;

class NFMDemod : public BasebandSampleSink {
public:
	NFMDemod();
	~NFMDemod();

	void configure(MessageQueue* messageQueue,
			Real rfBandwidth,
			Real afBandwidth,
			int  fmDeviation,
			Real volume,
			int  squelchGate,
            bool deltaSquelch,
			Real squelch,
			bool ctcssOn,
			bool audioMute,
			bool force);

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool po);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& cmd);

	void registerGUI(NFMDemodGUI *nfmDemodGUI) {
		m_nfmDemodGUI = nfmDemodGUI;
	}

	const Real *getCtcssToneSet(int& nbTones) const {
		nbTones = m_ctcssDetector.getNTones();
		return m_ctcssDetector.getToneSet();
	}

	void setSelectedCtcssIndex(int selectedCtcssIndex) {
		m_ctcssIndexSelected = selectedCtcssIndex;
	}

	Real getMag() { return m_magsq; }
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

private:
	class MsgConfigureNFMDemod : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		Real getRFBandwidth() const { return m_rfBandwidth; }
		Real getAFBandwidth() const { return m_afBandwidth; }
		int  getFMDeviation() const { return m_fmDeviation; }
		Real getVolume() const { return m_volume; }\
		int  getSquelchGate() const { return m_squelchGate; }
		bool getDeltaSquelch() const { return m_deltaSquelch; }
		Real getSquelch() const { return m_squelch; }
		bool getCtcssOn() const { return m_ctcssOn; }
		bool getAudioMute() const { return m_audioMute; }
		bool getForce() const { return m_force; }

		static MsgConfigureNFMDemod* create(Real rfBandwidth,
				Real afBandwidth,
				int  fmDeviation,
				Real volume,
				int  squelchGate,
				bool deltaSquelch,
				Real squelch,
				bool ctcssOn,
				bool audioMute,
				bool force)
		{
			return new MsgConfigureNFMDemod(
			        rfBandwidth,
			        afBandwidth,
			        fmDeviation,
			        volume,
			        squelchGate,
			        deltaSquelch,
			        squelch,
			        ctcssOn,
			        audioMute,
			        force);
		}

	private:
		Real m_rfBandwidth;
		Real m_afBandwidth;
		int  m_fmDeviation;
		Real m_volume;
		int  m_squelchGate;
		bool m_deltaSquelch;
		Real m_squelch;
		bool m_ctcssOn;
		bool m_audioMute;
		bool m_force;

		MsgConfigureNFMDemod(Real rfBandwidth,
				Real afBandwidth,
				int  fmDeviation,
				Real volume,
				int  squelchGate,
				bool deltaSquelch,
				Real squelch,
				bool ctcssOn,
				bool audioMute,
				bool force) :
			Message(),
			m_rfBandwidth(rfBandwidth),
			m_afBandwidth(afBandwidth),
			m_fmDeviation(fmDeviation),
			m_volume(volume),
			m_squelchGate(squelchGate),
			m_deltaSquelch(deltaSquelch),
			m_squelch(squelch),
			m_ctcssOn(ctcssOn),
			m_audioMute(audioMute),
			m_force(force)
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
		Real m_rfBandwidth;
		Real m_afBandwidth;
		int  m_fmDeviation;
		int  m_squelchGate;
		bool m_deltaSquelch;
		Real m_squelch;
		Real m_volume;
		bool m_ctcssOn;
		bool m_audioMute;
		int  m_ctcssIndex;
		quint32 m_audioSampleRate;

		Config() :
			m_inputSampleRate(-1),
			m_inputFrequencyOffset(0),
			m_rfBandwidth(-1),
			m_afBandwidth(-1),
			m_fmDeviation(1),
			m_squelchGate(1),
			m_deltaSquelch(false),
			m_squelch(0),
			m_volume(0),
			m_ctcssOn(false),
			m_audioMute(false),
			m_ctcssIndex(0),
			m_audioSampleRate(0)
		{ }
	};

	Config m_config;
	Config m_running;

	NCO m_nco;
	Interpolator m_interpolator;
	Real m_interpolatorDistance;
	Real m_interpolatorDistanceRemain;
	Lowpass<Real> m_lowpass;
	Bandpass<Real> m_bandpass;
	CTCSSDetector m_ctcssDetector;
	int m_ctcssIndex; // 0 for nothing detected
	int m_ctcssIndexSelected;
	int m_sampleCount;
	int m_squelchCount;
	int m_squelchGate;
	bool m_audioMute;

	Real m_squelchLevel;
	bool m_squelchOpen;
	bool m_afSquelchOpen;
	double m_magsq; //!< displayed averaged value
	double m_magsqSum;
	double m_magsqPeak;
    int  m_magsqCount;

	Real m_lastArgument;
	//Complex m_m1Sample;
	//Complex m_m2Sample;
	MovingAverage<double> m_movingAverage;
	AFSquelch m_afSquelch;
	Real m_agcLevel; // AGC will aim to  this level
	Real m_agcFloor; // AGC will not go below this level

	Real m_fmExcursion;
	//Real m_fmScaling;

	AudioVector m_audioBuffer;
	uint m_audioBufferFill;

	AudioFifo m_audioFifo;

	NFMDemodGUI *m_nfmDemodGUI;
	QMutex m_settingsMutex;

    PhaseDiscriminators m_phaseDiscri;

	void apply(bool force = false);

    float smootherstep(float x)
    {
        if (x == 1.0f) {
            return 1.0f;
        } else if (x == 0.0f) {
            return 0.0f;
        }

        double x3 = x * x * x;
        double x4 = x * x3;
        double x5 = x * x4;

        return (float) (6.0*x5 - 15.0*x4 + 10.0*x3);
    }
};

#endif // INCLUDE_NFMDEMOD_H
