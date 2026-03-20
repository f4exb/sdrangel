///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019-2020 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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

#ifndef INCLUDE_MESHTASTICDEMODSINK_H
#define INCLUDE_MESHTASTICDEMODSINK_H

#include <vector>
#include <queue>
#include <deque>
#include <cstdint>
#include <QtGlobal>

#include "dsp/channelsamplesink.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/fftwindow.h"
#include "util/movingaverage.h"

#include "meshtasticdemodsettings.h"

class BasebandSampleSink;
class FFTEngine;
namespace MeshtasticDemodMsg {
    class MsgDecodeSymbols;
}
class MessageQueue;

class MeshtasticDemodSink : public ChannelSampleSink {
public:
    MeshtasticDemodSink();
	~MeshtasticDemodSink();

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    bool getDemodActive() const { return m_demodActive; }
    void setDecoderMessageQueue(MessageQueue *messageQueue) { m_decoderMsgQueue = messageQueue; }
	void setSpectrumSink(BasebandSampleSink* spectrumSink) { m_spectrumSink = spectrumSink; }
    void setDeviceCenterFrequency(qint64 centerFrequency) { m_deviceCenterFrequency = centerFrequency; }
    void applyLoRaHeaderFeedback(
        uint32_t frameId,
        bool valid,
        bool hasCRC,
        unsigned int nbParityBits,
        unsigned int packetLength,
        bool ldro,
        unsigned int expectedSymbols,
        int headerParityStatus,
        bool headerCRCStatus
    );
    void applyChannelSettings(int channelSampleRate, int bandwidth, int channelFrequencyOffset, bool force = false);
    void applySettings(const MeshtasticDemodSettings& settings, bool force = false);
    double getCurrentNoiseLevel() const { return m_magsqOffAvg.instantAverage() / (1<<m_settings.m_spreadFactor); }
    double getTotalPower() const { return m_magsqTotalAvg.instantAverage() / (1<<m_settings.m_spreadFactor); }

private:
    enum LoRaFrameSyncState
    {
        LoRaStateDetect,
        LoRaStateSync,
        LoRaStateSFOCompensation
    };

    enum LoRaSyncState
    {
        LoRaSyncNetId1,
        LoRaSyncNetId2,
        LoRaSyncDownchirp1,
        LoRaSyncDownchirp2,
        LoRaSyncQuarterDown
    };

    MeshtasticDemodSettings m_settings;
    bool m_demodActive;
    MeshtasticDemodMsg::MsgDecodeSymbols *m_decodeMsg;
    MessageQueue *m_decoderMsgQueue;
	int m_bandwidth;
    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    qint64 m_deviceCenterFrequency;
	unsigned int m_chirp;
	unsigned int m_chirp0;

    static constexpr unsigned int m_minRequiredPreambleChirps = 4;  //!< Lower bound for preamble validation chirps
    static constexpr unsigned int m_maxRequiredPreambleChirps = 64; //!< Upper bound for preamble validation chirps
    static constexpr unsigned int m_maxSFDSearchChirps = 64;        //!< Maximum number of chirps when looking for SFD after preamble detection
    static constexpr unsigned int m_legacyFFTInterpolation = 4;     //!< Legacy interpolation for non-LoRa modes
    static constexpr unsigned int m_loRaFFTInterpolation = 1;       //!< Canonical gr-lora_sdr-like FFT binning for LoRa

    FFTEngine *m_fft;
    int m_fftSequence;
    Complex *m_downChirps;
    Complex *m_upChirps;
    Complex *m_spectrumLine;
    unsigned int m_fftCounter;
    std::deque<unsigned int> m_preambleBinHistory; //!< Rolling preamble bins for mode (k_hat) estimation
    unsigned int m_preambleConsecutive;            //!< Consecutive upchirp count in DETECT state
    bool m_havePrevPreambleBin;
    unsigned int m_prevPreambleBin;
    unsigned int m_requiredPreambleChirps;
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

