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
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstdlib>

static int dsdDebugLevel() { static int d = getenv("DSD_DEBUG") ? atoi(getenv("DSD_DEBUG")) : 0; return d; }
static bool dsdDebug() { return dsdDebugLevel() != 0; }

#include "dsddemodulator.h"
#include "dsdprotocoldmr.h"
#include "dsdprotocoldstar.h"
#include "dsdprotocolysf.h"
#include "dsdprotocolnxdn.h"
#include "dsdprotocoldpmr.h"
#include "dsdprotocolp25.h"
#include "dsdprotocolp25p2.h"
#include "dsdprotocolm17.h"

DSDDemodulator::DSDDemodulator() :
    m_protocolSelection(ProtocolAuto),
    m_rfBandwidth(12500.0f),
    m_narrowFilter(true),
    m_coherentFiltered(true),
    m_coherentGate(0.02),
    m_coherentRuns(0),
    m_coherentSkips(0),
    m_filterCutoffHz(0.0f),
    m_state(StateSearching),
    m_chain(nullptr),
    m_clock(nullptr),
    m_sps(10),
    m_isiSpan(2),
    m_history(2048),
    m_decoder(nullptr),
    m_lastScopeY(0.0f),
    m_gmskScopeY{},
    m_gmskScopeD{},
    m_gmskScopePresent{},
    m_gmskScopeCount(0),
    m_lastScopeFrequency(0.0f),
    m_discriminatorEnabled(false),
    m_coherent(false),
    m_iqRing(16384),
    m_sampleIndex(0),
    m_symbolIndex(0),
    m_lastSymbolIndex(-1),
    m_lastSymbolCentre(0.0),
    m_inverted(false),
    m_misses(0),
    m_silentBursts(0),
    m_confirmed(0),
    m_nextCallId(0),
    m_callId(0),
    m_protocolSequence(0),
    m_protocolPublished(false),
    m_protocolSuppressed(false),
    m_framesDiscarded(0),
    m_lockIndex(0),
    m_rejectedDecoder(nullptr),
    m_rejectedUntil(0),
    m_powerRing(16384, 0.0f),
    m_blockPower(0.0f),
    m_blockCount(0),
    m_blockHistory(200, 0.0f),
    m_blockIndex(0),
    m_blocksFilled(0),
    m_noiseFloor(0.0f),
    m_signalPower(0.0f),
    m_lastSyncIndex(-1),
    m_lastSyncCentre(0.0),
    m_expectedSyncIndex(-1),
    m_expectedPending(false)
{
    // Harness comparisons: the front end without its DC blocker, or with the slow estimate alone
    m_frontEnd.setDCBlock(getenv("DSD_NO_DC_BLOCK") == nullptr);
    m_frontEnd.setDropDetection(getenv("DSD_NO_DC_DROP") == nullptr);

    m_decoders.push_back(new DSDProtocolDMR(m_history));
    m_decoders.push_back(new DSDProtocolDStar(m_history));
    m_decoders.push_back(new DSDProtocolYSF(m_history));
    m_decoders.push_back(new DSDProtocolNXDN(m_history, 2400));
    m_decoders.push_back(new DSDProtocolNXDN(m_history, 4800));
    m_decoders.push_back(new DSDProtocolDPMR(m_history));
    m_decoders.push_back(new DSDProtocolP25(m_history));
    m_decoders.push_back(new DSDProtocolP25P2(m_history));
    m_decoders.push_back(new DSDProtocolM17(m_history));

    // One chain per pulse shape and symbol rate
    Chain *rrc4800 = new Chain();
    rrc4800->m_shaping = ShapingRRC;
    rrc4800->m_sps = 10;
    Chain *rrc2400 = new Chain();
    rrc2400->m_shaping = ShapingRRC;
    rrc2400->m_sps = 20;
    Chain *rrc6000 = new Chain();
    rrc6000->m_shaping = ShapingRRC;
    rrc6000->m_sps = 8;
    // (P25 Phase 2's alone. Its H-DQPSK and H-CPM are no RRC shaped four level FM, the coherent
    // detector's model, yet the detector gains it more than any other protocol: 94 exact frames of
    // 180 to 153 at 8 dB, tests/README.md. Kept)
    Chain *gmsk = new Chain();
    gmsk->m_shaping = ShapingGaussian;
    gmsk->m_sps = 10;
    // M17 shapes with a roll off of 0.5, so its matched filter is its own chain
    Chain *rrc4800m17 = new Chain();
    rrc4800m17->m_shaping = ShapingRRC;
    rrc4800m17->m_sps = 10;
    rrc4800m17->m_alpha = DVM17::RrcAlpha;
    m_chains.push_back(rrc4800);
    m_chains.push_back(rrc2400);
    m_chains.push_back(rrc6000);
    m_chains.push_back(gmsk);
    m_chains.push_back(rrc4800m17);

    for (DSDProtocol *d : m_decoders)
    {
        Chain *chain = !d->isFourLevel() ? gmsk
            : (std::string(d->getName()) == "M17") ? rrc4800m17
            : (d->getSymbolRate() == 2400) ? rrc2400 : (d->getSymbolRate() == 6000) ? rrc6000 : rrc4800;
        chain->m_decoders.push_back(d);
    }

    rebuild();
}

DSDDemodulator::~DSDDemodulator()
{
    for (DSDProtocol *d : m_decoders) {
        delete d;
    }

    for (Chain *c : m_chains) {
        delete c;
    }
}

void DSDDemodulator::setProtocol(Protocol protocol)
{
    m_protocolSelection = protocol;

    // Angel61's sessions on the IQ: the selected profile's alone (the FM chains idle), or in Auto
    // NB's and WB's beside the FM chains (HF is only ever selected)
    std::vector<DVAngel61::Profile> profiles;

    if (protocol == ProtocolAuto) {
        profiles = { DVAngel61::ProfileNB, DVAngel61::ProfileWB };
    } else if (protocol == ProtocolAngel61) {
        profiles = { DVAngel61::ProfileNB };
    } else if (protocol == ProtocolAngel61WB) {
        profiles = { DVAngel61::ProfileWB };
    } else if (protocol == ProtocolAngel61HFNB) {
        profiles = { DVAngel61::ProfileHFNB };
    } else if (protocol == ProtocolAngel61HFWB) {
        profiles = { DVAngel61::ProfileHFWB };
    }

    bool same = profiles.size() == m_angel61Sessions.size();

    for (size_t i = 0; same && (i < profiles.size()); i++) {
        same = m_angel61Sessions[i]->getProfile() == profiles[i];
    }

    if (!same)
    {
        m_angel61Sessions.clear();

        for (DVAngel61::Profile profile : profiles)
        {
            if (DVAngel61::isHF(profile)) {
                m_angel61Sessions.emplace_back(new DSDAngel61HF(profile));
            } else {
                m_angel61Sessions.emplace_back(new DSDAngel61Vhf(profile));
            }

            m_angel61Sessions.back()->setScopeEnabled(true);
        }
    }

    m_angel61Selected = protocol != ProtocolAuto && !m_angel61Sessions.empty();
    m_angel61Holding = -1;

    rebuild();
}

void DSDDemodulator::setP25P2System(uint32_t wacn, uint16_t sysid, uint16_t nac)
{
    for (DSDProtocol *d : m_decoders)
    {
        DSDProtocolP25P2 *p2 = dynamic_cast<DSDProtocolP25P2*>(d);

        if (p2) {
            p2->setSystem(wacn, sysid, nac);
        }
    }
}

void DSDDemodulator::setRfBandwidth(float bandwidth)
{
    m_rfBandwidth = bandwidth;
    m_filterCutoffHz = 0.0f;
    updateChannelFilter();
}

void DSDDemodulator::updateChannelFilter()
{
    float search = std::max(4000.0f, std::min(24000.0f, m_rfBandwidth / 2.0f));
    float cutoff = search;

    if ((m_state == StateLocked) && m_narrowFilter && m_decoder)
    {
        // Occupied bandwidth of the signal locked to, from its measured deviation (Carson: the
        // peak deviation plus the modulation bandwidth, (1 + alpha) / 2 of the symbol rate for
        // root raised cosine 4FSK, 0.4 of it for GMSK at BT 0.5, which the loopback spectra
        // confirm as their 99.9% bandwidth), plus the frequency offset the AFC has yet to fold
        // in and a margin
        float modulation = m_decoder->isFourLevel() ? 0.6f : 0.4f;
        float half = m_slicer.getOuterLevel() + modulation * m_decoder->getSymbolRate();
        cutoff = half + std::fabs(m_slicer.getOffset()) + 300.0f;
        cutoff = std::max(2500.0f, std::min(search, cutoff));
    }

    // The filter keeps its state across a rebuild, but its output takes a filter length to
    // settle: only move it for a worthwhile change
    if (std::fabs(cutoff - m_filterCutoffHz) > 300.0f)
    {
        m_frontEnd.setBandwidth(cutoff);
        m_filterCutoffHz = cutoff;

        if (dsdDebug()) {
            fprintf(stderr, "DSD channel filter cutoff %.0f Hz\n", cutoff);
        }
    }
}

