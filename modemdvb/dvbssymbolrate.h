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

#ifndef MODEMDVB_DVBSSYMBOLRATE_H
#define MODEMDVB_DVBSSYMBOLRATE_H

#include <cstdint>
#include <vector>

#include "export.h"
#include "dvbs.h"

namespace dvbs {

//! Finds a DVB-S or S2 signal's symbol rate without knowing it. The averaged spectrum gives the strongest signal's
//! half power width, which for a root raised cosine pulse is the symbol rate, to a few percent; then the spectrum of
//! the squared magnitude has a line at the symbol rate (for any roll-off above 0, whatever the carrier's frequency),
//! found near that, to tens of ppm: well within what the receivers' timing loops pull in
class MODEMDVB_API SymbolRateSearch
{
public:
    //! A signal in the spectrum: where, and how wide (Hz, from the input's centre), and its level over the noise
    struct Signal
    {
        double m_frequency = 0.0;
        double m_width = 0.0;           //!< Half power
        double m_levelDb = 0.0;
    };

    struct Result
    {
        bool m_found = false;
        double m_symbolRate = 0.0;      //!< S/s
        double m_frequency = 0.0;       //!< Hz, of the signal's centre from the channel's
        double m_width = 0.0;           //!< Hz, half power
        double m_lineSnr = 0.0;         //!< dB, of the line over the squared magnitude's spectrum near it
        std::vector<Signal> m_signals;  //!< All those in the spectrum (3 dB or more over the noise), in frequency order
        //! The averaged power spectrum, smoothed (spurs gone), centred: bin i at (i - size / 2) * m_binHz
        std::vector<float> m_spectrum;
        double m_binHz = 0.0;
        double m_minWidth = 0.0;        //!< Hz: signals narrower aren't taken
    };

    //! Symbol rates from minRate to maxRate are looked for
    void configure(double inputRate, double minRate, double maxRate);
    //! Only the signal in this band (Hz from the input's centre) is measured: the strongest there, its line from the
    //! input filtered to the band. A width of 0 for the whole input (the default). Kept through reset() and configure().
    //! Searches again, after any holdoff left (as a retune's)
    void setBand(double frequency, double width);
    //! Only the spectrum and its signals, no rate (as for a scan's sweep, which then costs little). Kept as setBand()
    void setSpectrumOnly(bool spectrumOnly) { m_spectrumOnly = spectrumOnly; }
    //! Searching again, after holdoffSeconds of samples (as after a retune, while the samples before it still come)
    void reset(double holdoffSeconds = 0.0);
    //! True when a search ends (each searchSamples samples), found or not
    bool process(const Cf *samples, int nbSamples);
    const Result& result() const { return m_result; }
    //! The signals in a smoothed power spectrum (as m_spectrum, or several stitched together): each run over times
    //! floor (3 dB by default), minWidth or wider at half power, in frequency order. Bin i at origin + i * binHz
    static std::vector<Signal> findSignals(const std::vector<double>& smooth, double floor, double binHz, double origin,
        double minWidth, double over = 2.0);

    static const int searchSamples = 1 << 18;

private:
    static constexpr double m_pi = 3.141592653589793;
    static constexpr int m_spectrumSize = 4096;             //!< FFT of the averaged spectrum
    static constexpr double m_minLevel = 1.25;              //!< In-band level over the floor, for a signal to be measured
    static constexpr double m_minLineSnr = 10.0;            //!< dB, for the line to be believed
    static constexpr double m_searchLow = 0.8;              //!< The line's looked for from this times the half power width
    static constexpr double m_searchHigh = 1.5;             //!< to this: the width's narrowed when the device's filter cuts
                                                            //!< the edges, as at 1.25 times the symbol rate (27.5 MS/s 0.76 of it)
    static constexpr double m_snapTolerance = 2e-4;         //!< A common rate this close is taken as the rate
    static constexpr double m_retrySeconds = 0.2;           //!< Between searches that find nothing, so one taking longer than its
                                                            //!< samples take to arrive (at high sample rates) doesn't hold up the
                                                            //!< caller for long
    static const double m_commonRates[20];                  //!< Symbol rates in common use

    void search();
    int zeroRunSamples() const;
    static const std::vector<double>& hann(std::vector<double>& window, int n);

    double m_inputRate = 0.0;
    double m_minRate = 0.0;
    double m_maxRate = 0.0;
    double m_bandFrequency = 0.0;
    double m_bandWidth = 0.0;
    bool m_spectrumOnly = false;
    std::vector<Cf> m_buffer;
    Result m_result;
    int64_t m_holdoff = 0;      //!< Samples skipped before the next search
    FFT m_fft;                  //!< Its twiddles kept from search to search (on the caller's thread, at up to 2^19 points)
    std::vector<double> m_spectrumWindow;
    std::vector<double> m_lineWindow;   //!< Of the squared magnitude, searchSamples or twice that
};

} // namespace dvbs

#endif // MODEMDVB_DVBSSYMBOLRATE_H
