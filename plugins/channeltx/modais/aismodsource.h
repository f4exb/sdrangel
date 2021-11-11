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

#ifndef INCLUDE_AISMODSOURCE_H
#define INCLUDE_AISMODSOURCE_H

#include <QMutex>
#include <QDebug>
#include <QVector>

#include <iostream>
#include <fstream>

#include "dsp/channelsamplesource.h"
#include "dsp/nco.h"
#include "dsp/ncof.h"
#include "dsp/interpolator.h"
#include "dsp/firfilter.h"
#include "dsp/gaussian.h"
#include "util/movingaverage.h"

#include "aismodsettings.h"

// Train, flag, data, crc, flag and a zero for ramp down. Longest message is 1008/8=126 bytes
#define AIS_MAX_BYTES   (3+1+126+2+1+1)
// Add extra space for bit stuffing
#define AIS_MAX_BITS    (AIS_MAX_BYTES*2)
#define AIS_TRAIN       0x55
#define AIS_FLAG        0x7e

class ScopeVis;
class BasebandSampleSink;
class ChannelAPI;

class AISModSource : public ChannelSampleSource
{
public:
    AISModSource();
    virtual ~AISModSource();

    virtual void pull(SampleVector::iterator begin, unsigned int nbSamples);
    virtual void pullOne(Sample& sample);
    virtual void prefetch(unsigned int nbSamples) { (void) nbSamples; }

    double getMagSq() const { return m_magsq; }
    void getLevels(qreal& rmsLevel, qreal& peakLevel, int& numSamples) const
    {
        rmsLevel = m_rmsLevel;
        peakLevel = m_peakLevelOut;
        numSamples = m_levelNbSamples;
    }
    void setSpectrumSink(BasebandSampleSink *sampleSink) { m_spectrumSink = sampleSink; }
    void setScopeSink(ScopeVis* scopeSink) { m_scopeSink = scopeSink; }
    void applySettings(const AISModSettings& settings, bool force = false);
    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void addTXPacket(const QString& data);
    void addTXPacket(QByteArray data);
    void encodePacket(uint8_t *packet, int packet_length, uint8_t *crc_start, uint8_t *packet_end);
    void setChannel(ChannelAPI *channel) { m_channel = channel; }
    void transmit();

private:
    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    AISModSettings m_settings;
    ChannelAPI *m_channel;

    NCO m_carrierNco;
    double m_fmPhase;                   // Double gives cleaner spectrum than Real
    double m_phaseSensitivity;
    Real m_linearGain;
    Complex m_modSample;

    int m_nrziBit;                      // Output of NRZI coder
    Gaussian<Real> m_pulseShape;        // Pulse shaping filter

    BasebandSampleSink* m_spectrumSink; // Spectrum GUI to display baseband waveform
    ScopeVis* m_scopeSink;    // Scope GUI to display baseband waveform

    Interpolator m_interpolator;        // Interpolator to channel sample rate
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;

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
    enum AISModState {
        idle, ramp_up, tx, ramp_down, wait
    } m_state;                          // States for sample modulation
    int m_packetRepeatCount;
    uint64_t m_waitCounter;             // Samples to wait before retransmission

    uint8_t m_bits[AIS_MAX_BITS];       // HDLC encoded bits to transmit
    int m_byteIdx;                      // Index in to m_bits
    int m_bitIdx;                       // Index in to current byte of m_bits
    int m_last5Bits;                    // Last 5 bits to be HDLC encoded
    int m_bitCount;                     // Count of number of valid bits in m_bits
    int m_bitCountTotal;

    std::ofstream m_iqFile;             // For debug output of baseband waveform

    QVector<qint16> m_demodBuffer;
    int m_demodBufferFill;

    SampleVector m_scopeSampleBuffer;
    static const int m_scopeSampleBufferSize = AISModSettings::AISMOD_SAMPLE_RATE / 20;
    int m_scopeSampleBufferIndex;

    SampleVector m_specSampleBuffer;
    static const int m_specSampleBufferSize = 1024;
    int m_specSampleBufferIndex;

    bool bitsValid();                   // Are there and bits to transmit
    int getBit();                       // Get bit from m_bits
    void addBit(int bit);               // Add bit to m_bits, with zero stuffing
    void initTX();

    void calculateLevel(Real& sample);
    void modulateSample();
    void sampleToSpectrum(Complex sample);
    void sampleToScope(Complex sample);

};

#endif // INCLUDE_AISMODSOURCE_H
