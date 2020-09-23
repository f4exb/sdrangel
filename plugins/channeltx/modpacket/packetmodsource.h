///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_PACKETMODSOURCE_H
#define INCLUDE_PACKETMODSOURCE_H

#include <QMutex>
#include <QDebug>

#include <iostream>
#include <fstream>

#include "dsp/channelsamplesource.h"
#include "dsp/nco.h"
#include "dsp/ncof.h"
#include "dsp/interpolator.h"
#include "dsp/lowpass.h"
#include "dsp/bandpass.h"
#include "dsp/highpass.h"
#include "dsp/raisedcosine.h"
#include "dsp/fmpreemphasis.h"
#include "util/lfsr.h"
#include "util/movingaverage.h"

#include "packetmodsettings.h"

#define AX25_MAX_FLAGS  1024
#define AX25_MAX_BYTES  (2*AX25_MAX_FLAGS+1+28+2+256+2+1)
#define AX25_MAX_BITS   (AX25_MAX_BYTES*2)
#define AX25_FLAG       0x7e
#define AX25_NO_L3      0xf0

class BasebandSampleSink;

class PacketModSource : public ChannelSampleSource
{
public:
    PacketModSource();
    virtual ~PacketModSource();

    virtual void pull(SampleVector::iterator begin, unsigned int nbSamples);
    virtual void pullOne(Sample& sample);
    virtual void prefetch(unsigned int nbSamples);

    double getMagSq() const { return m_magsq; }
    void getLevels(qreal& rmsLevel, qreal& peakLevel, int& numSamples) const
    {
        rmsLevel = m_rmsLevel;
        peakLevel = m_peakLevelOut;
        numSamples = m_levelNbSamples;
    }
    void setSpectrumSink(BasebandSampleSink *sampleSink) { m_spectrumSink = sampleSink; }
    void applySettings(const PacketModSettings& settings, bool force = false);
    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void addTXPacket(QString callsign, QString to, QString via, QString data);

private:
    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    int m_spectrumRate;
    PacketModSettings m_settings;

    NCO m_carrierNco;
    Real m_audioPhase;
    double m_fmPhase;                   // Double gives cleaner spectrum than Real
    double m_phaseSensitivity;
    Real m_linearGain;
    Complex m_modSample;

    int m_nrziBit;                      // Output of NRZI coder
    int m_scrambledBit;                 // Output from scrambler to be pulse shaped
    RaisedCosine<Real> m_pulseShape;    // Pulse shaping filter
    Bandpass<Real> m_bandpass;          // Baseband bandpass filter for AFSK
    Lowpass<Complex> m_lowpass;         // Low pass filter to limit RF bandwidth
    FMPreemphasis m_preemphasisFilter;  // FM preemphasis filter to amplify high frequencies

    BasebandSampleSink* m_spectrumSink; // Spectrum GUI to display baseband waveform
    SampleVector m_sampleBuffer;
    Interpolator m_interpolator;        // Interpolator to downsample to 4k in spectrum
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;
    bool m_interpolatorConsumed;

    double m_magsq;
    MovingAverageUtil<double, double, 16> m_movingAverage;

    quint32 m_levelCalcCount;
    qreal m_rmsLevel;
    qreal m_peakLevelOut;
    Real m_peakLevel;
    Real m_levelSum;

    static const int m_levelNbSamples = 480;  // every 10ms assuming 48k Sa/s

    int m_sampleIdx;                    // Sample index in to symbol
    int m_samplesPerSymbol;             // Number of samples per symbol
    Real m_pow;                         // In dB
    Real m_powRamp;                     // In dB
    enum PacketModState {
        idle, ramp_up, tx, ramp_down, wait
    } m_state;                          // States for sample modulation
    int m_packetRepeatCount;
    uint64_t m_waitCounter;             // Samples to wait before retransmission

    uint8_t m_bits[AX25_MAX_BITS];      // HDLC encoded bits to transmit
    int m_byteIdx;                      // Index in to m_bits
    int m_bitIdx;                       // Index in to current byte of m_bits
    int m_last5Bits;                    // Last 5 bits to be HDLC encoded
    int m_bitCount;                     // Count of number of valid bits in m_bits
    int m_bitCountTotal;

    LFSR m_scrambler;                   // Scrambler

    std::ofstream m_audioFile;          // For debug output of baseband waveform

    bool bitsValid();                   // Are there and bits to transmit
    int getBit();                       // Get bit from m_bits
    void addBit(int bit);               // Add bit to m_bits, with zero stuffing
    void initTX();

    void calculateLevel(Real& sample);
    void modulateSample();
    void sampleToSpectrum(Real sample);

};

#endif // INCLUDE_PACKETMODSOURCE_H
