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

#include <algorithm>
#include <cmath>

#include "dsp/dspengine.h"
#include "dsp/fftengine.h"
#include "dsp/fftfactory.h"

#include "atvcolourdecoder.h"

ATVVSBFrontEnd::ATVVSBFrontEnd() :
    m_upsampling(0),
    m_fft(nullptr),
    m_fftSequence(0),
    m_ifft(nullptr),
    m_ifftSequence(0),
    m_inCount(0),
    m_skipBlocks(0)
{
    FFTFactory *fftFactory = DSPEngine::instance()->getFFTFactory();
    m_fftSequence = fftFactory->getEngine(m_fftLen, false, &m_fft);
    m_fftIn = m_fft->in();
    m_response.resize(m_fftLen);
    configure(1000000, 0.0f, 3579545.0f, true, 1);
}

ATVVSBFrontEnd::~ATVVSBFrontEnd()
{
    FFTFactory *fftFactory = DSPEngine::instance()->getFFTFactory();
    fftFactory->releaseEngine(m_fftLen, false, m_fftSequence);

    if (m_ifft) {
        fftFactory->releaseEngine(m_fftLen * m_upsampling, true, m_ifftSequence);
    }
}

void ATVVSBFrontEnd::configure(int sampleRate, float carrierOffset, float videoBandwidth, bool usb, int upsampling)
{
    if (upsampling != m_upsampling)
    {
        FFTFactory *fftFactory = DSPEngine::instance()->getFFTFactory();

        if (m_ifft) {
            fftFactory->releaseEngine(m_fftLen * m_upsampling, true, m_ifftSequence);
        }

        m_upsampling = upsampling;
        m_ifftSequence = fftFactory->getEngine(m_fftLen * m_upsampling, true, &m_ifft);
        m_overlap.resize(m_blockLen * m_upsampling);
        m_out.resize(m_blockLen * m_upsampling);
    }

    const float slope = 0.3e6f;                          // Half width of the Nyquist slope
    const float videoTop = videoBandwidth;
    const float stopTop = videoBandwidth + 0.2e6f;
    const float guard = 0.02f * sampleRate;               // Taper to zero before the band edges, so upsampling has no discontinuity at Nyquist

    auto response = [&](float f) -> float
    {
        float fr = usb ? f - carrierOffset : carrierOffset - f; // Frequency relative to the vision carrier, video side positive
        float h;

        if (fr <= -slope) {
            return 0.0f;
        } else if (fr >= slope) {
            h = 1.0f;
        } else {
            h = 0.5f + fr / (2.0f * slope);
        }

        if (fr >= stopTop) {
            return 0.0f;
        } else if (fr > videoTop) {
            h *= 0.5f * (1.0f + std::cos((float) M_PI * (fr - videoTop) / (stopTop - videoTop)));
        }

        float edge = 0.5f * sampleRate - std::fabs(f);

        if (edge <= 0.0f) {
            return 0.0f;
        } else if (edge < guard) {
            h *= 0.5f * (1.0f - std::cos((float) M_PI * edge / guard));
        }

        return h;
    };

    // Frequency sampling design: impulse response of m_blockLen taps, centred and windowed,
    // so the linear convolution of a block still fits in the FFT
    const int len = m_blockLen;
    std::vector<float> desired(len);

    for (int k = 0; k < len; k++)
    {
        float f = (k < len / 2 ? k : k - len) * (float) sampleRate / (float) len;
        desired[k] = response(f);
    }

    std::fill(m_fftIn, m_fftIn + m_fftLen, Complex{0.0f, 0.0f});

    for (int n = 0; n < len; n++)
    {
        std::complex<double> acc = 0.0;

        for (int k = 0; k < len; k++) {
            acc += (double) desired[k] * std::polar(1.0, 2.0 * M_PI * k * (n - len / 2) / (double) len);
        }

        // Window, and scale by 1 / m_fftLen so the unnormalised FFT pair has unity gain
        double window = 0.42 - 0.5 * std::cos(2.0 * M_PI * n / len) + 0.08 * std::cos(4.0 * M_PI * n / len);
        m_fftIn[n] = Complex(acc * (window / ((double) len * m_fftLen)));
    }

    m_fft->transform();
    std::copy(m_fft->out(), m_fft->out() + m_fftLen, m_response.begin());

    std::fill(m_fftIn, m_fftIn + m_fftLen, Complex{0.0f, 0.0f});
    std::fill(m_overlap.begin(), m_overlap.end(), Complex{0.0f, 0.0f});
    m_inCount = 0;
    m_skipBlocks = 2;
}

