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

#ifndef INCLUDE_DSDFRONTEND_H
#define INCLUDE_DSDFRONTEND_H

#include <complex>
#include <vector>

/**
 * Digital voice receiver front end at 48 kHz: AFC rotation, complex channel
 * filter, envelope power and FM discriminator in hertz.
 *
 * Everything downstream works in hertz: the discriminator output is the
 * instantaneous frequency, f = atan2(z[n] conj(z[n-1])) * fs / 2pi. The
 * envelope is |z|^2 after the channel filter, in the input's linear units.
 */
class DSDFrontEnd
{
public:
    static constexpr int m_sampleRate = 48000;

    DSDFrontEnd();

    //! Channel filter: low pass with the given cutoff (Hz). Rebuilds the taps; keeps the state.
    void setBandwidth(float cutoffHz);
    //! Frequency correction applied before the filter (positive shifts the signal down)
    void setCorrection(float hz);
    //! Remove the input's DC before anything else (default on): a receiving SDR tuned to the channel
    //! puts its DC spike at the centre of it, and at the level of a weak signal it stops the Angel61
    //! receivers (whose UW metric and channel estimate take it for signal) decoding anything
    void setDCBlock(bool enabled) { m_dcBlock = enabled; }
    //! Take the DC afresh when the carrier goes off (default on; off: the plain slow estimate, for comparison)
    void setDropDetection(bool enabled) { m_dropDetection = enabled; }
    float getCorrection() const { return m_correctionHz; }
    void reset();

    //! Process one input sample. Returns the frequency in Hz and the envelope power.
    void process(const std::complex<float>& in, float& frequencyHz, float& power) { processDCFree(removeDC(in), frequencyHz, power); }
    //! The first step of process() alone: the input sample with the DC blocker's estimate taken
    //! off (as it is when the blocker is off). DSDDemodulator takes it once for the IQ that both the
    //! Angel61 sessions and the FM chains receive, and gives it to processDCFree()
    std::complex<float> removeDC(const std::complex<float>& in);
    //! The rest of process(), on a sample removeDC() gave
    void processDCFree(const std::complex<float>& in, float& frequencyHz, float& power);

    int getDelay() const { return (int) m_taps.size() / 2; }
    const std::vector<float>& getTaps() const { return m_taps; }
    //! The channel filtered sample of the last process() call (the discriminator's z[n])
    const std::complex<float>& getFiltered() const { return m_previous; }
    //! The frequency corrected but unfiltered sample of the last process() call: the filtered
    //! sample lags it by getDelay() samples
    const std::complex<float>& getRotated() const { return m_rotated; }

private:
    std::vector<float> m_taps;
    std::vector<std::complex<float>> m_delay;
    int m_delayIndex;
    float m_correctionHz;
    double m_phase;
    double m_phaseStep;
    std::complex<float> m_previous;
    std::complex<float> m_rotated;
    bool m_dcBlock;
    std::complex<float> m_dc;           //!< The input's DC, a one pole average with a 0.5 Hz corner
    static constexpr float m_dcAlpha = 6.545e-5f;   //!< 2 pi 0.5 Hz / 48 kHz: 0.3 s, a notch too narrow to take anything of a signal
    // The carrier going off (see process)
    float m_powerFast;                  //!< The input's power over about 1 ms
    float m_powerSlow;                  //!< and over about 100 ms
    bool m_dropped;                     //!< The input is 10 dB or more below its recent power
    bool m_dropUsed;                    //!< and the DC is being taken from the input since the drop
    int m_dropSamples;                  //!< for this many samples
    std::complex<float> m_dcDrop;       //!< The input's mean since the drop (5 ms once there are that many samples)
    std::complex<float> m_dcSaved;      //!< The slow estimate when the drop began, restored if it was a fade
    bool m_dropDetection;
    static constexpr float m_fastAlpha = 1.0f / 48.0f;    //!< 1 ms at 48 kHz
    static constexpr float m_slowAlpha = 1.0f / 4800.0f;  //!< 100 ms
    static constexpr float m_dropRatio = 0.1f;            //!< A fall of 10 dB
    static constexpr float m_deepRatio = 0.01f;           //!< 20 dB: deeper than all but 1% of a Rayleigh fade
    static constexpr float m_dcDominated = 0.5f;          //!< The mean since the drop holds this much of its power: DC
    static constexpr int m_dropMeanSamples = 240;         //!< The mean since the drop runs over 5 ms
    static constexpr int m_deepMeanSamples = 48;          //!< and is used after 1 ms once the drop is 20 dB deep
    static constexpr int m_dropCommitSamples = 2400;      //!< A drop of 50 ms or more is an end, not a fade
};

#endif // INCLUDE_DSDFRONTEND_H
