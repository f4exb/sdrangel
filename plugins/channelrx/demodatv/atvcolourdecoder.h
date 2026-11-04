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

#ifndef INCLUDE_ATVCOLOURDECODER_H
#define INCLUDE_ATVCOLOURDECODER_H

#include <complex>
#include <cstdint>
#include <vector>

#include "dsp/dsptypes.h"

#include "atvdemodsettings.h"

class FFTEngine;

/**
 * Front end for AM / VSB signals.
 *
 * Applies a Nyquist slope through the vision carrier, as a TV receiver's IF filter does,
 * which flattens the response across the vestigial sideband and removes anything below
 * the carrier (such as an aliased sound carrier). It also band limits above the video
 * (the chroma for colour) and upsamples by an integer factor in the frequency domain, so
 * the envelope of the signal can be taken without aliasing.
 *
 * For colour, input should have the centre of the video band at DC (see
 * ATVDemodSettings::getColourBandCentreOffset). For mono without upsampling, it should
 * have the vision carrier at DC, so the video is band limited to half the sample rate.
 */
class ATVVSBFrontEnd
{
public:
    ATVVSBFrontEnd();
    ~ATVVSBFrontEnd();
    ATVVSBFrontEnd(const ATVVSBFrontEnd&) = delete;
    ATVVSBFrontEnd& operator=(const ATVVSBFrontEnd&) = delete;

    /**
     * @param sampleRate input sample rate
     * @param carrierOffset frequency of the vision carrier in the input (Hz)
     * @param videoBandwidth top of the video above the vision carrier (Hz)
     * @param usb true when the video is above the vision carrier
     * @param upsampling integer upsampling factor (1, 2 or 4)
     */
    void configure(int sampleRate, float carrierOffset, float videoBandwidth, bool usb, int upsampling);

    /** Returns the number of output samples in *out, which is 0 until a block completes */
    int process(const Complex& in, Complex **out)
    {
        m_fftIn[m_inCount++] = in;

        if (m_inCount < m_blockLen) {
            return 0;
        }

        m_inCount = 0;
        int n = processBlock(out);

        // Drop the start-up transient while the overlap fills, as it would upset the AGC
        if (m_skipBlocks > 0)
        {
            m_skipBlocks--;
            return 0;
        }

        return n;
    }

private:
    static const int m_fftLen = 1024;           //!< Input FFT length
    static const int m_blockLen = m_fftLen / 2; //!< Input samples per block (and filter length)

    int m_upsampling;
    FFTEngine *m_fft;
    unsigned int m_fftSequence;
    FFTEngine *m_ifft;
    unsigned int m_ifftSequence;
    Complex *m_fftIn;               //!< m_fft's input, filled a block at a time
    std::vector<Complex> m_response; //!< Filter frequency response, m_fftLen bins, scaled by 1 / m_fftLen
    std::vector<Complex> m_overlap;
    std::vector<Complex> m_out;
    int m_inCount;
    int m_skipBlocks;

    int processBlock(Complex **out);
};

/**
 * Synchronous AM detector for the front end's output.
 *
 * Envelope detection of a vestigial sideband signal adds a quadrature distortion that
 * grows with the square of the chroma, darkening saturated colours, so the vision carrier
 * is recovered and the in-phase component taken instead, as a TV does.
 *
 * Carrier recovery is keyed to the sync tips, where the carrier is not modulated by the
 * picture. Transmitters that overmodulate cut the carrier off in white areas and shift its
 * phase, and through the Nyquist slope the picture's own detail is lopsided about the
 * carrier, which an unkeyed loop reads as a frequency error (+3.5 kHz on a white text page,
 * enough to tear the picture). Even within a sync tip the phase ramps, as the filter adds a
 * quadrature component at the pulse's edges, so frequency is measured between matching
 * points rather than along a pulse:
 *  - fine: the phase of each line's sync tip against the previous line's. This is unbiased,
 *    but unambiguous only to half the line rate (+/- 7.8 kHz for 525 lines)
 *  - coarse: the phase change along the flat middle of the vertical sync's broad pulses,
 *    away from their edges, which corrects the fine loop when it is a line rate out
 * The phase comes from a narrow low pass of the samples within 85% of the sync tip carrier.
 */
