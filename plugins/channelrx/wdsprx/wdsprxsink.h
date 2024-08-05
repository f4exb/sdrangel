///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#ifndef INCLUDE_WDSPRXSINK_H
#define INCLUDE_WDSPRXSINK_H

#include <QVector>

#include "dsp/channelsamplesink.h"
#include "dsp/ncof.h"
#include "dsp/interpolator.h"
#include "dsp/fftfilt.h"
#include "dsp/firfilter.h"
#include "audio/audiofifo.h"
#include "util/doublebufferfifo.h"
#include "bufferprobe.hpp"

#include "wdsprxsettings.h"

class SpectrumVis;
class ChannelAPI;

namespace WDSP {
    class RXA;
}

class WDSPRxSink : public ChannelSampleSink {
public:
    WDSPRxSink();
	~WDSPRxSink();

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

	void setSpectrumSink(SpectrumVis* spectrumSink) { m_spectrumSink = spectrumSink; }
	void applyChannelSettings(int inputSampleRate, int inputFrequencyOffset, bool force = false);
	void applySettings(const WDSPRxSettings& settings, bool force = false);
    void applyAudioSampleRate(int sampleRate);

    AudioFifo *getAudioFifo() { return &m_audioFifo; }
    double getMagSq() const { return m_sAvg; }
	bool getAudioActive() const { return m_audioActive; }
    void setChannel(ChannelAPI *channel) { m_channel = channel; }
    void setAudioFifoLabel(const QString& label) { m_audioFifo.setLabel(label); }
    void getMagSqLevels(double& avg, double& peak, int& nbSamples) const;

private:
    class SpectrumProbe : public WDSP::BufferProbe
    {
    public:
        explicit SpectrumProbe(SampleVector& sampleVector);
        virtual ~SpectrumProbe() = default;
        virtual void proceed(const float *in, int nbSamples);
        void setSpanLog2(int spanLog2);
        void setDSB(bool dsb) { m_dsb = dsb; }
        void setUSB(bool usb) { m_usb = usb; }
    private:
        SampleVector& m_sampleVector;
        int m_spanLog2;
        bool m_dsb;
        bool m_usb;
        uint32_t m_undersampleCount;
        std::complex<float> m_sum;
    };

    struct MagSqLevelsStore
    {
        MagSqLevelsStore() :
            m_magsq(1e-12),
            m_magsqPeak(1e-12)
        {}
        double m_magsq;
        double m_magsqPeak;
    };

    WDSPRxSettings m_settings;
    ChannelAPI *m_channel;

	Real m_Bandwidth;
	int m_undersampleCount;
	int m_channelSampleRate;
	int m_channelFrequencyOffset;
    double m_sAvg;
    double m_sPeak;
    int m_sCount;
    DoubleBufferFIFO<fftfilt::cmplx> m_squelchDelayLine;
    bool m_audioActive;         //!< True if an audio signal is produced (no AGC or AGC and above threshold)

	NCOF m_nco;
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;

	SpectrumVis* m_spectrumSink;
	SampleVector m_sampleBuffer;
    SpectrumProbe m_spectrumProbe;
    int m_inCount;

	AudioVector m_audioBuffer;
	std::size_t m_audioBufferFill;
	AudioFifo m_audioFifo;
	quint32 m_audioSampleRate;

    QVector<qint16> m_demodBuffer;
    int m_demodBufferFill;
    WDSP::RXA *m_rxa;

	static const int m_ssbFftLen;
    static const int m_wdspSampleRate;
    static const int m_wdspBufSize;

    void processOneSample(const Complex &ci);
};

#endif // INCLUDE_SSBDEMODSINK_H
