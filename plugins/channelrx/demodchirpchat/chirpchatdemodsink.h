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

#ifndef INCLUDE_CHIRPCHATDEMODSINK_H
#define INCLUDE_CHIRPCHATDEMODSINK_H

#include <vector>
#include <queue>

#include "dsp/channelsamplesink.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/fftwindow.h"
#include "util/message.h"
#include "util/movingaverage.h"

#include "chirpchatdemodsettings.h"

class BasebandSampleSink;
class FFTEngine;
namespace ChirpChatDemodMsg {
    class MsgDecodeSymbols;
}
class MessageQueue;

class ChirpChatDemodSink : public ChannelSampleSink {
public:
    ChirpChatDemodSink();
	~ChirpChatDemodSink();

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    bool getDemodActive() const { return m_demodActive; }
    void setDecoderMessageQueue(MessageQueue *messageQueue) { m_decoderMsgQueue = messageQueue; }
	void setSpectrumSink(BasebandSampleSink* spectrumSink) { m_spectrumSink = spectrumSink; }
    void applyChannelSettings(int channelSampleRate, int bandwidth, int channelFrequencyOffset, bool force = false);
    void applySettings(const ChirpChatDemodSettings& settings, bool force = false);
    double getCurrentNoiseLevel() const { return m_magsqOffAvg.instantAverage() / (1<<m_settings.m_spreadFactor); }
    double getTotalPower() const { return m_magsqTotalAvg.instantAverage() / (1<<m_settings.m_spreadFactor); }

private:
    enum ChirpChatState
    {
        ChirpChatStateReset,          //!< Reset everything to start all over
        ChirpChatStateDetectPreamble, //!< Look for preamble
        ChirpChatStatePreambleResyc,  //!< Synchronize with what is left of preamble chirp
        ChirpChatStatePreamble,       //!< Preamble is found and look for SFD start
        ChirpChatStateSkipSFD,        //!< Skip SFD
        ChirpChatStateReadPayload,
        ChirpChatStateTest
    };

    ChirpChatDemodSettings m_settings;
    ChirpChatState m_state;
    bool m_demodActive;
    ChirpChatDemodMsg::MsgDecodeSymbols *m_decodeMsg;
    MessageQueue *m_decoderMsgQueue;
	int m_bandwidth;
    int m_channelSampleRate;
    int m_channelFrequencyOffset;
	unsigned int m_chirp;
	unsigned int m_chirp0;

    static const unsigned int m_requiredPreambleChirps = 4; //!< Number of chirps required to estimate preamble
    static const unsigned int m_maxSFDSearchChirps = 8;     //!< Maximum number of chirps when looking for SFD after preamble detection
    static const unsigned int m_fftInterpolation = 2;       //!< FFT interpolation factor (usually a power of 2)

    FFTEngine *m_fft;
    FFTEngine *m_fftSFD;
    int m_fftSequence;
    int m_fftSFDSequence;
    FFTWindow m_fftWindow;
    Complex *m_downChirps;
    Complex *m_upChirps;
    Complex *m_spectrumLine;
    unsigned int m_fftCounter;
    int m_argMaxHistory[m_requiredPreambleChirps];
    unsigned int m_argMaxHistoryCounter;
    unsigned int m_preambleHistory[m_maxSFDSearchChirps];
    unsigned int m_syncWord;
    double m_magsqMax;
    MovingAverageUtil<double, double, 10> m_magsqOnAvg;
    MovingAverageUtil<double, double, 10> m_magsqOffAvg;
    MovingAverageUtil<double, double, 10> m_magsqTotalAvg;
    std::queue<double> m_magsqQueue;
    unsigned int m_chirpCount; //!< Generic chirp counter
    unsigned int m_sfdSkip;    //!< Number of samples in a SFD skip or slide (1/4) period
    unsigned int m_sfdSkipCounter; //!< Counter of skip or slide periods

	NCO m_nco;
	Interpolator m_interpolator;
	Real m_sampleDistanceRemain;
    Real m_interpolatorDistance;

	BasebandSampleSink* m_spectrumSink;
	Complex *m_spectrumBuffer;

    unsigned int m_nbSymbols;              //!< Number of symbols = length of base FFT
    unsigned int m_nbSymbolsEff;           //!< effective symbols considering DE bits
    unsigned int m_fftLength;              //!< Length of base FFT
    unsigned int m_interpolatedFFTLength;  //!< Length of interpolated FFT
    int m_deLength;                        //!< Number of FFT bins collated to represent one symbol
    int m_preambleTolerance;               //!< Number of FFT bins to collate when looking for preamble

    void processSample(const Complex& ci);
    void initSF(unsigned int sf, unsigned int deBits, FFTWindow::Function fftWindow); //!< Init tables, FFTs, depending on spread factor
    void reset();
    unsigned int argmax(
        const Complex *fftBins,
        unsigned int fftMult,
        unsigned int fftLength,
        double& magsqMax,
        double& magSqTotal,
        Complex *specBuffer,
        unsigned int specDecim
        );
    unsigned int argmaxSpreaded( //!< count energy in adjacent bins for same symbol (needs DE bits > 0)
        const Complex *fftBins,
        unsigned int fftMult,
        unsigned int fftLength,
        double& magsqMax,
        double& magsqNoise,
        double& magSqTotal,
        Complex *specBuffer,
        unsigned int specDecim
        );
    void decimateSpectrum(Complex *in, Complex *out, unsigned int size, unsigned int decimation);
    int toSigned(int u, int intSize);
    unsigned int evalSymbol(unsigned int rawSymbol);
};

#endif // INCLUDE_CHIRPCHATDEMODSINK_H