int ATVVSBFrontEnd::processBlock(Complex **out)
{
    const int outFFTLen = m_fftLen * m_upsampling;
    const int outLen = m_blockLen * m_upsampling;

    m_fft->transform();
    const Complex *spectrum = m_fft->out();
    Complex *upsampled = m_ifft->in();

    // Filter, and upsample by zero padding the middle of the spectrum
    std::fill(upsampled, upsampled + outFFTLen, Complex{0.0f, 0.0f});

    for (int i = 0; i < m_fftLen / 2; i++) {
        upsampled[i] = spectrum[i] * m_response[i];
    }

    for (int i = 1; i < m_fftLen / 2; i++) {
        upsampled[outFFTLen - i] = spectrum[m_fftLen - i] * m_response[m_fftLen - i];
    }

    m_ifft->transform();
    const Complex *filtered = m_ifft->out();

    // Overlap and add
    for (int i = 0; i < outLen; i++)
    {
        m_out[i] = m_overlap[i] + filtered[i];
        m_overlap[i] = filtered[outLen + i];
    }

    // The second half of the input is zero padding, so it only needs clearing in case the FFT overwrote it
    std::fill(m_fftIn + m_blockLen, m_fftIn + m_fftLen, Complex{0.0f, 0.0f});

    *out = m_out.data();
    return outLen;
}

ATVSynchronousDetector::ATVSynchronousDetector() :
    m_ghostOn(false),
    m_ghostBuffer(m_ghostBufferSize),
    m_ghostIndex(0)
{
    configure(10000000.0f, 0.0f, 15625.0f);
}

void ATVSynchronousDetector::configure(float sampleRate, float carrierFrequency, float lineFrequency)
{
    m_sampleRate = sampleRate;
    m_blockLength = std::max(2, (int) std::round(sampleRate / 1.0e6f));
    m_linePeriod = sampleRate / lineFrequency;
    m_nominalStep = -2.0 * M_PI * carrierFrequency / sampleRate;
    m_frequency = 0.0;
    m_rotation = 1.0f;
    m_step = std::polar(1.0f, (float) m_nominalStep);
    m_phaseCarrier = 0.0f;
    m_carrierUnit = 1.0f;
    m_phaseLowPassAlpha = 1.0f - std::exp(-2.0f * (float) M_PI * 5.0e3f / sampleRate); // 5 kHz
    m_count = 0;
    m_block = 0.0f;
    m_blockIndex = 0;
    m_tipLevel = 0.0f;
    m_keyLevel = 0.0f;
    m_run.clear();
    m_run.reserve(m_maxRunBlocks);
    m_runStart = 0;
    m_runOverflow = false;
    m_prevRunSum = 0.0f;
    m_prevRunLength = 0;
    m_prevRunStart = 0;
    m_coarseSum = 0.0f;
    m_coarseCount = 0;
    setGhost(1.0f, 0.0f);
    m_ghostInput = 0.0f;
}

void ATVSynchronousDetector::setGhost(float delay, std::complex<float> gain)
{
    delay = std::max(1.0f, std::min(delay, (float) (m_ghostBufferSize - 2)));
    m_ghostDelay = (int) delay;
    m_ghostFrac = delay - m_ghostDelay;
    m_ghostGain = gain;

    if ((gain != 0.0f) != m_ghostOn)
    {
        m_ghostOn = gain != 0.0f;
        std::fill(m_ghostBuffer.begin(), m_ghostBuffer.end(), 0.0f);
        m_ghostIndex = 0;
    }
}

void ATVSynchronousDetector::update()
{
    m_count = 0;
    float magnitude = std::sqrt(std::norm(m_phaseCarrier));

    if (magnitude > 0.0f) {
        m_carrierUnit = std::conj(m_phaseCarrier) / magnitude;
    }

    // Sync tip level: follows the strongest blocks, and decays over ~100 ms so keying
    // recovers if the signal level drops
    float blockAmplitude = std::sqrt(std::norm(m_block)) / m_blockLength;

    if (blockAmplitude > m_tipLevel) {
        m_tipLevel += 0.02f * (blockAmplitude - m_tipLevel);
    } else {
        m_tipLevel *= 0.99999f;
    }

    m_keyLevel = 0.85f * 0.85f * m_tipLevel * m_tipLevel;

    // Collect runs of blocks within 90% of the sync tip carrier
    if ((blockAmplitude > 0.9f * m_tipLevel) && (m_blockIndex > m_settleBlocks))
    {
        if (m_run.empty()) {
            m_runStart = m_blockIndex;
        }

        if ((int) m_run.size() < m_maxRunBlocks) {
            m_run.push_back(m_block);
        } else {
            m_runOverflow = true;
        }
    }
    else if (!m_run.empty())
    {
        endRun();
    }

    m_block = 0.0f;
    m_blockIndex++;
    m_rotation *= 1.0f / std::sqrt(std::norm(m_rotation)); // Keep the recursive rotation on the unit circle
}

