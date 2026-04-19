///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 SDRangel Contributors                                      //
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

#ifndef PLUGINS_CHANNELTX_MODCW_CWMODSOURCE_H
#define PLUGINS_CHANNELTX_MODCW_CWMODSOURCE_H

#include <QMutex>
#include <QVector>
#include <QQueue>

#include "dsp/channelsamplesource.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "util/movingaverage.h"

#include "cwmodsettings.h"

class BasebandSampleSink;
class ChannelAPI;

/**
 * Generates I/Q samples for a CW (Morse code) modulated carrier.
 *
 * The carrier is keyed on and off according to International Morse code
 * timing derived from the configured WPM (words per minute) speed.
 */
class CWModSource : public ChannelSampleSource
{
public:
    CWModSource();
    virtual ~CWModSource();

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
    void setMessageQueueToGUI(MessageQueue* messageQueue) { m_messageQueueToGUI = messageQueue; }
    void setSpectrumSink(BasebandSampleSink *sampleSink) { m_spectrumSink = sampleSink; }
    void applySettings(const QStringList& settingsKeys, const CWModSettings& settings, bool force = false);
    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    /** Queue text for Morse encoding and transmission. */
    void addTXText(const QString& text);
    void setChannel(ChannelAPI *channel) { m_channel = channel; }
    int getChannelSampleRate() const { return m_channelSampleRate; }

private:
    // Morse timing element type
    enum MorseElement {
        DOT,
        DASH,
        SYMBOL_SPACE,   ///< Space between symbols within a character (1 dot duration)
        CHAR_SPACE,     ///< Extra silence between characters (2 dot durations; 1 already given by SYMBOL_SPACE = 3 total)
        WORD_SPACE      ///< Extra silence between words (6 dot durations; 1 already given by SYMBOL_SPACE = 7 total)
    };

    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    CWModSettings m_settings;
    ChannelAPI *m_channel;

    NCO m_carrierNco;
    Real m_linearGain;
    Complex m_modSample;

    // Keying state
    QQueue<MorseElement> m_morseQueue; ///< Queue of Morse elements to send
    int m_samplesRemaining;             ///< Samples left in the current element
    bool m_keyOn;                       ///< Whether the carrier is currently keyed on
    int m_dotSamples;                   ///< Number of samples per dot period

    QString m_textQueue; ///< Text waiting to be encoded into Morse
    int m_repeatCounter;

    // Spectrum display
    BasebandSampleSink* m_spectrumSink;
    SampleVector m_specSampleBuffer;
    static const int m_specSampleBufferSize = 256;
    int m_specSampleBufferIndex;
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;
    bool m_interpolatorConsumed;

    // Level metering
    double m_magsq;
    MovingAverageUtil<double, double, 16> m_movingAverage;
    quint32 m_levelCalcCount;
    qreal m_rmsLevel;
    qreal m_peakLevelOut;
    Real m_peakLevel;
    Real m_levelSum;
    static const int m_levelNbSamples = 480; ///< ~10 ms at 48 kSa/s

    MessageQueue* m_messageQueueToGUI;

    /** Encode a text string into the Morse element queue. */
    void encodeMorse(const QString& text);
    /** Append dots/dashes for a single ASCII character. */
    void appendCharacter(QChar c);
    /** Advance to the next Morse element and update keying state. */
    void nextElement();
    /** Recompute m_dotSamples from current WPM and sample rate. */
    void updateTiming();
    void calculateLevel(Real& sample);
    void sampleToSpectrum(Complex sample);
};

#endif // PLUGINS_CHANNELTX_MODCW_CWMODSOURCE_H
