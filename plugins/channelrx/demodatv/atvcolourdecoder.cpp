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
    float magnitude = std::abs(m_phaseCarrier);

    if (magnitude > 0.0f) {
        m_carrierUnit = std::conj(m_phaseCarrier) / magnitude;
    }

    // Sync tip level: follows the strongest blocks, and decays over ~100 ms so keying
    // recovers if the signal level drops
    float blockAmplitude = std::abs(m_block) / m_blockLength;

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
    m_rotation /= std::abs(m_rotation); // Keep the recursive rotation on the unit circle
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
    m_subcarrierFrequency = ATVDemodSettings::getSubcarrierFrequency(colourSystem);
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

    // Chroma low pass: 6 dB at 1.5 MHz, stop band from about 2.1 MHz, which rejects
    // both the luma (shifted down to -fsc) and the image at -2 fsc
    float cutoff = 1.5e6f / sampleRate;
    int taps = ((int) std::ceil(5.5f * sampleRate / 1.2e6f)) | 1;
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

    m_chromaDecimation = std::max(1, (int) (sampleRate / 4.5e6f));

    // The displayed chroma is narrower again, as a TV's was, which halves its noise:
    // two 0.6 us boxcars make a triangle about 0.75 MHz wide at 6 dB. The wider chroma is
    // still used to separate it from the luma, so the luma keeps no subcarrier
    m_chromaSmoothing = std::max(1, (int) std::round(0.6e-6f * sampleRate));

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
        m_chroma.resize(length);
        m_chromaSum.resize(length + 1);
        m_chromaDisplay.resize(length);
        m_prevUV.resize(length);
    }
}

// Centred moving average of m_chromaSmoothing samples (in and out can be the same)
void ATVColourDecoder::smoothChroma(std::vector<std::complex<float>>& in, std::vector<std::complex<float>>& out, int length)
{
    const int w = m_chromaSmoothing;
    const int before = w / 2;
    m_chromaSum[0] = 0.0f;

    for (int n = 0; n < length; n++) {
        m_chromaSum[n + 1] = m_chromaSum[n] + in[n];
    }

    for (int n = 0; n < length; n++)
    {
        int a = std::max(n - before, 0);
        int b = std::min(n - before + w, length);
        out[n] = (m_chromaSum[b] - m_chromaSum[a]) / (float) (b - a);
    }
}

std::complex<float> ATVColourDecoder::filterChroma(int n, int length) const
{
    const int taps = (int) m_lpfTaps.size();
    const int half = taps / 2;
    const float *h = m_lpfTaps.data();
    float re = 0.0f;
    float im = 0.0f;

    if ((n - half >= 0) && (n + half < length))
    {
        // Separate real and imaginary arrays, so this vectorises
        const float *xr = &m_mixedRe[n - half];
        const float *xi = &m_mixedIm[n - half];

        for (int i = 0; i < taps; i++)
        {
            re += h[i] * xr[i];
            im += h[i] * xi[i];
        }
    }
    else
    {
        for (int i = 0; i < taps; i++)
        {
            int j = std::min(std::max(n + i - half, 0), length - 1);
            re += h[i] * m_mixedRe[j];
            im += h[i] * m_mixedIm[j];
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
        for (int n = 0; n < length; n++) {
            rgb[n] = packRGB(line[n], line[n], line[n]);
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
    m_chroma[0] = filterChroma(0, length);

    for (int n = d; n < length; n += d)
    {
        m_chroma[n] = filterChroma(n, length);

        for (int i = 1; i < d; i++) {
            m_chroma[last + i] = m_chroma[last] + (m_chroma[n] - m_chroma[last]) * ((float) i / d);
        }

        last = n;
    }

    for (int n = last + 1; n < length; n++) {
        m_chroma[n] = m_chroma[last];
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
        for (int n = m_burstStart; n < m_burstEnd; n++) {
            burst += m_chroma[n];
        }

        burst *= lineRotation / (float) (m_burstEnd - m_burstStart);
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
        for (int n = 0; n < length; n++)
        {
            float y = (m_ire[n] - m_setup) / range;
            rgb[n] = packRGB(y, y, y);
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

    smoothChroma(m_chroma, m_chromaDisplay, length);
    smoothChroma(m_chromaDisplay, m_chromaDisplay, length);

    for (int n = 0; n < length; n++)
    {
        const std::complex<float>& c = m_chroma[n];
        float chroma = 0.5f * (c.real() * m_phasor[n].real() + c.imag() * m_phasor[n].imag()); // Re(c * conj(phasor)) / 2
        float y = (m_ire[n] - chroma - m_setup) / range;
        std::complex<float> uv = m_chromaDisplay[n] * rot;

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
        rgb[n] = packRGB(y + ry, y - 0.509f * ry - 0.194f * by, y + by);
    }
}
