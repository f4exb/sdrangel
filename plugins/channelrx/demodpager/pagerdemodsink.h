///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019-2021 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2020-2021 Jon Beniston, M7RCE <jon@beniston.com>                //
// Copyright (C) 2020 Kacper Michajłow <kasper93@gmail.com>                      //
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

#ifndef INCLUDE_PAGERDEMODSINK_H
#define INCLUDE_PAGERDEMODSINK_H

#include <QVector>

#include <algorithm>
#include <cmath>
#include <vector>

#include "dsp/channelsamplesink.h"
#include "dsp/phasediscri.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/firfilter.h"
#include "util/movingaverage.h"
#include "util/messagequeue.h"

#include "pagerdemodsettings.h"

#define PAGERDEMOD_FRAMES_PER_BATCH     8
#define PAGERDEMOD_CODEWORDS_PER_FRAME  2
#define PAGERDEMOD_BATCH_WORDS          (1+(PAGERDEMOD_FRAMES_PER_BATCH*PAGERDEMOD_CODEWORDS_PER_FRAME))
#define PAGERDEMOD_POCSAG_SYNCCODE      0x7CD215D8
#define PAGERDEMOD_POCSAG_SYNCCODE_INV  ((quint32)~PAGERDEMOD_POCSAG_SYNCCODE)
#define PAGERDEMOD_POCSAG_IDLECODE      0x7a89c197      /*  0x7ac9c197 in spec */

class ChannelAPI;
class PagerDemod;
class ScopeVis;

// Detects the POCSAG preamble, which is 576 bits of 1010..., i.e. a square wave at half
// the baud rate. Correlates against that frequency and returns the fraction of the
// window's power in that bin: 1.0 for a pure sinusoid, 8/pi^2 = 0.81 for an ideal square
// wave and ~2/N for noise. The window is a whole number of cycles, so a sample leaving it
// has the same twiddle index as the one entering, making the update a single subtract.
class PagerDemodPreambleDetector
{
public:
    void create(int samplesPerSymbol, int cycles)
    {
        m_period = 2 * samplesPerSymbol;    // one cycle of the 1010... pattern
        m_n = cycles * m_period;

        m_cos.resize(m_period);
        m_sin.resize(m_period);

        for (int i = 0; i < m_period; i++)
        {
            double a = 2.0 * M_PI * i / m_period;
            m_cos[i] = cos(a);
            m_sin[i] = sin(a);
        }

        m_hist.assign(m_n, 0.0);
        m_ptr = 0;
        m_idx = 0;
        m_i = 0.0;
        m_q = 0.0;
        m_p = 0.0;
        m_filled = 0;
    }

    double process(double x)
    {
        double old = m_hist[m_ptr];
        m_hist[m_ptr] = x;
        m_ptr = (m_ptr + 1) % m_n;

        double d = x - old;
        m_i += d * m_cos[m_idx];
        m_q += d * m_sin[m_idx];
        m_p += x*x - old*old;
        m_idx = (m_idx + 1) % m_period;

        if (m_filled < m_n)
        {
            m_filled++;
            return 0.0;
        }

        double denom = (m_n / 2.0) * m_p;

        if (denom <= 0.0) {
            return 0.0;
        }

        double m = (m_i*m_i + m_q*m_q) / denom;
        return std::min(1.0, std::max(0.0, m));
    }

private:
    int m_period = 0;
    int m_n = 0;
    std::vector<double> m_cos;
    std::vector<double> m_sin;
    std::vector<double> m_hist;
    int m_ptr = 0;
    int m_idx = 0;
    int m_filled = 0;
    double m_i = 0.0;
    double m_q = 0.0;
    double m_p = 0.0;
};

// Determines which POCSAG baud rate is being transmitted, by running a preamble detector
// per rate. Each rate needs its own post detection filter, as the metric is a fraction of
// the window's power - sharing one wide filter would load the low rate detectors with
// noise they would not otherwise see, biasing detection towards the high rates.
class PagerDemodBaudDetector
{
public:
    static const int m_numRates = 3;
    static constexpr int m_rates[m_numRates] = {512, 1200, 2400};

    void create(int channelSampleRate)
    {
        for (int r = 0; r < m_numRates; r++)
        {
            // Short filters: the detector only has to give each correlator a roughly
            // rate-appropriate noise bandwidth, so it doesn't need the decoder's
            // selectivity - and this runs on every sample while unsynced
            m_lowpass[r].create(63, channelSampleRate, m_rates[r] * 5.0f);
            m_detector[r].create(channelSampleRate / m_rates[r], 8);
            m_metric[r] = 0.0;
        }
    }

    //!< Feed the raw FM demodulator output; returns the index of the most likely rate
    int process(Real fmDemod)
    {
        int best = 0;

        for (int r = 0; r < m_numRates; r++)
        {
            Real filt = m_lowpass[r].filter(fmDemod);
            m_dc[r](filt);
            m_metric[r] = m_detector[r].process(filt - m_dc[r].asDouble());

            if (m_metric[r] > m_metric[best]) {
                best = r;
            }
        }

        return best;
    }

