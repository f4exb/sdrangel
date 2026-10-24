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

#ifndef INCLUDE_ACARSIRIDIUM_H
#define INCLUDE_ACARSIRIDIUM_H

#include <QDebug>

#include <complex>
#include <cmath>
#include <cstdint>
#include <deque>
#include <algorithm>
#include <vector>

#include "acarsiridiumfft.h"

// Iridium L band burst receiver, physical layer.
//
// Iridium is unlike the other protocols this plugin decodes. ACARS, VDL-2 and HFDL are
// continuous narrowband channels, and Inmarsat Aero is a geostationary satellite whose
// Doppler barely moves. Iridium is burst TDMA from satellites in low earth orbit:
//
//  - bursts of about 8.3 ms in a 90 ms frame, so the receiver sees silence most of the
//    time and has nothing to hold a lock on between them
//  - DQPSK at 25 kbaud, an order of magnitude faster than the Aero MSK rates
//  - low earth orbit gives roughly 37 kHz of Doppler either way, and it moves while the
//    burst is being received, so every burst needs its own frequency estimate
//
// This stage finds bursts, demodulates them to symbols, and hands the bit stream from the
// unique word onwards to AcarsIridiumFrameDecoder in acarsiridiumframe.h, which does
// frame classification, BCH and the field decoding.
//
// The constants and the processing order follow gr-iridium, which is the reference this
// was checked against: coarse frequency from the preamble, fine frequency from the whole
// burst, root raised cosine matched filter, symbol timing, then differential demodulation.

// Five samples per symbol at 25 kbaud. One Iridium channel is 41.67 kHz wide, so 125 kHz
// leaves room either side for a burst that Doppler has pushed off its nominal frequency.
//
// The rate is what it is because of the channelizer in acarsiridiumchannelizer.h, which
// scans the whole band: its output rate is four times its channel spacing, and 31.25 kHz
// spacing - close enough to the 41.67 kHz Iridium grid that every channel is covered -
// gives 125 kHz out. That it also divides the symbol rate exactly is what makes 31.25 kHz
// the spacing to pick
#define ACARSIRIDIUM_CHANNEL_SAMPLE_RATE    125000
#define ACARSIRIDIUM_SYMBOL_RATE            25000
#define ACARSIRIDIUM_SAMPLES_PER_SYMBOL     (ACARSIRIDIUM_CHANNEL_SAMPLE_RATE / ACARSIRIDIUM_SYMBOL_RATE)

// A burst is nominally 8.28 ms, which is about 207 symbols, and a simplex burst about
// 20.6 ms. Accept a generous range either side: anything far outside it is interference
#define ACARSIRIDIUM_MIN_BURST_SYMBOLS      40
#define ACARSIRIDIUM_MAX_BURST_SYMBOLS      640

// From gr-iridium lib/iridium.h, which is the reference these were checked against.
// The unique word was then found at zero symbol error in 11 of 18 bursts of a recording
// of the ring alert channel, at symbol 66 every time - the 64 symbol preamble plus the
// two samples the burst detector picks up ahead of it
#define ACARSIRIDIUM_UW_LENGTH              12
#define ACARSIRIDIUM_PREAMBLE_SHORT         16      // Duplex
#define ACARSIRIDIUM_PREAMBLE_LONG          64      // Simplex, above 1626 MHz

// How far either side of the expected position the unique word is looked for, and how
// many of its symbols may be wrong before the burst is treated as having no sync.
//
// The window has to be tight, because the search is over two preamble lengths, two
// directions and four rotations, which is a lot of chances to match by luck: allowing two
// errors over a 24 symbol window gives roughly a one in ten chance of a false match on
// any burst, measured on a traffic channel recording where it produced 18 spurious syncs
// at scattered positions against 12 real ones. Over 8 symbols it is below one in a
// hundred, and a false sync is then thrown out by the BCH check in the frame layer, which
// is a far stronger test than the unique word - which is why two errors are allowed here.
// On a ring alert recording the bursts that need the second error are the ones at 10 to
// 18 dB; the strong ones match with no errors at all
#define ACARSIRIDIUM_UW_SEARCH              8
#define ACARSIRIDIUM_UW_MAX_ERRORS          2