const DSDProtocol *DSDDemodulator::getDecoder(Protocol protocol) const
{
    // Indexed by Protocol (Angel61 has no FM decoder: its receivers are separate)
    static const char *names[] = { "", "DMR", "D-STAR", "YSF", "NXDN48", "NXDN96", "dPMR", "P25", "P25P2", "M17", "Angel61", "Angel61 WB",
        "Angel61 HF-NB", "Angel61 HF-WB" };
    static_assert(sizeof(names) / sizeof(names[0]) == ProtocolAngel61HFWB + 1, "a name for every protocol");

    for (DSDProtocol *d : m_decoders)
    {
        if (std::string(d->getName()) == names[protocol]) {
            return d;
        }
    }

    return nullptr;
}

bool DSDDemodulator::decoderEnabled(const DSDProtocol *decoder) const
{
    if (m_protocolSelection == ProtocolAuto) {
        return true;
    }

    return decoder == getDecoder(m_protocolSelection);
}

std::vector<float> DSDDemodulator::gmskKernel(int sps, double rxBt, int rxSpan)
{
    // Symbol spaced response of the transmit pulse (NRZ through a Gaussian filter of BT 0.5, as
    // D-STAR radios shape it) followed by the receive filter, measured on an isolated symbol
    Gaussian<float> tx, rx;
    tx.create(0.5, 4, sps);
    rx.create(rxBt, rxSpan, sps);
    const int length = 12 * sps;
    std::vector<float> response(length);

    for (int i = 0; i < length; i++)
    {
        float x = (i >= 2 * sps) && (i < 3 * sps) ? 1.0f : 0.0f;
        response[i] = rx.filter(tx.filter(x));
    }

    int peak = (int) (std::max_element(response.begin(), response.end()) - response.begin());
    std::vector<float> kernel;
    float sum = 0.0f;

    for (int k = -2; k <= 2; k++)
    {
        int index = peak + k * sps;
        float v = ((index >= 0) && (index < length)) ? response[index] : 0.0f;
        kernel.push_back(v);
        sum += v;
    }

    for (float& v : kernel) {
        v /= sum;
    }

    return kernel;
}

void DSDDemodulator::buildChain(Chain& chain)
{
    // The matched filter taps are the impulse responses of the sdrbase pulse filters; the chain
    // runs them itself over a contiguous window (a fifth of the receiver's time went into their
    // ring buffer indexing)
    auto impulseResponse = [](auto& filter, int taps) {
        std::vector<float> h(taps);

        for (int i = 0; i < taps; i++) {
            h[i] = filter.filter(i == 0 ? 1.0f : 0.0f);
        }

        return h;
    };

    if (chain.m_shaping == ShapingRRC)
    {
        RootRaisedCosine<float> rrc;
        rrc.create(chain.m_alpha, 8, chain.m_sps, RootRaisedCosine<float>::Gain);
        chain.m_taps = impulseResponse(rrc, 8 * chain.m_sps + 1);
        chain.m_delay = 4 * chain.m_sps;
        chain.m_kernel.clear();

        if (!chain.m_coherentDetector) {
            chain.m_coherentDetector.reset(new DSDCoherentDetector(chain.m_sps, DSDFrontEnd::m_sampleRate, chain.m_alpha));
        }
    }
    else
    {
        const double bt = 0.3; // narrower than the transmit BT of 0.5: the discriminator noise rises with frequency and the refinement cancels the intersymbol interference
        const int span = 6;
        Gaussian<float> gaussian;
        gaussian.create(bt, span, chain.m_sps);
        chain.m_taps = impulseResponse(gaussian, span * chain.m_sps + 1);
        chain.m_delay = span * chain.m_sps / 2;
        chain.m_kernel = gmskKernel(chain.m_sps, bt, span);
    }

    chain.m_buffer.assign(2 * chain.m_taps.size(), 0.0f);
    chain.m_position = 0;

    std::vector<DVSync::Pattern> patterns;
    chain.m_enabled = false;
    chain.m_idle = false;

    for (DSDProtocol *d : chain.m_decoders)
    {
        if (!decoderEnabled(d)) {
            continue;
        }

        chain.m_enabled = true;
        std::vector<DVSync::Pattern> p = d->getSearchPatterns();
        patterns.insert(patterns.end(), p.begin(), p.end());
    }

    chain.m_searcher.setPatterns(patterns, chain.m_sps, chain.m_kernel);
    chain.m_searcher.setThreshold(0.75f);
    chain.m_clock.setSamplesPerSymbol(chain.m_sps);
}

void DSDDemodulator::rebuild()
{
    m_filterCutoffHz = 0.0f;
    updateChannelFilter();

    for (Chain *c : m_chains) {
        buildChain(*c);
    }

    reset();
}

void DSDDemodulator::reset()
{
    // Publish loss for a confirmed call before resetting its decoder and status.
    unlock();
    m_frontEnd.reset();
    m_frontEnd.setCorrection(0.0f);

    for (auto& session : m_angel61Sessions) {
        session->reset();
    }

    m_angel61Holding = -1;

    for (Chain *c : m_chains)
    {
        buildChain(*c);
        c->m_searcher.reset();
        c->m_clock.reset();
    }

    m_history.clear();
    m_sampleIndex = 0;
    m_rejectedDecoder = nullptr;
    m_rejectedUntil = 0;
    m_symbolIndex = 1000; // leaves room for the symbols sliced backwards at lock
    m_lastSymbolIndex = -1;
    m_lastSymbolCentre = 0.0;
    std::fill(m_powerRing.begin(), m_powerRing.end(), 0.0f);
    std::fill(m_iqRing.begin(), m_iqRing.end(), std::complex<float>(0.0f, 0.0f));
    m_blockPower = 0.0f;
    m_blockCount = 0;
    std::fill(m_blockHistory.begin(), m_blockHistory.end(), 0.0f);
    m_blockIndex = 0;
    m_blocksFilled = 0;
    m_noiseFloor = 0.0f;
    m_signalPower = 0.0f;
    m_frames.clear();
    m_pendingFrames.clear();
    m_pendingProtocolFrames.clear();
    m_scope.clear();
    m_lastScopeY = 0.0f;
    m_gmskScopeCount = 0;
    m_lastScopeFrequency = 0.0f;
    m_discriminator.clear();
    m_angel61Holding = -1;
    m_status = Status();
}

float DSDDemodulator::getScopeFullScale() const
{
    // an Angel61 constellation: its symbols at unit mean power (16-QAM's outer points at 0.95)
    if (m_angel61Selected || (m_angel61Holding >= 0)) {
        return 1.0f;
    }

    return m_decoder ? m_decoder->getNominalOuterLevel() : 1944.0f;
}

std::vector<DSDVoiceFrame> DSDDemodulator::takeFrames()
{
    std::vector<DSDVoiceFrame> frames;
    frames.swap(m_frames);
    return frames;
}

std::vector<DSDProtocolEvent> DSDDemodulator::takeProtocolEvents()
{
    std::vector<DSDProtocolEvent> events;
    events.swap(m_protocolEvents);
    return events;
}

std::vector<float> DSDDemodulator::takeDiscriminator()
{
    std::vector<float> samples;
    samples.swap(m_discriminator);
    return samples;
}

std::vector<std::pair<float, float>> DSDDemodulator::takeScopeSamples()
{
    std::vector<std::pair<float, float>> samples;
    samples.swap(m_scope);
    return samples;
}

DSDProtocol *DSDDemodulator::decoderFor(const Chain& chain, DVSync::Pattern pattern) const
{
    for (DSDProtocol *d : chain.m_decoders)
    {
        if (!decoderEnabled(d)) {
            continue;
        }

        for (DVSync::Pattern p : d->getSearchPatterns())
        {
            if (p == pattern) {
                return d;
            }
        }
    }

    return nullptr;
}

void DSDDemodulator::updateNoiseFloor(float power)
{
    m_blockPower += power;

    if (++m_blockCount >= 480)
    {
        m_blockHistory[m_blockIndex] = m_blockPower / m_blockCount;
        m_blockIndex = (m_blockIndex + 1) % (int) m_blockHistory.size();
        m_blocksFilled = std::min(m_blocksFilled + 1, (int) m_blockHistory.size());
        m_blockPower = 0.0f;
        m_blockCount = 0;
        float floor = 1e30f;

        for (int i = 0; i < m_blocksFilled; i++) {
            floor = std::min(floor, m_blockHistory[i]);
        }

        m_noiseFloor = floor;
    }
}

float DSDDemodulator::spanPower(int64_t syncIndex) const
{
    // Mean envelope power over the 24 symbols ending at the sync: one symbol is too noisy a judge
    float power = 0.0f;

    for (int k = 0; k < 24; k++) {
        power += symbolPower(sampleOfSymbol(syncIndex - k));
    }

    return power / 24.0f;
}

void DSDDemodulator::trackSignalPower(float power)
{
    // The signal power the presence test refers to follows the syncs and bursts found present,
    // half way each time: a fade of a few dB between bursts is followed, a silent slot never
    // reaches here
    m_signalPower = (m_signalPower > 0.0f) ? 0.5f * (m_signalPower + power) : power;
}

