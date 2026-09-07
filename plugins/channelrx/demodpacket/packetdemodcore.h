///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020-2021, 2023 Jon Beniston, M7RCE <jon@beniston.com>          //
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

#ifndef INCLUDE_PACKETDEMODCORE_H
#define INCLUDE_PACKETDEMODCORE_H

// The MLSE demodulator: the parallel detectors, the per burst estimator that measures what
// the transmitter is actually doing, and the replay that decodes a burst again once that is
// known. Deliberately knows nothing about the channel, the device, the message queue or the
// settings class - it takes samples and a small configuration, and hands finished frames
// back through a callback.
//
// That is what lets the offline test harness run THIS code rather than a mirror of it. The
// mirror drifted: it had no Chase decoding, so every CPU figure measured against it was
// optimistic for the plugin, and two separate investigations drew the wrong conclusion from
// it before that was noticed.

#include <QByteArray>

#include <algorithm>
#include <cmath>
#include <complex>
#include <deque>
#include <functional>
#include <utility>
#include <vector>

#include <QDebug>

#include "dsp/afskmlse.h"
#include "dsp/dsptypes.h"
#include "dsp/firfilter.h"

#include "packetdemodframer.h"
#include "packetdemodsettings.h"
#include "packetdemodtonecorrelator.h"

typedef PacketDemodFramer::State PacketDemodDeframer;

class PacketDemodCore
{
public:
    // Everything the detector needs from the settings, so it does not depend on them
    struct Config
    {
        int m_chase = 6;
        bool m_mlse = true;
        int m_baudRate = 1200;

        // The waveform model the detector correlates against. The plugin leaves every one of
        // these at the value it ships and behaves exactly as before; they exist so the test
        // harness can sweep the model through THIS code.
        //
        // That is not a convenience. The harness's own offline path has equivalent options,
        // but it is markedly less sensitive - on weak_signal_2026-08-01 it returns nothing on
        // a burst the streaming path decodes, and manages only the 34 dB one - so a sweep run
        // through it measures the offline path's threshold rather than the model. A model
        // hypothesis can only be tested where the sensitivity is.
        double m_toneMark = 1200.0;
        double m_toneSpace = 2200.0;
        double m_deviation = PacketDemodSettings::PACKETDEMOD_FM_DEVIATION;
        int m_ramp = PacketDemodSettings::PACKETDEMOD_MLSE_RAMP;

        // Hold the model where it was put. The estimator and the learn-from-decodes paths
        // normally move the tones, deviation and symbol rate at runtime, which is right on
        // air and ruinous for a controlled sweep - the thing being varied does not stay
        // varied. Only the harness sets this.
        bool m_adapt = true;
    };

    // stamp is when the frame was TRANSMITTED, in channel samples - a replay reports a
    // burst's frames seconds after the live chains would have, and the two must still
    // compare equal for deduplication. Returns true if the caller accepted it as new.
    typedef std::function<bool (const QByteArray& packet, quint64 stamp)> PacketHandler;

    void setPacketHandler(const PacketHandler& handler) { m_onPacket = handler; }

    void applyConfig(const Config& cfg)
    {
        m_cfg = cfg;
        m_framer.setChaseDepth(cfg.m_chase);
        m_framer.setRequirePlausible(cfg.m_mlse);

        // The framer emits frames; what a decode means to the DETECTOR - the burst's live
        // decode count, and the tone pair learned from replayed frames - is decided here,
        // and only for frames the caller accepted as new.
        m_framer.setFrameHandler([this](const QByteArray& packet, bool viaChase) -> bool {
            (void) viaChase;

            if (!emitPacket(packet)) {
                return false;           // a duplicate of one already reported
            }

            if (!m_replayReportTime) {
                m_estBurstLiveDecodes++;
            } else {
                noteReplayDecodeTones();
                noteReplayDecodeRate();
            }

            return true;
        });

        // None of what follows is free to keep around for a detector that is switched
        // off: the sample buffer is 4 MB, every chain and every replay chain carries a
        // symbol history, and the branch waveform templates are ~100k trig calls to
        // build. Release it instead - processMlse returns immediately on no chains.
        if (!cfg.m_mlse)
        {
            releaseDetector();
            return;
        }

        // The estimator reads tone transitions from these, and buildChains builds them
        // through calibrateCorrelators. Without them push() returns false forever, so no
        // transition is ever seen, the activity gate never opens, the live chains are
        // never started and nothing decodes at all.
        m_correlationLength = PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE
                            / std::max(1, cfg.m_baudRate);

        buildChains();
    }

    const Config& config() const { return m_cfg; }

    // Chase depth is a framing policy, not a detector parameter. Routing it through
    // applyConfig would rebuild every chain and template and reset the burst estimator, so
    // moving the spin box mid-pass would cost the tuning the estimator had just learned.
    void setChaseDepth(int depth)
    {
        m_cfg.m_chase = depth;
        m_framer.setChaseDepth(depth);
    }

    // Channel samples seen. The sink timestamps its own discriminator decodes with this.
    quint64 sampleCount() const { return m_sampleCount; }

    // One channel sample, with the discriminator output and power the sink already has
    void processSample(const Complex& ci, Real fmDemod, double magsq)
    {
        processMlse(ci, fmDemod, magsq);
    }

private:
    // One MLSE per symbol timing and carrier hypothesis. Timing is searched rather than
    // tracked because a timing loop driven by a discriminator is exactly what stops working
    // at the sensitivity this is here to reach. Each chain reads the channel at its own
    // fractional symbol period, so the symbol rate measured from the air - real
    // transmitters run over a thousand ppm out, far past the detector's -100/+50 ppm
    // tolerance - can be applied without resampling the channel.
    struct MlseChain
    {
        AfskMlse m_mlse;
        PacketDemodDeframer m_deframer;
        std::vector<Complex> m_rotSample; // Carrier hypothesis within a symbol period
        Complex m_rotSymbol;        // ... and the advance from one symbol period to the next
        Complex m_phase;            // ... accumulated to the start of the current symbol
        double m_readPos;           // Where the current symbol period starts, in samples
        double m_period;            // Symbol period in channel samples
        double m_carrier;           // This chain's carrier hypothesis, to re-seed the rotator
        double m_toneMark;          // ... and its tone pair, so a CRC-valid frame says
        double m_toneSpace;         //     which modem standard the transmitter used
    };

    std::vector<MlseChain> m_chains;
    std::vector<Complex> m_bitBuf;  // Recent channel samples, circular
    quint64 m_sampleCount = 0;
    int m_samplesPerSymbol = PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE / 1200;

    // Burst replay. The estimator can only know a burst's tuning once the burst is over,
    // so the live chains always decode a burst with the PREVIOUS burst's tuning - on a
    // satellite pass, where the carrier moves 60 Hz/s, stale by hundreds of Hz after a
    // gap. The samples are still in the buffer, so when a burst produces a retune, replay
    // it through chains carrying the tuning it just yielded. Only the timing phases are
    // needed, not the carrier grid - the carrier is now measured - so a replay costs a
    // fraction of the live chains and is time sliced against the incoming stream rather
    // than stalling it. Measured by turning it off: 14 distinct payloads against 12 on the
    // NO-84 pass, and 5 against 0 on the V.23 recording - everything, there, because the
    // live chains only learn a non-Bell tone pair from frames the REPLAY decodes.

    // 14 seconds of channel baseband, about 4 MB at 8 bytes a sample. Sized by the SLOWEST
    // case the replay can run at, not by the longest burst: it is paced down to half real
    // time when its hypothesis grid is largest, so a 2 second burst takes about 4 seconds
    // of wall time to get through, and a buffer that laps it means the burst is abandoned
    // half decoded. At 6 seconds that cost a payload on weak_signal_2026-08-01 and 35% of
    // packets at 1000 ppm. Memory is far cheaper than either.
    static const int PACKETDEMOD_BUF_SECONDS = 14;
    static const int PACKETDEMOD_REPLAY_RATE = 3;     // Replay samples per live sample
    static const int PACKETDEMOD_REPLAY_TAIL = 80;    // Symbols past the burst, see below

    // When the symbol rate fit does not converge the replay searches a coarse rate grid
    // instead of assuming the transmitter is on frequency. The estimator's fit needs about
    // 10 dB CNR, so on a weak burst it silently reports zero error - and clock error is the
    // one mismatch the detector cannot absorb: measured at 4 dB SNR, 500 ppm costs 25% of
    // packets and 1000 ppm costs 80%, where a 900 Hz deviation error or a 370 Hz carrier
    // offset cost nothing. A +/-1000 ppm span in 500 ppm steps leaves at most 250 ppm
    // uncorrected, which is inside the flat part of that curve. Only the replay pays for
    // it; the live chains are untouched.
    static constexpr double PACKETDEMOD_REPLAY_RATE_SPAN = 1000.0;   // ppm, either side
    static constexpr double PACKETDEMOD_REPLAY_RATE_STEP = 500.0;    // ppm between rates

    // The live chains spend most of a satellite pass grinding noise - 95% of it on the
    // NO-84 recording - so they run only while the activity gate says there is something
    // to decode, plus a tail to flush the trellis traceback. They restart from BEFORE the
    // gate opened: the gate needs a few ms of signal to decide, and a detector handed the
    // frame from there has already lost the preamble it acquires on. Reading back into
    // the buffer costs nothing, the chains simply catch up within a millisecond.
    static const int PACKETDEMOD_LIVE_TAIL = 100;         // Symbols past the burst
    static constexpr double PACKETDEMOD_LIVE_LEAD = 0.08; // Seconds before the gate opened

    // Detectors are built once per model and copied, never built per chain. create()
    // numerically integrates the branch waveform tables - about 100k sin/cos - and doing
    // that once per replay chain stalled the baseband thread long enough to overflow the
    // sample FIFO. Copying now shares the tables, so it is a reference count.
    AfskMlse m_mlseTemplate[2];         // [0] coherent, [1] differential
    bool m_templateValid = false;
    double m_templatePeriodDev[4];      // devMark, devSpace, toneMark, toneSpace as built
    AfskMlse m_mlseTemplateAlt[2];      // last different model, so a channel mixing Bell
    bool m_templateAltValid = false;            // 202 and V.23 stations swaps instead of rebuilding
    double m_templateAltParams[4];

    std::vector<MlseChain> m_replayChains;
    bool m_replayActive = false;
    double m_replayPos = 0.0;
    quint64 m_replayEnd = 0;