// Root raised cosine matched filter, as gr-iridium designs it in burst_downmix_impl.cc:
//
//   root_raised_cosine(1.0, d_output_sample_rate, SYMBOLS_PER_SECOND, .4, 51)
//
// Without it the preamble still demodulates perfectly, because it is one symbol repeated
// and so has no transitions to smear, but the data symbols that follow are corrupted by
// intersymbol interference. That showed up as bursts whose preamble was a clean run of
// zero differential symbols and whose unique word then failed to match
#define ACARSIRIDIUM_RRC_ALPHA              0.4
#define ACARSIRIDIUM_RRC_TAPS               51

// How much of the burst the coarse frequency estimate uses, in symbols. gr-iridium sizes
// its estimation FFT as PREAMBLE_LENGTH_SHORT + 10, which covers the whole short preamble
// plus the first symbols of the unique word, and for a long preamble is all preamble. The
// unique word is only symbols 0 and 2, so squaring the signal removes it as well as the
// preamble, leaving a single tone at twice the frequency offset
#define ACARSIRIDIUM_CFO_SYMBOLS            (ACARSIRIDIUM_PREAMBLE_SHORT + 10)

class AcarsIridiumReceiver
{
public:
    typedef std::complex<double> Cd;

    enum Direction
    {
        DirectionUnknown = 0,
        DirectionDownlink,              //!< Satellite to ground, which is what is heard
        DirectionUplink
    };

    struct Burst
    {
        qint64 m_sampleIndex = 0;       //!< Where the burst started, in channel samples
        double m_frequencyOffset = 0.0; //!< Hz from the centre of the channel, mostly Doppler
        double m_coarseOffset = 0.0;    //!< The preamble only estimate, before refinement
        double m_snrDb = 0.0;
        double m_powerDb = 0.0;
        int m_symbolCount = 0;
        std::vector<uint8_t> m_symbols; //!< Differential symbol, 0 to 3, rotation not removed

        // Filled in when the unique word is found
        Direction m_direction = DirectionUnknown;
        int m_uwPosition = -1;          //!< Index in m_symbols where the unique word starts
        int m_uwErrors = 0;
        int m_rotation = 0;             //!< Quarter turns of residual frequency error removed
        //!< The frame, as the bits the frame layer works on: two per differential symbol,
        //!< Gray coded, starting with the 24 bit access code. Empty when there is no sync
        std::vector<uint8_t> m_frameBits;
    };

    AcarsIridiumReceiver()
    {
        designRrc();
        reset();
    }

    void reset()
    {
        m_sampleIndex = 0;
        m_burstStart = 0;
        m_noiseFloor = 0.0;
        m_noiseValid = false;
        m_inBurst = false;
        m_belowCount = 0;
        m_aboveCount = 0;
        m_smoothedPower = 0.0;
        m_preTrigger.assign(m_preTriggerLength, Cd(0.0, 0.0));
        m_preTriggerWrite = 0;
        m_preTriggerCount = 0;
        m_burstSamples.clear();

        // Reserved once rather than grown: the band scanner runs one of these per channel,
        // so a reallocation here is a reallocation a couple of hundred times over
        m_burstSamples.reserve(ACARSIRIDIUM_MAX_BURST_SYMBOLS * ACARSIRIDIUM_SAMPLES_PER_SYMBOL);
        m_bursts.clear();
        m_burstCount = 0;
        m_rejectedCount = 0;
    }

    //! Two bits per differential symbol, Gray coded. The de_dqpsk() function in
    //! iridium-toolkit reads the bit pair as a number and maps it through
    //! imap[] = { 0, 1, 3, 2 } to recover the symbol, and that map is its own inverse, so
    //! the same table encodes. It is what makes the differential form of the downlink
    //! unique word come out as the access code "001100000011000011110011"
    static uint8_t grayMap(uint8_t symbol)
    {
        static const uint8_t imap[4] = { 0, 1, 3, 2 };

        return imap[symbol & 3];
    }