float DSDDemodulator::presenceThreshold() const
{
    // The noise floor is the quietest 10 ms of the last two seconds. A transmission longer than
    // that raises it to the signal level, where it says nothing about the noise: the threshold
    // then falls back to a fraction of the signal power tracked over the recent syncs and bursts
    // (a carrier 12 dB down on it is taken as absent; such a carrier has no silent slots to tell
    // apart, the fallback only has to notice it going away, and the signal of a moving station
    // fades by that much between one burst and the next). The fallback is not used while the
    // floor looks like noise, since at a low signal to noise ratio it would put the noise of a
    // silent slot above it. The power was once measured over one symbol at the lock alone: locked
    // at a peak of a fading base station, a quarter of its voice bursts were then "silent".
    float threshold = m_presenceFactor * m_noiseFloor;

    if ((m_state == StateLocked) && (m_signalPower > 0.0f) && (threshold > 0.8f * m_signalPower)) {
        threshold = m_signalPower / 16.0f;
    }

    return threshold + 1e-12f;
}

float DSDDemodulator::symbolPower(double centre) const
{
    // Mean envelope power over one symbol around the strobe (the matched filter output index
    // lags the envelope by the filter delay)
    int delay = m_chain ? m_chain->m_delay : 4 * m_sps;
    int64_t first = std::llround(centre) - delay - m_sps / 2;
    float sum = 0.0f;

    for (int i = 0; i < m_sps; i++)
    {
        int64_t k = first + i;
        sum += (k >= 0) ? m_powerRing[k % (long long) m_powerRing.size()] : 0.0f;
    }

    return sum / m_sps;
}

int64_t DSDDemodulator::symbolIndexNear(double sample) const
{
    double symbols = (sample - m_lastSymbolCentre) / (m_sps + m_clock->getRate());
    return m_lastSymbolIndex + std::llround(symbols);
}

double DSDDemodulator::sampleOfSymbol(int64_t index) const
{
    return m_lastSymbolCentre + (index - m_lastSymbolIndex) * (m_sps + m_clock->getRate());
}

void DSDDemodulator::process(const std::complex<float> *input, int n)
{
    // The receiving SDR's DC off first, for everything: the Angel61 sessions took the IQ before
    // the front end, and a DC spike at the channel centre as strong as a weak signal stopped them
    // decoding anything (their UW metric and channel estimates take it for signal)
    m_dcFree.resize(n);

    for (int i = 0; i < n; i++) {
        m_dcFree[i] = m_frontEnd.removeDC(input[i]);
    }

    const std::complex<float> *iq = m_dcFree.data();

    if (m_angel61Selected)
    {
        DSDAngel61Session& session = *m_angel61Sessions.front();
        session.process(iq, n);

        for (const DSDVoiceFrame& voice : session.takeVoice()) {
            m_frames.push_back(voice);
        }

        scopeAngel61(session);
        return;
    }

    if (!m_angel61Sessions.empty()) {
        processAngel61(iq, n);
    }

    for (int i = 0; i < n; i++)
    {
        float frequency, power;
        m_frontEnd.processDCFree(iq[i], frequency, power);
        m_powerRing[m_sampleIndex % (long long) m_powerRing.size()] = power;

        float previousScopeFrequency = m_lastScopeFrequency;
        m_lastScopeFrequency = frequency;

        // the discriminator while searching, unless an Angel61 transmission holds the channel (its
        // constellation instead)
        if ((m_state == StateSearching) && (m_angel61Holding < 0) && (m_scope.size() < 4096))
        {
            m_scope.push_back(std::make_pair(frequency, previousScopeFrequency));
        }

        if (m_coherent) {
            m_iqRing[m_sampleIndex % (long long) m_iqRing.size()] = m_coherentFiltered ? m_frontEnd.getFiltered() : m_frontEnd.getRotated();
        }

        if (m_discriminatorEnabled && (m_discriminator.size() < 96000)) {
            m_discriminator.push_back(frequency);
        }

        updateNoiseFloor(power);

        // While a decoder holds a confirmed lock without a miss, no other chain's detection can
        // take over (handleDetection): the other chains are not run at all. When one resumes,
        // its matched filter has a gap in its history: its searcher waits for the filter to settle
        // before fitting again. All of them idle while an Angel61 transmission holds the channel
        const bool angel61 = m_angel61Holding >= 0;
        const bool othersIdle = angel61 || ((m_state == StateLocked) && (m_confirmed > 0) && (m_misses == 0));

        for (Chain *chain : m_chains)
        {
            if (!chain->m_enabled) {
                continue;
            }

            if (othersIdle && (angel61 || (chain != m_chain)))
            {
                chain->m_idle = true;
                continue;
            }

            if (chain->m_idle)
            {
                chain->m_idle = false;
                chain->m_searcher.reset(2 * chain->m_delay + 1);
                chain->m_clock.reset();
            }

            float y = chain->filter(frequency);
            chain->m_clock.push(y, m_sampleIndex);
            DSDSyncSearcher::Detection detection;
            bool detected = chain->m_searcher.push(y, m_sampleIndex, detection);

            if ((m_state == StateLocked) && (chain == m_chain))
            {
                while ((m_state == StateLocked) && m_clock->ready()) {
                    sliceSymbol(); // may unlock
                }
            }

            if (detected) {
                handleDetection(detection, chain);
            }
        }

        // (and once the symbols the decoder confirms a burst by are in: most end at the sync, but
        // M17's payload follows its sync, and asked 3 symbols after it M17 could never confirm)
        int64_t confirmFirst = 0, confirmLast = 0;

        if ((m_state == StateLocked) && m_expectedPending) {
            m_decoder->getConfirmSpan(m_expectedSyncIndex, confirmFirst, confirmLast);
        }

        if ((m_state == StateLocked) && m_expectedPending && (m_lastSymbolIndex >= m_expectedSyncIndex + 3)
            && (m_lastSymbolIndex >= confirmLast))
        {
            // No detection around the expected sync: let the decoder confirm the burst otherwise
            // Carrier presence over the sync span (a single symbol's power is too noisy a judge)
            float power = spanPower(m_expectedSyncIndex);
            bool present = power > presenceThreshold();
            // The level of a signal through an overloaded or gain pumping front end swings by 10 dB
            // from one burst to the next (a recording of fifty DMR carriers did): on a proven lock a
            // burst the power calls absent is put to the decoder anyway, which judges it strictly
            // (DMR: a near exact EMB with the known colour code); one that fails is silent as before
            bool doubtful = !present && m_status.m_locked;
            m_expectedPending = false;

            if (dsdDebug()) {
                fprintf(stderr, "DSD no sync at symbol %lld (present %d%s: power %.3g threshold %.3g floor %.3g signal %.3g)\n", (long long) m_expectedSyncIndex, present,
                    doubtful ? ", doubtful" : "", power, presenceThreshold(), m_noiseFloor, m_signalPower);
            }

            if (present || doubtful)
            {
                // Refine the timing and levels of the symbols the decoder is about to examine
                int64_t first, last;
                m_decoder->getConfirmSpan(m_expectedSyncIndex, first, last);
                refineBurst(first, last);
            }

            m_decoder->setDoubtful(doubtful);
            bool confirmed = m_decoder->confirmWithoutSync(m_expectedSyncIndex, present || doubtful);
            m_decoder->setDoubtful(false);

            if (doubtful && !confirmed)
            {
                // Really silent: as if the decoder had not been asked
                confirmed = m_decoder->confirmWithoutSync(m_expectedSyncIndex, false);
                doubtful = false;
            }

            if (confirmed)
            {
                if (present || doubtful)
                {
                    m_misses = 0;
                    m_silentBursts = 0;
                    trackSignalPower(power);
                    confirmLock();
                }
                else if (++m_silentBursts >= 8)
                {
                    unlock(); // carrier gone
                }
            }
            else if (++m_misses >= 5)
            {
                unlock();
            }
        }

        if ((m_state == StateLocked) && !m_expectedPending)
        {
            int64_t index;
            std::vector<DVSync::Pattern> patterns;

            if (m_decoder->getNextSync(index, patterns))
            {
                m_expectedSyncIndex = index;
                m_expectedPatterns = patterns;
                m_expectedPending = true;
            }
            else
            {
                unlock();
            }
        }

        m_sampleIndex++;
    }
}

