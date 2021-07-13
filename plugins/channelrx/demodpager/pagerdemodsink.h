///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

class PagerDemodSink : public ChannelSampleSink {
public:
    PagerDemodSink(PagerDemod *pagerDemod);
    ~PagerDemodSink();

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    void setScopeSink(ScopeVis* scopeSink) { m_scopeSink = scopeSink; }
    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void applySettings(const PagerDemodSettings& settings, bool force = false);
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
    PagerDemod *m_pagerDemod;
    PagerDemodSettings m_settings;
    ChannelAPI *m_channel;
    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    int m_samplesPerSymbol;             // Number of samples per symbol

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

    Lowpass<Complex> m_lowpass;         // RF input filter
    PhaseDiscriminators m_phaseDiscri;  // FM demodulator
    Lowpass<Real> m_lowpassBaud;        // Low pass filter for FM demod output
    Real m_dcOffset;                    // Calculated DC offset of preamble
    int m_dataPrev;                     // m_data for previous sample
    bool m_inverted;                    // Whether low frequency is a 1 or 0
    int m_bit;                          // Sampled bit
    bool m_gotSOP;                      // Set when sync word received
    quint32 m_bits;                     // Received bit shift register
    int m_bitCount;                     // Number of bits in m_bits
    int m_syncCount;                    // Sample count to centre of bit

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
    int xorBits(quint32 word, int firstBit, int lastBit);
    bool evenParity(quint32 word, int firstBit, int lastBit, int parityBit);
    quint32 reverse(quint32 x);
    quint32 bchEncode(const quint32 cw);
    bool bchDecode(const quint32 cw, quint32& correctedCW);

};

#endif // INCLUDE_PAGERDEMODSINK_H
