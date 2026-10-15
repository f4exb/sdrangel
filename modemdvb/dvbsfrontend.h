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

#ifndef MODEMDVB_DVBSFRONTEND_H
#define MODEMDVB_DVBSFRONTEND_H

#include <algorithm>
#include <atomic>
#include <cmath>
#include <complex>
#include <cstdint>
#include <mutex>
#include <vector>

#include "export.h"

namespace dvbs {

//! The single carrier receivers' front end: samples at the channel rate in, symbols at one a symbol out, with the
//! carrier coarsely centred and the symbol timing recovered.
//!
//! - The carrier: before anything else, the spectrum of the first samples is measured, and the signal's centre found by
//!   correlating it with the raised cosine's shape, to a fraction of a percent of the symbol rate. An NCO moves it to 0
//!   Hz; the receivers' carrier recovery does the rest, and moves the NCO (adjustFrequency()) if what's left gets large.
//! - The matched filter: a root raised cosine, made a polyphase filter at the input rate (makePolyphaseFilter()), so one
//!   pass of the resampler's kernel both filters and interpolates, at two outputs a symbol: on the symbol, and half way.
//! - Timing: a Gardner detector on those outputs, averaged over blocks of symbols, steering a second order loop whose
//!   output moves the interpolation's time and step, so the symbol clock's offset is measured, as the OFDM receivers'
//!   is (in ppm).
//! - AGC: the symbols are scaled to unit average power.
//! - Equaliser: once a receiver's locked (setEqualiserReference()), the input's spectrum is averaged and compared with the
//!   raised cosine's, plus noise at the Es/N0 measured: the difference is the receive chain's response (the device's
//!   filter, the transponder's, cable tilt), which cut the signal's edges by 4 to 7 dB at 27.5 MS/s on Astra 2 and cost
//!   1 to 2 dB of Es/N0. It's undone in the matched filter, symmetrically (the filter's real), by up to 10 dB, so it
//!   costs nothing a sample. For signals of at most 8 samples a symbol (the device's filter is flat for narrower)
class MODEMDVB_API FrontEnd
{
public:
    typedef std::complex<float> Cf;

    struct Status
    {
        bool m_estimating = true;       //!< Measuring the spectrum, so no symbols yet
        double m_frequencyOffset = 0.0; //!< Hz, of the NCO: where the signal was found, plus adjustments
        double m_clockOffset = 0.0;     //!< ppm, of the symbol rate from that set (positive if faster)
        double m_measuredSymbolRate = 0.0;  //!< From the spectrum's half power width, 0 if not measured
        //! dB, from the spectrum: its in-band level over the noise floor outside the signal, when there's enough
        //! outside it (as for m_measuredSymbolRate), else a high value (taken as a strong signal)
        double m_spectralEsN0 = 99.0;
        uint64_t m_clockAdjustments = 0;    //!< adjustClock()s made, so a receiver knows when its has been
        double m_power = 0.0;           //!< dB, of the input
        uint64_t m_symbols = 0;         //!< Output since the reset
        bool m_equalising = false;      //!< The matched filter undoes the receive chain's response
        double m_equaliserEdgeDb = 0.0; //!< The power gain it applies half the symbol rate from the centre
    };

    FrontEnd();

    //! Input at inputRate, at least 2 symbolRate for the matched filter not to alias (a little less works)
    void configure(double inputRate, double symbolRate, double rollOff);
    void setRollOff(double rollOff);
    double rollOff() const { return m_rollOff; }
    double inputRate() const { return m_inputRate; }
    double symbolRate() const { return m_symbolRate; }
    //! Measure the spectrum again for the carrier, and start the timing loop afresh
    void reset();
    //! Keep the carrier's frequency (from adjustFrequency()) through a reset rather than measuring it again
    void setFrequency(double hz);
    //! Appends the symbols the samples complete (none while estimating)
    void process(const Cf *in, int nbIn, std::vector<Cf>& symbols);
    //! Move the carrier the NCO takes out by hz, from the next sample processed
    void adjustFrequency(double hz);
    //! Move the timing loop's clock (its integrator) by ppm: a clock error measured elsewhere (from frames' slips)
    void adjustClock(double ppm);
    //! Symbols still to come of the samples it holds (turned by the NCO as it was then)
    int heldSymbols() const;
    //! Loop noise bandwidth times the symbol period: wide to acquire, narrow to track
    void setTimingBandwidth(double bnT);
    //! The tracking bandwidth for a lock at an Es/N0 (dB): bnT down to -2 dB (DVB-S2's lowest MODCOD's threshold), then
    //! less with the SNR, a tenth at -7 dB. The Gardner detector's noise grows as the SNR falls (its noise by noise
    //! term), and at DVB-S2X VL-SNR's -6 to -10 dB, the loop at bnT wandered tens of ppm, half a symbol across a frame,
    //! losing the next header
    static double trackingBandwidth(double bnT, double esn0)
    {
        return esn0 >= -2.0 ? bnT : std::max(0.04 * bnT, bnT * std::pow(10.0, (esn0 + 2.0) / 5.0));
    }
    //! The equaliser on (the default) or off: off, the matched filter's the plain root raised cosine again
    void setEqualiser(bool enabled) { m_eqEnabled = enabled; }
    //! From the receiver, each block: whether it's locked (so the spectrum's the signal's), and its Es/N0 (dB)
    void setEqualiserReference(bool locked, double esn0);
    const Status& getStatus() const { return m_status; }
    //! The input's power spectrum, dB, points bins from -inputRate/2 to +inputRate/2. Empty if not measured yet. From
    //! any thread
    void getSpectrum(std::vector<float>& db, int points) const;

