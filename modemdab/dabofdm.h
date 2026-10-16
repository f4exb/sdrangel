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

#ifndef MODEMDAB_DABOFDM_H_
#define MODEMDAB_DABOFDM_H_

#include <complex>
#include <cstdint>
#include <functional>
#include <vector>

#include "export.h"
#include "dabfft.h"
#include "dabtables.h"

namespace modemdab
{

// One demodulated transmission frame (96 ms): soft bits for the FIC and the four CIFs.
// Soft bits are positive for zero, scaled by the estimated reliability of their carrier.
struct DABFrame
{
    int64_t startSample = 0;            // Input sample index of the start of the null symbol
    std::vector<float> ficSoft;         // 3 * 3072: four FIC blocks of 2304 bits
    std::vector<float> mscSoft;         // 72 * 3072: four CIFs of 55296 bits
    std::vector<float> nullSpectrum;    // Power of each FFT bin in the null symbol (TII)
    float frequencyOffsetHz = 0.0f;     // Frequency correction applied
    float clockErrorPPM = 0.0f;         // Estimated sample clock error
    float merDB = 0.0f;                 // Of the differential symbols, before weighting
    float snrDB = 0.0f;                 // Estimated carrier to noise ratio
    float nullDepthDB = 0.0f;           // Null symbol power relative to the other symbols (without TII)
    // Carrier to noise ratio: the carriers' power against the noise in the null symbol,
    // which sees noise and interference but not impairments of the signal itself (overload
    // of the receiver by the ensemble, multipath beyond the guard interval, phase noise)
    float cnDB = 0.0f;                  // Of the median carrier
    std::vector<float> carrierCN;       // Of each carrier (dB), by carrier index, smoothed over frames
    float prsQualityDB = 0.0f;          // Impulse response peak to mean
    int timingAdjustment = 0;           // Samples the frame timing moved by
    // With setExportCarriers: the 76 symbols' carriers (PRS first), 1536 each by carrier
    // index (k = -768..-1, then 1..768), and each carrier's weight from its noise
    std::vector<std::complex<float>> carriers;
    std::vector<float> weights;
};

class MODEMDAB_API DABFrontEnd
{
public:
    enum class State
    {
        Searching,
        Locked
    };

    enum class Detection
    {
        Differential,   // Each symbol against the one before
        ThreeSymbol     // Each symbol over a window of three (multiple symbol differential detection)
    };

    using FrameCallback = std::function<void(const DABFrame&)>;

    DABFrontEnd();

    void setFrameCallback(FrameCallback callback) { m_frameCallback = callback; }
    // Integer carrier offsets are searched within +/- rangeHz (default 35 kHz).
    void setSearchRange(int rangeHz);
    // Filter the input to the 1.536 MHz ensemble, so that adjacent channels cannot disturb
    // synchronization. On by default; a host whose channel filter does this can disable it.
    void setBandFilter(bool enabled);
    // Remove a DC offset from the input, such as an RTL-SDR's LO leakage, which would
    // otherwise sit on the carriers next to the centre (an ensemble has no DC of its own).
    // On by default.
    void setDCRemoval(bool enabled) { m_dcRemoval = enabled; }
    std::complex<float> dcOffset() const { return std::complex<float>(m_dc); }
    void setDetection(Detection detection) { m_detection = detection; }
    // Also deliver each frame's carriers (by carrier index) and noise weights, for the
    // decoder aided (coherent) second pass
    void setExportCarriers(bool exportCarriers) { m_exportCarriers = exportCarriers; }
    // Drop all buffered input and synchronization.
    void reset();
    // Keep the input but search for the frame timing and frequency again.
    void reacquire();
    // Input at 2.048 MS/s. Frames are delivered through the callback.
    void process(const std::complex<float> *samples, std::size_t count);

    State state() const { return m_state; }
    float frequencyOffsetHz() const { return static_cast<float>(m_frequency); }
    float clockErrorPPM() const { return m_clockErrorPPM; }
    int64_t samplesProcessed() const { return m_bufferStart + static_cast<int64_t>(m_buffer.size()); }
    int acquisitionAttempts() const { return m_acquisitionAttempts; }
    int locks() const { return m_locks; }   // Successful acquisitions

private:
    typedef DABFFT FFT;

    FrameCallback m_frameCallback;
    State m_state;
    int m_searchRange;                      // Carriers
    std::vector<std::complex<float>> m_buffer;
    int64_t m_bufferStart;                  // Sample index of m_buffer[0]
    int64_t m_frameStart;                   // Sample index of the next frame's null symbol
    double m_frameStartFraction;            // Accumulated sub-sample timing drift
    double m_frequency;                     // Hz removed from the input
    float m_clockErrorPPM;
    float m_driftPerFrame;                  // Samples per frame
    bool m_haveTiming;
    double m_lastFineTiming;
    int m_integerOffsetVotes;
    int m_lastIntegerOffset;
    int m_badFrames;
    int m_pendingShift;
    int m_acquisitionAttempts;
    int m_locks;

    FFT m_fft;
    FFT m_ifft;
    bool m_bandFilter;
    bool m_dcRemoval;
    float m_impulseAverage;                 // Impulse response quality while locked (dB)
    bool m_haveImpulseAverage;
    float m_lastNullDepth;
    bool m_haveLastNullDepth;
    std::complex<double> m_dc;              // Running mean of the input
    std::vector<std::complex<float>> m_dcRemoved;
    Detection m_detection;
    bool m_exportCarriers;
    std::vector<std::complex<float>> m_threeSymbol;  // Equivalent differential products
    FFT m_filterFFT;
    FFT m_filterIFFT;
    std::vector<std::complex<float>> m_filterResponse;
    std::vector<std::complex<float>> m_filterInput;
    std::vector<std::complex<float>> m_filterWork;
    std::vector<std::complex<float>> m_mixed;
    std::vector<std::complex<float>> m_fftIn;
    std::vector<std::vector<std::complex<float>>> m_symbols;   // FFT of l = 1..76
    std::vector<std::complex<float>> m_differential;    // Differential products of a frame, by carrier
    std::vector<int> m_softCarrier;         // Carrier index of each QPSK symbol (frequency interleaving)
    std::vector<float> m_carrierNoise;      // Noise power of each carrier, from the null symbol
    std::vector<float> m_carrierPower;      // Mean power of each carrier over the frame's symbols
    bool m_haveCarrierPower;
    std::vector<float> m_lastNullPower;
    bool m_haveCarrierNoise;
    int m_noiseFrames;
    DABFrame m_frame;

    void filter(const std::complex<float> *samples, std::size_t count);
    bool acquire();
    bool processFrame();
    void discard(int64_t upTo, bool now = false);
    std::complex<float> cyclicPrefixCorrelation(const std::complex<float> *frame, int windowOffset) const;
    void mix(int64_t start, int length, double frequency, std::vector<std::complex<float>>& out) const;
    void symbolFFT(const std::complex<float> *window, std::vector<std::complex<float>>& out);
    int integerFrequencyOffset(const std::vector<std::complex<float>>& prs, int range, float& quality) const;
    int timingFromImpulseResponse(const std::vector<std::complex<float>>& prs, float& qualityDB, double& fineTiming);
    void estimateNoise(const std::vector<std::complex<float>>& nullSymbol);
    void carrierToNoise();
    void demodulate();
};

} // namespace modemdab

#endif // MODEMDAB_DABOFDM_H_