class ATVSynchronousDetector
{
public:
    ATVSynchronousDetector();

    /**
     * @param carrierFrequency nominal frequency of the vision carrier in the input (Hz)
     * @param lineFrequency lines per second
     */
    void configure(float sampleRate, float carrierFrequency, float lineFrequency);

    /** Returns the in-phase amplitude, which is the envelope when the carrier is tracked */
    float process(const std::complex<float>& in)
    {
        std::complex<float> mixed = in * m_rotation;
        m_rotation *= m_step;
        m_ghostInput = mixed;

        if (m_ghostOn)
        {
            // y[n] = x[n] - g y[n - D], which cancels an echo g x[n - D] exactly
            int i0 = (m_ghostIndex - m_ghostDelay) & m_ghostMask;
            int i1 = (i0 - 1) & m_ghostMask;
            mixed -= m_ghostGain * (m_ghostBuffer[i0] + m_ghostFrac * (m_ghostBuffer[i1] - m_ghostBuffer[i0]));
            m_ghostBuffer[m_ghostIndex] = mixed;
            m_ghostIndex = (m_ghostIndex + 1) & m_ghostMask;
        }

        if (std::norm(mixed) > m_keyLevel) {
            m_phaseCarrier += m_phaseLowPassAlpha * (mixed - m_phaseCarrier);
        }

        m_block += mixed;
        float out = mixed.real() * m_carrierUnit.real() - mixed.imag() * m_carrierUnit.imag(); // Re(mixed * unit)

        if (++m_count == m_blockLength) {
            update();
        }

        return out;
    }

    float getFrequencyError() const; //!< Hz, carrier relative to its nominal frequency

    /**
     * Cancels an echo (ghost) of the signal, delayed by delay samples (1 or more) with the
     * given gain relative to the carrier. 0 gain turns cancellation off
     */
    void setGhost(float delay, std::complex<float> gain);
    /** The last sample before ghost cancellation, with the carrier's phase removed (real is the in-phase amplitude) */
    std::complex<float> getGhostInput() const { return m_ghostInput * m_carrierUnit; }

private:
    static const int m_ghostBufferSize = 256;           //!< samples, a power of 2
    static const int m_ghostMask = m_ghostBufferSize - 1;
    static const int m_maxRunBlocks = 64;  //!< longer runs (when the blanking is keyed through the vertical interval) are not used
    static const int m_runEdgeBlocks = 3;  //!< blocks at each end of a broad pulse left out of the coarse estimate
    static const int m_settleBlocks = 2000; //!< blocks for the sync tip level to settle before keying

    float m_sampleRate;
    int m_blockLength;                 //!< samples per block, ~1 us, so several fit in a sync tip
    float m_linePeriod;                //!< samples
    double m_nominalStep;              //!< radians / sample
    double m_frequency;                //!< FLL correction, radians / sample
    std::complex<float> m_rotation;
    std::complex<float> m_step;
    std::complex<float> m_phaseCarrier; //!< narrow low pass of the keyed samples, for the phase
    std::complex<float> m_carrierUnit;
    float m_phaseLowPassAlpha;
    int m_count;
    std::complex<float> m_block;       //!< sum of the samples in the current block
    int64_t m_blockIndex;
    float m_tipLevel;                  //!< sync tip block amplitude
    float m_keyLevel;                  //!< power above which samples are used for the phase
    std::vector<std::complex<float>> m_run; //!< blocks of the current sync tip
    int64_t m_runStart;                //!< block index
    bool m_runOverflow;
    std::complex<float> m_prevRunSum;
    int m_prevRunLength;
    int64_t m_prevRunStart;
    std::complex<float> m_coarseSum;   //!< phase change per block along broad pulses
    int m_coarseCount;
    bool m_ghostOn;
    int m_ghostDelay;                  //!< samples, integer part
    float m_ghostFrac;                 //!< samples, fractional part
    std::complex<float> m_ghostGain;
    std::vector<std::complex<float>> m_ghostBuffer; //!< output, for the cancellation's feedback
    int m_ghostIndex;
    std::complex<float> m_ghostInput;