    // One burst queued behind the active replay - see startBurstReplay
    struct PendingReplay
    {
        bool m_valid = false;
        quint64 m_spanStart = 0;
        quint64 m_spanEnd = 0;
        double m_period = 0.0;
        double m_carrier = 0.0;
        double m_devMark = 0.0;
        double m_devSpace = 0.0;
        double m_toneMark = 0.0;
        double m_toneSpace = 0.0;
        double m_tone2Mark = 0.0;
        double m_tone2Space = 0.0;
        int m_rates = 1;
    };
    PendingReplay m_pendingReplay;
    quint64 m_replayReportTime = 0;     // Burst time, so replayed frames dedup against live
    bool m_liveRunning = false;
    quint64 m_liveTailEnd = 0;

    // Symbol rate estimator. The tone decisions of a burst land on the transmitter's real
    // symbol grid, whose period a least squares fit recovers to tens of ppm - accurate
    // enough to matter, since the trellis tolerates only -100/+50 ppm over a long frame.
    // The chains are retuned after each measured burst: the burst that provided the
    // estimate is lost, the retransmissions that follow are not.
    // Noise floor as a LOW PERCENTILE of the recent per slot MEAN power. Averaging the
    // gaps instead only works on a sparse channel - at the duty cycle of a busy one the
    // average IS the signal, the gate never fires and the estimator silently never runs.
    // A minimum sits several dB below the level noise actually presents and drags every
    // threshold with it; the median is the signal once the channel is more than half
    // busy. The 20th percentile is the noise up to an 80% duty cycle, stable to a couple
    // of percent. Thresholds are therefore signal-plus-noise ratios: a burst at 0 dB CNR
    // presents twice the noise power, so 1.8 still catches the weak signals the MLSE
    // exists for while leaving the gate shut on noise.
    //
    // The SLOT LENGTH is what decides whether that percentile sees noise at all, and it
    // has to be shorter than the gaps between transmissions - not shorter than the
    // transmissions. At 0.1 s every slot on a busy channel straddled a burst edge, so
    // there were no clean noise slots to take a percentile OF: the floor climbed towards
    // the signal, the gate stopped opening, and packets were lost while the demodulator
    // reported nothing wrong. Back-to-back traffic 0.05 to 0.25 s apart cost 5% of
    // packets that way, at any signal level. 20 ms resolves the shortest gap that can
    // separate two AX.25 frames, and the history stays at 3 s.
    // Shortening the slot is not free, and the cost is invisible. A percentile of slot
    // MEANS depends on how many samples went into a mean: the fewer, the wider their
    // distribution and the further the 20th percentile sits BELOW the true noise power.
    // Slot means are Gamma distributed, so the offset is 0.84 standard deviations and the
    // standard deviation is 1/sqrt(N) of the mean. Uncorrected, going from 0.1 s to 20 ms
    // lowers the floor by 2.3% and so lowers every threshold that is a ratio to it. That
    // is 0.1 dB, and it cost two of the five payloads on weak_signal_2026-08-02, where
    // the gate is what limits sensitivity - the thresholds sit close enough to those
    // bursts that a tenth of a dB decides them.
    //
    // Correcting for it makes the floor read the same whatever the slot length, so the
    // slot can be chosen for time resolution alone and the thresholds keep the meaning
    // they were measured with. Referred to the 0.1 s slot they were tuned against.
    static const int PACKETDEMOD_NOISE_SLOT_DIVIDER = 50;   // 20 ms slots
    static const int PACKETDEMOD_NOISE_SLOTS = 150;         // 3 s of history

    // Where the gate sits IS the demodulator's sensitivity, and it used to sit above the
    // detector. At 1.8 the chains were not started until the burst reached -1.0 dB C/N in
    // the channel noise bandwidth, while the trellis was still recovering a third of the
    // packets 1.5 dB below that - measurable by running the same signals through the
    // ungated offline path, which decoded 28 of 80 at -3 dB C/N where the gated path
    // decoded none.
    //
    // Swept against every recording and a generated AWGN channel:
    //
    //   open  NO-84  V.23  08-01 | C/N -3  C/N -2  C/N 0 | NO-84 CPU
    //   1.8     14     5      5  |   0/80   31/80  76/80 |  22x realtime
    //   1.7     14     4      5  |  13/80   47/80  78/80 |  20x
    //   1.6     14     4      5  |  25/80   57/80  79/80 |  17x
    //   1.5     14     6      5  |  33/80   58/80  79/80 |  12x
    //   1.4     14     4      5  |  28/80   59/80  79/80 |   7x
    //   1.3     14     6      5  |  32/80   60/80  79/80 |   6x
    //   1.2     14     4      5  |  32/80   61/80  79/80 |   5x
    //
    // Sensitivity saturates at 1.5 - below it the weak signal columns stop improving and
    // only the CPU moves, and it moves hard, because the live chains then spend a quiet
    // channel grinding noise. NO-84 is the worst case for that: a satellite pass that is
    // 95% idle. 12x realtime there still leaves room to run files accelerated, which 7x
    // does not. The real recordings are otherwise flat across the whole range - the V.23
    // column swings 4 to 6 without a trend, which is what a channel whose bursts sit
    // within a tenth of a dB of the threshold does, not a signal to tune on.
    //
    // Hysteresis stays proportional to the excess over the floor: close = 1 + 0.375 x
    // (open - 1), which is what 1.8 / 1.3 was.
    static constexpr double PACKETDEMOD_NOISE_TUNED_SIGMA = 0.8416; // 20th percentile
    static constexpr int PACKETDEMOD_NOISE_TUNED_DIVIDER = 10;      // thresholds tuned here
    static const int PACKETDEMOD_NOISE_PCT_DIVIDER = 5;             // 20th percentile
    static constexpr double PACKETDEMOD_NOISE_PCT_SIGMA = 0.8416;

    // A percentile only measures noise if enough of the slots ARE noise, and at the start
    // of a stream that is not true: the ring fills from empty, and if a transmission is in
    // progress it fills with signal. Measured on generated traffic, the 20th percentile
    // read SEVENTY TIMES the true floor half a second in - the gate then cannot open at
    // all, and the strongest packets in the run are the ones lost, which is the opposite
    // of what a sensitivity limit looks like.
    //
    // The smallest slot in the ring is the guard. Noise slot means are tightly clustered -
    // 3.6% apart at 20 ms, so the 20th percentile of pure noise sits about 1.1x its own
    // minimum - while a contaminated ring puts them orders of magnitude apart. Capping at
    // 1.5x leaves ample headroom for the clean case and never binds there, so steady state
    // is untouched; it only takes over when the percentile has nothing to work with.
    // Lowering the percentile instead was tried and rejected: the 10th costs a payload on
    // weak_signal_2026-08-02, where the gate decides sensitivity.
    static constexpr double PACKETDEMOD_NOISE_MIN_RATIO = 1.5;

    // Where the 20th percentile of the mean of n samples sits, relative to the true mean
    static double noisePercentileBias(double sigma, double n)
    {
        return 1.0 - sigma / std::sqrt(n);
    }
    static constexpr double PACKETDEMOD_GATE_OPEN = 1.5;
    static constexpr double PACKETDEMOD_GATE_CLOSE = 1.1875;

    // Fading measurement, to choose the branch metric. A constant envelope FM signal only
    // varies in amplitude if the channel does, so the spread of the received power IS the
    // fading - provided noise is averaged out of it first, which a heavily smoothed power
    // does because fading is slow (1 to 10 Hz) and noise is not. Measured: synthetic
    // steady 0.21-0.36, synthetic Rayleigh at 1-5 Hz 0.63-1.03, noise alone at 0 dB
    // 0.08-0.13, and the real NO-84 pass 0.45-0.81 - which overlaps the fading range and
    // is why the bar is set above everything that pass produces rather than in the middle
    // of the synthetic gap. Switching lower cost a payload there that the coherent metric
    // decodes perfectly well. A false positive costs 6 dB of sensitivity, a false
    // negative only forgoes a gain, and the replay runs both metrics regardless.
    static constexpr double PACKETDEMOD_FADE_SMOOTH = 0.005;    // ~5 ms, six symbols
    static constexpr double PACKETDEMOD_FADE_SETTLE = 0.04;     // Skip the power ramp, s
    static constexpr double PACKETDEMOD_FADE_TO_DIFF = 0.85;
    static constexpr double PACKETDEMOD_FADE_TO_COHERENT = 0.70;

    double m_fadeSmooth = 0.0;
    double m_fadeSum = 0.0;
    double m_fadeSumSq = 0.0;
    quint64 m_fadeCount = 0;
    bool m_useDifferential = false;

    bool m_estActive = false;
    quint64 m_estBurstStart = 0;
    double m_estMagAvg = 0.0;
    double m_estMagSlow = 0.0;            // Slower average, for the gate itself
    double m_estNoise = 0.0;
    double m_estNoiseRing[PACKETDEMOD_NOISE_SLOTS];
    double m_estSlotSum = 0.0;
    int m_estSlotCount = 0;
    int m_estNoiseSlot = 0;
    double m_estDiffPrev = 0.0;
    double m_estRatePpm = 0.0;
    double m_estCarrier = 0.0;            // Carrier the hypothesis grid is centred on, Hz
    Complex m_prevSample{0.0f, 0.0f};
    std::complex<double> m_estCarrierAcc{0.0, 0.0}; // Sum of z[n].conj(z[n-1]) over the burst
    std::vector<double> m_estTrans; // Transition times, absolute channel samples

    // Per tone deviation, from the correlation power over run interiors. The opposite
    // tone's correlator is the in-burst noise reference: both see the same noise power, so
    // their difference is the tone's signal power with the noise cancelled at any SNR.
    double m_estDevMark = 0.0;
    double m_estDevSpace = 0.0;
    double m_devCalPow[2];          // Correlator power for a reference deviation tone
    double m_devCalCross[2];        // ... and its leakage into the other correlator
    double m_devAcc[2];             // Same and opposite correlator powers this burst
    double m_devAccX[2];
    int m_devCnt[2];
    quint64 m_lastTransSample = 0;      // For keeping the accumulation clear of transitions

    struct DevSample
    {
        int m_tone;
        double m_pSame;
        double m_pOpp;
        std::complex<double> m_c;   // The decided tone's correlator output
    };

    std::deque<DevSample> m_devDelay;

    // Tone frequencies, from the rotation of the sliding correlator outputs. Estimates
    // need a strong burst - the gate is a power ratio of 16, roughly 10 dB CNR - and
    // several bursts agreeing, because the detector only tolerates about half a percent
    // of tone error and corrupted estimates arrive with a consistent bias.
    double m_estToneMark = 0.0;           // Applied tone frequencies
    double m_estToneSpace = 0.0;
    double m_toneCalBias[2];        // Correction for the real correlator's image pull
    double m_calMark = 1200.0;      // Tone pair the correlators above are built for
    double m_calSpace = 2200.0;
    std::complex<double> m_toneRotAcc[2];
    std::complex<double> m_tonePrev[2];
    bool m_tonePrevValid[2];
    std::deque<double> m_toneHist[2]; // Recent accepted per burst estimates
    std::deque<double> m_tonePairHist[2]; // Loose-gate history for tone-pair classification
    int m_estBurstLiveDecodes = 0;        // Valid frames the LIVE chains got from this burst