    //! Feeds one channel sample. Returns true when a burst has been completed and is
    //! waiting to be popped
    bool processSample(const Cd& sample)
    {
        const double power = std::norm(sample);

        // Short smoothing, so one noisy sample neither starts nor ends a burst
        m_smoothedPower += (power - m_smoothedPower) * 0.25;

        if (!m_inBurst)
        {
            // The noise floor only tracks while nothing is being received: a long burst
            // would otherwise drag it up and the burst would end early
            if (!m_noiseValid)
            {
                m_noiseFloor = m_smoothedPower;
                m_noiseValid = true;
            }
            else
            {
                m_noiseFloor += (m_smoothedPower - m_noiseFloor) * 0.0001;
            }

            if (m_smoothedPower > (m_noiseFloor * m_startThreshold))
            {
                m_aboveCount++;

                if (m_aboveCount >= m_startSamples)
                {
                    m_inBurst = true;
                    m_belowCount = 0;
                    m_burstStart = m_sampleIndex - (qint64) m_preTriggerCount;
                    m_burstSamples.clear();

                    // The samples from before the detector fired are part of the burst,
                    // and the preamble they hold is what the frequency estimate needs.
                    // Oldest first, which with a ring is wherever the next write would go
                    for (int i = 0; i < m_preTriggerCount; i++)
                    {
                        const int index = (m_preTriggerWrite + m_preTriggerLength - m_preTriggerCount + i)
                                        % m_preTriggerLength;

                        m_burstSamples.push_back(m_preTrigger[index]);
                    }
                }
            }
            else
            {
                m_aboveCount = 0;
            }
        }
        else
        {
            m_burstSamples.push_back(sample);

            if (m_smoothedPower < (m_noiseFloor * m_endThreshold))
            {
                m_belowCount++;

                if (m_belowCount >= m_endSamples)
                {
                    endBurst();
                    m_inBurst = false;
                    m_aboveCount = 0;
                }
            }
            else
            {
                m_belowCount = 0;
            }

            if ((int) m_burstSamples.size() > (ACARSIRIDIUM_MAX_BURST_SYMBOLS * ACARSIRIDIUM_SAMPLES_PER_SYMBOL))
            {
                // Not a burst: something is transmitting continuously, so drop it and
                // let the noise floor re-converge rather than growing the buffer for ever
                m_burstSamples.clear();
                m_inBurst = false;
                m_aboveCount = 0;
                m_noiseValid = false;
                m_rejectedCount++;
            }
        }

        m_preTrigger[m_preTriggerWrite] = sample;
        m_preTriggerWrite = (m_preTriggerWrite + 1) % m_preTriggerLength;

        if (m_preTriggerCount < m_preTriggerLength) {
            m_preTriggerCount++;
        }

        m_sampleIndex++;

        return !m_bursts.empty();
    }

    bool hasBurst() const { return !m_bursts.empty(); }

    Burst popBurst()
    {
        Burst burst = m_bursts.front();
        m_bursts.pop_front();
        return burst;
    }

    int burstCount() const { return m_burstCount; }
    int rejectedCount() const { return m_rejectedCount; }
    double noiseFloor() const { return m_noiseFloor; }

private:

    //! Root raised cosine taps, by the root_raised_cosine function in GNU Radio firdes,
    //! so the matched filter is the same one gr-iridium uses
    void designRrc()
    {
        const int ntaps = ACARSIRIDIUM_RRC_TAPS | 1;
        const double alpha = ACARSIRIDIUM_RRC_ALPHA;
        const double spb = (double) ACARSIRIDIUM_SAMPLES_PER_SYMBOL;

        m_rrc.resize(ntaps);
        double scale = 0.0;

        for (int i = 0; i < ntaps; i++)
        {
            const double index = (double) (i - (ntaps / 2));
            const double x1 = M_PI * index / spb;
            double x2 = 4.0 * alpha * index / spb;
            double x3 = (x2 * x2) - 1.0;
            double num;
            double den;

            if (std::fabs(x3) >= 0.000001)
            {
                if (i != (ntaps / 2)) {
                    num = std::cos((1.0 + alpha) * x1) + (std::sin((1.0 - alpha) * x1) / (4.0 * alpha * index / spb));
                } else {
                    num = std::cos((1.0 + alpha) * x1) + ((1.0 - alpha) * M_PI / (4.0 * alpha));
                }

                den = x3 * M_PI;
            }
            else
            {
                // The removable singularity, where index is plus or minus spb / (4 alpha)
                x3 = (1.0 - alpha) * x1;
                x2 = (1.0 + alpha) * x1;
                num = (std::sin(x2) * (1.0 + alpha) * M_PI)
                    - (std::cos(x3) * ((1.0 - alpha) * M_PI * spb) / (4.0 * alpha * index))
                    + (std::sin(x3) * spb * spb / (4.0 * alpha * index * index));
                den = -32.0 * M_PI * alpha * alpha * index / spb;
            }

            m_rrc[i] = 4.0 * alpha * num / den;
            scale += m_rrc[i];
        }

        for (int i = 0; i < ntaps; i++) {
            m_rrc[i] /= scale;
        }
    }

    //! Matched filter, with the group delay taken out so an output sample lines up with
    //! the input sample of the same index and the unique word stays where it was found
    std::vector<Cd> matchedFilter(const std::vector<Cd>& samples) const
    {
        const int ntaps = (int) m_rrc.size();
        const int centre = ntaps / 2;
        const int length = (int) samples.size();
        std::vector<Cd> out(length);

        for (int i = 0; i < length; i++)
        {
            Cd sum(0.0, 0.0);

            for (int k = 0; k < ntaps; k++)
            {
                const int index = i + k - centre;

                if ((index >= 0) && (index < length)) {
                    sum += samples[index] * m_rrc[k];
                }
            }

            out[i] = sum;
        }

        return out;
    }

    //! Magnitude of the tone at power times offset, which is how the fine frequency
    //! estimate below scores a candidate. The phasor is stepped rather than recomputed,
    //! because this is searched over hundreds of candidates for every burst
    double toneMagnitude(const std::vector<Cd>& raised, int power, double offset, int length) const
    {
        const double w = -2.0 * M_PI * ((double) power * offset) / ACARSIRIDIUM_CHANNEL_SAMPLE_RATE;
        const Cd step = std::exp(Cd(0.0, w));
        Cd phasor(1.0, 0.0);
        Cd sum(0.0, 0.0);

        for (int i = 0; i < length; i++)
        {
            sum += raised[i] * phasor;
            phasor *= step;
        }

        return std::abs(sum);
    }