void ATVSynchronousDetector::endRun()
{
    int length = (int) m_run.size();

    if (!m_runOverflow)
    {
        std::complex<float> sum = 0.0f;

        for (const auto& block : m_run) {
            sum += block;
        }

        // Coarse: phase change between blocks along the middle of a broad pulse
        for (int i = m_runEdgeBlocks + 1; i < length - m_runEdgeBlocks; i++)
        {
            m_coarseSum += m_run[i] * std::conj(m_run[i - 1]);
            m_coarseCount++;
        }

        // Fine: phase change since the same pulse a line earlier
        float spacing = (float) ((m_runStart - m_prevRunStart) * m_blockLength);

        if ((length >= 2) && (std::abs(length - m_prevRunLength) <= 1) && (std::fabs(spacing - m_linePeriod) < 2.0f * m_blockLength))
        {
            float dphi = std::arg(sum * std::conj(m_prevRunSum));
            m_frequency -= 0.1 * dphi / m_linePeriod;
        }

        m_prevRunSum = sum;
        m_prevRunLength = length;
        m_prevRunStart = m_runStart;
    }

    m_run.clear();
    m_runOverflow = false;

    // Once a vertical sync's worth of broad pulses is measured, correct the frequency if it is
    // well out, which is when the fine loop could be on the wrong multiple of the line rate.
    // The pulses' edges still bias it by ~2 kHz, so small errors are left to the fine loop
    if (m_coarseCount >= 40)
    {
        double error = std::arg(m_coarseSum) / m_blockLength; // radians / sample

        if (std::fabs(error) > 2.0 * M_PI * 4000.0 / m_sampleRate) {
            m_frequency -= error;
        }

        m_coarseSum = 0.0f;
        m_coarseCount = 0;
    }

    m_step = std::polar(1.0f, (float) (m_nominalStep + m_frequency));
}

float ATVSynchronousDetector::getFrequencyError() const
{
    return (float) (-m_frequency * m_sampleRate / (2.0 * M_PI));
}

ATVColourDecoder::ATVColourDecoder() :
    m_carrierReferenced(false),
    m_saturation(1.0f),
    m_hue(1.0f, 0.0f),
    m_delayLine(true)
{
    configure(10000000.0f, ATVDemodSettings::ColourNTSC);
}

void ATVColourDecoder::configure(float sampleRate, ATVDemodSettings::ColourSystem colourSystem)
{
    m_sampleRate = sampleRate;
    m_secam = colourSystem == ATVDemodSettings::ColourSECAM;
    m_subcarrierFrequency = m_secam ? m_secamBandCentre : ATVDemodSettings::getSubcarrierFrequency(colourSystem);
    m_pal = (colourSystem == ATVDemodSettings::ColourPAL)
        || (colourSystem == ATVDemodSettings::ColourPALM)
        || (colourSystem == ATVDemodSettings::ColourPALN);

    // Levels in IRE (100 IRE = 700 mV for 625 line systems)
    bool system525 = (colourSystem == ATVDemodSettings::ColourNTSC) || (colourSystem == ATVDemodSettings::ColourPALM);
    m_syncDepth = system525 ? 40.0f : 300.0f / 7.0f;
    m_nominalBurst = system525 ? 20.0f : 150.0f / 7.0f;
    m_setup = system525 ? 7.5f : 0.0f;
    m_blankCarrier = 0.75f; // Negative modulation, as a fraction of the sync tip carrier
    m_whiteCarrier = ATVDemodSettings::getWhiteCarrier(colourSystem);

    // Measurement windows from the leading edge of sync: sync tip, and the burst
    // (NTSC burst is 5.3 to 7.8 us, PAL 5.6 to 7.85 us)
    m_tipStart = (int) (1.0e-6f * sampleRate);
    m_tipEnd = (int) (3.5e-6f * sampleRate);
    m_burstStart = (int) (5.8e-6f * sampleRate);
    m_burstEnd = (int) (7.4e-6f * sampleRate);

    m_chromaDecimation = std::max(1, (int) (sampleRate / 4.5e6f));

    // At the highest rates the chroma filter is the main cost, and most of the band it rejects
    // can go first: a 1 2 1 filter and half the rate, which needs half the taps at half the
    // rate. Below 18 MS/s the luma would alias near the chroma, so it is not done. SECAM's
    // chroma is narrow enough to do it twice
    bool prefilter = m_chromaDecimation >= 4;
    m_chromaPrefilter = !prefilter ? 1 : m_secam ? 4 : 2;

    if (prefilter) {
        m_chromaDecimation = (m_chromaDecimation / m_chromaPrefilter) * m_chromaPrefilter;
    }

    // The picture has no use for more than half of that rate
    m_pixelStep = prefilter ? 2 : 1;

    // Chroma low pass: 6 dB at 1.5 MHz, stop band from about 2.1 MHz, which rejects
    // both the luma (shifted down to -fsc) and the image at -2 fsc.
    // SECAM's chroma is confined to 3.9 to 4.75 MHz, so needs only that
    float filterRate = sampleRate / m_chromaPrefilter;
    float cutoff = (m_secam ? 0.65e6f : 1.5e6f) / filterRate;
    int taps = ((int) std::ceil(5.5f * filterRate / (m_secam ? 0.8e6f : 1.2e6f))) | 1;
    int half = taps / 2;
    m_lpfTaps.resize(taps);
    float sum = 0.0f;

    for (int i = 0; i < taps; i++)
    {
        int k = i - half;
        float sinc = (k == 0) ? 2.0f * cutoff : std::sin(2.0f * (float) M_PI * cutoff * k) / ((float) M_PI * k);
        float window = 0.42f + 0.5f * std::cos((float) M_PI * k / (half + 1)) + 0.08f * std::cos(2.0f * (float) M_PI * k / (half + 1));
        m_lpfTaps[i] = sinc * window;
        sum += m_lpfTaps[i];
    }

    for (auto& tap : m_lpfTaps) {
        tap /= sum;
    }

    // The displayed chroma is narrower again, as a TV's was, which halves its noise:
    // two 0.6 us boxcars make a triangle about 0.75 MHz wide at 6 dB. The wider chroma is
    // still used to separate it from the luma, so the luma keeps no subcarrier
    m_chromaSmoothing = std::max(1, (int) std::round(0.6e-6f * sampleRate / m_chromaDecimation));

    m_phasor.clear(); // Recalculated for the new rate by resize()
    m_prevUV.clear();
    m_tipLevel = 0.0f;
    m_blankLevel = 0.0f;
    m_levelsValid = false;
    m_burstRef = 0.0f;
    m_prevBurst = 0.0f;
    m_burstAmplitude = 0.0f;
    m_colourOn = false;
    m_missingBursts = 1000;
    m_vSign = 1.0f;

    // SECAM's subcarrier is on from 5.6 us after the sync's leading edge
    m_secamPorchStart = ((int) (6.0e-6f * sampleRate) / m_chromaDecimation) * m_chromaDecimation;
    m_secamPorchEnd = (int) (9.8e-6f * sampleRate);
    m_secamLag = std::max(1, (int) std::round(1.0e-6f * sampleRate / m_chromaDecimation)) * m_chromaDecimation;
    m_secamDb = false;
    m_secamLineType = 0.0f;
    m_secamAmplitude = 0.0f;
    m_secamQuality = 0.0f;
}