void DSDDemodulator::processAngel61(const std::complex<float> *iq, int n)
{
    // Auto: every session on the block. A transmission confirmed (a frame through its CRC-12) takes
    // the channel from an FM lock (the discriminator output of a QPSK signal can match an FSK sync
    // by chance) and holds it until it ends; the other session's frames meanwhile are dropped.
    // An FM lock its decoder confirmed and that has missed no sync is a transmission on the channel,
    // as for the other FM chains (process): the sessions skip the block rather than search it, which
    // was most of the receiver's time on an FM signal in Auto
    if ((m_angel61Holding < 0) && (m_state == StateLocked) && (m_confirmed > 0) && (m_misses == 0))
    {
        for (auto& session : m_angel61Sessions) {
            session->skip(n);
        }

        return;
    }

    for (auto& session : m_angel61Sessions) {
        session->process(iq, n);
    }

    // The block's voice of the session that held the channel goes out even as it ends: V1's flush
    const int held = m_angel61Holding;

    if ((m_angel61Holding >= 0) && !m_angel61Sessions[m_angel61Holding]->isActive())
    {
        m_angel61Holding = -1;

        if (m_state != StateLocked)
        {
            m_status.m_locked = false;
            m_status.m_protocol.clear();
        }
    }

    for (int i = 0; (m_angel61Holding < 0) && (i < (int) m_angel61Sessions.size()); i++)
    {
        if (!m_angel61Sessions[i]->isActive()) {
            continue;
        }

        if (m_state == StateLocked) {
            unlock();
        }

        m_angel61Holding = i;
        m_scope.clear();
        m_status.m_locked = true;
        m_status.m_protocol = m_angel61Sessions[i]->getProfile() == DVAngel61::ProfileWB ? "Angel61 WB" : "Angel61 NB";
        m_status.m_lastPattern = "UW";
        m_status.m_inverted = false;
        m_status.m_outerLevelHz = 0.0f;
    }

    for (int i = 0; i < (int) m_angel61Sessions.size(); i++)
    {
        std::vector<DSDVoiceFrame> voice = m_angel61Sessions[i]->takeVoice();

        if ((i == m_angel61Holding) || (i == held)) {
            m_frames.insert(m_frames.end(), voice.begin(), voice.end());
        }

        if (i == m_angel61Holding) {
            scopeAngel61(*m_angel61Sessions[i]);
        } else {
            m_angel61Sessions[i]->takeScopeSymbols();   // not shown: dropped
        }
    }
}

int DSDDemodulator::getScopeSampleRate() const
{
    // Angel61's points are the equalised payload cells of its accepted frames (scopeAngel61): one
    // frame's every 80 ms (VHF) or 240 ms (HF). Reported as the 48 kHz of a search, the GUI sized a
    // redraw at 14400 points (the default 300 ms trace), and NB's constellation moved every 4 s
    if (const DSDAngel61Session *session = getAngel61Holding())
    {
        const DVAngel61::Profile profile = session->getProfile();
        return std::max(1, DVAngel61::layout(profile).payloadCount() * 1000 / DVAngel61::frameMs(profile));
    }

    return (m_state == StateLocked) ? 48000 / m_sps : 48000;
}

void DSDDemodulator::scopeAngel61(DSDAngel61Session& session)
{
    // the equalised payload symbols as I against Q, in units of getScopeFullScale's 1
    for (const std::complex<float>& z : session.takeScopeSymbols())
    {
        if (m_scope.size() >= 4096) {
            break;
        }

        m_scope.push_back(std::make_pair(z.real(), z.imag()));
    }
}

std::vector<DSDAngel61Session*> DSDDemodulator::getAngel61Sessions()
{
    std::vector<DSDAngel61Session*> sessions;

    for (auto& session : m_angel61Sessions) {
        sessions.push_back(session.get());
    }

    return sessions;
}

DSDAngel61Session *DSDDemodulator::getAngel61Holding()
{
    return m_angel61Selected ? m_angel61Sessions.front().get()
        : m_angel61Holding >= 0 ? m_angel61Sessions[m_angel61Holding].get() : nullptr;
}

const DSDAngel61Session *DSDDemodulator::getAngel61Holding() const
{
    return m_angel61Selected ? m_angel61Sessions.front().get()
        : m_angel61Holding >= 0 ? m_angel61Sessions[m_angel61Holding].get() : nullptr;
}

void DSDDemodulator::handleDetection(const DSDSyncSearcher::Detection& detection, Chain *chain)
{
    DSDProtocol *decoder = decoderFor(*chain, detection.m_pattern);

    if (!decoder) {
        return;
    }

    // A decoder whose lock the constellation check just rejected is ignored for a second: the data
    // of another protocol matches its syncs again and again (M17's in D-STAR's GMSK, rho 0.99, about
    // every 30 ms), and each new lock hid that protocol's own syncs for 120 symbols until rejected
    if ((decoder == m_rejectedDecoder) && (m_sampleIndex < m_rejectedUntil)) {
        return;
    }

    float nominal = decoder->getNominalOuterLevel() / 3.0f;
    // Radios deviate less or more than nominal, and a recording replayed through the harness has
    // a guessed scale: the level only has to be within a factor of about 3 of nominal
    bool plausibleLevel = (detection.m_scale > 0.3f * nominal) && (detection.m_scale < 2.0f * nominal);
    double centre = detection.m_sample + detection.m_fraction;

    // A short pattern (NXDN FSW, dPMR FS2) fits noise too easily: it needs a near perfect fit,
    // and the 19 symbol NXDN preamble FSW a better one than the 24 symbol patterns
    int length = DVSync::get(detection.m_pattern).m_length;
    float lockRho = length >= 24 ? decoder->getLockThreshold() : length >= 19 ? 0.9f : 0.95f;

    // The correlation of a short pattern of outer levels says little (its template variance
    // swamps the residual): noise alone reaches 0.97 on the 10 symbol NXDN FSW. The residual
    // relative to the level is the better judge there: a real signal at the edge of coverage
    // (6 dB) sits near 0.5, noise near 0.8 and above
    if ((length < 24) && (std::sqrt(detection.m_sigma2) > 0.7f * detection.m_scale)) {
        plausibleLevel = false;
    }

    if (m_state == StateSearching)
    {
        if ((std::fabs(detection.m_rho) >= lockRho) && plausibleLevel) {
            lock(detection, chain, decoder);
        } else if (dsdDebugLevel() >= 2) {
            fprintf(stderr, "DSD candidate: %s at sample %lld rho %.3f scale %.1f (nominal %.1f) offset %.1f\n",
                DVSync::get(detection.m_pattern).m_name, (long long) detection.m_sample, detection.m_rho, detection.m_scale, nominal, detection.m_offset);
        }

        return;
    }

    // Another decoder may take over only while this one has not proved itself: a decoder whose
    // syncs or bursts are confirmed is receiving the transmission, and the data of one protocol
    // now and then looks like a sync of another
    // A binary lock (D-STAR) is not taken over by a four level decoder until it has missed a sync:
    // D-STAR confirms only at its next sync, 420 ms on, and M17's syncs match in its GMSK data at
    // rho 0.99 about every 30 ms, so a real D-STAR transmission picked up mid-over never held (the
    // constellation check rejects such a four level lock, but the binary one was already gone)
    bool takeoverAllowed = (decoder != m_decoder) && ((m_confirmed == 0) || (m_misses > 0)) && (std::fabs(detection.m_rho) >= std::max(0.95f, lockRho))
        && (m_decoder->isFourLevel() || !decoder->isFourLevel() || (m_misses > 0)) && plausibleLevel;

    if (chain != m_chain)
    {
        if (takeoverAllowed)
        {
            unlock();
            lock(detection, chain, decoder);
        }

        return;
    }

    // Locked: accept a detection near the expected sync, or a strong new sync of another decoder
    int64_t index = symbolIndexNear(centre);
    bool expected = m_expectedPending && (std::labs(index - m_expectedSyncIndex) <= 2)
        && (std::find(m_expectedPatterns.begin(), m_expectedPatterns.end(), detection.m_pattern) != m_expectedPatterns.end());

    // A strong sync of the locked protocol elsewhere: the decoder may know why (a P25 trunking
    // unit of one to three blocks, a unit whose NID it could not read)
    if (!expected && (decoder == m_decoder) && (detection.m_inverted == m_inverted) && (std::fabs(detection.m_rho) >= lockRho) && plausibleLevel
        && m_decoder->acceptUnexpectedSync(detection.m_pattern, index))
    {
        expected = true;
        m_expectedPending = true;
        m_expectedSyncIndex = index;
    }

    if (expected && (std::fabs(detection.m_rho) >= DSDSyncSearcher::thresholdFor(0.7f, DVSync::get(detection.m_pattern).m_length))
        && (detection.m_inverted == m_inverted))
    {
        m_status.m_lastRho = detection.m_rho;
        m_status.m_lastPattern = DVSync::get(detection.m_pattern).m_name;
        m_status.m_syncCount++;
        m_misses = 0;
        m_silentBursts = 0;
        m_expectedPending = false;
        trackSignalPower(spanPower(index));

        // A sync where one was expected confirms the lock when it is hard to match by chance: a
        // long pattern, or a short one as good as at the lock (a 10 symbol NXDN FSW reaches 0.7
        // in noise easily, and confirmed noise locks with it, playing their frames). Otherwise the
        // decoder's own checks (LICH, SACCH, EMB, NID...) confirm the lock
        if ((length >= 19) || (std::fabs(detection.m_rho) >= lockRho)) {
            confirmLock();
        }
        // The drift since the previous sync measures the clock rate error; the loop then only has
        // to follow the residual. Realign the clock and refresh the levels from the fit.
        if ((m_lastSyncIndex >= 0) && (index - m_lastSyncIndex >= 100))
        {
            double measured = (centre - m_lastSyncCentre) / (double) (index - m_lastSyncIndex) - m_sps;
            m_clock->setRate(0.5 * (m_clock->getRate() + measured));
        }

        int symbols;
        m_clock->takeCorrection(symbols);

        m_lastSyncIndex = index;
        m_lastSyncCentre = centre;
        m_clock->setPhase(centre);
        m_lastSymbolCentre = centre;
        m_lastSymbolIndex = index;
        m_symbolIndex = index + 1;
        float outer = 3.0f * detection.m_scale;
        m_slicer.initialise(0.5f * (m_slicer.getOuterLevel() + outer), 0.5f * (m_slicer.getOffset() + detection.m_offset), m_slicer.getSigma2());
        // Fold a quarter of the residual offset into the front end correction
        float delta = 0.25f * m_slicer.getOffset();
        float bound = m_rfBandwidth / 4.0f;
        float correction = std::max(-bound, std::min(bound, m_frontEnd.getCorrection() + delta));
        delta = correction - m_frontEnd.getCorrection();
        m_frontEnd.setCorrection(correction);
        m_slicer.setOffset(m_slicer.getOffset() - delta);
        updateChannelFilter();
        m_decoder->resync(detection.m_pattern, index);

        if (dsdDebug()) {
            fprintf(stderr, "DSD resync: %s at sample %lld symbol %lld (expected %lld) rho %.3f outer %.1f offset %.1f corr %.1f rate %.4f power %.3g\n",
                DVSync::get(detection.m_pattern).m_name, (long long) detection.m_sample, (long long) index, (long long) m_expectedSyncIndex, detection.m_rho,
                m_slicer.getOuterLevel(), m_slicer.getOffset(), m_frontEnd.getCorrection(), m_clock->getRate(), spanPower(index));
        }
    }
    else if (!expected && takeoverAllowed)
    {
        unlock();
        lock(detection, chain, decoder);
    }
    else if (dsdDebug() && m_expectedPending)
    {
        // (logged after the takeover's test: before it, DSD_DEBUG stopped a takeover it only meant to report)
        fprintf(stderr, "DSD detection ignored: %s at sample %lld symbol %lld (expected %lld) rho %.3f inverted %d\n",
            DVSync::get(detection.m_pattern).m_name, (long long) detection.m_sample, (long long) index, (long long) m_expectedSyncIndex, detection.m_rho, detection.m_inverted);
    }
}

