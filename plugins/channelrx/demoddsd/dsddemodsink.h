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

#ifndef INCLUDE_DSDDEMODSINK_H
#define INCLUDE_DSDDEMODSINK_H

#include <QVector>

#include "dsp/channelsamplesink.h"
#include "dsp/phasediscri.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/firfilter.h"
#include "dsp/afsquelch.h"
#include "dsp/afsquelch.h"
#include "audio/audiofifo.h"
#include "util/movingaverage.h"
#include "util/doublebufferfifo.h"

#include "dsddemodsettings.h"
#include "dsddecoder.h"

class BasebandSampleSink;
class ChannelAPI;
class Feature;

class DSDDemodSink : public ChannelSampleSink {
public:
    DSDDemodSink();
	~DSDDemodSink();

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    void applyAudioSampleRate(int sampleRate);
    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
	void applySettings(const DSDDemodSettings& settings, bool force = false);
    AudioFifo *getAudioFifo1() { return &m_audioFifo1; }
    AudioFifo *getAudioFifo2() { return &m_audioFifo2; }
    void setAudioFifoLabel(const QString& label) {
        m_audioFifo1.setLabel("1:" + label);
        m_audioFifo2.setLabel("2:" + label);
    }
    int getAudioSampleRate() const { return m_audioSampleRate; }
    void setChannel(ChannelAPI *channel) { m_channel = channel; }

	void setScopeXYSink(BasebandSampleSink* scopeSink) { m_scopeXY = scopeSink; }
	void configureMyPosition(float myLatitude, float myLongitude);

	double getMagSq() { return m_magsq; }
	bool getSquelchOpen() const { return m_squelchOpen; }

	const DSDDecoder& getDecoder() const { return m_dsdDecoder; }

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

    const char *updateAndGetStatusText();
    void setAmbeFeature(Feature *feature) { m_ambeFeature = feature; }

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

    typedef enum
    {
        signalFormatNone,
        signalFormatDMR,
        signalFormatDStar,
        signalFormatDPMR,
        signalFormatYSF,
        signalFormatNXDN
    } SignalFormat; //!< Used for status text formatting

	enum RateState {
		RSInitialFill,
		RSRunning
	};

    int m_channelSampleRate;
	int m_channelFrequencyOffset;
	DSDDemodSettings m_settings;
    ChannelAPI *m_channel;
    Feature *m_ambeFeature;
    int m_audioSampleRate;
    QVector<qint16> m_demodBuffer;
    int m_demodBufferFill;

	NCO m_nco;
	Interpolator m_interpolator;
	Real m_interpolatorDistance;
	Real m_interpolatorDistanceRemain;
	int m_sampleCount;
	int m_squelchCount;
	int m_squelchGate;
	double m_squelchLevel;
	bool m_squelchOpen;
    DoubleBufferFIFO<Real> m_squelchDelayLine;

    MovingAverageUtil<Real, double, 16> m_movingAverage;
    double m_magsq;
    double m_magsqSum;
    double m_magsqPeak;
    int  m_magsqCount;
    MagSqLevelsStore m_magSqLevelStore;

	SampleVector m_scopeSampleBuffer;
	AudioVector m_audioBuffer;
	uint m_audioBufferFill;
	FixReal *m_sampleBuffer; //!< samples ring buffer
	int m_sampleBufferIndex;
	int m_scaleFromShort;

	AudioFifo m_audioFifo1;
    AudioFifo m_audioFifo2;
	BasebandSampleSink* m_scopeXY;
	bool m_scopeEnabled;

	DSDDecoder m_dsdDecoder;

	char m_formatStatusText[82+1]; //!< Fixed signal format dependent status text
    SignalFormat m_signalFormat;   //!< Used to keep formatting during successive calls for the same standard type
    PhaseDiscriminators m_phaseDiscri;

    void formatStatusText();
    bool isNotYSFWide();
};

#endif // INCLUDE_DSDDEMODSINK_H
