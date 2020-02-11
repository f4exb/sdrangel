///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_LORAMODSOURCE_H
#define INCLUDE_LORAMODSOURCE_H

#include <QMutex>

#include "dsp/channelsamplesource.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/bandpass.h"
#include "util/movingaverage.h"

#include "loramodsettings.h"

class LoRaModSource : public ChannelSampleSource
{
public:
    LoRaModSource();
    virtual ~LoRaModSource();

    virtual void pull(SampleVector::iterator begin, unsigned int nbSamples);
    virtual void pullOne(Sample& sample);
    virtual void prefetch(unsigned int nbSamples) {}

    double getMagSq() const { return m_magsq; }
    void getLevels(qreal& rmsLevel, qreal& peakLevel, int& numSamples) const
    {
        rmsLevel = m_rmsLevel;
        peakLevel = m_peakLevelOut;
        numSamples = m_levelNbSamples;
    }
    void applySettings(const LoRaModSettings& settings, bool force = false);
    void applyChannelSettings(int channelSampleRate, int bandwidth, int channelFrequencyOffset, bool force = false);

private:
    enum LoRaMode
    {
        LoRaStandard, //!< Classic
        LoRaTTY       //!< Special TTY (SF=7, DE=2)
    };

    enum LoRaState
    {
        LoRaStateIdle,       //!< Quiet time
        LoRaStatePreamble,   //!< Transmit preamble
        LoRaStateSyncWord,   //!< Tramsmit sync word
        LoRaStateSFD,        //!< Transmit SFD
        LoRaStatePayload,    //!< Tramsmoit payload
        LoRaStateTest
    };

    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    int m_bandwidth;
    LoRaModSettings m_settings;

    LoRaState m_state;
    Complex *m_downChirps;
    Complex *m_upChirps;
    double *m_phaseIncrements;
    std::vector<unsigned int> m_symbols;
    unsigned int m_fftLength;      //!< chirp length in samples
    unsigned int m_chirp;          //!< actual chirp index in chirps table
    unsigned int m_chirp0;         //!< half index of chirp start in chirps table
    unsigned int m_sampleCounter;  //!< actual sample counter
    unsigned int m_fftCounter;     //!< chirp sample counter
    unsigned int m_chirpCount;     //!< chirp or quarter chirp counter
    unsigned int m_quietSamples;   //!< number of samples during quiet period
    unsigned int m_quarterSamples; //!< number of samples in a quarter chirp

    NCO m_carrierNco;
    double m_modPhasor; //!< baseband modulator phasor
    Complex m_modSample;

    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;
    bool m_interpolatorConsumed;

    Bandpass<Real> m_bandpass;

    double m_magsq;
    MovingAverageUtil<double, double, 16> m_movingAverage;

    quint32 m_levelCalcCount;
    qreal m_rmsLevel;
    qreal m_peakLevelOut;
    Real m_peakLevel;
    Real m_levelSum;

    static const int m_levelNbSamples;

    void initSF(unsigned int sf); //!< Init tables, FFTs, depending on spread factor
    void initTest(unsigned int sf, unsigned int deBits);
    void reset();
    void processOneSample(Complex& ci);
    void calculateLevel(Real& sample);
    void modulateSample();
    unsigned int encodeSymbol(unsigned int symbol); //!< Encodes symbol with possible DE bits spacing
};

#endif // INCLUDE_LORAMODSOURCE_H
