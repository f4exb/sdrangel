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

#ifndef INCLUDE_BFMDEMODSINK_H
#define INCLUDE_BFMDEMODSINK_H

#include <vector>

#include <QVector>

#include "dsp/channelsamplesink.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/firfilter.h"
#include "dsp/movingaverage.h"
#include "dsp/fftfilt.h"
#include "dsp/phaselock.h"
#include "dsp/filterrc.h"
#include "dsp/phasediscri.h"
#include "audio/audiofifo.h"

#include "rdsparser.h"
#include "rdsdecoder.h"
#include "rdsdemod.h"
#include "bfmdemodsettings.h"

class ChannelAPI;
class BasebandSampleSink;

class BFMDemodSink : public ChannelSampleSink {
public:
    BFMDemodSink();
	~BFMDemodSink();

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    void setSpectrumSink(BasebandSampleSink* spectrumSink) { m_spectrumSink = spectrumSink; }
    void setChannel(ChannelAPI *channel) { m_channel = channel; }

	double getMagSq() const { return m_magsq; }

	bool getPilotLock() const { return m_pilotPLL.locked(); }
	Real getPilotLevel() const { return m_pilotPLL.get_pilot_level(); }

	Real getDecoderQua() const { return m_rdsDecoder.m_qua; }
	bool getDecoderSynced() const { return m_rdsDecoder.synced(); }
	Real getDemodAcc() const { return m_rdsDemod.m_report.acc; }
	Real getDemodQua() const { return m_rdsDemod.m_report.qua; }
	Real getDemodFclk() const { return m_rdsDemod.m_report.fclk; }
    int getSquelchState() const { return m_squelchState; }

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

    RDSParser& getRDSParser() { return m_rdsParser; }

    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void applySettings(const BFMDemodSettings& settings, bool force = false);

    AudioFifo *getAudioFifo() { return &m_audioFifo; }
    void setAudioFifoLabel(const QString& label) { m_audioFifo.setLabel(label); }
    void applyAudioSampleRate(int sampleRate);
    int getAudioSampleRate() const { return m_audioSampleRate; }

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

    ChannelAPI *m_channel;
    int m_channelSampleRate;
    int m_channelFrequencyOffset;
	BFMDemodSettings m_settings;

    int m_audioSampleRate;
    AudioVector m_audioBuffer;
    std::size_t m_audioBufferFill;
    AudioFifo m_audioFifo;
	SampleVector m_sampleBuffer;

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
    MagSqLevelsStore m_magSqLevelStore;

	RDSPhaseLock m_pilotPLL;
	Real m_pilotPLLSamples[4];

	RDSDemod m_rdsDemod;
	RDSDecoder m_rdsDecoder;
	RDSParser m_rdsParser;

	LowPassFilterRC m_deemphasisFilterX;
	LowPassFilterRC m_deemphasisFilterY;
    static const Real default_deemphasis;

	Real m_fmExcursion;
	static const int default_excursion;

	PhaseDiscriminators m_phaseDiscri;

    BasebandSampleSink *m_spectrumSink;

    QVector<qint16> m_demodBuffer;
    int m_demodBufferFill;
};

#endif // INCLUDE_BFMDEMODSINK_H