void DSDDemodulator::lock(const DSDSyncSearcher::Detection& detection, Chain *chain, DSDProtocol *decoder)
{
    double centre = detection.m_sample + detection.m_fraction;
    m_scope.clear();
    m_lastScopeY = 0.0f;
    m_gmskScopeCount = 0;
    m_state = StateLocked;
    m_chain = chain;
    m_clock = &chain->m_clock;
    m_sps = chain->m_sps;
    m_decoder = decoder;
    m_inverted = detection.m_inverted;
    m_misses = 0;
    m_silentBursts = 0;
    m_confirmed = 0;
    m_callId = ++m_nextCallId;
    m_protocolSequence = 0;
    m_protocolPublished = false;
    m_protocolSuppressed = false;
    m_pendingProtocolFrames.clear();
    m_lockIndex = m_symbolIndex;
    m_clock->resetLoop();
    m_clock->setPhase(centre);
    m_slicer.setFourLevel(decoder->isFourLevel());
    m_slicer.initialise(3.0f * detection.m_scale, detection.m_offset, detection.m_sigma2);
    m_lastSymbolIndex = m_symbolIndex;
    m_lastSymbolCentre = centre;
    m_lastSyncIndex = m_lastSymbolIndex;
    m_lastSyncCentre = centre;
    m_symbolIndex++;
    m_signalPower = spanPower(m_lastSymbolIndex);
    m_expectedPending = false;
    // The lock shows in the status once confirmed (confirmLock): a chance match in noise dies
    // before it does
    m_lockPattern = detection.m_pattern;
    m_status.m_inverted = m_inverted;
    m_status.m_lastRho = detection.m_rho;
    m_status.m_syncCount++;

    if (m_status.m_firstSyncSample < 0) {
        m_status.m_firstSyncSample = detection.m_sample;
    }

    // Slice the symbols before and including the sync from the buffered matched filter output,
    // so that the decoder has the first half of the burst that carried the sync
    const int backfill = 200;
    double rate = m_sps;

    for (int k = backfill; k >= 0; k--)
    {
        double position = centre - k * rate;
        int64_t index = m_lastSymbolIndex - k;

        if ((position < m_clock->oldest() + 1) || (index < 0)) {
            continue;
        }

        float y = m_clock->interpolateAt(position);

        if (m_inverted) {
            y = -y;
        }

        float power = symbolPower(position);
        bool present = power > presenceThreshold();
        DSDSlicer::Symbol s = m_slicer.slice(y, present ? 1.0f : 0.05f, false);
        DSDSoftSymbol symbol;
        symbol.m_dibit = s.m_dibit;
        symbol.m_llrMsb = s.m_llrMsb;
        symbol.m_llrLsb = s.m_llrLsb;
        symbol.m_reliability = s.m_reliability;
        symbol.m_present = present;
        m_history.push(index, symbol, position);
    }

    updateChannelFilter();

    if (dsdDebug()) {
        fprintf(stderr, "DSD lock: pattern %s at sample %lld (+%.2f) rho %.3f scale %.1f offset %.1f sigma %.1f symbol %lld\n",
            DVSync::get(detection.m_pattern).m_name, (long long) detection.m_sample, detection.m_fraction, detection.m_rho, detection.m_scale,
            detection.m_offset, std::sqrt(detection.m_sigma2), (long long) m_lastSymbolIndex);
    }

    decoder->start(detection.m_pattern, m_lastSymbolIndex);
}

void DSDDemodulator::unlock()
{
    if (dsdDebug() && (m_state == StateLocked)) {
        fprintf(stderr, "DSD unlock at sample %lld symbol %lld misses %d silent %d\n", (long long) m_sampleIndex, (long long) m_lastSymbolIndex, m_misses, m_silentBursts);
    }

    if (m_decoder && m_protocolPublished)
    {
        DSDProtocolEvent event;
        event.m_kind = m_decoder->endedNormally() ? DSDProtocolEvent::End : DSDProtocolEvent::Lost;
        event.m_callId = m_callId;
        event.m_sequence = m_protocolSequence++;
        event.m_sample = m_sampleIndex;
        event.m_protocol = m_status.m_protocol;
        event.m_reason = event.m_kind == DSDProtocolEvent::Lost ? "receiver lock lost" : std::string();
        m_protocolEvents.push_back(event);
    }

    if (m_decoder) {
        m_decoder->end();
    }

    m_scope.clear();
    m_state = StateSearching;
    m_decoder = nullptr;
    m_chain = nullptr;
    m_clock = nullptr;
    m_expectedPending = false;
    m_misses = 0;

    if (m_angel61Holding < 0)
    {
        m_status.m_locked = false;
        m_status.m_protocol.clear();
    }

    // A lock that was never confirmed was a chance match: its voice frames were noise
    m_framesDiscarded += (long long) m_pendingFrames.size();
    m_pendingFrames.clear();
    m_pendingProtocolFrames.clear();
    m_protocolPublished = false;
    m_protocolSuppressed = false;
    updateChannelFilter();
}

void DSDDemodulator::confirmLock()
{
    m_confirmed++;

    // The lock shows and its voice frames go once it is confirmed and the decoder has ruled out
    // a chance match of the sync in noise (isProven: strong codes do at once, weak ones such as
    // the DMR EMB or the NXDN LICH and SACCH after two bursts)
    if (!m_status.m_locked && m_decoder->isProven())
    {
        if (dsdDebug()) {
            fprintf(stderr, "DSD confirmed: %s at symbol %lld, %d voice frames held%c", m_decoder->getName(), (long long) m_lastSymbolIndex, (int) m_pendingFrames.size(), 10);
        }

        m_status.m_locked = true;
        m_status.m_protocol = m_decoder->getName();
        m_status.m_lastPattern = DVSync::get(m_lockPattern).m_name;
        if (!m_protocolSuppressed)
        {
            DSDProtocolEvent event;
            event.m_kind = DSDProtocolEvent::Start;
            event.m_callId = m_callId;
            event.m_sequence = m_protocolSequence++;
            event.m_sample = m_lockIndex;
            event.m_protocol = m_decoder->getName();
            m_protocolEvents.push_back(event);
            m_protocolPublished = true;

            for (const DSDProtocolFrame& frame : m_pendingProtocolFrames) {
                publishProtocolFrame(frame);
            }
        }

        m_pendingProtocolFrames.clear();
        // The voice frames held back since the lock
        m_frames.insert(m_frames.end(), m_pendingFrames.begin(), m_pendingFrames.end());
        m_pendingFrames.clear();
    }
}

void DSDDemodulator::publishProtocolFrame(const DSDProtocolFrame& frame)
{
    if (!m_protocolPublished || m_protocolSuppressed) {
        return;
    }

    DSDProtocolEvent event;
    event.m_kind = DSDProtocolEvent::Frame;
    event.m_callId = m_callId;
    event.m_sequence = m_protocolSequence++;
    event.m_sample = frame.m_sample;
    event.m_protocol = m_decoder ? m_decoder->getName() : m_status.m_protocol;
    event.m_frame = frame;
    m_protocolEvents.push_back(event);
}

DSDSoftSymbol DSDDemodulator::sliceAt(double position, bool track)
{
    float y = m_clock->interpolateAt(position);

    if (m_inverted) {
        y = -y;
    }

    return sliceValue(y, position, track);
}