    void update();
    void endRun();
};

/**
 * Decodes NTSC, PAL and SECAM colour, one line at a time.
 *
 * Works on a whole line of composite video once it is complete, so its filters can be
 * zero phase and the burst, which comes before the active video, is known before any
 * pixel is decoded.
 *
 * SECAM has no burst or quadrature modulation: B-Y and R-Y are sent on alternate lines, each
 * frequency modulating its own subcarrier, so the other one always comes from the line
 * before. Each line's subcarrier sits at its rest frequency on the back porch, which shows
 * which line this is. As FM needs far more signal than quadrature modulation, and the chroma
 * of a weak picture can be no stronger than the noise in its band, where a discriminator
 * gives streaks and is pulled to the middle of the band, a narrow phase locked loop follows
 * the subcarrier, started each line from the rest frequency.
 */
class ATVColourDecoder
{
public:
    ATVColourDecoder();

    void configure(float sampleRate, ATVDemodSettings::ColourSystem colourSystem);
    void setSaturation(int saturation) { m_saturation = saturation / 100.0f; }
    void setHue(int hue) { m_hue = std::polar(1.0f, hue * (float) M_PI / 180.0f); }
    void setDelayLine(bool delayLine) { m_delayLine = delayLine; } //!< SECAM always needs it, so ignores this
    /** Line is the envelope of a negatively modulated AM signal, rather than video scaled with sync tip low */
    void setCarrierReferenced(bool carrierReferenced) { m_carrierReferenced = carrierReferenced; }

    /**
     * Decode one line.
     *
     * @param line composite samples, index 0 at the leading edge of the sync pulse, scaled so sync tip is low,
     *        or the AM envelope when carrier referenced
     * @param length number of samples in the line
     * @param firstValid first index written for this line (earlier ones are stale after a sync jump)
     * @param lineStart sample count at index 0 of the line, so the subcarrier phase is continuous across lines
     * @param rgb output, one pixel every getPixelStep() samples of the line, packed as 0x00BBGGRR
     *        (the byte order the TV screen texture uses)
     */
    void decodeLine(const float *line, int length, int firstValid, int64_t lineStart, unsigned int *rgb);

    bool getColourLocked() const { return m_colourOn; }
    float getBurstAmplitude() const { return m_burstAmplitude; } //!< Smoothed burst amplitude (for SECAM, of the subcarrier on the back porch), IRE
    int getPixelStep() const { return m_pixelStep; } //!< Samples of the line per pixel of the output

private:
    static constexpr float m_secamDbRest = 4250000.0f;      //!< Hz, B-Y subcarrier
    static constexpr float m_secamDrRest = 4406250.0f;      //!< Hz, R-Y subcarrier
    static constexpr float m_secamDbDeviation = 230000.0f;  //!< Hz for D'b = 1
    static constexpr float m_secamDrDeviation = 280000.0f;  //!< Hz for D'r = 1
    static constexpr float m_secamDbGain = 1.505f;          //!< D'b = 1.505 (B-Y)
    static constexpr float m_secamDrGain = -1.902f;         //!< D'r = -1.902 (R-Y)
    static constexpr float m_secamBandCentre = 4325000.0f;  //!< Hz, of the chroma band, half way between the band's limits
    static constexpr float m_secamMinLoopBandwidth = 200000.0f; //!< Hz, of the phase locked loop for a weak signal
    static constexpr float m_secamMaxLoopBandwidth = 500000.0f; //!< and a strong one, which can follow the steepest edges
    static constexpr float m_secamWeakQuality = 0.65f;      //!< Back porch coherence of a weak signal, which gets the narrowest loop
    static constexpr float m_secamStrongQuality = 0.8f;     //!< and of one strong enough for the widest