    //! Where the burst actually sits in the channel, from its power spectrum.
    //!
    //! This exists only to settle which alias the estimates below have landed on, and is
    //! the one step in the chain that cannot be ambiguous. Raising the signal to a power
    //! to strip the modulation also folds the frequency axis: squaring leaves the offset
    //! known only modulo half the sample rate, and the fourth power modulo a quarter of
    //! it. Neither fold matters to differential demodulation, because an offset that is a
    //! whole number of symbol rates advances the phase by a whole turn per symbol, which
    //! is why this went unnoticed until the matched filter was added. It matters a great
    //! deal to the matched filter, which would otherwise be centred up to 50 kHz away
    //! from the signal - at this sample rate, on the far side of Nyquist.
    //!
    //! The score is the power a matched filter centred on the candidate would pass, so
    //! the whole occupied bandwidth votes rather than any single bin
    double estimateFrequencyCentre(const std::vector<Cd>& samples)
    {
        const int length = (int) samples.size();
        const double rate = ACARSIRIDIUM_CHANNEL_SAMPLE_RATE;

        // The spectrum from a transform rather than a bin at a time. Scoring each
        // candidate separately meant a pass over the whole burst per candidate, which for
        // a few hundred candidates is most of the cost of handling a burst - and with the
        // whole band being scanned there are a great many bursts
        const int size = AcarsIridiumFft::nextPowerOfTwo(std::max(length, 1024));
        m_fft.configure(size);

        std::vector<Cd> spectrum(size, Cd(0.0, 0.0));

        for (int i = 0; i < length; i++) {
            spectrum[i] = samples[i];
        }

        m_fft.transform(spectrum);

        // A 25 kbaud signal shaped at alpha 0.4 occupies about 35 kHz, so a filter centred
        // on the signal passes roughly 17.5 kHz either side. The score for a candidate is
        // how much power such a filter would pass, so the whole occupied bandwidth votes
        const double binHz = rate / (double) size;
        const int half = (int) std::lround(17500.0 / binHz);

        // Running totals, so each candidate costs a subtraction rather than a sweep over
        // its whole bandwidth
        std::vector<double> prefix(size + 1, 0.0);

        for (int k = 0; k < size; k++) {
            prefix[k + 1] = prefix[k] + std::norm(spectrum[k]);
        }

        const double total = prefix[size];
        int bestBin = 0;
        double bestPower = -1.0;

        for (int c = 0; c < size; c++)
        {
            const int from = c - half;
            const int to = c + half + 1;
            double sum;

            if ((from >= 0) && (to <= size))
            {
                sum = prefix[to] - prefix[from];
            }
            else
            {
                // The spectrum is periodic at the sample rate, so a candidate near the
                // edge takes the rest of its bandwidth from the other end
                const int lo = ((from % size) + size) % size;
                const int hi = ((to % size) + size) % size;

                sum = (total - prefix[lo]) + prefix[hi];
            }

            if (sum > bestPower)
            {
                bestPower = sum;
                bestBin = c;
            }
        }

        return ((bestBin < (size / 2)) ? (double) bestBin : (double) (bestBin - size)) * binHz;
    }

    //! Fine frequency from the whole burst, raised to the fourth power so the QPSK data
    //! cancels as well as the preamble, and read off a transform of the result.
    //!
    //! The fourth power puts a tone at four times the frequency offset, and its peak is
    //! only about 1/T wide - some tens of Hz for a burst this long. Searching for it by
    //! scoring candidate offsets one at a time therefore needs a step of a few Hz over
    //! the whole range, which is thousands of passes over the burst: with a few hundred
    //! channels being scanned at once that was most of the time the scanner spent, and
    //! coarsening the step simply steps over the peak and misses it. A transform gives
    //! every candidate at once instead.
    //!
    //! The result is still ambiguous - a tone at four times the offset repeats every
    //! quarter of the sample rate - so the answer taken is the one nearest the coarse
    //! estimate, which is what that estimate is for
    double estimateFrequencyFine(const std::vector<Cd>& samples, double coarse)
    {
        const int length = (int) samples.size();
        const double rate = ACARSIRIDIUM_CHANNEL_SAMPLE_RATE;

        // Sized to the burst rather than fixed. A burst defines its own frequency only to
        // about 1 / (4T), which is 30 Hz for a short duplex burst, and the bin spacing
        // here is rate / (4 * size) - so a transform the length of the burst already
        // resolves it as finely as it can be resolved, and the interpolation below picks
        // up the rest. Padding every burst out to a fixed large size was eight times the
        // work for the short ones, which are most of them
        const int size = AcarsIridiumFft::nextPowerOfTwo(std::max(length, 1024));
        m_fft.configure(size);

        std::vector<Cd> fourth(size, Cd(0.0, 0.0));

        for (int i = 0; i < length; i++)
        {
            const Cd s2 = samples[i] * samples[i];
            fourth[i] = s2 * s2;
        }

        m_fft.transform(fourth);

        // One alias of the fourth power tone every quarter of the sample rate
        const double aliasHz = rate / 4.0;
        const double searchHz = aliasHz / 2.0;

        int bestBin = -1;
        double bestMagnitude = -1.0;
        double best = coarse;

        for (int k = 0; k < size; k++)
        {
            // Bin to the frequency of the tone, then to the offset that produced it
            const double tone = ((k < (size / 2)) ? (double) k : (double) (k - size)) * rate / (double) size;
            double offset = tone / 4.0;

            // Slide to whichever alias sits nearest the coarse estimate
            offset += std::round((coarse - offset) / aliasHz) * aliasHz;

            if (std::fabs(offset - coarse) > searchHz) {
                continue;
            }

            const double magnitude = std::norm(fourth[k]);

            if (magnitude > bestMagnitude)
            {
                bestMagnitude = magnitude;
                bestBin = k;
                best = offset;
            }
        }

        if (bestBin < 0) {
            return coarse;
        }

        // Parabolic interpolation between the neighbouring bins, so the answer is not
        // limited to the bin spacing
        const double left = std::abs(fourth[(bestBin - 1 + size) % size]);
        const double centre = std::abs(fourth[bestBin]);
        const double right = std::abs(fourth[(bestBin + 1) % size]);
        const double denominator = left - (2.0 * centre) + right;

        if (std::fabs(denominator) > 1e-20)
        {
            const double shift = 0.5 * (left - right) / denominator;

            if (std::fabs(shift) <= 1.0) {
                best += shift * rate / (4.0 * (double) size);
            }
        }

        return best;
    }