void ATVColourDecoder::resize(int length)
{
    if ((int) m_phasor.size() < length)
    {
        int oldSize = (int) m_phasor.size();
        m_phasor.resize(length);
        float w = 2.0f * (float) M_PI * m_subcarrierFrequency / m_sampleRate;

        for (int n = oldSize; n < length; n++) {
            m_phasor[n] = std::polar(2.0f, -w * n);
        }

        m_ire.resize(length);
        m_mixedRe.resize(length);
        m_mixedIm.resize(length);
        m_filterRe.resize(length / 2 + 1);
        m_filterIm.resize(length / 2 + 1);
        m_chroma.resize(length);
        m_chromaPoints.resize(length / m_chromaDecimation + 2);
        m_chromaSum.resize(length / m_chromaDecimation + 3);
        m_secamSum.resize(length / m_chromaDecimation + 3);
        m_secamLine.resize(length);
        m_secamPrevious.resize(length);
        m_prevUV.resize(length);
        m_secamFrequency.resize(length / m_chromaDecimation + 2);

        for (auto& lines : m_secamHistory)
        {
            for (auto& line : lines) {
                line.resize(length);
            }
        }
    }
}

// Centred moving average of w samples
template <typename T>
static void movingAverage(std::vector<T>& values, std::vector<T>& sum, int length, int w)
{
    if (w <= 1) {
        return;
    }

    const int before = w / 2;
    const float scale = 1.0f / w;
    sum[0] = T(0.0f);

    for (int n = 0; n < length; n++) {
        sum[n + 1] = sum[n] + values[n];
    }

    for (int n = 0; n < length; n++)
    {
        int a = std::max(n - before, 0);
        int b = std::min(n - before + w, length);
        values[n] = (sum[b] - sum[a]) * (b - a == w ? scale : 1.0f / (b - a));
    }
}

// 1 2 1, keeping every second sample. out can be in
static void halveRate(const float *in, float *out, int length)
{
    float first = in[0];
    float end = in[2 * (length - 1)];

    for (int n = 1; n < length - 1; n++) {
        out[n] = 0.25f * (in[2 * n - 1] + in[2 * n + 1]) + 0.5f * in[2 * n];
    }

    out[0] = first;
    out[length - 1] = end;
}