    // Learning the channel's tone pair from CRC-valid decodes rather than from the tone
    // correlators - see the comment at the counting site
    // The period of the chain that just decoded, and the last one before it. A replay
    // decode is CRC-verified ground truth about the transmitter's symbol clock, and it is
    // obtained below the FM threshold by construction - the decode itself is the proof.
    // The rate fit cannot do that: it reads transition times off the discriminator, which
    // at these levels is producing noise crossings, and it is rejected on 27 of 27 bursts
    // of weak_signal_2026-08-02 with residuals that are exactly uniform over a symbol.
    double m_replayChainPeriod = 0.0;
    double m_learnedRatePpm = 0.0;      // Last replay decode's rate, awaiting a second
    int m_learnedRateCount = 0;
    double m_replayChainToneMark = 0.0;
    double m_replayChainToneSpace = 0.0;
    int m_altPairDecodes = 0;
    double m_replayCredit = 0.0;   // Fractional replay-sample budget, see runBurstReplay

    // The branch metric follows the measured fading - there is no user control, because
    // thresholding the measurement is only half of it: the replay runs BOTH metrics and
    // lets the CRC decide, which is what a checkbox cannot do
    bool useDifferential() const { return m_useDifferential; }

    bool emitPacket(const QByteArray& packet)
    {
        // Timestamp by transmission, not by decode - see PacketHandler
        const quint64 stamp = m_replayReportTime ? m_replayReportTime : m_sampleCount;

        return m_onPacket && m_onPacket(packet, stamp);
    }

    // Give back everything buildChains allocated. Vectors are cleared AND shrunk: a
    // channel left on the discriminator would otherwise still be holding the buffer and
    // the chains, which is most of what this class costs.
    void releaseDetector()
    {
        m_chains.clear();
        m_chains.shrink_to_fit();
        m_replayChains.clear();
        m_replayChains.shrink_to_fit();
        m_bitBuf.clear();
        m_bitBuf.shrink_to_fit();
        m_estTrans.clear();
        m_estTrans.shrink_to_fit();
        m_devDelay.clear();

        for (int d = 0; d < 2; d++)
        {
            m_mlseTemplate[d] = AfskMlse();
            m_mlseTemplateAlt[d] = AfskMlse();
        }

        m_templateValid = false;
        m_templateAltValid = false;
        m_replayActive = false;
        m_pendingReplay.m_valid = false;
        m_replayReportTime = 0;
        m_liveRunning = false;
        m_liveTailEnd = 0;
        m_estActive = false;
        m_sampleCount = 0;
    }

    // Build the estimator's tone correlators for a tone pair, and calibrate what they read
    // for it.
    //
    // These have to FOLLOW the pair the chains are decoding, and used to be nailed to
    // 1200/2200. On a V.23 channel the chains learn the right pair from CRC-valid decodes
    // and move there, while every measurement feeding them - per tone deviation, tone
    // frequency, the transition times the symbol rate is fitted to - carried on being taken
    // through Bell 202 correlators the signal does not match. The deviation then reads high
    // (2299/2462 measured on weak_signal_2026-08-02, against 2100 that actually decodes it)
    // and the replay compensated with a flat 0.88 factor, which is a constant standing in
    // for a measurement. Forcing the deviation is worth three of that recording's eight
    // payloads, so the constant was not good enough.
    //
    // Cheap enough to redo on a pair change: a few thousand trig calls, against the ~100k
    // of AfskMlse::create().
    void calibrateCorrelators(double mark, double space)
    {
        m_calMark = mark;
        m_calSpace = space;

        // f0 is the space correlator and f1 the mark one - see the decision in
        // updateRateEstimator, where t == 0 means mark won
        m_correlator.create(m_correlationLength,
                            PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE,
                            space, mark);

        const int sps = m_samplesPerSymbol;

        // What the discriminator gives for a tone at the reference deviation, which every
        // deviation below is reported as a multiple of.
        //
        // PhaseDiscriminators::phaseDiscriminatorDelta returns (dPhi / pi) * scaling, NOT
        // dPhi * scaling. For peak deviation D the phase step is 2.pi.D/rate, so the
        // output is 2D/rate * rate/(2.FM_DEVIATION) = D/FM_DEVIATION - and a reference
        // tone comes out at exactly ONE, not at pi.
        //
        // This was pi, and so every deviation the estimator produced was pi times too
        // small: 810 Hz measured on a synthetic signal generated at exactly 2500. That is
        // below the 1500 Hz plausibility floor, so the estimate was discarded on every
        // burst of every recording and the deviation silently stayed at its default. The
        // estimator has therefore never once measured a deviation in anger.
        const double amp = 1.0;
        const double toneF[2] = { mark, space };
        const double toneW[2] = {
            2.0*M_PI*mark/PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE,
            2.0*M_PI*space/PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE };

        for (int t = 0; t < 2; t++)
        {
            m_devCalPow[t] = 0.0;
            m_devCalCross[t] = 0.0;

            for (int ph = 0; ph < 16; ph++)
            {
                std::complex<double> c(0.0, 0.0), x(0.0, 0.0);

                for (int m = 0; m < sps; m++)
                {
                    double a = amp * std::cos(toneW[t]*m + 2.0*M_PI*ph/16.0);
                    c += a * std::complex<double>(std::cos(toneW[t]*m), std::sin(toneW[t]*m));
                    x += a * std::complex<double>(std::cos(toneW[1-t]*m), std::sin(toneW[1-t]*m));
                }

                m_devCalPow[t] += std::norm(c) / 16.0;
                m_devCalCross[t] += std::norm(x) / 16.0;
            }
        }

        // The tone frequency reading, from the rotation of the correlator output. The live
        // correlator runs newest sample against tap 0 - time reversed - so a real tone's
        // rotation reads +f, with a small deterministic pull from the frequency image;
        // measure both on synthetic tones through the same reversed order.
        for (int t = 0; t < 2; t++)
        {
            std::complex<double> rot(0.0, 0.0);

            for (int ph = 0; ph < 16; ph++)
            {
                std::complex<double> cPrev(0.0, 0.0);

                for (int k = 0; k < 2; k++)
                {
                    std::complex<double> c(0.0, 0.0);

                    for (int m = 0; m < sps; m++)
                    {
                        double v = std::cos(2.0*M_PI*toneF[t]*(k - m)
                            / PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE
                            + 2.0*M_PI*ph/16.0);
                        c += v * std::complex<double>(std::cos(toneW[t]*m), std::sin(toneW[t]*m));
                    }

                    if (k == 1) {
                        rot += c * std::conj(cPrev);
                    }

                    cPrev = c;
                }
            }

            double measured = std::arg(rot)
                * PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE / (2.0*M_PI);
            m_toneCalBias[t] = toneF[t] - measured;
        }
    }

    // One detector per symbol timing offset and carrier hypothesis. A chain consumes a whole
    // symbol period at a time, so each fires once every samplesPerSymbol samples, staggered.
    void buildChains()
    {
        m_samplesPerSymbol = PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE / m_cfg.m_baudRate;

        int phases = std::max(1, std::min(m_samplesPerSymbol, (int) PacketDemodSettings::PACKETDEMOD_MLSE_PHASES));
        int step = PacketDemodSettings::PACKETDEMOD_MLSE_FREQ_STEP;
        int range = std::max(0, (int) PacketDemodSettings::PACKETDEMOD_MLSE_FREQ_RANGE);
        int nFreq = 2 * (range / step) + 1;

        m_chains.clear();
        m_chains.resize((size_t) phases * nFreq);

        // Deep enough to hold the longest burst the estimator will accept, so that burst can
        // be replayed once its own tuning is known - see startBurstReplay
        m_bitBuf.assign((size_t) PACKETDEMOD_BUF_SECONDS
            * PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE, Complex(0.0f, 0.0f));
        m_sampleCount = 0;

        m_replayChains.clear();
        m_pendingReplay.m_valid = false;
        m_replayCredit = 0.0;
        m_replayActive = false;
        m_replayPos = 0.0;
        m_replayEnd = 0;
        m_replayReportTime = 0;
        m_liveRunning = false;
        m_liveTailEnd = 0;

        m_fadeSmooth = 0.0;
        m_fadeSum = 0.0;
        m_fadeSumSq = 0.0;
        m_fadeCount = 0;
        m_useDifferential = false;

        m_estActive = false;
        m_estBurstLiveDecodes = 0;
        m_estMagAvg = 0.0;
        m_estMagSlow = 0.0;
        m_estNoise = 0.0;
        std::fill(std::begin(m_estNoiseRing), std::end(m_estNoiseRing), 0.0);
        m_estSlotSum = 0.0;
        m_estSlotCount = 0;
        m_estNoiseSlot = 0;
        m_estDiffPrev = 0.0;
        m_estRatePpm = 0.0;
        m_estCarrier = 0.0;
        m_prevSample = Complex(0.0f, 0.0f);
        m_estCarrierAcc = std::complex<double>(0.0, 0.0);
        m_estTrans.clear();
        m_estDevMark = m_cfg.m_deviation;
        m_estDevSpace = m_cfg.m_deviation;
        m_devAcc[0] = m_devAcc[1] = 0.0;
        m_devAccX[0] = m_devAccX[1] = 0.0;
        m_devCnt[0] = m_devCnt[1] = 0;
        m_lastTransSample = 0;
        m_devDelay.clear();
        m_estToneMark = m_cfg.m_toneMark;
        m_estToneSpace = m_cfg.m_toneSpace;
        m_toneRotAcc[0] = m_toneRotAcc[1] = std::complex<double>(0.0, 0.0);
        m_tonePrev[0] = m_tonePrev[1] = std::complex<double>(0.0, 0.0);
        m_tonePrevValid[0] = m_tonePrevValid[1] = false;
        m_toneHist[0].clear();
        m_toneHist[1].clear();
        m_tonePairHist[0].clear();
        m_tonePairHist[1].clear();

        m_templateValid = false;
        m_templateAltValid = false;
        buildTemplates(m_cfg.m_deviation, m_cfg.m_deviation,
                       m_cfg.m_toneMark, m_cfg.m_toneSpace);

        calibrateCorrelators(m_cfg.m_toneMark, m_cfg.m_toneSpace);

        double period = (double) m_samplesPerSymbol;

        size_t c = 0;

        for (int f = 0; f < nFreq; f++)
        {
            double offset = (double) ((f - nFreq/2) * step);
            double w = -2.0 * M_PI * offset / PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE;

            for (int p = 0; p < phases; p++, c++)
            {
                MlseChain& chain = m_chains[c];

                chain.m_mlse = m_mlseTemplate[useDifferential() ? 1 : 0];
                chain.m_mlse.start();
                chain.m_deframer.reset();

                chain.m_period = period;
                chain.m_carrier = offset;
                chain.m_readPos = (double) ((p * m_samplesPerSymbol) / phases);
                chain.m_rotSample.resize(m_samplesPerSymbol);

                double stepSize = chain.m_period / m_samplesPerSymbol;

                for (int m = 0; m < m_samplesPerSymbol; m++) {
                    chain.m_rotSample[m] = Complex((float) std::cos(w*m*stepSize),
                                                   (float) std::sin(w*m*stepSize));
                }

                chain.m_rotSymbol = Complex((float) std::cos(w*chain.m_period),
                                            (float) std::sin(w*chain.m_period));
                chain.m_phase = Complex(1.0f, 0.0f);
            }
        }

        qDebug() << "buildChains:" << phases << "timing x" << nFreq
                 << "carrier =" << m_chains.size() << "MLSE chains";
    }

