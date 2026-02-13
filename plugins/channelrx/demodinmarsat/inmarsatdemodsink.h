///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019-2021 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2020-2026 Jon Beniston, M7RCE <jon@beniston.com>                //
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

#ifndef INCLUDE_INMARSATDEMODSINK_H
#define INCLUDE_INMARSATDEMODSINK_H

#include <inmarsatc_decoder.h>
#include <inmarsatc_parser.h>

#include "dsp/channelsamplesink.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/fftengine.h"
#include "dsp/fftwindow.h"
#include "dsp/costasloop.h"
#include "dsp/rootraisedcosine.h"
#include "util/movingaverage.h"
#include "util/messagequeue.h"

#include "inmarsatdemodsettings.h"

class ChannelAPI;
class InmarsatDemod;
class ScopeVis;

// Automatic Gain Control
class AGC {
public:
    explicit AGC();
    Complex processOneSample(const Complex &iq, bool locked);
    Real getGain() const { return m_gain; }
    Real getAverage() const { return m_agcMovingAverage.instantAverage(); }

private:
    Real m_gain;
    MovingAverageUtil<Real, double, 200> m_agcMovingAverage;
};

class FrequencyOffsetEstimate {
public:
    explicit FrequencyOffsetEstimate();
    ~FrequencyOffsetEstimate();
    void processOneSample(Complex& iq, bool locked);
    Real getFreqHz() const { return m_freqOffsetHz; }
    Real getCurrentFreqHz() const { return m_currentFreqOffsetHz; }
    Real getFreqMagSq() const { return m_freqMagSq; }
    Real getCurrentFreqMagSq() const { return m_currentFreqMagSq; }

private:
    int m_fftSequence;
    FFTEngine *m_fft;
    int m_fftCounter;
    FFTWindow m_fftWindow;
    int m_fftSize;
    int m_freqOffsetBin;
    Real m_freqOffsetHz;
    Real m_currentFreqOffsetHz;
    Real m_freqMagSq;
    Real m_currentFreqMagSq;
    NCO m_freqOffsetNCO;

    Real magSq(int bin) const;
};

// Circular symbol/bit buffer for unique word detection, EVM calculation and equalizer training
class SymbolBuffer {
public:
    explicit SymbolBuffer(int size=64*162);
    void push(quint8 bit, Complex symbol);
    bool checkUW() const;
    Complex getSymbol(int idx) const;
    float evm() const;
    qsizetype size() const { return m_bits.size(); }
private:
    QVector<quint8> m_bits;
    QVector<Complex> m_symbols;
    QVector<float> m_error;
    double m_totalError;
    int m_idx;
    int m_count;
};

class Equalizer {
public:
    explicit Equalizer(int samplesPerSymbol);
    virtual ~Equalizer() {}
    virtual Complex processOneSample(Complex x, bool update, bool training=false) = 0;
    Complex getError() const { return m_error; }
    void printTaps() const;
protected:
    int m_samplesPerSymbol;
    QVector<Complex> m_delay;
    QVector<Complex> m_taps;
    int m_idx;
    Complex m_error;
};

// Constant Modulus Equalizer
class CMAEqualizer : public Equalizer {
public:
    explicit CMAEqualizer(int samplesPerSymbol);
    Complex processOneSample(Complex x, bool update, bool training=false) override;
};

// Least Mean Square Equalizer
class LMSEqualizer : public Equalizer {
public:
    explicit LMSEqualizer(int samplesPerSymbol);
    Complex processOneSample(Complex x, bool update, bool training=false) override;
};

class InmarsatDemodSink : public ChannelSampleSink {
public:
    InmarsatDemodSink(InmarsatDemod *stdcDemod);
    ~InmarsatDemodSink();

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    void setScopeSink(ScopeVis* scopeSink) { m_scopeSink = scopeSink; }
    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void applySettings(const InmarsatDemodSettings& settings, const QStringList& settingsKeys, bool force = false);
    void setMessageQueueToChannel(MessageQueue *messageQueue) { m_messageQueueToChannel = messageQueue; }
    void setChannel(ChannelAPI *channel) { m_channel = channel; }