DSDSoftSymbol DSDDemodulator::sliceValue(float y, double position, bool track)
{
    float power = symbolPower(position);
    bool present = power > presenceThreshold();

    if (present && track) {
        m_signalPower += 0.02f * (power - m_signalPower);
    }

    float weight = (m_signalPower > 0.0f) ? std::min(1.0f, power / m_signalPower) : 1.0f;

    if (!present) {
        weight = 0.05f;
    }

    DSDSlicer::Symbol s = m_slicer.slice(y, weight, present && track);

    if (track)
    {
        m_clock->setHold(!present);
        m_clock->update(s.m_u, s.m_level, present ? s.m_reliability : 0.0f);
    }

    DSDSoftSymbol symbol;
    symbol.m_dibit = s.m_dibit;
    symbol.m_llrMsb = s.m_llrMsb;
    symbol.m_llrLsb = s.m_llrLsb;
    symbol.m_reliability = s.m_reliability;
    symbol.m_present = present;
    return symbol;
}

void DSDDemodulator::refineBurst(int64_t first, int64_t last)
{
    // Data aided refinement over a burst (or the part of it received so far). The symbol instants
    // of the first pass are replaced by a straight line through them (the loop's wander within
    // the burst is noise, its mean drift is the clock error), then the offset that maximises the
    // correlation of the matched filter output with the first pass decisions (searched over +-2
    // samples, parabola around the best) is measured over the burst, each symbol weighted by its
    // reliability so that the transients of a carrier ramp and the doubtful decisions count for
    // little; on a long burst the two halves give the residual slope as well. A regression of the
    // samples at the refined instants on the decided levels then gives the outer level, the DC
    // offset and the noise, and the burst is sliced again with those, so its first symbols do not
    // suffer from the drift since the previous burst. The drift between refinements gives the
    // clock rate.
    static const int levelOf[4] = { 1, 3, -1, -3 };
    const float outlier = 1.5f * m_slicer.getOuterLevel();

    if (!m_history.has(first) || !m_history.has(last)) {
        return;
    }

    // Straight line through the first pass instants
    double sumK = 0.0, sumKK = 0.0, sumP = 0.0, sumKP = 0.0;
    int nk = 0;

    for (int64_t k = first; k <= last; k++)
    {
        double kk = (double) (k - first);
        double pos = m_history.positionAt(k);
        sumK += kk;
        sumKK += kk * kk;
        sumP += pos;
        sumKP += kk * pos;
        nk++;
    }

    double denominatorK = nk * sumKK - sumK * sumK;

    if (denominatorK <= 0.0) {
        return;
    }

    double period = (nk * sumKP - sumK * sumP) / denominatorK;
    double p0 = (sumP - period * sumK) / nk;

    if (std::fabs(period - m_sps) > 0.05 * m_sps) {
        return; // not a burst of this rate
    }

    auto lineAt = [&](int64_t k) { return p0 + period * (double) (k - first); };

    // Correlation of the samples around the line instants with the decisions
    const int span = 2; // search range in samples
    double c[2][2 * span + 1] = { { 0.0 }, { 0.0 } };
    double centreOf[2] = { 0.0, 0.0 };
    int count[2] = { 0, 0 };
    int64_t middle = (first + last + 1) / 2;
    // A position whose cubic interpolation (the sample after the next) is in the clock's buffer
    auto inClock = [this](double position) { return std::floor(position) + 2.0 <= (double) m_clock->newest(); };

    for (int64_t k = first; k <= last; k++)
    {
        const DSDSoftSymbol& s = m_history.at(k);

        if (!s.m_present || (s.m_reliability < 0.15f)) {
            continue;
        }

        double position = lineAt(k);

        if (position < m_clock->oldest() + span + 1) {
            return;
        }

        // The burst is refined as its last symbol is sliced: the probes around the last ones reach
        // samples not in yet (read as 0), which only bias the correlation
        if (!inClock(position + span)) {
            continue;
        }

        int half = k < middle ? 0 : 1;
        float d = (float) levelOf[s.m_dibit];
        float w = 0.25f + 0.75f * s.m_reliability;
        float y0 = m_clock->interpolateAt(position);

        if (std::fabs(y0 - m_slicer.getOffset()) > outlier) {
            continue; // carrier ramp transients and clicks: not symbols
        }

        for (int i = 0; i <= 2 * span; i++)
        {
            float y = m_clock->interpolateAt(position + (i - span));
            c[half][i] += w * (m_inverted ? -y : y) * d;
        }

        centreOf[half] += k;
        count[half]++;
    }

    if (count[0] + count[1] < 20) {
        return;
    }

    auto peakOf = [&](const double *cc) {
        int best = 0;

        for (int i = 1; i <= 2 * span; i++)
        {
            if (cc[i] > cc[best]) {
                best = i;
            }
        }

        double t = best - span;

        if ((best > 0) && (best < 2 * span))
        {
            double denominator = cc[best - 1] - 2.0 * cc[best] + cc[best + 1];
            t += (std::fabs(denominator) > 1e-9) ? std::max(-0.5, std::min(0.5, 0.5 * (cc[best - 1] - cc[best + 1]) / denominator)) : 0.0;
        }

        return t;
    };

    double whole[2 * span + 1];

    for (int i = 0; i <= 2 * span; i++) {
        whole[i] = c[0][i] + c[1][i];
    }

    double offset = peakOf(whole);
    double tau[2] = { offset, offset };
    double slope = 0.0;

    if ((count[0] >= 150) && (count[1] >= 150))
    {
        // Long burst: the halves resolve the residual slope
        tau[0] = peakOf(c[0]);
        tau[1] = peakOf(c[1]);
        centreOf[0] /= count[0];
        centreOf[1] /= count[1];
        slope = std::max(-0.01, std::min(0.01, (tau[1] - tau[0]) / (centreOf[1] - centreOf[0])));
    }

    auto tauAt = [&](int64_t k) { return offset + slope * (double) (k - middle); };
    // Refined instant of symbol k, and the movement of the last one relative to the first pass
    auto refinedAt = [&](int64_t k) { return lineAt(k) + tauAt(k); };
    double movement = refinedAt(last) - m_history.positionAt(last);

    // Regression at the refined instants: the sample is modelled as a linear combination of the
    // decided levels of the symbol and of its neighbours (the pulse response through the channel:
    // near zero for root raised cosine shaping, the Gaussian intersymbol interference for GMSK, an
    // echo otherwise) plus the DC offset. The centre tap is the level; the other taps are cancelled
    // from each sample before it is sliced again.
    const int L = m_isiSpan;
    const int P = 2 * L + 2;                    // taps plus the offset
    double ata[6][6] = { { 0.0 } };
    double aty[6] = { 0.0 };
    double sumYY = 0.0;
    int n = 0;
    auto levelAt = [&](int64_t k) -> double { return m_history.has(k) ? (double) levelOf[m_history.at(k).m_dibit] : 0.0; };

    for (int64_t k = first; k <= last; k++)
    {
        const DSDSoftSymbol& s = m_history.at(k);

        if (!s.m_present) {
            continue;
        }

        if (!inClock(refinedAt(k))) {
            continue;
        }

        float y = m_clock->interpolateAt(refinedAt(k));
        y = m_inverted ? -y : y;

        if (std::fabs(y - m_slicer.getOffset()) > outlier) {
            continue;
        }

        double x[6];

        for (int j = -L; j <= L; j++) {
            x[j + L] = levelAt(k + j);
        }

        x[P - 1] = 1.0;

        for (int r = 0; r < P; r++)
        {
            aty[r] += x[r] * y;

            for (int c = 0; c < P; c++) {
                ata[r][c] += x[r] * x[c];
            }
        }

        sumYY += y * y;
        n++;
    }

    if (n < 20) {
        return;
    }

    double coefficient[6];

    if (!solveNormalEquations(ata, aty, P, coefficient)) {
        return;
    }

    double a = coefficient[L];
    double b = coefficient[P - 1];
    double rss = sumYY;

    for (int r = 0; r < P; r++) {
        rss -= coefficient[r] * aty[r];
    }

    rss = std::max(0.0, rss);
    double meanY = aty[P - 1] / n;
    double varY = sumYY - n * meanY * meanY;
    double rho = (varY > 0.0) ? std::sqrt(std::max(0.0, 1.0 - rss / varY)) : 0.0;
    float nominal = m_decoder->getNominalOuterLevel() / 3.0f;

    if ((rho < 0.8) || (a < 0.5 * nominal) || (a > 1.6 * nominal)) {
        return;
    }

    m_slicer.initialise((float) (3.0 * a), (float) b, (float) (rss / std::max(1, n - P)));
    // The strobe of the next symbol moves by the burst offset; the drift since the previous
    // burst (loop corrections plus this offset) over the symbols in between is the clock rate error
    m_clock->adjust(movement);
    int symbols;
    double loopCorrection = m_clock->takeCorrection(symbols);

    if (symbols >= 100) {
        m_clock->setRate(m_clock->getRate() + 0.3 * (loopCorrection + movement) / symbols);
    }

    // Second pass over the burst with the interference of the neighbours cancelled, twice so that
    // the second run cancels with the improved decisions. A sample far outside the constellation
    // is a transient, not a symbol, and carries no information
    for (int pass = 0; pass < 2; pass++)
    {
        for (int64_t k = first; k <= last; k++)
        {
            double position = refinedAt(k);

            if (!inClock(position)) {
                continue;   // (its first decision stands: the refined instant's samples are not in yet)
            }

            float y = m_clock->interpolateAt(position);
            y = m_inverted ? -y : y;
            float isi = 0.0f;

            for (int j = -L; j <= L; j++)
            {
                if (j != 0) {
                    isi += (float) (coefficient[j + L] * levelAt(k + j));
                }
            }

            DSDSoftSymbol refined = sliceValue(y - isi, position);

            if (std::fabs(y - m_slicer.getOffset()) > outlier)
            {
                refined.m_llrMsb = 0.0f;
                refined.m_llrLsb = 0.0f;
                refined.m_reliability = 0.0f;
            }

            m_history.replace(k, refined, position); // later refinements measure from the corrected positions

            if ((pass == 1) && (dsdDebugLevel() >= 2)) {
                fprintf(stderr, "  k %lld y %.0f isi %.0f u %.2f dibit %d llr %.0f %.0f present %d pos %.2f%s", (long long) k, y, isi, (y - isi - m_slicer.getOffset()) / (m_slicer.getOuterLevel() / 3.0f), refined.m_dibit, refined.m_llrMsb, refined.m_llrLsb, refined.m_present, position, "\n");
            }
        }
    }

    if (m_coherent && (m_chain->m_shaping == ShapingRRC) && m_chain->m_coherentDetector && m_decoder->isFourLevel())
    {
        // The coherent detector costs far more than the rest of the receiver and can only help a
        // burst the first pass may have got wrong: from the residual against the decided levels,
        // the first pass's symbol error probability is about 1.5 Q(a / sigma) (an inner level has
        // two neighbours at a), and the detector runs when the burst is expected to hold more
        // than a small fraction of an error. An echo or a fade inflates the residual and opens
        // the gate too. A strong clean signal never pays for it
        double q = std::sqrt(rss / std::max(1, n - P)) / std::max(1e-6, a);
        double expectedErrors = n * 1.5 * 0.5 * std::erfc(1.0 / (q * std::sqrt(2.0)));
        if (expectedErrors > m_coherentGate)
        {
            std::vector<double> positions;

            for (int64_t k = first; k <= last; k++) {
                positions.push_back(refinedAt(k));
            }

            coherentBurst(first, last, positions);
            m_coherentRuns++;
        }
        else
        {
            m_coherentSkips++;

            if (dsdDebugLevel() >= 2) {
                fprintf(stderr, "DSD coherent %lld..%lld: skipped, sigma/level %.3f expects %.4f errors\n", (long long) first, (long long) last, q, expectedErrors);
            }
        }
    }

    if (dsdDebug()) {
        fprintf(stderr, "DSD refine %lld..%lld: tau %+.2f %+.2f slope %+.4f move %+.2f loop %+.2f outer %.1f offset %.1f sigma %.1f rho %.3f rate %.4f taps", (long long) first, (long long) last,
            tau[0], tau[1], slope, movement, loopCorrection, 3.0 * a, b, std::sqrt(rss / std::max(1, n - P)), rho, m_clock->getRate());

        for (int j = -L; j <= L; j++) {
            fprintf(stderr, " %+.3f", coefficient[j + L] / a);
        }

        fprintf(stderr, "\n");
    }
}

