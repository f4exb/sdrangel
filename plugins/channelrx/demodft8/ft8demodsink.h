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

#ifndef INCLUDE_FT8DEMODSINK_H
#define INCLUDE_FT8DEMODSINK_H

#include <QVector>

#include "dsp/channelsamplesink.h"
#include "dsp/ncof.h"
#include "dsp/interpolator.h"
#include "dsp/fftfilt.h"
#include "dsp/agc.h"
#include "audio/audiofifo.h"
#include "util/doublebufferfifo.h"

#include "ft8demodsettings.h"

class SpectrumVis;
class ChannelAPI;

class FT8DemodSink : public ChannelSampleSink {
public:
    FT8DemodSink();
	~FT8DemodSink();

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

	void setSpectrumSink(SpectrumVis* spectrumSink) { m_spectrumSink = spectrumSink; }
	void applyChannelSettings(int inputSampleRate, int inputFrequencyOffset, bool force = false);
	void applySettings(const FT8DemodSettings& settings, bool force = false);
    void applyFT8SampleRate(int sampleRate);

    AudioFifo *getAudioFifo() { return &m_audioFifo; }
    double getMagSq() const { return m_magsq; }
	bool getAudioActive() const { return m_audioActive; }
    void setChannel(ChannelAPI *channel) { m_channel = channel; }
    void setAudioFifoLabel(const QString& label) { m_audioFifo.setLabel(label); }

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

    FT8DemodSettings m_settings;
    ChannelAPI *m_channel;

	Real m_Bandwidth;
	Real m_LowCutoff;
	Real m_volume;
	int m_spanLog2;
	fftfilt::cmplx m_sum;
	int m_undersampleCount;
	int m_channelSampleRate;
	int m_channelFrequencyOffset;
	bool m_usb;
	double m_magsq;
	double m_magsqSum;
	double m_magsqPeak;
    int  m_magsqCount;
    MagSqLevelsStore m_magSqLevelStore;
    MagAGC m_agc;
    bool m_agcActive;
    bool m_audioActive;         //!< True if an audio signal is produced (no AGC or AGC and above threshold)

	NCOF m_nco;
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;
	fftfilt* SSBFilter;

	SpectrumVis* m_spectrumSink;
	SampleVector m_sampleBuffer;

	AudioVector m_audioBuffer;
	uint m_audioBufferFill;
	AudioFifo m_audioFifo;
	quint32 m_ft8SampleRate;

    QVector<qint16> m_demodBuffer;
    int m_demodBufferFill;

	static const int m_ssbFftLen;
	static const int m_agcTarget;

    void processOneSample(Complex &ci);
};

#endif // INCLUDE_FT8DEMODSINK_H