// n and length are in samples of m_filterRe and m_filterIm
std::complex<float> ATVColourDecoder::filterChroma(int n, int length) const
{
    const std::vector<float>& mixedRe = m_chromaPrefilter > 1 ? m_filterRe : m_mixedRe;
    const std::vector<float>& mixedIm = m_chromaPrefilter > 1 ? m_filterIm : m_mixedIm;
    const int taps = (int) m_lpfTaps.size();
    const int half = taps / 2;
    const float *h = m_lpfTaps.data();
    float re = 0.0f;
    float im = 0.0f;

    if ((n - half >= 0) && (n + half < length))
    {
        // This is the decoder's main cost. The taps are symmetric, so pairs of samples share a
        // multiply, and compilers will not vectorise a floating point sum without being allowed
        // to reorder it, so four sums are kept by hand, which can then run in parallel
        const float *xr = &mixedRe[n - half];
        const float *xi = &mixedIm[n - half];
        const int last = taps - 1;
        float re0 = 0.0f, re1 = 0.0f, re2 = 0.0f, re3 = 0.0f;
        float im0 = 0.0f, im1 = 0.0f, im2 = 0.0f, im3 = 0.0f;
        int i = 0;

        for (; i + 4 <= half; i += 4)
        {
            re0 += h[i] * (xr[i] + xr[last - i]);
            im0 += h[i] * (xi[i] + xi[last - i]);
            re1 += h[i + 1] * (xr[i + 1] + xr[last - i - 1]);
            im1 += h[i + 1] * (xi[i + 1] + xi[last - i - 1]);
            re2 += h[i + 2] * (xr[i + 2] + xr[last - i - 2]);
            im2 += h[i + 2] * (xi[i + 2] + xi[last - i - 2]);
            re3 += h[i + 3] * (xr[i + 3] + xr[last - i - 3]);
            im3 += h[i + 3] * (xi[i + 3] + xi[last - i - 3]);
        }

        for (; i < half; i++)
        {
            re0 += h[i] * (xr[i] + xr[last - i]);
            im0 += h[i] * (xi[i] + xi[last - i]);
        }

        re = re0 + re1 + re2 + re3 + h[half] * xr[half];
        im = im0 + im1 + im2 + im3 + h[half] * xi[half];
    }
    else
    {
        for (int i = 0; i < taps; i++)
        {
            int j = std::min(std::max(n + i - half, 0), length - 1);
            re += h[i] * mixedRe[j];
            im += h[i] * mixedIm[j];
        }
    }

    return {re, im};
}

static inline unsigned int packRGB(float r, float g, float b)
{
    int ri = std::min(std::max((int) (r * 255.0f), 0), 255);
    int gi = std::min(std::max((int) (g * 255.0f), 0), 255);
    int bi = std::min(std::max((int) (b * 255.0f), 0), 255);
    return ri | (gi << 8) | (bi << 16);
}