    float m_sampleRate;
    float m_subcarrierFrequency;
    bool m_pal;
    bool m_secam;
    float m_syncDepth;      //!< Sync depth below blanking, IRE
    float m_nominalBurst;   //!< Nominal burst peak amplitude, IRE
    float m_setup;          //!< Black level above blanking, IRE
    float m_blankCarrier;   //!< Blanking and white carrier levels, fraction of the sync tip, when carrier referenced
    float m_whiteCarrier;
    int m_tipStart, m_tipEnd;
    int m_burstStart, m_burstEnd;
    int m_chromaDecimation; //!< Chroma is band limited, so is only filtered every this many samples
    int m_chromaPrefilter;  //!< The rate is divided by this before the chroma filter
    int m_pixelStep;        //!< Pixels are only calculated every this many samples

    bool m_carrierReferenced;
    float m_saturation;
    std::complex<float> m_hue;
    bool m_delayLine;

    std::vector<float> m_lpfTaps;                 //!< Chroma low pass (zero phase, odd length)
    std::vector<std::complex<float>> m_phasor;    //!< 2 * exp(-j w n) for the subcarrier, from the start of the line
    std::vector<float> m_ire;
    std::vector<float> m_mixedRe;                 //!< Composite mixed down by the subcarrier
    std::vector<float> m_mixedIm;
    std::vector<float> m_filterRe;                //!< The above at the rate of the chroma filter, when m_chromaPrefilter is more than 1
    std::vector<float> m_filterIm;
    std::vector<std::complex<float>> m_chroma;    //!< Baseband chroma, every sample
    std::vector<std::complex<float>> m_chromaPoints; //!< Baseband chroma, every m_chromaDecimation samples, then smoothed for display
    std::vector<std::complex<float>> m_chromaSum; //!< Running sums for that smoothing
    int m_chromaSmoothing;                        //!< Boxcar length in m_chromaPoints, applied twice
    std::vector<std::complex<float>> m_prevUV;    //!< Previous line's U + jV for the delay line

    float m_tipLevel;
    float m_blankLevel;
    bool m_levelsValid;
    std::complex<float> m_burstRef;       //!< Smoothed burst vector (NTSC) or sum of two lines' bursts (PAL)
    std::complex<float> m_prevBurst;
    float m_burstAmplitude;
    bool m_colourOn;
    int m_missingBursts;                  //!< Consecutive lines without a burst
    float m_vSign;                        //!< PAL V switch for the current line

    int m_secamPorchStart, m_secamPorchEnd; //!< Where the rest frequency is measured
    int m_secamLag;                       //!< Samples, for the frequency comparison on the back porch
    bool m_secamDb;                       //!< This line carries B-Y
    float m_secamLineType;                //!< How well the back porches fit the assumed line sequence: 0 is no fit, up to 1
    float m_secamAmplitude;               //!< Smoothed rms of the chroma over the picture
    float m_secamQuality;                 //!< Smoothed coherence of the subcarrier along the back porch, 0 to 1
    std::vector<float> m_secamFrequency;  //!< The loop's frequency, radians per decimated sample
    std::vector<float> m_secamHistory[2][2]; //!< The last two lines of each type, for the vertical median
    std::vector<float> m_secamSum;        //!< Running sums for smoothing
    std::vector<float> m_secamLine;       //!< This line's colour difference, per pixel
    std::vector<float> m_secamPrevious;   //!< The previous line's, which is the other one, as displayed

    void resize(int length);
    void updateSECAMLineType(int firstValid, int length);
    void demodulateSECAM(int firstValid, int length);
    void decodeLineSECAM(int firstValid, int length, unsigned int *rgb);
    std::complex<float> filterChroma(int n, int length) const;
};

#endif // INCLUDE_ATVCOLOURDECODER_H
