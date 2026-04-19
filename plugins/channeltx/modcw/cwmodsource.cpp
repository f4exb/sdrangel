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

#include <cmath>

#include <QDebug>

#include "dsp/basebandsamplesink.h"
#include "channel/channelapi.h"

#include "cwmodsource.h"

// ITU International Morse Code table (A-Z, 0-9 and common punctuation).
// Each entry is a NUL-terminated string of '.' and '-'.
static const struct { char ch; const char* code; } s_morseTable[] = {
    {'A', ".-"},   {'B', "-..."}, {'C', "-.-."}, {'D', "-.."},
    {'E', "."},    {'F', "..-."}, {'G', "--."},  {'H', "...."},
    {'I', ".."},   {'J', ".---"}, {'K', "-.-"},  {'L', ".-.."},
    {'M', "--"},   {'N', "-."},   {'O', "---"},  {'P', ".--."},
    {'Q', "--.-"}, {'R', ".-."},  {'S', "..."},  {'T', "-"},
    {'U', "..-"},  {'V', "...-"}, {'W', ".--"},  {'X', "-..-"},
    {'Y', "-.--"}, {'Z', "--.."},
    {'0', "-----"},{'1', ".----"},{'2', "..---"},{'3', "...--"},
    {'4', "....-"},{'5', "....."},{'6', "-...."},{'7', "--..."},
    {'8', "---.."},{'9', "----."},
    {'.', ".-.-.-"},{',', "--..--"},{'?', "..--.."},{'\'',".--.-."},
    {'!', "-.-.--"},{'/', "-..-."}, {'(', "-.--."}, {')', "-.--.-"},
    {'&', ".-..."},  {':', "---..."},  {';', "-.-.-."}, {'=', "-...-"},
    {'+', ".-.-."}, {'-', "-....-"}, {'_', "..--.-"},  {'"', ".-..-."},
    {'$', "...-..-"},{'@', ".--.-."},
    {0, nullptr}
};