void ATVColourDecoder::decodeLine(const float *line, int length, int firstValid, int64_t lineStart, unsigned int *rgb)
{
    resize(length);

    // DC restore at the blanking level. The burst averages to zero, so the burst window
    // gives the blanking level
    if ((firstValid <= m_tipStart) && (length > m_burstEnd))
    {
        float tip = 0.0f;
        float blank = 0.0f;

        for (int n = m_tipStart; n < m_tipEnd; n++) {
            tip += line[n];
        }

        for (int n = m_burstStart; n < m_burstEnd; n++) {
            blank += line[n];
        }

        tip /= m_tipEnd - m_tipStart;
        blank /= m_burstEnd - m_burstStart;

        bool valid = m_carrierReferenced ? (tip - blank > 0.02f * tip) : (blank - tip > 0.02f);

        if (valid)
        {
            if (m_levelsValid)
            {
                m_tipLevel += 0.2f * (tip - m_tipLevel);
                m_blankLevel += 0.2f * (blank - m_blankLevel);
            }
            else
            {
                m_tipLevel = tip;
                m_blankLevel = blank;
                m_levelsValid = true;
            }
        }
    }

    if (!m_levelsValid)
    {
        for (int n = 0; n < length; n += m_pixelStep) {
            *rgb++ = packRGB(line[n], line[n], line[n]);
        }

        return;
    }

    // Scale to IRE. From the depth of sync, unless the carrier level is known, when the sync
    // tip carrier is used as a TV's keyed AGC does: sync is often compressed in transmission
    // or by fading, which would exaggerate the gain
    float scale = m_carrierReferenced
        ? -100.0f / ((m_blankCarrier - m_whiteCarrier) * m_tipLevel)
        : m_syncDepth / (m_blankLevel - m_tipLevel);

    for (int n = 0; n < length; n++)
    {
        m_ire[n] = (line[n] - m_blankLevel) * scale;
        m_mixedRe[n] = m_ire[n] * m_phasor[n].real();
        m_mixedIm[n] = m_ire[n] * m_phasor[n].imag();
    }

    // Baseband chroma, filtered every m_chromaDecimation samples and linearly interpolated between
    int d = m_chromaDecimation;
    int last = 0;
    int filterLength = length / m_chromaPrefilter;

    if (m_chromaPrefilter > 1)
    {
        // Each pass is 1 2 1 and half the rate: the second works on the first's output, in place
        halveRate(m_mixedRe.data(), m_filterRe.data(), length / 2);
        halveRate(m_mixedIm.data(), m_filterIm.data(), length / 2);

        if (m_chromaPrefilter > 2)
        {
            halveRate(m_filterRe.data(), m_filterRe.data(), length / 4);
            halveRate(m_filterIm.data(), m_filterIm.data(), length / 4);
        }
    }

    m_chroma[0] = filterChroma(0, filterLength);
    m_chromaPoints[0] = m_chroma[0];
    const int filterStep = d / m_chromaPrefilter;
    int filterIndex = filterStep;
    int point = 1;
    const float perSample = 1.0f / d;

    for (int n = d; n < length; n += d)
    {
        m_chroma[n] = filterChroma(filterIndex, filterLength);
        m_chromaPoints[point] = m_chroma[n];
        filterIndex += filterStep;
        point++;

        for (int i = m_pixelStep; i < d; i += m_pixelStep) {
            m_chroma[last + i] = m_chroma[last] + (m_chroma[n] - m_chroma[last]) * (i * perSample);
        }

        last = n;
    }

    for (int n = last + 1; n < length; n++) {
        m_chroma[n] = m_chroma[last];
    }

    if (m_secam)
    {
        decodeLineSECAM(firstValid, length, rgb);
        return;
    }

    // m_phasor restarts at each line, but the subcarrier does not (in NTSC its phase is
    // inverted on successive lines), so bursts are compared at a phase continuous across lines
    double w = 2.0 * M_PI * m_subcarrierFrequency / m_sampleRate;
    std::complex<float> lineRotation = std::polar(1.0f, (float) -std::fmod(w * (double) lineStart, 2.0 * M_PI));

    // Burst
    bool burstValid = (firstValid <= m_burstStart) && (length > m_burstEnd);
    bool burstPresent = false;
    std::complex<float> burst = 0.0f;

    if (burstValid)
    {
        int count = 0;

        for (int n = (m_burstStart / m_pixelStep) * m_pixelStep; n < m_burstEnd; n += m_pixelStep) // the samples of m_chroma that are set
        {
            burst += m_chroma[n];
            count++;
        }

        burst *= lineRotation / (float) count;
        float amplitude = std::abs(burst);
        burstPresent = amplitude > 0.25f * m_nominalBurst;

        if (burstPresent) {
            m_burstAmplitude += 0.1f * (amplitude - m_burstAmplitude);
        }

        if (m_pal)
        {
            // Successive bursts swing +/-45 degrees either side of -U, so their sum points along -U
            if (burstPresent && (std::abs(m_prevBurst) > 0.0f)) {
                m_burstRef += 0.3f * (burst + m_prevBurst - m_burstRef);
            }

            m_prevBurst = burstPresent ? burst : 0.0f;
        }
        else if (burstPresent)
        {
            m_burstRef += 0.3f * (burst - m_burstRef);
        }
    }

    // Colour killer: off when no burst for longer than the vertical blanking interval
    m_missingBursts = burstPresent ? 0 : m_missingBursts + 1;
    m_colourOn = (m_missingBursts < 50) && (std::abs(m_burstRef) > 0.25f * m_nominalBurst);

    float range = 100.0f - m_setup;

    if (!m_colourOn)
    {
        for (int n = 0; n < length; n += m_pixelStep)
        {
            float y = (m_ire[n] - m_setup) / range;
            *rgb++ = packRGB(y, y, y);
        }

        return;
    }

    // Rotate so the burst reference is at 180 degrees (-U); U is then real and V imaginary
    std::complex<float> rot = -std::conj(m_burstRef) / std::abs(m_burstRef);

    if (m_pal)
    {
        // V switch: the burst is at 135 degrees on lines where V is not inverted
        if (burstPresent) {
            m_vSign = (burst * rot).imag() > 0.0f ? 1.0f : -1.0f;
        } else {
            m_vSign = -m_vSign;
        }
    }
    else
    {
        rot *= m_hue;
    }

    rot *= lineRotation * (m_saturation * m_nominalBurst / std::max(m_burstAmplitude, 1.0f) / range);

    int points = last / d + 1;
    movingAverage(m_chromaPoints, m_chromaSum, points, m_chromaSmoothing);
    movingAverage(m_chromaPoints, m_chromaSum, points, m_chromaSmoothing);

    point = 0;
    int within = 0;

    for (int n = 0; n < length; n += m_pixelStep)
    {
        const std::complex<float>& c = m_chroma[n];
        float chroma = 0.5f * (c.real() * m_phasor[n].real() + c.imag() * m_phasor[n].imag()); // Re(c * conj(phasor)) / 2
        float y = (m_ire[n] - chroma - m_setup) / range;
        std::complex<float> display = m_chromaPoints[point];

        if (point + 1 < points) {
            display += (m_chromaPoints[point + 1] - display) * (within * perSample);
        }

        within += m_pixelStep;

        if (within >= d)
        {
            within -= d;
            point++;
        }

        std::complex<float> uv = display * rot;

        if (m_pal) {
            uv.imag(uv.imag() * m_vSign);
        }

        // PAL's delay line cancels phase errors. For NTSC too, averaging with the previous line
        // of the field halves the chroma noise, for half the vertical chroma resolution
        if (m_delayLine)
        {
            std::complex<float> current = uv;
            uv = 0.5f * (uv + m_prevUV[n]);
            m_prevUV[n] = current;
        }

        float ry = uv.imag() / 0.877f;
        float by = uv.real() / 0.492f;
        *rgb++ = packRGB(y + ry, y - 0.509f * ry - 0.194f * by, y + by);
    }
}