    // Retune every chain to a measured symbol period, carrier and per tone deviation. The
    // read schedule and carrier rotators change cheaply; a deviation change rebuilds the
    // branch waveform tables once in a template detector that the chains then copy, rather
    // than paying the trigonometry per chain. The carrier hypothesis grid keeps its width but
    // recentres on the measurement, so a satellite's Doppler is followed burst to burst while
    // the grid still covers the drift between them.
    void applyEstimatedTuning(double periodSamples, double carrier,
                                               double devMark, double devSpace,
                                               double toneMark, double toneSpace)
    {
        // A swept model has to stay where the sweep put it - see Config::m_adapt
        if (!m_cfg.m_adapt) {
            return;
        }

        bool rebuild = (std::fabs(devMark - m_estDevMark) > 1.0)
                    || (std::fabs(devSpace - m_estDevSpace) > 1.0)
                    || (std::fabs(toneMark - m_estToneMark) > 0.5)
                    || (std::fabs(toneSpace - m_estToneSpace) > 0.5);

        // A different tone PAIR, not a refinement of the current one: the correlators the
        // estimator measures through have to move with the chains, or every measurement
        // that feeds them is taken against the wrong tones - see calibrateCorrelators
        if ((std::fabs(toneMark - m_calMark) > 20.0)
            || (std::fabs(toneSpace - m_calSpace) > 20.0))
        {
            qDebug() << "PacketDemodCore: measuring through tones" << toneMark << "/"
                     << toneSpace << "- was" << m_calMark << "/" << m_calSpace;
            calibrateCorrelators(toneMark, toneSpace);
        }

        if (rebuild)
        {
            buildTemplates(devMark, devSpace, toneMark, toneSpace);
            m_estDevMark = devMark;
            m_estDevSpace = devSpace;
            m_estToneMark = toneMark;
            m_estToneSpace = toneSpace;
        }

        int phases = std::max(1, std::min(m_samplesPerSymbol, (int) PacketDemodSettings::PACKETDEMOD_MLSE_PHASES));
        int step = PacketDemodSettings::PACKETDEMOD_MLSE_FREQ_STEP;
        int range = std::max(0, (int) PacketDemodSettings::PACKETDEMOD_MLSE_FREQ_RANGE);
        int nFreq = 2 * (range / step) + 1;

        size_t c = 0;

        for (int f = 0; f < nFreq; f++)
        {
            double offset = carrier + (double) ((f - nFreq/2) * step);
            double w = -2.0 * M_PI * offset / PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE;

            for (int p = 0; (p < phases) && (c < m_chains.size()); p++, c++)
            {
                MlseChain& chain = m_chains[c];

                if (rebuild) {
                    chain.m_mlse = m_mlseTemplate[useDifferential() ? 1 : 0];
                }

                chain.m_period = periodSamples;

                double stepSize = chain.m_period / m_samplesPerSymbol;

                for (int m = 0; m < m_samplesPerSymbol; m++) {
                    chain.m_rotSample[m] = Complex((float) std::cos(w*m*stepSize),
                                                   (float) std::sin(w*m*stepSize));
                }

                chain.m_rotSymbol = Complex((float) std::cos(w*chain.m_period),
                                            (float) std::sin(w*chain.m_period));
                chain.m_carrier = offset;
                chain.m_mlse.start();
                chain.m_deframer.reset();
            }
        }
    }

    // Build the two branch metric detectors for a model, if they are not already built for it.
    // Everything else copies from these - see the header comment on m_mlseTemplate.
    //
    // Two models are kept: the current one and the last different one. On a channel shared
    // between Bell 202 and V.23 stations the replay alternates tone pairs burst by burst, and
    // without the stash each alternation would re-run create()'s ~100k trig calls in the
    // baseband thread - the same stall that used to drop samples.
    void buildTemplates(double devMark, double devSpace,
                                         double toneMark, double toneSpace)
    {
        if (m_templateValid
            && (std::fabs(devMark - m_templatePeriodDev[0]) < 0.5)
            && (std::fabs(devSpace - m_templatePeriodDev[1]) < 0.5)
            && (std::fabs(toneMark - m_templatePeriodDev[2]) < 0.25)
            && (std::fabs(toneSpace - m_templatePeriodDev[3]) < 0.25))
        {
            return;
        }

        if (m_templateAltValid
            && (std::fabs(devMark - m_templateAltParams[0]) < 0.5)
            && (std::fabs(devSpace - m_templateAltParams[1]) < 0.5)
            && (std::fabs(toneMark - m_templateAltParams[2]) < 0.25)
            && (std::fabs(toneSpace - m_templateAltParams[3]) < 0.25))
        {
            // The stash matches: swap it in. AfskMlse copies are cheap - the waveform tables
            // are behind a shared_ptr.
            for (int d = 0; d < 2; d++) {
                std::swap(m_mlseTemplate[d], m_mlseTemplateAlt[d]);
            }
            for (int i = 0; i < 4; i++) {
                std::swap(m_templatePeriodDev[i], m_templateAltParams[i]);
            }
            std::swap(m_templateValid, m_templateAltValid);
            return;
        }

        // Stash the outgoing model before building the new one over it
        if (m_templateValid)
        {
            for (int d = 0; d < 2; d++) {
                m_mlseTemplateAlt[d] = m_mlseTemplate[d];
            }
            for (int i = 0; i < 4; i++) {
                m_templateAltParams[i] = m_templatePeriodDev[i];
            }
            m_templateAltValid = true;
        }

        for (int d = 0; d < 2; d++)
        {
            m_mlseTemplate[d].create(m_samplesPerSymbol, m_cfg.m_baudRate,
                                     toneMark, toneSpace, devMark, devSpace,
                                     6, m_cfg.m_ramp);
            m_mlseTemplate[d].setLoopGains(PacketDemodSettings::PACKETDEMOD_MLSE_PHASE_GAIN,
                                           PacketDemodSettings::PACKETDEMOD_MLSE_FREQ_GAIN);
            m_mlseTemplate[d].setDifferential(d == 1);
        }

        m_templatePeriodDev[0] = devMark;
        m_templatePeriodDev[1] = devSpace;
        m_templatePeriodDev[2] = toneMark;
        m_templatePeriodDev[3] = toneSpace;
        m_templateValid = true;
    }

    // Restart the live chains when the activity gate opens, from before it opened - see the
    // header comment on PACKETDEMOD_LIVE_LEAD
    void restartLiveChains()
    {
        const int sps = m_samplesPerSymbol;
        const size_t bufLen = m_bitBuf.size();
        int phases = std::max(1, std::min(sps, (int) PacketDemodSettings::PACKETDEMOD_MLSE_PHASES));

        quint64 lead = (quint64)(PACKETDEMOD_LIVE_LEAD
            * PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE);
        quint64 from = (m_sampleCount > lead) ? (m_sampleCount - lead) : 0;

        if ((m_sampleCount > bufLen) && (from < m_sampleCount - bufLen + (quint64) sps)) {
            from = m_sampleCount - bufLen + (quint64) sps;
        }

        for (size_t c = 0; c < m_chains.size(); c++)
        {
            MlseChain& chain = m_chains[c];
            int p = (int)(c % (size_t) phases);
            double w = -2.0 * M_PI * chain.m_carrier
                / PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE;

            chain.m_readPos = (double) from + (double) ((p * sps) / phases);
            chain.m_phase = Complex((float) std::cos(w*chain.m_readPos),
                                    (float) std::sin(w*chain.m_readPos));
            chain.m_mlse.start();
            chain.m_deframer.reset();
        }
    }