CWModSource::CWModSource() :
    m_channelSampleRate(48000),
    m_channelFrequencyOffset(0),
    m_channel(nullptr),
    m_samplesRemaining(0),
    m_keyOn(false),
    m_dotSamples(0),
    m_repeatCounter(0),
    m_spectrumSink(nullptr),
    m_specSampleBufferIndex(0),
    m_interpolatorDistance(0.0f),
    m_interpolatorDistanceRemain(0.0f),
    m_interpolatorConsumed(false),
    m_magsq(0.0),
    m_levelCalcCount(0),
    m_rmsLevel(0.0f),
    m_peakLevelOut(0.0f),
    m_peakLevel(0.0f),
    m_levelSum(0.0f),
    m_messageQueueToGUI(nullptr)
{
    m_specSampleBuffer.resize(m_specSampleBufferSize);
    applySettings(QStringList(), m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

CWModSource::~CWModSource()
{
}

void CWModSource::pull(SampleVector::iterator begin, unsigned int nbSamples)
{
    for (SampleVector::iterator it = begin; it != begin + nbSamples; ++it) {
        pullOne(*it);
    }
}

void CWModSource::pullOne(Sample& sample)
{
    // If nothing queued, try to encode more text
    if (m_morseQueue.isEmpty() && !m_textQueue.isEmpty())
    {
        encodeMorse(m_textQueue);
        m_textQueue.clear();
    }

    // If still nothing and repeat is set, re-queue the text
    if (m_morseQueue.isEmpty() && m_settings.m_repeat)
    {
        if (m_settings.m_repeatCount < 0 || m_repeatCounter < m_settings.m_repeatCount)
        {
            encodeMorse(m_settings.m_text);
            if (m_settings.m_repeatCount >= 0) {
                m_repeatCounter++;
            }
        }
    }

    // Advance to next element when the current one has elapsed
    if (m_samplesRemaining <= 0) {
        nextElement();
    }

    // Generate the sample
    Complex c;
    if (m_keyOn && !m_settings.m_channelMute)
    {
        c = m_carrierNco.nextIQ() * (double)m_linearGain;
    }
    else
    {
        // Still advance the NCO to keep phase coherent
        m_carrierNco.nextIQ();
        c = {0.0f, 0.0f};
    }

    m_samplesRemaining--;

    // Level metering
    double magsq = c.real() * c.real() + c.imag() * c.imag();
    m_movingAverage(magsq);
    m_magsq = m_movingAverage.asDouble();
    calculateLevel(c.real());

    // Spectrum sink
    sampleToSpectrum(c);

    Real outputI = SDR_TX_SCALEF * c.real();
    Real outputQ = SDR_TX_SCALEF * c.imag();
    sample.m_real = (FixReal)outputI;
    sample.m_imag = (FixReal)outputQ;
}

void CWModSource::addTXText(const QString& text)
{
    m_textQueue += text;
}

void CWModSource::nextElement()
{
    if (m_morseQueue.isEmpty())
    {
        // Silence when queue is empty
        m_keyOn = false;
        m_samplesRemaining = m_dotSamples > 0 ? m_dotSamples : 48;
        return;
    }

    MorseElement elem = m_morseQueue.dequeue();
    switch (elem)
    {
    case DOT:
        m_keyOn = true;
        m_samplesRemaining = m_dotSamples;
        break;
    case DASH:
        m_keyOn = true;
        m_samplesRemaining = 3 * m_dotSamples;
        break;
    case SYMBOL_SPACE:
        m_keyOn = false;
        m_samplesRemaining = m_dotSamples;
        break;
    case CHAR_SPACE:
        m_keyOn = false;
        m_samplesRemaining = 3 * m_dotSamples; // 3 dots minus the 1 already counted
        break;
    case WORD_SPACE:
        m_keyOn = false;
        m_samplesRemaining = 7 * m_dotSamples; // 7 dots minus the 1 already counted
        break;
    default:
        m_keyOn = false;
        m_samplesRemaining = m_dotSamples;
        break;
    }
}

void CWModSource::updateTiming()
{
    // A dot in standard Morse = 1.2 / WPM seconds
    double dotDurationSec = 1.2 / (double)m_settings.m_wpm;
    m_dotSamples = (int)(dotDurationSec * m_channelSampleRate);
    if (m_dotSamples < 1) {
        m_dotSamples = 1;
    }
}

void CWModSource::encodeMorse(const QString& text)
{
    QString upper = text.toUpper();
    bool firstChar = true;

    for (int i = 0; i < upper.size(); i++)
    {
        QChar ch = upper[i];

        if (ch == ' ')
        {
            // Word space (7 dot durations, but we subtract the symbol space already added)
            if (!firstChar) {
                m_morseQueue.enqueue(WORD_SPACE);
            }
            firstChar = true;
            continue;
        }

        // Inter-character space (3 dot durations)
        if (!firstChar) {
            m_morseQueue.enqueue(CHAR_SPACE);
        }
        firstChar = false;

        appendCharacter(ch);
    }
}

void CWModSource::appendCharacter(QChar c)
{
    for (int i = 0; s_morseTable[i].ch != 0; i++)
    {
        if (s_morseTable[i].ch == c.toLatin1())
        {
            const char* code = s_morseTable[i].code;
            bool firstSymbol = true;

            for (int j = 0; code[j] != '\0'; j++)
            {
                if (!firstSymbol) {
                    m_morseQueue.enqueue(SYMBOL_SPACE);
                }
                firstSymbol = false;

                if (code[j] == '.') {
                    m_morseQueue.enqueue(DOT);
                } else if (code[j] == '-') {
                    m_morseQueue.enqueue(DASH);
                }
            }
            return;
        }
    }
    // Unknown character — send a word space as separator
    m_morseQueue.enqueue(WORD_SPACE);
}

void CWModSource::applySettings(const QStringList& settingsKeys, const CWModSettings& settings, bool force)
{
    if (force || settingsKeys.contains("gain")) {
        m_linearGain = std::pow(10.0f, settings.m_gain / 20.0f);
    }
    if (force || settingsKeys.contains("wpm") || settingsKeys.contains("rfBandwidth")) {
        m_settings = settings;
        updateTiming();
    }
    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

void CWModSource::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    if (force || (channelSampleRate != m_channelSampleRate))
    {
        m_carrierNco.setFreq((double)channelFrequencyOffset, (double)channelSampleRate);
        m_channelSampleRate = channelSampleRate;
        updateTiming();
    }

    if (force || (channelFrequencyOffset != m_channelFrequencyOffset))
    {
        m_carrierNco.setFreq((double)channelFrequencyOffset, (double)channelSampleRate);
        m_channelFrequencyOffset = channelFrequencyOffset;
    }
}

void CWModSource::calculateLevel(Real& sample)
{
    if (m_levelCalcCount < (quint32)m_levelNbSamples)
    {
        m_peakLevel = std::max(std::fabs(sample), m_peakLevel);
        m_levelSum += sample * sample;
        m_levelCalcCount++;
    }
    else
    {
        qreal rmsLevel = (m_levelSum > 0.0f) ? std::sqrt(m_levelSum / (Real)m_levelNbSamples) : 0.0f;
        m_rmsLevel = rmsLevel;
        m_peakLevelOut = m_peakLevel;
        m_levelSum = 0.0f;
        m_peakLevel = 0.0f;
        m_levelCalcCount = 0;
    }
}

void CWModSource::sampleToSpectrum(Complex sample)
{
    if (m_spectrumSink)
    {
        m_specSampleBuffer[m_specSampleBufferIndex++] = Sample(
            (FixReal)(SDR_TX_SCALEF * sample.real()),
            (FixReal)(SDR_TX_SCALEF * sample.imag())
        );

        if (m_specSampleBufferIndex == m_specSampleBufferSize)
        {
            m_spectrumSink->feed(m_specSampleBuffer.begin(), m_specSampleBuffer.end(), false);
            m_specSampleBufferIndex = 0;
        }
    }
}