    double getMagSq() const { return m_magsq; }

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

    void getPLLStatus(bool &locked, Real &coarseFreqCurrent, Real &coarseFreqCurrentPower, Real &coarseFreq, Real &coarseFreqPower,
        Real &fineFreq, Real& evm, bool &synced) const;

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

    ScopeVis* m_scopeSink;    // Scope GUI to display baseband waveform
    InmarsatDemod *m_inmarsatDemod;
    InmarsatDemodSettings m_settings;
    ChannelAPI *m_channel;
    int m_channelSampleRate;
    int m_channelFrequencyOffset;

    NCO m_nco;
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;

    double m_magsq;
    double m_magsqSum;
    double m_magsqPeak;
    int  m_magsqCount;
    MagSqLevelsStore m_magSqLevelStore;

    MessageQueue *m_messageQueueToChannel;

    MovingAverageUtil<Real, double, 16> m_movingAverage;

    inmarsatc::decoder::Decoder m_decoder;
    inmarsatc::frameParser::PacketDecoder m_parser;

    SymbolBuffer m_symbolBuffer;
    int m_symbolCounter;
    bool m_syncedToUW;

    Equalizer *m_equalizer;
    Complex m_eq;
    Complex m_eqError;
    float m_evm;

    CostasLoop m_costasLoop;
    Complex m_ref;
    Complex m_derot;
    MovingAverageUtil<Real, double, 1000> m_lockAverage;
    bool m_locked;

    static const int RRC_FILTER_SIZE = 256;
    Complex m_rrcBuffer[RRC_FILTER_SIZE];
    RootRaisedCosine<Real> m_rrcI;                       //!< Square root raised cosine filter for I samples
    RootRaisedCosine<Real> m_rrcQ;                       //!< Square root raised cosine filter for Q samples
    int m_rrcBufferIndex;

    static const int SAMPLES_PER_SYMBOL = InmarsatDemodSettings::CHANNEL_SAMPLE_RATE / InmarsatDemodSettings::BAUD_RATE;
    static const int MAX_SAMPLES_PER_SYMBOL = SAMPLES_PER_SYMBOL * 2;
    static const int COSTAS_LOOP_RATE = InmarsatDemodSettings::BAUD_RATE; // Costas loop is run at symbol rate

    int m_sampleIdx;
    QQueue<Complex> m_filteredSamples;
    float m_ki;
    float m_kp;
    Real m_error;
    Real m_errorSum;
    Real m_mu;
    int m_adjustedSPS;
    int m_adjustment;
    int m_bit;
    uint8_t m_bits[DEMODULATOR_SYMBOLSPERCHUNK];
    int m_bitCount;

    AGC m_agc;
    FrequencyOffsetEstimate m_frequencyOffsetEst;

    ComplexVector m_sampleBuffer[InmarsatDemodSettings::m_scopeStreams];
    static const int SAMPLE_BUFFER_SIZE = RRC_FILTER_SIZE * 10; // Needs to be a multiple of m_rrcFilterSize, so A & B fill up at the same time
    int m_sampleBufferIndexA;
    int m_sampleBufferIndexB;

    void processOneSample(Complex &ci);
    void decodeBits();
    MessageQueue *getMessageQueueToChannel() { return m_messageQueueToChannel; }
    void sampleToScopeA(Complex sample, Real magsq, Complex postAGC, Real agcGain, Real agcAvg, Complex postCFO);
    void sampleToScopeB(Complex rrc, Complex tedError, Complex tedErrorSum, Complex ref, Complex derot, Complex eq, Complex eqEerror, int bit, Real mu);
};

#endif // INCLUDE_INMARSATDEMODSINK_H