void DSDDemodulator::coherentBurst(int64_t first, int64_t last, const std::vector<double>& allPositions)
{
    // Leave out the transients and the silence at the ends of a burst (carrier ramps of a mobile
    // burst): a phase reference taken there is worthless
    const int64_t spanFirst = first;
    auto usable = [&](int64_t k) {
        const DSDSoftSymbol& s = m_history.at(k);
        return s.m_present && ((s.m_reliability > 0.0f) || (s.m_llrMsb != 0.0f) || (s.m_llrLsb != 0.0f));
    };

    while ((first < last) && !usable(first)) {
        first++;
    }

    while ((last > first) && !usable(last)) {
        last--;
    }

    if (last - first < 16) {
        return;
    }

    std::vector<double> positions(allPositions.begin() + (first - spanFirst), allPositions.begin() + (last - spanFirst + 1));

    // The matched filter output at clock position P is the discriminator sample P - delay, which
    // is the frequency between the channel filtered samples P - delay - 1 and P - delay: the
    // symbol centre in IQ samples is P - delay - 0.5
    static const int8_t levelOf[4] = { 1, 3, -1, -3 };
    DSDCoherentDetector& detector = *m_chain->m_coherentDetector;
    // The coherent detector works on the channel filtered samples with the filter in its channel
    // model (it needs the filter's length more samples before the burst), or on the unfiltered
    // ones, which lead the filtered ones by the channel filter delay
    detector.setChannelFilter(m_coherentFiltered ? m_frontEnd.getTaps() : std::vector<float>());
    const int lead = detector.getFilterDelay();
    const double iqOffset = m_chain->m_delay + 0.5 + (m_coherentFiltered ? 0.0 : m_frontEnd.getDelay());
    // The detector uses the samples of the symbol intervals only (half a symbol either side of
    // the first and last centres); the burst's last samples have just arrived
    // ... plus the channel memory (two symbols) before the first
    const int margin = m_sps / 2 + 2;
    int64_t firstSample = (long long) std::floor(positions.front() - iqOffset) - margin - 2 * m_sps - 2 - 2 * lead;
    int64_t lastSample = (long long) std::ceil(positions.back() - iqOffset) + margin;

    if ((firstSample < 0) || (firstSample < m_sampleIndex - (long long) m_iqRing.size() + 1) || (lastSample > m_sampleIndex)) {
        return;
    }

    std::vector<std::complex<float>> samples(lastSample - firstSample + 1);

    for (int64_t i = firstSample; i <= lastSample; i++) {
        samples[i - firstSample] = m_iqRing[i % (long long) m_iqRing.size()];
    }

    std::vector<double> centres(positions.size());
    std::vector<int8_t> firstPass(positions.size());
    int8_t before[4] = { 0, 0, 0, 0 }, after[4] = { 0, 0, 0, 0 };

    for (int64_t k = first; k <= last; k++)
    {
        centres[k - first] = positions[k - first] - iqOffset - firstSample;
        const DSDSoftSymbol& s = m_history.at(k);
        firstPass[k - first] = s.m_present ? levelOf[s.m_dibit] : 0;
    }

    for (int j = 0; j < 4; j++)
    {
        if (m_history.has(first - 1 - j)) {
            before[j] = levelOf[m_history.at(first - 1 - j).m_dibit];
        }

        if (m_history.has(last + 1 + j)) {
            after[j] = levelOf[m_history.at(last + 1 + j).m_dibit];
        }
    }

    float level = m_slicer.getOuterLevel() / 3.0f;
    float sign = m_inverted ? -1.0f : 1.0f;
    std::vector<DSDCoherentDetector::Result> results;

    if (!detector.detect(samples, centres, firstPass, before, after, sign * level, sign * m_slicer.getOffset(), results)) {
        return;
    }

    // A detector that disagrees with the discriminator path on a quarter of the symbols has
    // lost the burst (a channel fit that failed), not found it: keep the first pass
    int disagreements = 0;

    for (int64_t k = first; k <= last; k++) {
        disagreements += results[k - first].m_dibit != m_history.at(k).m_dibit;
    }

    if (disagreements > (last - first + 1) / 4)
    {
        if (dsdDebug()) {
            fprintf(stderr, "DSD coherent %lld..%lld: rejected, %d disagreements\n", (long long) first, (long long) last, disagreements);
        }

        return;
    }

    int changed = 0;

    for (int64_t k = first; k <= last; k++)
    {
        DSDSoftSymbol s = m_history.at(k);

        if (!s.m_present || (s.m_reliability == 0.0f && s.m_llrMsb == 0.0f && s.m_llrLsb == 0.0f)) {
            continue; // absent or transient: keep the discriminator path's verdict
        }

        const DSDCoherentDetector::Result& r = results[k - first];
        changed += r.m_dibit != s.m_dibit;

        if ((r.m_dibit != s.m_dibit) && (dsdDebugLevel() >= 2)) {
            fprintf(stderr, "  coherent k %lld (%lld of %lld): dibit %d -> %d llr %.0f %.0f -> %.0f %.0f rel %.2f\n", (long long) k, (long long) (k - first), (long long) (last - first), s.m_dibit, r.m_dibit, s.m_llrMsb, s.m_llrLsb, r.m_llrMsb, r.m_llrLsb, s.m_reliability);
        }
        s.m_dibit = r.m_dibit;
        s.m_llrMsb = r.m_llrMsb;
        s.m_llrLsb = r.m_llrLsb;
        s.m_reliability = r.m_reliability;
        m_history.replace(k, s);
    }

    if (dsdDebug()) {
        fprintf(stderr, "DSD coherent %lld..%lld: %d decisions changed%s snr %.1f dB residual %.1f Hz\n", (long long) first, (long long) last, changed, detector.wasDispersive() ? " (dispersive channel)" : "",
            10.0 * std::log10(std::max(1e-9f, detector.getChannelEnergy() / std::max(1e-12f, detector.getNoisePower()))), detector.getFrequencyHz());
    }
}