    //! Symbol timing by Oerder and Meyr: the squared magnitude carries a line at the
    //! symbol rate, and its phase is the timing offset within a symbol.
    //!
    //! Mind the sign. If the squared magnitude peaks a fraction tau of the way into each
    //! symbol, the sum below comes out proportional to e^(-j 2 pi tau / sps), so the
    //! offset is MINUS its phase. Taking the phase as it stands gives -tau mod sps, which
    //! is only right when tau happens to be zero or half a symbol - anywhere else it
    //! samples as far the wrong side of the peak as the peak is from the symbol start
    double estimateTiming(const std::vector<Cd>& samples) const
    {
        const int sps = ACARSIRIDIUM_SAMPLES_PER_SYMBOL;
        Cd sum(0.0, 0.0);

        // The phase only takes as many values as there are samples in a symbol, so a
        // small table replaces a transcendental per sample
        Cd rotations[ACARSIRIDIUM_SAMPLES_PER_SYMBOL];

        for (int i = 0; i < sps; i++)
        {
            const double w = -2.0 * M_PI * (double) i / (double) sps;

            rotations[i] = Cd(std::cos(w), std::sin(w));
        }

        int slot = 0;

        for (size_t i = 0; i < samples.size(); i++)
        {
            sum += std::norm(samples[i]) * rotations[slot];

            if (++slot == sps) {
                slot = 0;
            }
        }

        // Negated, as above, then wrapped into [0, 2 pi) so the offset is in [0, sps)
        double phase = -std::arg(sum);

        if (phase < 0.0) {
            phase += 2.0 * M_PI;
        }

        return (phase / (2.0 * M_PI)) * (double) sps;
    }

    //! The unique words, as gr-iridium gives them: absolute symbols, only 0 and 2, so
    //! the sync is effectively BPSK within the QPSK constellation
    static const uint8_t *uniqueWord(Direction direction)
    {
        static const uint8_t downlink[ACARSIRIDIUM_UW_LENGTH] = { 0, 2, 2, 2, 2, 0, 0, 0, 2, 0, 0, 2 };
        static const uint8_t uplink[ACARSIRIDIUM_UW_LENGTH]   = { 2, 2, 0, 0, 0, 2, 0, 0, 2, 0, 2, 2 };

        return (direction == DirectionUplink) ? uplink : downlink;
    }