    // Arm a replay of the span the estimator just measured, using the tuning it yielded. Only
    // the timing phases are built - the carrier is measured to a few Hz against a +/-75 Hz
    // tolerance, so one carrier hypothesis does - and BOTH branch metrics are run, letting the
    // CRC decide rather than a threshold on the measured fading, whose populations overlap on
    // real signals. That is what makes a fading burst decodable without the live chains having
    // had to guess the channel in advance.
    void startBurstReplay(quint64 spanStart, quint64 spanEnd, double period,
                                           double carrier, double devMark, double devSpace,
                                           double toneMark, double toneSpace,
                                           double tone2Mark, double tone2Space, int rates)
    {
        const int sps = m_samplesPerSymbol;
        const size_t bufLen = m_bitBuf.size();

        // Cap the REPLAY's timing phases independently of the live chains'. The replay
        // multiplies its phases by two branch metrics, up to five symbol rates and up to two
        // tone pairs, so it pays for each one up to twenty times over: 16 phases build 320
        // detectors and 32 would build 640. That is not hypothetical - the phase count was
        // once a setting whose stored default was 32, and a buffer then took 443 ms in the
        // field while the source handed over ever larger ones as it fell behind, which is a
        // dropped-sample overflow. The count is a constant now and this cap is a no-op at
        // its present value; it is here so that raising PACKETDEMOD_MLSE_PHASES stays a
        // decision about live-chain sensitivity rather than silently doubling the replay.
        int phases = std::max(1, std::min(sps, std::min((int) PacketDemodSettings::PACKETDEMOD_MLSE_PHASES, 16)));
        int metrics = 2;
        int pairs = (tone2Mark > 0.0) ? 2 : 1;

        // Held model: replay the burst against exactly what was configured, no hedging on
        // the alternate pair and no measured deviation
        if (!m_cfg.m_adapt)
        {
            pairs = 1;
            devMark = m_cfg.m_deviation;
            devSpace = m_cfg.m_deviation;
            toneMark = m_cfg.m_toneMark;
            toneSpace = m_cfg.m_toneSpace;
        }

        rates = std::max(1, rates);

        // A replay is already running: queue this burst instead of cancelling it. Hedged
        // replays run several times longer than the original single-model ones, so on a
        // busy channel - a satellite pass, packets back to back - cancellation was
        // silently losing most bursts. One pending slot suffices; the newest burst wins
        // because the oldest is closest to being overwritten in the circular buffer.
        if (m_replayActive)
        {
            m_pendingReplay.m_valid = true;
            m_pendingReplay.m_spanStart = spanStart;
            m_pendingReplay.m_spanEnd = spanEnd;
            m_pendingReplay.m_period = period;
            m_pendingReplay.m_carrier = carrier;
            m_pendingReplay.m_devMark = devMark;
            m_pendingReplay.m_devSpace = devSpace;
            m_pendingReplay.m_toneMark = toneMark;
            m_pendingReplay.m_toneSpace = toneSpace;
            m_pendingReplay.m_tone2Mark = tone2Mark;
            m_pendingReplay.m_tone2Space = tone2Space;
            m_pendingReplay.m_rates = rates;
            return;
        }

        // Start ahead of the burst so the per survivor phase loops acquire on the preamble
        quint64 margin = (quint64)(0.1 * PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE);
        quint64 start = (spanStart > margin) ? (spanStart - margin) : 0;

        // Never ask for history the circular buffer has already overwritten
        if ((m_sampleCount > bufLen) && (start < m_sampleCount - bufLen + (quint64) sps)) {
            start = m_sampleCount - bufLen + (quint64) sps;
        }

        if (start + (quint64) sps >= spanEnd) {
            return;
        }

        m_replayChains.resize((size_t) phases * metrics * rates * pairs);

        double w = -2.0 * M_PI * carrier / PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE;
        int perPair = phases * metrics * rates;

        for (int i = 0; i < perPair * pairs; i++)
        {
            int p = i % phases;
            int metric = (i / phases) % metrics;
            int r = (i / (phases * metrics)) % rates;
            MlseChain& chain = m_replayChains[i];

            // Chains copy from the templates, so the second pair can rebuild them after the
            // first pair's chains are filled - and the two-slot template cache means a
            // channel alternating pairs never actually re-runs create()
            if (i == 0) {
                buildTemplates(devMark, devSpace, toneMark, toneSpace);
            } else if (i == perPair) {
                // The deviation was measured through the LIVE pair's correlators; if this
                // burst actually uses the other pair, that measurement reads about 10%
                // high (the tone sits off the correlator bin and the calibration
                // overcorrects). Scale it back for the alternate-pair hypothesis -
                // measured on the V.23 recording, where the raw value costs two of seven
                // payloads.
                buildTemplates(0.88 * devMark, 0.88 * devSpace, tone2Mark, tone2Space);
            }

            chain.m_mlse = m_mlseTemplate[metric];
            chain.m_mlse.start();
            chain.m_deframer.reset();

            // Centred grid, so rates == 1 reproduces the measured period exactly
            double ppmOff = (rates > 1)
                ? (-PACKETDEMOD_REPLAY_RATE_SPAN + r * PACKETDEMOD_REPLAY_RATE_STEP)
                : 0.0;

            chain.m_period = period * (1.0 + ppmOff * 1e-6);
            chain.m_carrier = carrier;
            chain.m_toneMark = (i < perPair) ? toneMark : tone2Mark;
            chain.m_toneSpace = (i < perPair) ? toneSpace : tone2Space;
            chain.m_readPos = (double) start + (double) ((p * sps) / phases);
            chain.m_rotSample.resize(sps);

            double stepSize = chain.m_period / sps;

            for (int m = 0; m < sps; m++) {
                chain.m_rotSample[m] = Complex((float) std::cos(w*m*stepSize),
                                               (float) std::sin(w*m*stepSize));
            }

            chain.m_rotSymbol = Complex((float) std::cos(w*chain.m_period),
                                        (float) std::sin(w*chain.m_period));

            // The carrier rotator is absolute, so seed it at this chain's start position
            chain.m_phase = Complex((float) std::cos(w*chain.m_readPos),
                                    (float) std::sin(w*chain.m_readPos));
        }

        // Run past the burst by more than the trellis traceback depth. The detector emits
        // decisions delayed by that depth, so a replay stopping at the burst end never emits
        // its last few dozen symbols - exactly where the closing flag and the CRC are. The
        // live chains never notice because they simply keep running.
        m_replayPos = (double) start;
        m_replayEnd = spanEnd + (quint64)(PACKETDEMOD_REPLAY_TAIL * sps);
        m_replayActive = true;
    }

    // Advance the replay, time sliced against the live stream
    void runBurstReplay()
    {
        const size_t bufLen = m_bitBuf.size();
        const int sps = m_samplesPerSymbol;

        // Pace so that a replay costs no more per live sample than the live chains do,
        // whatever size its hypothesis grid is. Integer pacing could not do this - its floor
        // of one replay sample per live sample still let a 320-detector replay cost several
        // times the live chains, which measured 1.8x realtime here and goes under 1.0x on a
        // slower machine, which is a dropped-sample overflow. Carrying a fractional credit
        // lets the rate fall below one: the replay then takes longer in wall time, and
        // PACKETDEMOD_BUF_SECONDS plus the overwrite check below decide whether it still
        // fits. A 1 second burst at the 0.5 floor finishes about 2 seconds later.
        double budget = m_chains.empty()
            ? (double) PACKETDEMOD_REPLAY_RATE
            : (double) m_chains.size() / std::max((size_t) 1, m_replayChains.size());

        // The floor trades CPU headroom against back-to-back traffic, measured at snr 4:
        //
        //   floor   replay cost      real recordings   generated 0.05-0.25 s spacing
        //   1.0     1.8-2.4x rt      14 / 5 / 5        5% / 5% / 10% PER
        //   0.5     3.2-4.9x rt      14 / 5 / 5        5% / 15% / 40% PER
        //   0.25    7.5x rt          14 / 5 / 4        5% / 15% / 40% PER
        //
        // Slower pacing means a burst is still being worked through when the next arrives and
        // the one-deep queue drops it. Every real recording holds at 0.5 - only the generator,
        // whose default spacing is far denser than any real APRS channel, suffers - so 0.5 is
        // the setting that buys headroom on a slow PC without costing anything measurable on
        // air. Raise it to 1.0 if a genuinely saturated channel ever matters more than CPU.
        m_replayCredit += std::min((double) PACKETDEMOD_REPLAY_RATE, std::max(0.5, budget));

        while ((m_replayCredit >= 1.0) && m_replayActive)
        {
            m_replayCredit -= 1.0;

            // The tail can reach past what has been received; wait for live rather than
            // reading samples that do not exist yet
            if (m_replayPos + 2.0 >= (double) m_sampleCount) {
                break;
            }

            // Abandon only if the live writer is about to overwrite what is still to be
            // replayed. Abandoning whenever a new burst starts looks safer but is not: a
            // momentary blip in the activity gate then cancels every replay before it can
            // finish, and the whole mechanism silently does nothing.
            if ((double) m_sampleCount - m_replayPos > (double)(bufLen - 2 * sps))
            {
                m_replayActive = false;
                break;
            }

            m_replayPos += 1.0;

            if (m_replayPos >= (double) m_replayEnd)
            {
                m_replayActive = false;
                break;
            }

            // Frames found here belong to the burst, not to now, so they deduplicate against
            // anything the live chains decoded from the same transmission
            m_replayReportTime = (quint64) m_replayPos;

            for (size_t c = 0; c < m_replayChains.size(); c++)
            {
                MlseChain& chain = m_replayChains[c];

                if (m_replayPos < chain.m_readPos + chain.m_period + 2.0) {
                    continue;
                }

                m_replayChainToneMark = chain.m_toneMark;
                m_replayChainToneSpace = chain.m_toneSpace;
                m_replayChainPeriod = chain.m_period;

                const Complex phase = chain.m_phase;
                const double readPos = chain.m_readPos;
                const double stepSize = chain.m_period / sps;
                const std::vector<Complex>& buf = m_bitBuf;
                const std::vector<Complex>& rot = chain.m_rotSample;

                Real soft;

                bool ready = chain.m_mlse.step(
                    [&buf, bufLen, &rot, readPos, stepSize, phase](int m) {
                        double x = readPos + m * stepSize;
                        quint64 i = (quint64) x;
                        float fr = (float) (x - (double) i);
                        Complex v = buf[i % bufLen] * (1.0f - fr) + buf[(i + 1) % bufLen] * fr;
                        v *= phase * rot[m];
                        return std::complex<double>(v.real(), v.imag());
                    },
                    soft);

                chain.m_phase *= chain.m_rotSymbol;

                float mag = std::abs(chain.m_phase);

                if (mag > 1e-6f) {
                    chain.m_phase /= mag;
                }

                chain.m_readPos += chain.m_period;

                if (ready) {
                    m_framer.process(chain.m_deframer, soft < 0.0f ? 1 : 0, -soft, !m_replayReportTime);
                }
            }
        }

        m_replayReportTime = 0;

        // A replay that just finished or was abandoned hands over to the queued burst. The
        // buffer clamp in startBurstReplay discards it naturally if it has grown stale.
        if (!m_replayActive && m_pendingReplay.m_valid)
        {
            PendingReplay p = m_pendingReplay;
            m_pendingReplay.m_valid = false;
            startBurstReplay(p.m_spanStart, p.m_spanEnd, p.m_period, p.m_carrier,
                             p.m_devMark, p.m_devSpace, p.m_toneMark, p.m_toneSpace,
                             p.m_tone2Mark, p.m_tone2Space, p.m_rates);
        }
    }

    void processMlse(const Complex &ci, Real fmDemod, double magsq)
    {
        if (m_chains.empty()) {
            return;
        }

        const size_t bufLen = m_bitBuf.size();

        m_bitBuf[m_sampleCount % bufLen] = ci;
        m_sampleCount++;

        updateRateEstimator(ci, fmDemod, magsq);

        // Idle gating: the chains only run while there is something to decode, plus a tail to
        // flush the traceback
        if (m_estActive)
        {
            m_liveTailEnd = m_sampleCount
                + (quint64)(PACKETDEMOD_LIVE_TAIL * m_samplesPerSymbol);
        }

        if (!m_estActive && (m_sampleCount >= m_liveTailEnd))
        {
            m_liveRunning = false;

            if (m_replayActive) {
                runBurstReplay();
            }

            return;
        }

        if (!m_liveRunning)
        {
            restartLiveChains();
            m_liveRunning = true;
        }

        for (size_t c = 0; c < m_chains.size(); c++)
        {
            MlseChain& chain = m_chains[c];

            // A chain fires when its whole symbol period is in the buffer. The period is
            // fractional - it carries the measured transmitter symbol rate - so samples are
            // read at interpolated positions.
            if ((double) m_sampleCount < chain.m_readPos + chain.m_period + 2.0) {
                continue;
            }

            const Complex phase = chain.m_phase;
            const int sps = m_samplesPerSymbol;
            const double readPos = chain.m_readPos;
            const double stepSize = chain.m_period / sps;
            const std::vector<Complex>& buf = m_bitBuf;
            const std::vector<Complex>& rot = chain.m_rotSample;

            Real soft;

            bool ready = chain.m_mlse.step(
                [&buf, bufLen, &rot, readPos, stepSize, phase](int m) {
                    double x = readPos + m * stepSize;
                    quint64 i = (quint64) x;
                    float fr = (float) (x - (double) i);
                    Complex v = buf[i % bufLen] * (1.0f - fr) + buf[(i + 1) % bufLen] * fr;
                    v *= phase * rot[m];
                    return std::complex<double>(v.real(), v.imag());
                },
                soft);

            // Advance the carrier hypothesis to the start of the next symbol period, and
            // renormalise before it drifts off the unit circle
            chain.m_phase *= chain.m_rotSymbol;

            float mag = std::abs(chain.m_phase);

            if (mag > 1e-6f) {
                chain.m_phase /= mag;
            }

            chain.m_readPos += chain.m_period;

            if (ready)
            {
                // AfskMlse gives positive for space; the deframer expects positive for mark,
                // as diff is mark minus space
                m_framer.process(chain.m_deframer, soft < 0.0f ? 1 : 0, -soft, !m_replayReportTime);
            }
        }

        if (m_replayActive) {
            runBurstReplay();
        }
    }

