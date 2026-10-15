///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#ifndef INCLUDE_DRONEIDDECODER_H
#define INCLUDE_DRONEIDDECODER_H

#include <algorithm>
#include <complex>
#include <cstdint>
#include <vector>

#include "dsp/kissfft.h"
#include "dsp/lteturbo.h"
#include "util/crc.h"

class FFTEngine;
class FFTFactory;

// Physical layer decoder for DJI DroneID bursts.
//
// A burst is 9 OFDM symbols at 15.36 MS/s (1024 point FFT, 15 kHz subcarrier spacing,
// 600 used subcarriers plus DC). Symbols 4 and 6 are Zadoff-Chu sequences used for
// synchronisation and channel estimation. Symbol 1 is a known (scrambled zero) symbol and
// is not sent by older drones. The remaining 6 symbols carry 7200 QPSK bits, which are
// a Gold sequence scrambled, rate matched, LTE turbo coded 1408 bit block with CRC24A.
class DroneIDDecoder
{
public:
    typedef std::complex<float> cf;

    static constexpr int m_sampleRate = 15360000;
    static constexpr int m_fftSize = 1024;
    static constexpr int m_nbCarriers = 600;
    static constexpr int m_nbSymbols = 9;
    static constexpr int m_frameLength = 2 * (80 + 1024) + 7 * (72 + 1024);    // 9880 samples
    static const int m_cpLengths[m_nbSymbols];
    static const int m_zcSymbols[2];            // Symbol indices (0-based) holding ZC sequences
    static const int m_zcRoots[2];
    static const int m_dataSymbols[6];          // Symbol indices holding data
    static constexpr int m_codedBits = 6 * m_nbCarriers * 2;  // 7200
    static constexpr int m_blockBits = 1408;        // Turbo code block size K
    static constexpr int m_blockBytes = m_blockBits / 8;
    static constexpr int m_payloadBytes = 91;       // Length of flight information message
    static constexpr int m_dataBytes = m_blockBytes - 3;   // Bytes available for messages (excluding CRC24)
    static constexpr uint32_t m_scramblerInit = 0x12345678;

    struct Result {
        int m_frameStart;       // Index of start of frame (first CP) in buffer
        bool m_legacy;          // True if first symbol wasn't transmitted
        float m_cpMetric;       // Normalised cyclic prefix correlation 0..1
        float m_zcMetric;       // Normalised Zadoff-Chu channel estimate consistency 0..1
        float m_cfo;            // Carrier frequency offset in Hz
        float m_snr;            // SNR in dB, estimated from ZC symbols
        float m_evm;            // RMS error vector magnitude of data symbols
        int m_iterations;       // Number of turbo iterations used
        bool m_crcValid;        // Whether CRC24A of turbo decoded block was valid
        std::vector<uint8_t> m_payload; // m_dataBytes, if CRC valid. Message is at start, followed by zero padding.

        Result() :
            m_frameStart(-1),
            m_legacy(false),
            m_cpMetric(0.0f),
            m_zcMetric(0.0f),
            m_cfo(0.0f),
            m_snr(0.0f),
            m_evm(0.0f),
            m_iterations(0),
            m_crcValid(false)
        {}
    };

    // If fftFactory is null, a built-in FFT is used (E.g. for tests that run without DSPEngine)
    DroneIDDecoder(FFTFactory *fftFactory = nullptr);
    ~DroneIDDecoder();

    void setMaxIterations(int iterations) { m_maxIterations = std::max(1, iterations); }
    void setMaxIntegerCFO(int bins) { m_maxIntegerCFO = bins; }

    // Search samples (at m_sampleRate) for a frame and decode it.
    // Returns true if a frame was detected (CP and ZC correlation), with result.m_crcValid
    // indicating whether it was successfully decoded.
    bool decode(const cf *samples, int length, Result& result);

    // Shared with modulator for tests
    static void carrierBins(std::vector<int>& bins);    // FFT bin for each of the 600 carriers, lowest frequency first
    static void zadoffChu(int root, std::vector<cf>& seq); // Frequency domain ZC for the 600 carriers
    static void symbolStarts(std::vector<int>& starts);  // Start of each symbol's CP relative to frame start

private:
    static constexpr int m_maxSymbolOffset = 2;  // Timing candidates checked either side of best CP correlation, in symbols
    static constexpr int m_fftBackoff = 4;       // Samples before the end of the CP that the FFT window starts, to allow for timing error
    static constexpr int m_channelSmoothing = 4; // Carriers either side used for smoothing channel estimate
    static constexpr int m_noiseSmoothing = 8;   // Carriers either side used for smoothing noise estimate
    static constexpr float m_minCPMetric = 0.1f; // Minimum metrics for a frame to be considered detected
    static constexpr float m_minZCMetric = 0.3f;

    int m_maxIterations;
    int m_maxIntegerCFO;
    std::vector<int> m_bins;
    std::vector<cf> m_zc[2];
    std::vector<cf> m_zcRef;                // conj(m_zc[0]) * m_zc[1]
    std::vector<int> m_symbolStarts;
    std::vector<uint8_t> m_scrambler;
    FFTFactory *m_fftFactory;
    FFTEngine *m_fftEngine;
    unsigned int m_fftSequence;
    kissfft<float, cf> m_kissFFT;
    LTETurboDecoder m_turbo;
    crc24a m_crc;

    // Working buffers
    std::vector<cf> m_derotated;
    std::vector<cf> m_fftIn;
    std::vector<std::vector<cf>> m_symbols; // FFT output per symbol, length m_fftSize
    std::vector<cf> m_zcDiff;               // Normalised y4*conj(y6) per FFT bin

    struct Candidate {
        int m_start;
        cf m_correlation;
        float m_metric;
    };

    void findTiming(const cf *samples, int length, std::vector<Candidate>& candidates);
    void fftSymbol(const cf *samples, int length, int frameStart, double cyclesPerSample, int symbol);
    void fftSymbols(const cf *samples, int length, int frameStart, double cyclesPerSample);
    void zcPhaseDifference();
    float zcConsistency(int shift);
    bool checkCRC(const std::vector<uint8_t>& bits);
};

#endif // INCLUDE_DRONEIDDECODER_H