    //! Looks for a unique word in the differential symbols and, when one is found, emits
    //! the frame bit stream from it onwards.
    //!
    //! The search allows for a residual frequency error of a multiple of a quarter of the
    //! symbol rate, which leaves a fixed quarter turn on every symbol and so adds a
    //! constant to every differential symbol. Trying all four rotations resolves it
    void findUniqueWord(Burst& burst) const
    {
        if (burst.m_symbolCount < (ACARSIRIDIUM_PREAMBLE_SHORT + ACARSIRIDIUM_UW_LENGTH)) {
            return;
        }

        // A simplex burst carries the long preamble, a duplex burst the short one. Which
        // this is depends on the frequency, which the receiver does not know, so both
        // are looked for
        const int expected[2] = { ACARSIRIDIUM_PREAMBLE_LONG, ACARSIRIDIUM_PREAMBLE_SHORT };

        int bestErrors = ACARSIRIDIUM_UW_MAX_ERRORS + 1;
        int bestPosition = -1;
        int bestRotation = 0;
        Direction bestDirection = DirectionUnknown;

        for (int d = 0; d < 2; d++)
        {
            const Direction direction = (d == 0) ? DirectionDownlink : DirectionUplink;
            const uint8_t *uw = uniqueWord(direction);

            // The differential form of the unique word, which is what the symbols are
            uint8_t wanted[ACARSIRIDIUM_UW_LENGTH - 1];

            for (int i = 1; i < ACARSIRIDIUM_UW_LENGTH; i++) {
                wanted[i-1] = (uint8_t) ((4 + uw[i] - uw[i-1]) % 4);
            }

            for (int p = 0; p < 2; p++)
            {
                for (int offset = -ACARSIRIDIUM_UW_SEARCH; offset <= ACARSIRIDIUM_UW_SEARCH; offset++)
                {
                    // One differential symbol before the unique word is part of the access
                    // code, so the frame cannot start at symbol zero
                    const int start = expected[p] + offset;

                    if ((start < 1) || ((start + ACARSIRIDIUM_UW_LENGTH - 1) > burst.m_symbolCount)) {
                        continue;
                    }

                    for (int rotation = 0; rotation < 4; rotation++)
                    {
                        int errors = 0;

                        for (int i = 0; i < (ACARSIRIDIUM_UW_LENGTH - 1); i++)
                        {
                            if (burst.m_symbols[start + i] != ((wanted[i] + rotation) % 4))
                            {
                                errors++;

                                if (errors >= bestErrors) {
                                    break;
                                }
                            }
                        }

                        if (errors < bestErrors)
                        {
                            bestErrors = errors;
                            bestPosition = start;
                            bestRotation = rotation;
                            bestDirection = direction;
                        }
                    }
                }
            }
        }

        if (bestPosition < 0) {
            return;
        }

        burst.m_direction = bestDirection;
        burst.m_uwPosition = bestPosition;
        burst.m_uwErrors = bestErrors;
        burst.m_rotation = bestRotation;

        // The access code is the twelve differential symbols ending with the eleven of
        // the unique word. The one before them is the step from the last preamble symbol
        // into the unique word, and because the preamble is symbol 0 repeated and the
        // downlink unique word also starts at symbol 0 it is a step of nothing - which is
        // why the access code begins with two zero bits
        for (int i = bestPosition - 1; i < burst.m_symbolCount; i++)
        {
            const uint8_t symbol = (uint8_t) ((4 + burst.m_symbols[i] - bestRotation) % 4);
            const uint8_t mapped = grayMap(symbol);

            burst.m_frameBits.push_back((uint8_t) ((mapped >> 1) & 1));
            burst.m_frameBits.push_back((uint8_t) (mapped & 1));
        }
    }