    // Measure the transmitter's symbol rate and carrier from each burst, and retune the
    // chains when either is meaningfully different from what they run at. Tone decisions land
    // on the transmitter's symbol grid; a least squares fit of the transition times recovers
    // its period to tens of ppm, where the trellis only tolerates -100/+50 over a long frame
    // and real transmitters run over a thousand ppm out (NO-84 measures +1600). The carrier
    // is the argument of the averaged sample-to-sample product - averaged before the
    // argument, so it stays honest below the discriminator threshold - and recentres the
    // hypothesis grid, which follows a satellite's Doppler burst to burst. The burst that
    // provides the estimates is lost; the traffic that follows is not, and packet traffic
    // always repeats.
    void updateRateEstimator(const Complex &ci, Real fmDemod, double magsq)
    {
        // Activity gate on the channel power, fast average against a floor tracked only in
        // the gaps so a long transmission cannot pull it up. No burst can be declared until
        // the floor has been learned: from a cold start the fast average is instantly
        // enormous against a floor still near zero, the gate latches "in a burst" with the
        // floor frozen, and no measurement ever completes - which leaves the chains on the
        // nominal rate and decodes nothing from a real transmitter.
        //
        // The wait is a real deafness at the start of a stream, and it used to be half a
        // second: long enough to lose a strong packet outright, which is what it did to the
        // first frame of every generated run. 20 ms slots fill the ring fast enough that
        // an eighth of a second gives the percentile more slots than the old half second
        // did, so the deafness is now shorter than one AX.25 frame.
        const quint64 warmup = PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE / 8;
        const int slotSamples = PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE
                              / PACKETDEMOD_NOISE_SLOT_DIVIDER;

        m_estMagAvg += 0.02 * (magsq - m_estMagAvg);

        // A slower average for the gate itself. The fast one fluctuates 15% on noise, enough
        // to trip any threshold tight enough to catch a weak burst; this one is good to a few
        // percent and its 13 ms of onset latency costs nothing, because a frame opens with
        // far more preamble than that and the chains restart from before the gate opened.
        m_estMagSlow += 0.002 * (magsq - m_estMagSlow);
        m_fadeSmooth += PACKETDEMOD_FADE_SMOOTH * (magsq - m_fadeSmooth);

        // Spread of the smoothed power over the burst, clear of its power ramp
        if (m_estActive
            && (m_sampleCount - m_estBurstStart
                > (quint64)(PACKETDEMOD_FADE_SETTLE
                    * PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE)))
        {
            m_fadeSum += m_fadeSmooth;
            m_fadeSumSq += m_fadeSmooth * m_fadeSmooth;
            m_fadeCount++;
        }

        m_estSlotSum += magsq;

        if (++m_estSlotCount >= slotSamples)
        {
            m_estNoiseRing[m_estNoiseSlot] = m_estSlotSum / slotSamples;
            m_estNoiseSlot = (m_estNoiseSlot + 1) % PACKETDEMOD_NOISE_SLOTS;
            m_estSlotSum = 0.0;
            m_estSlotCount = 0;

            double vals[PACKETDEMOD_NOISE_SLOTS];
            int n = 0;

            for (int i = 0; i < PACKETDEMOD_NOISE_SLOTS; i++)
            {
                if (m_estNoiseRing[i] > 0.0) {
                    vals[n++] = m_estNoiseRing[i];
                }
            }

            if (n >= 8)
            {
                std::nth_element(vals, vals + n/5, vals + n);

                // Referred to the slot length the gate thresholds were tuned against
                const double tuned = noisePercentileBias(PACKETDEMOD_NOISE_TUNED_SIGMA,
                    PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE
                    / (double) PACKETDEMOD_NOISE_TUNED_DIVIDER);

                double floor = vals[n/PACKETDEMOD_NOISE_PCT_DIVIDER] * tuned
                    / noisePercentileBias(PACKETDEMOD_NOISE_PCT_SIGMA, (double) slotSamples);

                // ... unless the ring holds too little noise to take a percentile of
                double least = *std::min_element(vals, vals + n);

                m_estNoise = std::min(floor, PACKETDEMOD_NOISE_MIN_RATIO * least);
            }
        }

        if (m_estActive)
        {
            std::complex<double> cur(ci.real(), ci.imag());
            std::complex<double> prev(m_prevSample.real(), m_prevSample.imag());
            m_estCarrierAcc += cur * std::conj(prev);
        }

        m_prevSample = ci;

        // The tone correlators the discriminator path would have run - idle in MLSE mode, so
        // reuse them for the estimator's decisions
        Complex corrF0;
        Complex corrF1;

        if (m_correlator.push(fmDemod, corrF0, corrF1))
        {
            double p1 = std::norm(corrF1);
            double p0 = std::norm(corrF0);
            double diff = std::abs(corrF1) - std::abs(corrF0);

            if ((diff < 0.0) != (m_estDiffPrev < 0.0))
            {
                // A crossing means the windows around here straddle a transition: what is
                // waiting in the deviation delay line is contaminated, and the clock restarts
                m_devDelay.clear();
                m_tonePrevValid[0] = m_tonePrevValid[1] = false;
                m_lastTransSample = m_sampleCount;

                if (m_estActive)
                {
                    double d = diff - m_estDiffPrev;
                    double t = (double) m_sampleCount - 1.0 + ((d != 0.0) ? (-m_estDiffPrev / d) : 0.0);

                    if ((m_estTrans.empty() || (t - m_estTrans.back() > m_samplesPerSymbol / 2))
                        && (m_estTrans.size() < 4000)) {
                        m_estTrans.push_back(t);
                    }
                }
            }
            else if (m_estActive)
            {
                // A window contributes to the deviation estimate once it is over half a symbol
                // clear of the transitions on both sides. The trailing side cannot be known
                // yet, so candidates wait in a delay line that a crossing wipes.
                const quint64 guard = (quint64)(m_samplesPerSymbol / 2 + 6);

                if (m_sampleCount - m_lastTransSample > guard)
                {
                    int t = (diff > 0.0) ? 0 : 1;
                    Complex cc = (t == 0) ? corrF1 : corrF0;
                    m_devDelay.push_back({ t, (t == 0) ? p1 : p0, (t == 0) ? p0 : p1,
                                           std::complex<double>(cc.real(), cc.imag()) });
                }

                while (m_devDelay.size() > guard)
                {
                    const DevSample& s = m_devDelay.front();
                    int t = s.m_tone;
                    m_devAcc[t] += s.m_pSame;
                    m_devAccX[t] += s.m_pOpp;
                    m_devCnt[t]++;

                    // Consecutive committed windows of the same tone measure its frequency
                    // through the rotation of the correlator output
                    if (m_tonePrevValid[t]) {
                        m_toneRotAcc[t] += s.m_c * std::conj(m_tonePrev[t]);
                    }

                    m_tonePrev[t] = s.m_c;
                    m_tonePrevValid[t] = true;
                    m_devDelay.pop_front();
                }
            }

            m_estDiffPrev = diff;
        }


        if (!m_estActive && (m_sampleCount > warmup)
            && (m_estMagSlow > PACKETDEMOD_GATE_OPEN * m_estNoise) && (m_estNoise > 0.0))
        {
            m_estActive = true;
            m_estBurstStart = m_sampleCount;
            m_estBurstLiveDecodes = 0;
            m_estTrans.clear();
            m_estCarrierAcc = std::complex<double>(0.0, 0.0);
            m_fadeSum = 0.0;
            m_fadeSumSq = 0.0;
            m_fadeCount = 0;
            m_devAcc[0] = m_devAcc[1] = 0.0;
            m_devAccX[0] = m_devAccX[1] = 0.0;
            m_devCnt[0] = m_devCnt[1] = 0;
            m_devDelay.clear();
            m_toneRotAcc[0] = m_toneRotAcc[1] = std::complex<double>(0.0, 0.0);
            m_tonePrevValid[0] = m_tonePrevValid[1] = false;
        }
        else if (m_estActive
            && ((m_estMagSlow < PACKETDEMOD_GATE_CLOSE * m_estNoise)
                || (m_sampleCount - m_estBurstStart
                    > (quint64) 3 * PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE)))
        {
            // The duration cap is the escape from a latched gate: a span that long is not one
            // AX.25 frame, and whatever it is, its fit will fail the residual test
            m_estActive = false;

            quint64 duration = m_sampleCount - m_estBurstStart;

            // A minimum AX.25 frame is 17 bytes, which with its flags is about 150 ms at 1200
            // baud, so a quarter second floor rejected the shortest legal packets outright -
            // and with them every estimate and every replay. A channel carrying only short
            // packets could therefore never learn its symbol rate, carrier or tone pair, which
            // matters most for exactly the traffic that needs it. The transition count is the
            // real guard on whether a span can be measured at all; the duration floor only has
            // to exclude spans too brief to be a frame.
            if ((duration < (quint64)(0.1 * PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE))
                || (m_estTrans.size() < 40)) {
                return;
            }

            double carrier = std::arg(m_estCarrierAcc)
                * PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE / (2.0 * M_PI);

            // Transitions land on the symbol grid. Grid search the period - starting a least
            // squares fit from nominal diverges beyond a few hundred ppm, because the symbol
            // counts round wrongly - then refine the winner.
            const double sps = (double) m_samplesPerSymbol;
            double bestP = 0.0, bestR = 1e30;

            // Coarse search over a subsample: 120 trial periods against every transition of a
            // long burst is a few hundred thousand operations in the baseband thread, and
            // several hundred transitions locate the period just as well. The least squares
            // refinement below then uses all of them.
            size_t stride = 1 + m_estTrans.size() / 400;

            for (double p = sps * 0.994; p <= sps * 1.006; p += sps * 100e-6)
            {
                double acc = 0.0;

                for (size_t k = stride; k < m_estTrans.size(); k += stride)
                {
                    double d = m_estTrans[k] - m_estTrans[0];
                    double e = d - std::round(d / p) * p;
                    acc += std::fabs(e);
                }

                if (acc < bestR) {
                    bestR = acc;
                    bestP = p;
                }
            }

            // Least squares with outlier rejection. The activity gate deliberately spans the
            // power ramps at each end of a burst, where a handful of transitions are mistimed
            // or spurious - and a plain fit over 500 good transitions is wrecked by five bad
            // ones, which shows up as a residual just over the accept limit and silently
            // costs every packet in the burst.
            double period = bestP;
            double offset = m_estTrans[0];
            std::vector<char> keep(m_estTrans.size(), 1);
            double resid = 0.0;

            for (int pass = 0; pass < 4; pass++)
            {
                double sk = 0.0, st = 0.0, skk = 0.0, skt = 0.0, n = 0.0;

                for (size_t k = 0; k < m_estTrans.size(); k++)
                {
                    if (!keep[k]) {
                        continue;
                    }

                    double idx = std::round((m_estTrans[k] - offset) / period);
                    sk += idx;
                    st += m_estTrans[k];
                    skk += idx * idx;
                    skt += idx * m_estTrans[k];
                    n += 1.0;
                }

                double den = n * skk - sk * sk;

                if ((n < 20.0) || (std::fabs(den) < 1e-9)) {
                    return;
                }

                period = (n * skt - sk * st) / den;
                offset = (st - period * sk) / n;

                resid = 0.0;

                for (size_t k = 0; k < m_estTrans.size(); k++)
                {
                    if (!keep[k]) {
                        continue;
                    }

                    double e = m_estTrans[k] - offset
                        - std::round((m_estTrans[k] - offset) / period) * period;
                    resid += e * e;
                }

                resid = std::sqrt(resid / n);

                if (pass == 3) {
                    break;
                }

                double limit = std::max(2.5 * resid, 0.05 * period);

                for (size_t k = 0; k < m_estTrans.size(); k++)
                {
                    double e = m_estTrans[k] - offset
                        - std::round((m_estTrans[k] - offset) / period) * period;

                    if (std::fabs(e) > limit) {
                        keep[k] = 0;
                    }
                }
            }

            // A real burst fits its grid to a couple of samples; noise does not. A burst that
            // fails this used to be discarded outright, which threw away the carrier, deviation
            // and tone estimates too - none of which depend on the fit - and skipped the replay
            // entirely. Weak bursts fail it routinely, so that was the case where the replay was
            // needed most. Keep going, and let the replay search a rate grid instead.
            bool fitOk = !((resid > 0.1 * sps) || (period < sps * 0.994) || (period > sps * 1.006));

            if (!fitOk) {
                period = sps;
            }

            double ppm = (period / sps - 1.0) * 1e6;

            // Per tone deviation: same-correlator power minus opposite-correlator power over
            // the run interiors, leakage corrected - the noise cancels at any SNR. Estimates
            // outside the plausible range fall back to what is currently applied.
            double devMark = m_estDevMark;
            double devSpace = m_estDevSpace;

            if ((m_devCnt[0] >= 50) && (m_devCalPow[0] > m_devCalCross[0]))
            {
                double s = (m_devAcc[0] - m_devAccX[0]) / m_devCnt[0]
                    / (1.0 - m_devCalCross[0]/m_devCalPow[0]);
                double d = PacketDemodSettings::PACKETDEMOD_FM_DEVIATION
                    * std::sqrt(std::max(0.0, s) / m_devCalPow[0]);

                if ((d > 1500.0) && (d < 5500.0)) {
                    devMark = d;
                }
            }

            if ((m_devCnt[1] >= 50) && (m_devCalPow[1] > m_devCalCross[1]))
            {
                double s = (m_devAcc[1] - m_devAccX[1]) / m_devCnt[1]
                    / (1.0 - m_devCalCross[1]/m_devCalPow[1]);
                double d = PacketDemodSettings::PACKETDEMOD_FM_DEVIATION
                    * std::sqrt(std::max(0.0, s) / m_devCalPow[1]);

                if ((d > 1500.0) && (d < 5500.0)) {
                    devSpace = d;
                }
            }

            // Tone frequencies, behind the ~10 dB power ratio gate that keeps the per-burst
            // bias down. The acceptance window is deliberately wide: real 1200 baud AX.25
            // traffic is not all Bell 202. V.23 modems transmit mark 1300 / space 2100 - the
            // CML FX604 (V.23) and FX614 (Bell 202) are pin compatible and get mixed up, and
            // one such digipeater was found relaying live UK APRS traffic - and analog TNCs
            // run a few percent off. Mark is accepted in 1100-1400 and space in 2000-2300,
            // which spans all of those while staying clear of 1200/1800 FFSK (MDC/MPT1327),
            // a different protocol this detector must not lock onto.
            //
            // Two consumers with different needs. The LIVE chains retune only on a history
            // of several agreeing bursts, because a shared channel mixes Bell and V.23
            // stations and one burst must not steer the chains away from the majority. The
            // REPLAY serves exactly one burst - one transmitter - so it takes this burst's
            // own measurement whenever it clearly disagrees with the live tuning.
            double toneMark = m_estToneMark;
            double toneSpace = m_estToneSpace;
            const double toneLo[2] = { 1100.0, 2000.0 };
            const double toneHi[2] = { 1400.0, 2300.0 };
            double toneCand[2] = { 0.0, 0.0 };
            double toneBurst[2] = { 0.0, 0.0 };

            for (int t = 0; t < 2; t++)
            {
                // Two gates on the same measurement. The strict power ratio (16) keeps the
                // reading precise enough for the live retune's 12 Hz consistency test - but a
                // mismatched tone pair CANNOT reach it, because the tone then sits 900 Hz
                // from the opposite correlator instead of 1000 and the leakage caps the
                // ratio near 11. Classifying WHICH pair a burst uses only needs +/-60 Hz,
                // so a looser gate (5) feeds the pair classifier: measured clean V.23 bursts
                // read 5.7-10, while junk readings on real weak bursts read 3.9-4.9 and
                // scatter by +/-150 Hz - those must stay out, or a junk reading that lands
                // near a pair suppresses the both-pairs hedge that would have decoded the
                // burst.
                if ((m_devCnt[t] >= 50) && (m_devAcc[t] > 5.0 * m_devAccX[t])
                    && (std::abs(m_toneRotAcc[t]) > 0.0))
                {
                    double f = std::arg(m_toneRotAcc[t])
                        * PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE / (2.0*M_PI)
                        + m_toneCalBias[t];

                    if ((f > toneLo[t]) && (f < toneHi[t]))
                    {
                        toneBurst[t] = f;
                        m_tonePairHist[t].push_back(f);

                        if (m_tonePairHist[t].size() > 8) {
                            m_tonePairHist[t].pop_front();
                        }

                        if (m_devAcc[t] > 16.0 * m_devAccX[t])
                        {
                            m_toneHist[t].push_back(f);

                            if (m_toneHist[t].size() > 8) {
                                m_toneHist[t].pop_front();
                            }
                        }
                    }
                }

                if (m_toneHist[t].size() >= 5)
                {
                    double lo = 1e9, hi = -1e9, acc = 0.0;

                    for (double v : m_toneHist[t])
                    {
                        lo = std::min(lo, v);
                        hi = std::max(hi, v);
                        acc += v;
                    }

                    if (hi - lo < 12.0) {
                        toneCand[t] = acc / m_toneHist[t].size();
                    }
                }
            }

            // Known-pair snap: measurements on a mismatched pair scatter by more than the
            // 12 Hz consistency gate ever accepts (14 Hz spread is typical for V.23 read
            // through Bell correlators), so a history that clusters near a KNOWN pair
            // retunes to that pair's exact tones instead of the noisy mean. This is what
            // lets the live chains follow a channel whose traffic is predominantly V.23.
            //
            // Two hard-won guards. The snap must be JOINT - both tones voting for the same
            // pair - because independent per-tone snapping once produced the hybrid
            // 1200/2100, which is no modem at all, off one noisy space history late in the
            // NO-84 pass. And it must only ever snap to a DIFFERENT pair than the live
            // tuning belongs to: snapping within the live pair fights the precise
            // refinement (NO-84's transmitter really is at 1190 Hz, and an unguarded snap
            // kept "correcting" the chains back to 1200.0 - three payloads' worth of
            // damage).
            if ((toneCand[0] <= 0.0) && (toneCand[1] <= 0.0)
                && (m_tonePairHist[0].size() >= 5) && (m_tonePairHist[1].size() >= 5))
            {
                const double mA = 1200.0, sA = 2200.0;
                const double mB = 1300.0, sB = 2100.0;
                int cA = 0, cB = 0, nA = 0, nB = 0;

                for (double v : m_tonePairHist[0])
                {
                    if (std::fabs(v - mA) < 60.0) {
                        cA++;
                    }
                    if (std::fabs(v - mB) < 60.0) {
                        cB++;
                    }
                }

                for (double v : m_tonePairHist[1])
                {
                    if (std::fabs(v - sA) < 60.0) {
                        nA++;
                    }
                    if (std::fabs(v - sB) < 60.0) {
                        nB++;
                    }
                }

                bool liveIsA = std::fabs(m_estToneMark - mA) < std::fabs(m_estToneMark - mB);

                if (!liveIsA && (cA >= 5) && (nA >= 5) && (cA > cB) && (nA > nB))
                {
                    toneCand[0] = mA;
                    toneCand[1] = sA;
                }
                else if (liveIsA && (cB >= 5) && (nB >= 5) && (cB >= cA) && (nB >= nA))
                {
                    toneCand[0] = mB;
                    toneCand[1] = sB;
                }
            }

            if (toneCand[0] > 0.0) {
                toneMark = toneCand[0];
            }
            if (toneCand[1] > 0.0) {
                toneSpace = toneCand[1];
            }

            // Only a converged fit may retune the live chains - except for a tone PAIR
            // change. Without a trusted symbol rate there is nothing to retune the rate to,
            // and steering the chains off a weak burst's guess would cost the strong traffic
            // they are there to catch. But waiting for the fit before switching tone pairs
            // is circular: the fit itself runs through the tone correlators, and against the
            // wrong pair its residual hovers just over the accept limit, so an all-V.23
            // channel would never retune at all. A pair change (40+ Hz, i.e. a different
            // standard pair, not a refinement of the current one) therefore retunes tones
            // while leaving the rate untouched.
            bool pairChange = (std::fabs(toneMark - m_estToneMark) > 40.0)
                           || (std::fabs(toneSpace - m_estToneSpace) > 40.0);

            if (pairChange
                || (fitOk
                && ((std::fabs(ppm - m_estRatePpm) > 75.0) || (std::fabs(carrier - m_estCarrier) > 75.0)
                || (std::fabs(devMark - m_estDevMark) > 0.1 * m_estDevMark)
                || (std::fabs(devSpace - m_estDevSpace) > 0.1 * m_estDevSpace)
                || (std::fabs(toneMark - m_estToneMark) > 2.0)
                || (std::fabs(toneSpace - m_estToneSpace) > 2.0))))
            {
                qDebug() << "PacketDemodCore: measured symbol period" << period
                         << "samples (" << ppm << "ppm ), carrier" << carrier
                         << "Hz, deviation" << devMark << "/" << devSpace
                         << "Hz, tones" << toneMark << "/" << toneSpace
                         << "- retuning MLSE chains from" << m_estRatePpm << "ppm,"
                         << m_estCarrier << "Hz," << m_estDevMark << "/" << m_estDevSpace << "Hz,"
                         << m_estToneMark << "/" << m_estToneSpace;

                // A pair change without a converged fit keeps the current rate - the fit
                // fell back to nominal, which must not clobber a previously fitted rate
                double livePeriod = fitOk
                    ? period
                    : (double) sps * (1.0 + m_estRatePpm * 1e-6);

                if (fitOk) {
                    m_estRatePpm = ppm;
                }

                m_estCarrier = carrier;
                applyEstimatedTuning(livePeriod, carrier, devMark, devSpace, toneMark, toneSpace);
            }

            // Steer the LIVE chains from how much this burst faded. The bar is set above
            // anything a real steady pass produces, because a false positive costs 6 dB of
            // sensitivity; the replay runs both metrics regardless, so a missed switch only
            // forgoes the live chains' share of a fading burst.
            if (fitOk && (m_fadeCount > 1000))
            {
                double mean = m_fadeSum / m_fadeCount;
                double var = m_fadeSumSq / m_fadeCount - mean * mean;
                double cv = (mean > 0.0) ? (std::sqrt(std::max(0.0, var)) / mean) : 0.0;

                bool useDiff = m_useDifferential
                    ? (cv > PACKETDEMOD_FADE_TO_COHERENT)
                    : (cv > PACKETDEMOD_FADE_TO_DIFF);

                if (useDiff != m_useDifferential)
                {
                    qDebug() << "PacketDemodCore: fading" << cv << "- switching live chains to"
                             << (useDiff ? "differential" : "coherent");
                    m_useDifferential = useDiff;

                    for (size_t c = 0; c < m_chains.size(); c++) {
                        m_chains[c].m_mlse.setDifferential(useDifferential());
                    }
                }
            }

            // Replay every measured burst, not only the ones that moved the tuning. On a
            // retune the live chains had the wrong tuning and the burst is otherwise lost;
            // and even when the tuning was right, the replay is what runs the other branch
            // metric over the burst.

            int rates = fitOk
                ? 1
                : (int)(2.0 * PACKETDEMOD_REPLAY_RATE_SPAN / PACKETDEMOD_REPLAY_RATE_STEP) + 1;

            // Choose the replay's tone pair(s). The per-burst measurement is only reliable
            // enough to pick a KNOWN pair, not to be used raw: measured on a matched pair it
            // reads +/-10 Hz, but on a mismatched one the cross-correlator leakage drops the
            // same/opposite ratio to 4-6 at real weak-signal CNRs and the reading scatters
            // by +/-150 Hz. So: a measurement near the live tuning keeps the single-pair
            // replay; one clearly nearer the other standard pair snaps to that pair exactly;
            // and a missing or off-grid measurement hedges by replaying BOTH standard pairs
            // and letting the CRC decide - the same philosophy as running both branch
            // metrics. The hedge is what decodes a V.23 burst too weak to classify.
            const double othM = (std::fabs(toneMark - 1200.0) < 50.0) ? 1300.0 : 1200.0;
            const double othS = (std::fabs(toneMark - 1200.0) < 50.0) ? 2100.0 : 2200.0;

            // Hedge only while the channel's pair is genuinely unknown. Once a few bursts
            // have classified cleanly onto the live pair with none near the other, an
            // unmeasurable burst almost certainly belongs to the same population, and
            // hedging it doubles the replay for nothing - long enough on a busy satellite
            // pass that queued replays go stale and real payloads are lost.
            int known = 0, othEvidence = 0;

            for (int t = 0; t < 2; t++)
            {
                for (double v : m_tonePairHist[t])
                {
                    if (std::fabs(v - (t ? toneSpace : toneMark)) < 60.0) {
                        known++;
                    }
                    if (std::fabs(v - (t ? othS : othM)) < 60.0) {
                        othEvidence++;
                    }
                }
            }

            // Deliberately NOT gated on how many frames have already decoded on this pair.
            // Confirming the channel from decodes is self-reinforcing: with the hedge off, a
            // station on the other pair can never be decoded, so it can never reset the
            // confirmation, and it is invisible forever. Tried and measured - it is worth
            // about 15% throughput and costs both V.23 bursts in weak_signal_2026-08-01,
            // which is mostly Bell 202. Probing every 8th replay instead was too sparse to
            // find them. The replay only runs at all when the live pair has already failed on
            // this burst, so trying the other pair there is exactly the right thing to do;
            // the CPU is bought back by skipping the replay entirely when the live chains
            // succeeded, which is the common case once the pair has been learned.
            bool channelKnown = (known >= 3) && (othEvidence == 0);
            double replayToneMark = toneMark;
            double replayToneSpace = toneSpace;
            double replayTone2Mark = channelKnown ? 0.0 : othM;
            double replayTone2Space = channelKnown ? 0.0 : othS;

            if ((toneBurst[0] > 0.0) && (toneBurst[1] > 0.0))
            {
                double dLive = std::fabs(toneBurst[0] - toneMark)
                    + std::fabs(toneBurst[1] - toneSpace);
                double dOth = std::fabs(toneBurst[0] - othM)
                    + std::fabs(toneBurst[1] - othS);

                if (dLive < 50.0)
                {
                    replayTone2Mark = 0.0;      // confidently the live pair
                    replayTone2Space = 0.0;
                }
                else if ((dOth < 120.0) && (dOth < dLive))
                {
                    replayToneMark = othM;      // confidently the other pair
                    replayToneSpace = othS;
                    replayTone2Mark = 0.0;
                    replayTone2Space = 0.0;
                }
            }

            // The replay exists to recover a burst the live chains could not. If they
            // already decoded a valid frame from it, arming up to 320 chains recovers
            // almost nothing - and on a busy channel of strong packets that is most
            // bursts, so this is worth real CPU for no loss of payloads.
            if (!m_estBurstLiveDecodes)
            {
                startBurstReplay(m_estBurstStart, m_sampleCount, period, carrier,
                                 devMark, devSpace, replayToneMark, replayToneSpace,
                                 replayTone2Mark, replayTone2Space, rates);
            }
        }
    }

