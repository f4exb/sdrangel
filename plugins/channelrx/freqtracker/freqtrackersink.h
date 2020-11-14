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

#ifndef INCLUDE_FREQTRACKERSINK_H
#define INCLUDE_FREQTRACKERSINK_H

#include <QObject>

#include <vector>

#include "dsp/channelsamplesink.h"
#include "dsp/ncof.h"
#include "dsp/interpolator.h"
#include "util/movingaverage.h"
#include "dsp/agc.h"
#include "dsp/firfilter.h"
#include "dsp/phaselockcomplex.h"
#include "dsp/freqlockcomplex.h"
#include "util/message.h"
#include "util/doublebufferfifo.h"

#include "freqtrackersettings.h"

class SpectrumVis;
class fftfilt;
class MessageQueue;
class QTimer;

class FreqTrackerSink : public QObject, public ChannelSampleSink {
    Q_OBJECT
public:
    FreqTrackerSink();
	~FreqTrackerSink();

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    void setSpectrumSink(SpectrumVis* spectrumSink) { m_spectrumSink = spectrumSink; }
    void applySettings(const FreqTrackerSettings& settings, bool force = false);
    void applyChannelSettings(int sinkSampleRate, int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void setMessageQueueToInput(MessageQueue *messageQueue) { m_messageQueueToInput = messageQueue;}

    uint32_t getSampleRate() const { return m_sinkSampleRate; }
	double getMagSq() const { return m_magsq; }
	bool getSquelchOpen() const { return m_squelchOpen; }
	bool getPllLocked() const { return (m_settings.m_trackerType == FreqTrackerSettings::TrackerPLL) && m_pll.locked(); }
	Real getFrequency() const;
    Real getAvgDeltaFreq() const { return m_avgDeltaFreq; }

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

    FreqTrackerSettings m_settings;

    int m_channelSampleRate;
    int m_inputFrequencyOffset;
    int m_sinkSampleRate;

	SpectrumVis* m_spectrumSink;
	SampleVector m_sampleBuffer;
    unsigned int m_sampleBufferCount;
    unsigned int m_sampleBufferSize;
    Complex m_sum;
  	int m_undersampleCount;

	NCOF m_nco;
    PhaseLockComplex m_pll;
    FreqLockComplex m_fll;
	Interpolator m_interpolator;
	Real m_interpolatorDistance;
	Real m_interpolatorDistanceRemain;

	fftfilt* m_rrcFilter;

	Real m_squelchLevel;
	uint32_t m_squelchCount;
	bool m_squelchOpen;
    uint32_t m_squelchGate; //!< Squelch gate in samples
	double m_magsq;
	double m_magsqSum;
	double m_magsqPeak;
	int  m_magsqCount;
	MagSqLevelsStore m_magSqLevelStore;

	MovingAverageUtil<Real, double, 16> m_movingAverage;

    const QTimer *m_timer;
    bool m_timerConnected;
    uint32_t m_tickCount;
    int m_lastCorrAbs;
    Real m_avgDeltaFreq;

    MessageQueue *m_messageQueueToInput;

    MessageQueue *getMessageQueueToInput() { return m_messageQueueToInput; }
    void setInterpolator();
    void connectTimer();
    void disconnectTimer();
    void processOneSample(Complex &ci);

private slots:
	void tick();
};

#endif // INCLUDE_FREQTRACKERSINK_H