// The lines alternate strictly, so only their order has to be found. Each back porch is above
// or below the middle of the chroma band, which is unbiased however weak the subcarrier is, as
// the noise is equally on both sides, and enough lines are averaged to ride through a poor one
void ATVColourDecoder::updateSECAMLineType(int firstValid, int length)
{
    m_secamDb = !m_secamDb;

    if ((firstValid > m_secamPorchStart) || (length <= m_secamPorchEnd)) {
        return;
    }

    std::complex<float> sum = 0.0f;
    float magnitude = 0.0f;

    for (int n = m_secamPorchStart; n + m_secamLag < m_secamPorchEnd; n += m_chromaDecimation)
    {
        std::complex<float> d = m_chroma[n + m_secamLag] * std::conj(m_chroma[n]);
        sum += d;
        magnitude += std::sqrt(std::norm(d));
    }

    if (magnitude > 0.0f)
    {
        float above = sum.imag() / magnitude;
        m_secamLineType += 0.02f * ((m_secamDb ? -above : above) - m_secamLineType);
        m_secamQuality += 0.02f * (std::abs(sum) / magnitude - m_secamQuality);

        if (m_secamLineType < -0.01f)
        {
            m_secamDb = !m_secamDb;
            m_secamLineType = -m_secamLineType;
        }
    }
}

// Writes the colour difference this line carries to m_secamLine
void ATVColourDecoder::demodulateSECAM(int firstValid, int length)
{
    const int d = m_chromaDecimation;
    const float rate = m_sampleRate / d;
    const float rest = (m_secamDb ? m_secamDbRest : m_secamDrRest) - m_secamBandCentre;
    const float radiansPerHz = 2.0f * (float) M_PI / rate;

    // Second order loop. Gain is normalised by the magnitude, so it is the same for any signal
    // level, and samples where the signal and noise cancel count for less. A wide loop follows
    // the steep edges the pre-emphasis gives, but with a weak signal lets noise through
    float strength = (m_secamQuality - m_secamWeakQuality) / (m_secamStrongQuality - m_secamWeakQuality);
    float bandwidth = m_secamMinLoopBandwidth
        + std::min(std::max(strength, 0.0f), 1.0f) * (m_secamMaxLoopBandwidth - m_secamMinLoopBandwidth);
    const float wn = 2.0f * (float) M_PI * bandwidth / rate;
    const float k1 = 2.0f * 0.8f * wn;
    const float k2 = wn * wn;
    const float scale = 1.0f / std::max(1.15f * m_secamAmplitude, 1e-6f);

    float frequency = rest * radiansPerHz;
    float ncoRe = 1.0f;
    float ncoIm = 0.0f;
    float power = 0.0f;
    int powerCount = 0;
    int first = std::max(m_secamPorchStart, (firstValid / d + 1) * d);

    int point = 0;

    for (int n = 0; n < first; n += d) {
        m_secamFrequency[point++] = frequency;
    }

    for (int n = first; n < length; n += d)
    {
        const float re = m_chroma[n].real();
        const float im = m_chroma[n].imag();
        float error = (im * ncoRe - re * ncoIm) * scale; // Im(c * conj(nco))
        error = std::min(std::max(error, -1.5f), 1.5f);
        frequency += k2 * error;

        // A step is well under a radian, so a short series will do, and is much quicker than
        // sin and cos here. The tiny error in its magnitude does not build up in a line
        float step = frequency + k1 * error;
        float step2 = step * step;
        float cosine = 1.0f - step2 * (0.5f - step2 * (1.0f / 24.0f - step2 / 720.0f));
        float sine = step * (1.0f - step2 * (1.0f / 6.0f - step2 / 120.0f));
        float rotated = ncoRe * cosine - ncoIm * sine;
        ncoIm = ncoRe * sine + ncoIm * cosine;
        ncoRe = rotated;
        m_secamFrequency[point++] = frequency;
        power += re * re + im * im;
        powerCount++;
    }

    if (powerCount > 0) {
        m_secamAmplitude += 0.05f * (std::sqrt(power / powerCount) - m_secamAmplitude);
    }

    // To the colour difference, with the low frequency de-emphasis, (1 + jf/255k) / (1 + jf/85k)
    const float deviation = (m_secamDb ? m_secamDbDeviation : m_secamDrDeviation) * (m_secamDb ? m_secamDbGain : m_secamDrGain);
    const float w1 = 2.0f * (float) M_PI * 85000.0f;
    const float w2 = 2.0f * (float) M_PI * 255000.0f;
    const float k = 2.0f * rate;
    const float a0 = 1.0f + k / w1;
    const float a1 = (1.0f - k / w1) / a0;
    const float b0 = (1.0f + k / w2) / a0;
    const float b1 = (1.0f - k / w2) / a0;
    const float toDifference = 1.0f / (radiansPerHz * deviation);
    float x1 = 0.0f;
    float y1 = 0.0f;
    int points = (length - 1) / d + 1;

    for (int i = 0; i < points; i++)
    {
        float x = (m_secamFrequency[i] - rest * radiansPerHz) * toDifference;
        float y = b0 * x + b1 * x1 - a1 * y1;
        x1 = x;
        y1 = y;
        m_secamFrequency[i] = std::min(std::max(y, -1.0f), 1.0f); // The range of the signal, as the loop can slip
    }

    movingAverage(m_secamFrequency, m_secamSum, points, m_chromaSmoothing);
    movingAverage(m_secamFrequency, m_secamSum, points, m_chromaSmoothing);

    // The de-emphasis delays by this many samples at low frequencies, so take it back out
    const float delay = (1.0f / w1 - 1.0f / w2) * m_sampleRate;
    const int advance = (int) std::round(delay);
    const float perSample = 1.0f / d;
    int i = advance / d;
    int within = advance - i * d;
    int pixel = 0;

    for (int n = 0; n < length; n += m_pixelStep)
    {
        float value = m_secamFrequency[std::min(i, points - 1)];

        if (i + 1 < points) {
            value += (within * perSample) * (m_secamFrequency[i + 1] - value);
        }

        m_secamLine[pixel++] = value;
        within += m_pixelStep;

        if (within >= d)
        {
            within -= d;
            i++;
        }
    }
}