    // A CRC-valid frame is ground truth about which modem the transmitter used, and it is the
    // only reliable source. The tone correlators cannot classify a mismatched pair on real weak
    // traffic: measured on the V.23 recording, mark reads a power ratio of 3.9-4.9 - under any
    // gate that keeps noise out - and its frequency estimate scatters over 1041-1258 Hz. So the
    // live chains could never learn the channel that way, and on an all-V.23 channel they stayed
    // on Bell 202, failed every burst, and left the replay to do all the work with both tone
    // pairs. That is 320 detectors on every single burst, which is what overflowed the FIFO.
    //
    // Learning from decodes inverts that: after two frames recovered on the other pair, the live
    // chains move there, decode directly, and the replay is skipped entirely for the traffic that
    // used to be most expensive.
    // A replay decode says what symbol rate actually worked. Learn from it the way the tone
    // pair is learned - two frames agreeing before the live chains are moved - because that
    // is the only rate measurement available below the FM threshold.
    void noteReplayDecodeRate()
    {
        if (m_replayChainPeriod <= 0.0) {
            return;
        }

        double ppm = (m_replayChainPeriod / (double) m_samplesPerSymbol - 1.0) * 1e6;

        // Already running this rate, within the grid step that would have found it anyway
        if (std::fabs(ppm - m_estRatePpm) < 0.5 * PACKETDEMOD_REPLAY_RATE_STEP)
        {
            m_learnedRateCount = 0;
            return;
        }

        // Two decodes have to agree, and agree with each other, not merely disagree with
        // what is applied - one frame recovered at an edge of the rate grid is as likely to
        // be that burst's own jitter as the transmitter's clock
        if ((m_learnedRateCount == 0)
            || (std::fabs(ppm - m_learnedRatePpm) > 0.5 * PACKETDEMOD_REPLAY_RATE_STEP))
        {
            m_learnedRatePpm = ppm;
            m_learnedRateCount = 1;
            return;
        }

        qDebug() << "PacketDemodCore: two frames recovered at" << ppm
                 << "ppm - moving the live chains there from" << m_estRatePpm;

        m_estRatePpm = 0.5 * (ppm + m_learnedRatePpm);
        m_learnedRateCount = 0;

        applyEstimatedTuning((double) m_samplesPerSymbol * (1.0 + m_estRatePpm * 1e-6),
                             m_estCarrier, m_estDevMark, m_estDevSpace,
                             m_estToneMark, m_estToneSpace);
    }

    void noteReplayDecodeTones()
    {
        if ((m_replayChainToneMark <= 0.0)
            || ((std::fabs(m_replayChainToneMark - m_estToneMark) < 40.0)
                && (std::fabs(m_replayChainToneSpace - m_estToneSpace) < 40.0)))
        {
            m_altPairDecodes = 0;       // decoded on the pair we are already using
            return;
        }

        if (++m_altPairDecodes < 2) {
            return;
        }

        qDebug() << "PacketDemodCore: two frames recovered at tones"
                 << m_replayChainToneMark << "/" << m_replayChainToneSpace
                 << "- moving the live chains there from"
                 << m_estToneMark << "/" << m_estToneSpace;

        m_altPairDecodes = 0;
        applyEstimatedTuning((double) m_samplesPerSymbol * (1.0 + m_estRatePpm * 1e-6),
                             m_estCarrier, m_estDevMark, m_estDevSpace,
                             m_replayChainToneMark, m_replayChainToneSpace);
    }

    Config m_cfg;
    PacketHandler m_onPacket;
    PacketDemodFramer m_framer;
    PacketDemodToneCorrelator m_correlator;
    int m_correlationLength = 0;
};

#endif // INCLUDE_PACKETDEMODCORE_H