    static constexpr int m_fftSize = 1024;

private:
    static constexpr double m_pi = 3.141592653589793;
    static constexpr int m_spectrumBlocks = 48;             //!< Averaged for the carrier's estimate
    static constexpr int m_symbolsPerUpdate = 32;           //!< Of the timing loop
    static constexpr double m_filterSpan = 10.0;            //!< Symbols, of the matched filter
    static constexpr double m_maxClockOffset = 0.005;       //!< Of the timing loop's integrator: 5000 ppm
    static constexpr double m_maxPhaseStep = 0.1;           //!< Symbols, the timing loop may move a block
    static constexpr int m_eqUpdateBlocks = 40;             //!< Spectrum blocks (one each 0.1 s) averaged for each equaliser update
    static constexpr double m_eqMaxSamplesPerSymbol = 8.0;  //!< Wider signals only
    static constexpr double m_eqMaxGainDb = 10.0;           //!< Of the correction, of power
    static constexpr double m_eqMinGainDb = -3.0;
    static constexpr double m_eqSmoothing = 0.02;           //!< Of the symbol rate either side, the response is averaged over
    static constexpr int m_eqFrequencies = 128;             //!< Points of the filter's frequency response, for its impulse
    static constexpr double m_eqMinChangeDb = 0.1;          //!< Of the correction at any frequency, for the filter to be made again
    static constexpr double m_eqMinEsN0 = 3.0;              //!< dB, for the response to be measured: below, the model of the spectrum
                                                            //!< (the raised cosine over noise) needs the Es/N0 too exactly, and the
                                                            //!< noise swamps the interference undoing the response would remove

    static double raisedCosine(double t, double a);
    static double besselI0(double x);

    void makeFilter();
    void computeGains();
    void accumulateEqualiser(const std::vector<double>& power);
    void computeEqualiser();
    void clearEqualiser();
    void spectrumBlock(const Cf *block, std::vector<double>& power);
    void displayBlock(const Cf *block);
    void estimateCarrier();
    void fft(std::vector<std::complex<double>>& x) const;

    double m_inputRate;
    double m_symbolRate;
    double m_rollOff;
    double m_samplesPerSymbol;      //!< Input samples
    int m_taps;
    std::vector<float> m_pairs;     //!< The matched filter's phases
    static constexpr int m_phases = 256;

    // NCO
    double m_ncoFrequency;          //!< Hz taken out
    double m_ncoPhase;              //!< Radians, at the next sample
    bool m_frequencyKnown;          //!< setFrequency() was called, so no estimate after the reset

    // Spectrum: the carrier estimate, then for display
    std::vector<double> m_window;
    std::vector<std::complex<double>> m_twiddles;
    std::vector<int> m_bitReverse;
    std::vector<Cf> m_spectrumBlock;    //!< Being gathered (for the estimate, then for display from short blocks)
    int m_spectrumFill;
    std::vector<double> m_estimatePower;
    int m_estimateBlocks;
    std::vector<double> m_displayPower;
    mutable std::mutex m_displayMutex;  //!< For m_displayPower
    uint64_t m_untilDisplayBlock;   //!< Input samples

    // Matched filter and timing
    std::vector<Cf> m_buffer;       //!< NCO'd input not yet used
    double m_time;                  //!< Of the next output, input samples from m_buffer[0]
    double m_step;                  //!< Nominal, input samples between outputs (half a symbol)
    bool m_onSymbol;                //!< The next output is on a symbol (else half way)
    Cf m_lastSymbol;
    Cf m_lastMid;
    bool m_haveLast;
    double m_integrator;            //!< Symbols per symbol: the clock's offset
    double m_kp;
    double m_ki;
    double m_bandwidth;
    double m_detectorGain;          //!< Of the normalised Gardner detector, per symbol of timing error
    double m_power;                 //!< Of the symbols, for the AGC
    bool m_powerValid;
    std::vector<Cf> m_outputs;      //!< Scratch
    double m_inputPowerSum;
    uint64_t m_inputPowerCount;

    // Equaliser: the spectrum averaged while locked, and the power gain (dB) applied at each bin's frequency from the
    // signal's centre (empty for none)
    std::atomic<bool> m_eqEnabled;
    bool m_eqLocked;
    double m_eqEsN0;
    std::vector<double> m_eqPower;
    int m_eqBlocks;
    std::vector<double> m_eqGainDb;

    Status m_status;
};

} // namespace dvbs

#endif // MODEMDVB_DVBSFRONTEND_H