    double metric(int r) const { return m_metric[r]; }

private:
    Lowpass<Real> m_lowpass[m_numRates];
    PagerDemodPreambleDetector m_detector[m_numRates];
    MovingAverageUtil<Real, double, 2048> m_dc[m_numRates];
    double m_metric[m_numRates] = {0.0};
};

class PagerDemodSink : public ChannelSampleSink {
public:
    PagerDemodSink();
    ~PagerDemodSink();

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    void setScopeSink(ScopeVis* scopeSink) { m_scopeSink = scopeSink; }
    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void applySettings(const QStringList& settingsKeys, const PagerDemodSettings& settings, bool force = false);
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

    ScopeVis* m_scopeSink;              // Scope GUI to display debug waveforms
    PagerDemodSettings m_settings;
    ChannelAPI *m_channel;
    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    int m_samplesPerSymbol;             // Number of samples per symbol
    int m_baud;                         // Currently detected baud rate
    PagerDemodBaudDetector m_baudDetector;

    // Matched filter (boxcar integrate and dump over a symbol, which is the matched filter
    // for NRZ) and Gardner timing loop. The symbol clock free runs and is only nudged, so
    // an isolated noise transition can't insert or delete a bit - and a bit slip destroys
    // codeword alignment for the rest of the batch
    std::vector<Real> m_mfBuf;          // Last symbol's worth of samples
    Real m_mfSum;                       // Sum of them, i.e. the matched filter output
    int m_mfPtr;
    Real m_timing;                      // Samples until the next symbol instant
    Real m_yPrev;                       // Matched filter output at the last symbol instant
    Real m_yMid;                        // ...and midway between the last two
    bool m_gotMid;
    Real m_yPower;                      // Running power, to normalise the timing error
    //!< Gardner loop gain. Must be positive - a negative gain locks to the wrong phase and
    //!< decodes nothing, while still sampling at the full symbol rate
    static constexpr Real m_dpllGain = 0.2f;
    //!< Fraction of power at half the baud rate needed to accept a preamble. Measured
    //!< separation is wide - noise and data sit below 0.2, a real preamble above 0.85
    static constexpr double m_preambleThreshold = 0.5;

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

    MovingAverageUtil<Real, double, 2048> m_preambleMovingAverage;

    MovingAverageUtil<Real, double, 16> m_movingAverage;

    PhaseDiscriminators m_phaseDiscri;  // FM demodulator
    Lowpass<Real> m_lowpassBaud;        // Low pass filter for FM demod output
    Real m_dcOffset;                    // Calculated DC offset of preamble
    bool m_inverted;                    // Whether low frequency is a 1 or 0
    int m_bit;                          // Sampled bit
    bool m_gotSOP;                      // Set when sync word received
    quint32 m_bits;                     // Received bit shift register
    int m_bitCount;                     // Number of bits in m_bits

    int m_batchNumber;                  // Count of batches in current transmission
    quint32 m_codeWords[PAGERDEMOD_BATCH_WORDS];        // Received codewords within a batch
    bool m_codeWordsBCHError[PAGERDEMOD_BATCH_WORDS];   // Records if BCH error when decoding this codeword
    int m_wordCount;                                    // Count of number of receive codewords

    bool m_addressValid;                // Indicates we received a (non-idle) address
    quint32 m_address;                  // 21-bit address of current message
    int m_functionBits;                 // 0 = Numeric only, 3 = 7-bit ASCII (CCITT Alphabet No. 5)
    int m_parityErrors;                 // Count of parity errors in current message
    int m_bchErrors;                    // Count of BCH errors in current message
    QString m_numericMessage;           // Message decoded in numeric character set
    QString m_alphaMessage;             // Message decoded in to alphanumeric character set
    quint32 m_alphaBitBuffer;           // Bit buffer to 7-bit chars spread across codewords
    int m_alphaBitBufferBits;           // Count of bits in m_alphaBitBuffer

    QVector<qint16> m_demodBuffer;
    int m_demodBufferFill;
    ComplexVector m_sampleBuffer;
    static const int m_sampleBufferSize = PagerDemodSettings::m_channelSampleRate / 20; // 50ms
    int m_sampleBufferIndex;

    void processOneSample(Complex &ci);
    MessageQueue *getMessageQueueToChannel() { return m_messageQueueToChannel; }
    void sampleToScope(Complex sample);
    void decodeBatch();
    void sendMessage();
    void addMessageBits(int messageBits);
    void setBaud(int baud);
    void handleBit(int bit);
    bool matchedFilterAndDpll(Real v);
    int xorBits(quint32 word, int firstBit, int lastBit);
    bool evenParity(quint32 word, int firstBit, int lastBit, int parityBit);
    quint32 reverse(quint32 x);
    quint32 bchEncode(const quint32 cw);
    bool bchDecode(const quint32 cw, quint32& correctedCW);

};

#endif // INCLUDE_PAGERDEMODSINK_H