    void endBurst()
    {
        const int sps = ACARSIRIDIUM_SAMPLES_PER_SYMBOL;
        const int symbols = (int) m_burstSamples.size() / sps;

        if ((symbols < ACARSIRIDIUM_MIN_BURST_SYMBOLS) || (symbols > ACARSIRIDIUM_MAX_BURST_SYMBOLS))
        {
            m_rejectedCount++;
            m_burstSamples.clear();
            return;
        }

        Burst burst;
        burst.m_sampleIndex = m_burstStart;
        burst.m_coarseOffset = estimateFrequencyCentre(m_burstSamples);
        burst.m_frequencyOffset = estimateFrequencyFine(m_burstSamples, burst.m_coarseOffset);

        // De-rotate by the estimated offset, so what is left is the modulation
        std::vector<Cd> corrected(m_burstSamples.size());
        const double w = -2.0 * M_PI * burst.m_frequencyOffset / ACARSIRIDIUM_CHANNEL_SAMPLE_RATE;
        double power = 0.0;

        // Stepped rather than computed per sample: this used to be a transcendental for
        // every sample of every burst, and when the whole band is scanned there are a
        // great many bursts. The phasor is renormalised every so often, because repeated
        // multiplication lets its magnitude drift
        const Cd step(std::cos(w), std::sin(w));
        Cd phasor(1.0, 0.0);

        for (size_t i = 0; i < m_burstSamples.size(); i++)
        {
            corrected[i] = m_burstSamples[i] * phasor;
            power += std::norm(m_burstSamples[i]);
            phasor *= step;

            if ((i & 0xff) == 0xff) {
                phasor /= std::abs(phasor);
            }
        }
        power /= (double) m_burstSamples.size();

        burst.m_powerDb = 10.0 * std::log10(power + 1e-20);
        burst.m_snrDb = 10.0 * std::log10((power / (m_noiseFloor + 1e-20)) + 1e-20);

        // Matched filter, now the burst is centred. Doing it after the de-rotation rather
        // than before is what lets the filter be centred on the signal
        const std::vector<Cd> filtered = matchedFilter(corrected);

        const double timing = estimateTiming(filtered);

        // Differential demodulation: each symbol is the phase STEP from the one before,
        // which is what lets a burst be read without ever knowing absolute phase, and is
        // why the frequency estimate above only has to be good enough to stop the phase
        // walking within a single symbol
        Cd previous(0.0, 0.0);
        bool havePrevious = false;

        for (int s = 0; s < symbols; s++)
        {
            const int index = (int) std::lround((double) (s * sps) + timing);

            if ((index < 0) || (index >= (int) filtered.size())) {
                continue;
            }

            const Cd sample = filtered[index];

            if (havePrevious)
            {
                const Cd difference = sample * std::conj(previous);
                double angle = std::arg(difference);

                if (angle < 0.0) {
                    angle += 2.0 * M_PI;
                }

                const int quadrant = ((int) std::floor((angle + (M_PI / 4.0)) / (M_PI / 2.0))) & 3;

                burst.m_symbols.push_back((uint8_t) quadrant);
            }

            previous = sample;
            havePrevious = true;
        }

        burst.m_symbolCount = (int) burst.m_symbols.size();
        findUniqueWord(burst);

        if (burst.m_symbolCount >= ACARSIRIDIUM_MIN_BURST_SYMBOLS)
        {
            m_bursts.push_back(burst);
            m_burstCount++;
        }
        else
        {
            m_rejectedCount++;
        }

        m_burstSamples.clear();
    }

    // Detection thresholds, as ratios against the noise floor. The end one is lower than
    // the start one so a fade within a burst does not chop it in two
    const double m_startThreshold = 8.0;
    const double m_endThreshold = 3.0;
    const int m_startSamples = 8;
    const int m_endSamples = 16;
    const int m_preTriggerLength = 16;

    std::vector<double> m_rrc;
    AcarsIridiumFft m_fft;

    qint64 m_sampleIndex;
    qint64 m_burstStart;
    double m_noiseFloor;
    bool m_noiseValid;
    double m_smoothedPower;
    bool m_inBurst;
    int m_aboveCount;
    int m_belowCount;

    std::vector<Cd> m_preTrigger;
    int m_preTriggerWrite;
    int m_preTriggerCount;
    std::vector<Cd> m_burstSamples;
    std::deque<Burst> m_bursts;

    int m_burstCount;
    int m_rejectedCount;
};

#endif // INCLUDE_ACARSIRIDIUM_H