bool DSDDemodulator::solveNormalEquations(double ata[6][6], const double *aty, int p, double *x)
{
    // Gaussian elimination with partial pivoting on a copy
    double m[6][7];

    for (int r = 0; r < p; r++)
    {
        for (int c = 0; c < p; c++) {
            m[r][c] = ata[r][c];
        }

        m[r][p] = aty[r];
    }

    for (int c = 0; c < p; c++)
    {
        int pivot = c;

        for (int r = c + 1; r < p; r++)
        {
            if (std::fabs(m[r][c]) > std::fabs(m[pivot][c])) {
                pivot = r;
            }
        }

        if (std::fabs(m[pivot][c]) < 1e-9) {
            return false;
        }

        if (pivot != c)
        {
            for (int k = 0; k <= p; k++) {
                std::swap(m[c][k], m[pivot][k]);
            }
        }

        for (int r = 0; r < p; r++)
        {
            if (r == c) {
                continue;
            }

            double f = m[r][c] / m[c][c];

            for (int k = c; k <= p; k++) {
                m[r][k] -= f * m[c][k];
            }
        }
    }

    for (int r = 0; r < p; r++) {
        x[r] = m[r][p] / m[r][r];
    }

    return true;
}

bool DSDDemodulator::constellationPlausible(int64_t first, int64_t last) const
{
    // Four level protocols carry inner levels in about half of their symbols; a binary protocol
    // carries none (the reverse test is not usable: the intersymbol interference of the narrow
    // GMSK receive filter puts many first pass symbols of a genuine binary signal at inner levels)
    int present = 0, inner = 0;

    for (int64_t k = first; k <= last; k++)
    {
        if (!m_history.has(k) || !m_history.at(k).m_present) {
            continue;
        }

        const DSDSoftSymbol& s = m_history.at(k);
        present++;

        if (m_decoder->isFourLevel()) {
            inner += (s.m_dibit == 0) || (s.m_dibit == 2);
        } else {
            inner += s.m_reliability < 0.6f;
        }
    }

    if (present < 40) {
        return true; // not enough carrier to judge
    }

    float fraction = (float) inner / present;
    return m_decoder->isFourLevel() ? (fraction > 0.15f) : (fraction < 0.35f);
}

void DSDDemodulator::pushScopeSymbol(float y, const DSDSoftSymbol& symbol)
{
    const std::vector<float>& kernel = m_chain->m_kernel;

    if ((int) kernel.size() == GmskScopeSpan)
    {
        // A GMSK pulse spreads into the symbols either side (gmskKernel), so a sample sits at one
        // of several levels depending on its neighbours, which the 4FSK shaping does not do. The
        // sample two symbols back is shown instead, once the decisions either side of it are made:
        // their share taken out and the rest scaled from the centre tap to the full level, so that
        // a clean signal is the four corners of a binary constellation. The slicer's level is the
        // centre tap's share of the deviation (the interference averages out of its tracking),
        // though it starts from the fit's full deviation at a lock and a resync and takes a few
        // dozen symbols to settle: the points are spread a little wider meanwhile. (Starting it
        // at the centre tap's share instead decoded worse: 4 dB C/N, 30 seeds, raw BER 1.85% to 1.93%)
        const float sign = m_inverted ? -1.0f : 1.0f;

        for (int i = 0; i < GmskScopeSpan - 1; i++)
        {
            m_gmskScopeY[i] = m_gmskScopeY[i + 1];
            m_gmskScopeD[i] = m_gmskScopeD[i + 1];
            m_gmskScopePresent[i] = m_gmskScopePresent[i + 1];
        }

        m_gmskScopeY[GmskScopeSpan - 1] = sign * y;
        m_gmskScopeD[GmskScopeSpan - 1] = (symbol.m_dibit & 2) ? -1.0f : 1.0f; // (levels +3 and -3: dibits 1 and 3)
        m_gmskScopePresent[GmskScopeSpan - 1] = symbol.m_present;

        if (m_gmskScopeCount < GmskScopeSpan) {
            m_gmskScopeCount++;
        }

        // kernel[2 + j] is a symbol's share of the sample j symbols after it
        const int middle = GmskScopeSpan / 2;

        // (no point without a carrier: the decisions either side are of noise, and there is no
        // pulse to take their part out of)
        if ((m_gmskScopeCount < GmskScopeSpan) || !m_gmskScopePresent[middle]) {
            return;
        }

        const float tap = std::max(0.2f, kernel[middle]);
        const float deviation = m_slicer.getOuterLevel() / tap;
        const float offset = m_slicer.getOffset();
        float interference = 0.0f;

        for (int i = 0; i < GmskScopeSpan; i++)
        {
            if (i != middle) {
                interference += kernel[middle + (middle - i)] * m_gmskScopeD[i];
            }
        }

        y = sign * (offset + (m_gmskScopeY[middle] - offset - deviation * interference) / tap);
    }

    if (m_scope.size() < 4096)
    {
        m_scope.push_back(std::make_pair(y, m_lastScopeY));
        m_lastScopeY = y;
    }
}

void DSDDemodulator::sliceSymbol()
{
    double centre;
    m_clock->next(centre);
    DSDSoftSymbol symbol = sliceAt(centre, true);
    int64_t index = m_symbolIndex++;
    pushScopeSymbol(m_clock->interpolateAt(centre), symbol);

    m_lastSymbolIndex = index;
    m_lastSymbolCentre = centre;
    m_history.push(index, symbol, centre);

    if ((index == m_lockIndex + 120) && (m_confirmed == 0) && m_decoder->isFourLevel() && !constellationPlausible(m_lockIndex + 1, index))
    {
        // The symbols after the sync are not of this decoder's kind (a binary signal seen by a
        // four level decoder or the reverse): the sync was a chance match in the data of another
        // protocol
        if (dsdDebug()) {
            fprintf(stderr, "DSD lock rejected: %s constellation\n", m_decoder->getName());
        }

        m_rejectedDecoder = m_decoder;
        m_rejectedUntil = m_sampleIndex + 48000;
        unlock();
        return;
    }

    if (m_decoder->pushSymbol(index, symbol, std::llround(centre)))
    {
        int64_t first, last;
        m_decoder->getBurstSpan(first, last);
        refineBurst(first, last);
        m_decoder->processBurst();

        if (m_decoder->wasBurstValid()) {
            confirmLock();
        }
    }

    // Voice frames reach the vocoder once the lock is confirmed by something checkable (a valid
    // burst, an expected sync, an EMB): until then they wait, and a lock that is never confirmed
    // (a chance match of a sync in noise) plays nothing. A real transmission loses nothing: one
    // burst of latency for DMR, up to a voice sync for D-STAR heard without its header
    std::vector<DSDVoiceFrame> frames = m_decoder->takeFrames();
    std::vector<DSDProtocolFrame> protocolFrames = m_decoder->takeProtocolFrames();

    if (m_status.m_locked) // confirmed and proven
    {
        m_frames.insert(m_frames.end(), frames.begin(), frames.end());
        for (const DSDProtocolFrame& frame : protocolFrames) {
            publishProtocolFrame(frame);
        }
    }
    else
    {
        m_pendingFrames.insert(m_pendingFrames.end(), frames.begin(), frames.end());
        m_pendingProtocolFrames.insert(m_pendingProtocolFrames.end(), protocolFrames.begin(), protocolFrames.end());

        if (m_pendingFrames.size() > 100) { // 2 s: something is wrong, keep the latest
            m_pendingFrames.erase(m_pendingFrames.begin(), m_pendingFrames.end() - 100);
        }

        if (m_pendingProtocolFrames.size() > 100)
        {
            m_pendingProtocolFrames.clear();
            m_protocolSuppressed = true;
        }
    }

    // The decoder may have learned that the next sync comes sooner than assumed (a unit length
    // read from a header): follow it while the expectation is still ahead
    if (m_expectedPending)
    {
        int64_t index;
        std::vector<DVSync::Pattern> patterns;

        // Only an earlier sync than expected can be adopted: a later one would skip the check
        // of the current expectation
        if (m_decoder->getNextSync(index, patterns) && (index < m_expectedSyncIndex) && (index > m_lastSymbolIndex))
        {
            m_expectedSyncIndex = index;
            m_expectedPatterns = patterns;
        }
    }

    // The GMSK chain slices at the eye of the narrow receive filter; the deviation of the
    // transmitter is that level over the centre tap of the pulse response
    float eye = m_chain->m_kernel.empty() ? 1.0f : m_chain->m_kernel[m_chain->m_kernel.size() / 2];
    m_status.m_outerLevelHz = m_slicer.getOuterLevel() / std::max(0.2f, eye);
    m_status.m_offsetHz = m_slicer.getOffset();
    m_status.m_correctionHz = m_frontEnd.getCorrection();
    m_status.m_snrDB = m_slicer.getSnrDB();
    m_status.m_clockPpm = (float) (1e6 * m_clock->getRate() / m_sps);
}
