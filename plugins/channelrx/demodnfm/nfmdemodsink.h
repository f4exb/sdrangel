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

#ifndef INCLUDE_NFMDEMODSINK_H
#define INCLUDE_NFMDEMODSINK_H

#include <vector>

#include "dsp/channelsamplesink.h"
#include "dsp/phasediscri.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/lowpass.h"
#include "dsp/bandpass.h"
#include "dsp/afsquelch.h"
#include "dsp/agc.h"
#include "dsp/ctcssdetector.h"
#include "util/movingaverage.h"
#include "util/doublebufferfifo.h"
#include "audio/audiofifo.h"

#include "nfmdemodsettings.h"

class NFMDemodSink : public ChannelSampleSink {
public:
    NFMDemodSink();
	~NFMDemodSink();

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

	const Real *getCtcssToneSet(int& nbTones) const {
		nbTones = m_ctcssDetector.getNTones();
		return m_ctcssDetector.getToneSet();
	}

	void setSelectedCtcssIndex(int selectedCtcssIndex) {
		m_ctcssIndexSelected = selectedCtcssIndex;
	}

	bool getSquelchOpen() const { return m_squelchOpen; }

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

    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void applySettings(const NFMDemodSettings& settings, bool force = false);
    void setMessageQueueToGUI(MessageQueue *messageQueue) { m_messageQueueToGUI = messageQueue; }

    AudioFifo *getAudioFifo() { return &m_audioFifo; }
    void applyAudioSampleRate(unsigned int sampleRate);
    unsigned int getAudioSampleRate() const { return m_audioSampleRate; }

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

	enum RateState {
		RSInitialFill,
		RSRunning
	};

    int m_channelSampleRate;
    int m_channelFrequencyOffset;
	NFMDemodSettings m_settings;

    quint32 m_audioSampleRate;
    AudioVector m_audioBuffer;
    uint m_audioBufferFill;
    AudioFifo m_audioFifo;

	float m_discriCompensation; //!< compensation factor that depends on audio rate (1 for 48 kS/s)

	NCO m_nco;
	Interpolator m_interpolator;
	Real m_interpolatorDistance;
	Real m_interpolatorDistanceRemain;
	Lowpass<Real> m_ctcssLowpass;
	Bandpass<Real> m_bandpass;
    Lowpass<Real> m_lowpass;
	CTCSSDetector m_ctcssDetector;
	int m_ctcssIndex; // 0 for nothing detected
	int m_ctcssIndexSelected;
	int m_sampleCount;
	int m_squelchCount;
	int m_squelchGate;

	Real m_squelchLevel;
	bool m_squelchOpen;
	bool m_afSquelchOpen;
	double m_magsq; //!< displayed averaged value
	double m_magsqSum;
	double m_magsqPeak;
    int  m_magsqCount;
    MagSqLevelsStore m_magSqLevelStore;

	MovingAverageUtil<Real, double, 32> m_movingAverage;
	AFSquelch m_afSquelch;
	Real m_agcLevel; // AGC will aim to  this level
	DoubleBufferFIFO<Real> m_squelchDelayLine;

    PhaseDiscriminators m_phaseDiscri;
    MessageQueue *m_messageQueueToGUI;

    static const double afSqTones[];
    static const double afSqTones_lowrate[];

    void processOneSample(Complex &ci);
    MessageQueue *getMessageQueueToGUI() { return m_messageQueueToGUI; }

    inline float arctan2(Real y, Real x)
    {
        Real coeff_1 = M_PI / 4;
        Real coeff_2 = 3 * coeff_1;
        Real abs_y = fabs(y) + 1e-10;      // kludge to prevent 0/0 condition
        Real angle;
        if( x>= 0) {
            Real r = (x - abs_y) / (x + abs_y);
            angle = coeff_1 - coeff_1 * r;
        } else {
            Real r = (x + abs_y) / (abs_y - x);
            angle = coeff_2 - coeff_1 * r;
        }
        if(y < 0) {
            return(-angle);
        } else {
            return(angle);
        }
    }

    inline Real angleDist(Real a, Real b)
    {
        Real dist = b - a;

        while(dist <= M_PI)
            dist += 2 * M_PI;
        while(dist >= M_PI)
            dist -= 2 * M_PI;

        return dist;
    }

};

#endif // INCLUDE_NFMDEMODSINK_H