void ATVColourDecoder::decodeLineSECAM(int firstValid, int length, unsigned int *rgb)
{
    updateSECAMLineType(firstValid, length);

    if ((firstValid <= m_secamPorchStart) && (length > m_secamPorchEnd))
    {
        float power = 0.0f;
        int count = 0;

        for (int n = m_secamPorchStart; n < m_secamPorchEnd; n += m_chromaDecimation)
        {
            power += std::norm(m_chroma[n]);
            count++;
        }

        m_burstAmplitude += 0.1f * (std::sqrt(power / count) - m_burstAmplitude);
    }

    // Colour killer. There is no fit to the line sequence without SECAM's subcarriers
    m_colourOn = m_secamLineType > (m_colourOn ? 0.03f : 0.06f);
    demodulateSECAM(firstValid, length);

    float range = 100.0f - m_setup;

    if (!m_colourOn)
    {
        for (int n = 0; n < length; n += m_pixelStep)
        {
            float y = (m_ire[n] - m_setup) / range;
            *rgb++ = packRGB(y, y, y);
        }

        return;
    }

    // On a weak signal, what noise gets through the loop is mostly in streaks a line high, so a
    // median of the last three lines of this type removes them, for less vertical colour detail
    bool median = m_secamQuality < m_secamStrongQuality;
    std::vector<float>& older = m_secamHistory[m_secamDb ? 1 : 0][1];
    std::vector<float>& old = m_secamHistory[m_secamDb ? 1 : 0][0];

    int pixel = 0;

    for (int n = 0; n < length; n += m_pixelStep)
    {
        const std::complex<float>& c = m_chroma[n];
        float chroma = 0.5f * (c.real() * m_phasor[n].real() + c.imag() * m_phasor[n].imag()); // Re(c * conj(phasor)) / 2
        float y = (m_ire[n] - chroma - m_setup) / range;
        const float latest = m_secamLine[pixel];
        float value = latest;

        if (median)
        {
            float b = old[pixel];
            float e = older[pixel];
            value = std::max(std::min(value, b), std::min(std::max(value, b), e));
        }

        older[pixel] = old[pixel];
        old[pixel] = latest;
        float current = value * m_saturation;
        float previous = m_secamPrevious[pixel];
        m_secamPrevious[pixel] = current;
        float by = m_secamDb ? current : previous;
        float ry = m_secamDb ? previous : current;
        *rgb++ = packRGB(y + ry, y - 0.509f * ry - 0.194f * by, y + by);
        pixel++;
    }
}
