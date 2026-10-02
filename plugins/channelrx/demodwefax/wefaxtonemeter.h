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

#ifndef INCLUDE_WEFAXTONEMETER_H
#define INCLUDE_WEFAXTONEMETER_H

#include <complex>
#include <vector>

#include "dsp/dsptypes.h"
#include "dsp/firfilter.h"

/**
 * Measures the black and white tone frequencies of a WEFAX signal for AFC.
 *
 * The FM discriminator is a poor tone estimator on weak signals: during fades
 * it follows noise or any weak carrier in the passband. An averaged power
 * spectrum integrates coherently instead, so the tones stand far above the
 * noise. The phasing signal (mostly black, with a short white pulse) and the
 * image (mostly white) give strong spectral lines at the tones. The start
 * tone gives a comb at multiples of its modulation rate instead, so the black
 * and white peaks are chosen as a pair one FM shift apart, within 8%.
 */
class WefaxToneMeter
{
public:
    struct Result
    {
        bool valid = false;
        double lowHz = 0.0;     // Black tone, relative to the channel centre
        double highHz = 0.0;    // White tone
    };

    // shiftHz is the expected black to white separation.
    void configure(int sampleRate, double shiftHz);
    void reset();
    // Feed channel-filtered complex baseband at the configured rate. Returns
    // true when the spectrum has been updated and result() re-evaluated.
    bool feed(const Complex& sample);
    const Result& result() const { return m_result; }

private:
    static constexpr int FFTSize = 1024;

    int m_decimation = 1;
    double m_decimatedRate = 0.0;
    double m_shiftHz = 800.0;
    Lowpass<Complex> m_lowpass;
    int m_phase = 0;
    std::vector<std::complex<double>> m_block;
    std::vector<double> m_window;
    std::vector<double> m_spectrum;     // Decaying average power per bin
    double m_decay = 1.0;
    Result m_result;

    void evaluate();
    static void fft(std::vector<std::complex<double>>& data);
};

#endif // INCLUDE_WEFAXTONEMETER_H