    bool m_headerLocked;               //!< True when header decode succeeded and we have a deterministic symbol budget
    unsigned int m_expectedSymbols;    //!< Total expected symbols (header + payload) from header decode
    bool m_waitHeaderFeedback;
    unsigned int m_headerFeedbackWaitSteps;
    uint32_t m_loRaFrameId;
    static constexpr unsigned int m_headerFeedbackMaxWaitSteps = 128;

    unsigned int m_osFactor;       //!< Oversampling factor at frame-sync input (gr-lora_sdr os_factor)
    unsigned int m_osCenterPhase;  //!< Selected downsample phase inside oversampled symbol
    unsigned int m_osCounter;      //!< Oversampled sample counter
    LoRaFrameSyncState m_loRaState;
    LoRaSyncState m_loRaSyncState;
    std::deque<Complex> m_loRaSampleFifo;
    std::vector<Complex> m_loRaInDown;
    std::vector<Complex> m_loRaPreambleRaw;
    std::vector<Complex> m_loRaPreambleRawUp;
    std::vector<Complex> m_loRaPreambleUpchirps;
    std::vector<Complex> m_loRaRefUpchirp;
    std::vector<Complex> m_loRaRefDownchirp;
    std::vector<Complex> m_loRaCFOFracCorrec;
    std::vector<Complex> m_loRaPayloadDownchirp;
    std::vector<Complex> m_loRaSymbCorr;
    std::vector<Complex> m_loRaNetIdSamp;
    std::vector<Complex> m_loRaAdditionalSymbolSamp;
    std::vector<int> m_loRaPreambleVals;
    std::vector<int> m_loRaNetIds;
    int m_loRaSymbolCnt;
    int m_loRaBinIdx;
    int m_loRaKHat;
    int m_loRaDownVal;
    int m_loRaCFOInt;
    int m_loRaNetIdOff;
    int m_loRaAdditionalUpchirps;
    int m_loRaUpSymbToUse;
    unsigned int m_loRaRequiredUpchirps;
    unsigned int m_loRaSymbolSpan;
    unsigned int m_loRaFrameSymbolCount;
    float m_loRaCFOFrac;
    float m_loRaSTOFrac;
    float m_loRaSFOHat;
    float m_loRaSFOCum;
    bool m_loRaCFOSTOEstimated;
    bool m_loRaReceivedHeader;
    bool m_loRaOneSymbolOff;

	NCO m_nco;
	Interpolator m_interpolator;
	Real m_sampleDistanceRemain;
    Real m_interpolatorDistance;

	BasebandSampleSink* m_spectrumSink;
	Complex *m_spectrumBuffer;

    unsigned int m_nbSymbols;              //!< Number of symbols = length of base FFT
    unsigned int m_nbSymbolsEff;           //!< effective symbols considering DE bits
    unsigned int m_fftLength;              //!< Length of base FFT
    unsigned int m_fftInterpolation;       //!< FFT interpolation factor (LoRa=1, legacy modes=4)
    unsigned int m_interpolatedFFTLength;  //!< Length of interpolated FFT
    int m_deLength;                        //!< Number of FFT bins collated to represent one symbol
    int m_preambleTolerance;               //!< Number of FFT bins to collate when looking for preamble

    void initSF(unsigned int sf, unsigned int deBits); //!< Init tables, FFTs, depending on spread factor
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
    unsigned int evalSymbol(unsigned int rawSymbol, bool headerSymbol = false);
    void tryHeaderLock();  //!< Attempt inline header decode after 8 symbols to determine expected frame length
    bool sendLoRaHeaderProbe();
    void processSampleLoRa(const Complex& ci);
    int processLoRaFrameSyncStep();
    void resetLoRaFrameSync();
    void clearSpectrumHistoryForNewFrame();
    int loRaMod(int a, int b) const;
    int loRaRound(float number) const;
    unsigned int getLoRaSymbolVal(
        const Complex *samples,
        const Complex *refChirp,
        std::vector<float> *symbolMagnitudes = nullptr,
        bool publishSpectrum = false
    );
    float estimateLoRaCFOFracBernier(const Complex *samples);
    float estimateLoRaSTOFrac();
    void buildLoRaPayloadDownchirp();
    void finalizeLoRaFrame();
};

#endif // INCLUDE_MESHTASTICDEMODSINK_H
