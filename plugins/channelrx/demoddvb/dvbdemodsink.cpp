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
#include <chrono>
#include <cmath>
#include <system_error>

#include <QDebug>

#include "dsp/dspengine.h"
#include "dsp/fftfactory.h"
#include "util/db.h"
#include "util/profiler.h"

#include "dvbdemodsink.h"

typedef DVBDemodSettings S; // For the standards

DVBDemodSink::Status::Status() :
    m_standard(0),
    m_channelSampleRate(0),
    m_sampleRateOK(false),
    m_requiredRate(0.0),
    m_autoSymbolRate(0.0),
    m_power(-100.0),
    m_tsBitRate(0.0),
    m_udpDatagrams(0),
    m_fifoOverflows(0),
    m_impulseResolution(0.0),
    m_impulseGuard(0)
{
}

QString DVBDemodSink::Status::stateText(int bandwidth) const
{
    if (!m_sampleRateOK) {
        return QString("Needs %1 MS/s+").arg((m_requiredRate > 0.0 ? m_requiredRate : m_minRateRatio * S::terrestrialSampleRate(bandwidth)) / 1e6, 0, 'f', 2);
    }

    if (isSatellite() && (m_autoSymbolRate < 0.0)) {
        return "SR search";
    }

    if (isSatellite() && (m_signalSymbolRate > 0.0) && !isLocked()) {
        return "Wrong SR";
    }

    if (m_standard == S::StandardAutoSatellite) {
        return m_s.m_frontEnd.m_estimating ? "Searching" : "Sync";
    }

    if (m_standard == S::StandardDVBS)
    {
        switch (m_s.m_state)
        {
        case dvbs::Receiver::State::Spectrum: return "Searching";
        case dvbs::Receiver::State::Carrier: return "Carrier";
        case dvbs::Receiver::State::Code: return "FEC";
        case dvbs::Receiver::State::Sync: return "No TS";
        default: return "Locked";
        }
    }

    if (m_standard == S::StandardDVBS2)
    {
        switch (m_s2.m_state)
        {
        case dvbs2::Receiver::State::Spectrum:
        case dvbs2::Receiver::State::Searching: return "Searching";
        case dvbs2::Receiver::State::Frames: return isGeneric(m_s2.m_bb) ? "Generic" : "No TS";
        default: return "Locked";
        }
    }

    if (m_standard == S::StandardDVBT2)
    {
        switch (m_t2.m_ofdm.m_state)
        {
        case dvbt2::OFDMDemod::SearchP1:
            return "Searching";
        case dvbt2::OFDMDemod::FindGuard:
            return "Sync";
        default:
            return !m_t2.m_l1Valid ? "L1" : m_t2.m_bb.m_packets > 0 ? "Locked" : isGeneric(m_t2.m_bb) ? "Generic" : "No TS";
        }
    }

    if (m_standard == S::StandardAutoTerrestrial) {
        return "Searching";
    }

    if (!m_receiver.m_supported) {
        return "Unsupported";
    }

    switch (m_receiver.m_ofdm.m_state)
    {
    case dvbt::OFDMDemod::Acquire:
        return "Searching";
    case dvbt::OFDMDemod::IntegerCFO:
    case dvbt::OFDMDemod::PilotSync:
        return "Sync";
    case dvbt::OFDMDemod::TPSSync:
        return "TPS";
    default:
        return m_receiver.m_tsSync ? "Locked" : "No TS";
    }
}

bool DVBDemodSink::Status::isLocked() const
{
    if (!m_sampleRateOK) {
        return false;
    }

    if (m_standard == S::StandardDVBS) {
        return m_s.m_state == dvbs::Receiver::State::Locked;
    }

    if (m_standard == S::StandardDVBS2) {
        return m_s2.m_state == dvbs2::Receiver::State::Locked;
    }

    if (m_standard == S::StandardAutoSatellite) {
        return false;
    }

    if (m_standard == S::StandardDVBT2)
    {
        switch (m_t2.m_ofdm.m_state)
        {
        case dvbt2::OFDMDemod::SearchP1:
        case dvbt2::OFDMDemod::FindGuard:
            return false;
        default:
            return m_t2.m_l1Valid && (m_t2.m_bb.m_packets > 0);
        }
    }

    if ((m_standard == S::StandardAutoTerrestrial) || !m_receiver.m_supported) {
        return false;
    }

    switch (m_receiver.m_ofdm.m_state)
    {
    case dvbt::OFDMDemod::Acquire:
    case dvbt::OFDMDemod::IntegerCFO:
    case dvbt::OFDMDemod::PilotSync:
    case dvbt::OFDMDemod::TPSSync:
        return false;
    default:
        return m_receiver.m_tsSync;
    }
}

bool DVBDemodSink::Status::isSearching() const
{
    if (m_standard == S::StandardDVBS) {
        return (m_s.m_state == dvbs::Receiver::State::Spectrum) || (m_s.m_state == dvbs::Receiver::State::Carrier);
    }

    if (m_standard == S::StandardDVBS2) {
        return m_s2.m_state <= dvbs2::Receiver::State::Searching;
    }

    if (m_standard == S::StandardAutoSatellite) {
        return true;
    }

    if (m_standard == S::StandardDVBT2) {
        return m_t2.m_ofdm.m_state == dvbt2::OFDMDemod::SearchP1;
    }

    if (m_standard == S::StandardDVBT) {
        return m_receiver.m_ofdm.m_state == dvbt::OFDMDemod::Acquire;
    }

    // Automatic, neither locked yet: both are searching, so each's state counts
    return (m_receiver.m_ofdm.m_state == dvbt::OFDMDemod::Acquire) && (m_t2.m_ofdm.m_state == dvbt2::OFDMDemod::SearchP1);
}

DVBDemodSink::DVBDemodSink() :
    m_channelSampleRate(0),
    m_channelFrequencyOffset(0),
    m_sampleRateOK(false),
    m_receiver(nullptr),
    m_receiverT2(nullptr),
    m_receiverS(nullptr),
    m_receiverS2(nullptr),
    m_active(0),
    m_unlockedSamples(0),
    m_autoRate(0.0),
    m_autoUnlockedSamples(0),
    m_spectrumBinHz(0.0),
    m_minSignalWidth(0.0),
    m_rateSearches(0),
    m_bandGeneration(0),
    m_s2GoodFrames(0),
    m_sGoodPackets(0),
    m_ncoShift(false),
    m_ncoRot(1.0f, 0.0f),
    m_ncoStep(1.0f, 0.0f),
    m_ncoPhase(0.0),
    m_ncoStepAngle(0.0),
    m_ncoCount(0),
    m_dcRemoval(false),
    m_dcMean(0.0, 0.0),
    m_dcRot(1.0f, 0.0f),
    m_dcStep(1.0f, 0.0f),
    m_dcPhase(0.0),
    m_dcStepAngle(0.0),
    m_dcCount(0),
    m_fftWarmUpStop(false),
    m_buffer(m_bufferSize),
    m_bufferFill(0),
    m_powerSum(0.0),
    m_powerCount(0),
    m_power(-100.0),
    m_displaySamples(0),
    m_rateSamples(0),
    m_ratePackets(0),
    m_tsBitRate(0.0),
    m_tsFifo(m_tsFifoSize, true),
    m_nextRecordingId(1),
    m_audioFifo(48000)
{
    // Plan the FFTs the receivers may use now, as planning in the baseband thread can take longer than the sample
    // FIFO holds. Released engines stay in the factory for the receivers to use. Without FFTW wisdom, planning can
    // take seconds (the 32K FFT especially), so it's done in a thread of its own, rather than hold up the GUI. The
    // smaller ones, needed first, are planned first
    m_fftWarmUp = std::thread([this]() {
        FFTFactory *fftFactory = DSPEngine::instance()->getFFTFactory();

        for (int size : {1024, 2048, 8192, 4096, 16384, 32768})
        {
            // Deleted meanwhile: the rest are planned when needed
            if (m_fftWarmUpStop) {
                break;
            }

            FFTEngine *engine;
            unsigned int sequence = fftFactory->getEngine(size, false, &engine);
            fftFactory->releaseEngine(size, false, sequence);
        }
    });

    m_packetBlock.reserve(m_packetBlockSize + 188);
    createReceiver();
}

DVBDemodSink::~DVBDemodSink()
{
    m_fftWarmUpStop = true;
    m_fftWarmUp.join();

    // Each finishes writing its file: those recording now, all at once, and those stopped before
    std::vector<int> ids;

    {
        QMutexLocker locker(&m_recordersMutex);

        for (const auto& recorder : m_recorders) {
            ids.push_back(recorder.first);
        }
    }

    for (int id : ids) {
        stopRecording(id);
    }

    for (auto& stopping : m_stoppingRecorders) {
        stopping.wait();
    }

    m_stoppingRecorders.clear();
    delete m_receiver;
    delete m_receiverT2;
    m_sharedFrontEnd.reset(); // Uses the satellite receivers
    delete m_receiverS;
    delete m_receiverS2;
}

void DVBDemodSink::createReceiver()
{
    const bool terrestrial = !DVBDemodSettings::isSatellite(m_settings.m_standard);

    // Not wanted, or for another bandwidth
    if (!terrestrial || (m_receiverBandwidth != m_settings.m_bandwidth))
    {
        delete m_receiver;
        delete m_receiverT2;
        m_receiver = nullptr;
        m_receiverT2 = nullptr;
    }

    if (terrestrial && !m_receiver)
    {
        m_receiver = new dvbt::Receiver(m_settings.m_bandwidth, DSPEngine::instance()->getFFTFactory());
        m_receiver->setPacketCallback([this](const uint8_t *packet) {
            processPacket(packet);
        });
        m_receiver->setLogCallback([](const char *message) {
            qDebug("DVBDemodSink: %s", message);
        });
        m_receiver->setLowPriority(m_settings.m_lowPriority);

        m_receiverT2 = new dvbt2::Receiver(m_settings.m_bandwidth, DSPEngine::instance()->getFFTFactory());
        m_receiverT2->selectPLP(m_settings.m_plpId);
        m_receiverT2->setProfile(m_settings.m_standard == S::StandardDVBT2Lite ? dvbt2::Profile::Lite : dvbt2::Profile::Auto);
        m_receiverT2->setPacketCallback([this](const uint8_t *packet) {
            processPacket(packet);
        });
        m_receiverT2->setLogCallback([](const char *message) {
            qDebug("DVBDemodSink: T2: %s", message);
        });
        m_receiverBandwidth = m_settings.m_bandwidth;
    }

    m_active = 0;
    m_unlockedSamples = 0;
    createSatelliteReceivers();
}

// The DVB-S and S2 receivers, when a satellite standard is chosen (else not, as each has threads)
void DVBDemodSink::createSatelliteReceivers()
{
    if (!DVBDemodSettings::isSatellite(m_settings.m_standard))
    {
        m_sharedFrontEnd.reset();
        delete m_receiverS;
        delete m_receiverS2;
        m_receiverS = nullptr;
        m_receiverS2 = nullptr;
        return;
    }

    if (!m_receiverS)
    {
        m_receiverS = new dvbs::Receiver();
        m_receiverS->setPacketCallback([this](const uint8_t *packet) {
            processPacket(packet);
        });
    }

    if (!m_receiverS2)
    {
        m_receiverS2 = new dvbs2::Receiver();
        m_receiverS2->setPacketCallback([this](const uint8_t *packet) {
            processPacket(packet);
        });
    }

    m_receiverS->setCodeRate(m_settings.m_codeRate);
    m_receiverS2->setRollOff(m_settings.m_rollOff / 100.0);
    m_receiverS2->setISI(m_settings.m_isi);

    if (m_sharedFrontEnd) {
        m_sharedFrontEnd->discard(); // The receiver's reset, from this thread
    }

    m_receiverS2->setScramblingCode(m_settings.m_plScramblingCode);

    // One front end for both (configured with them, as the channel's set)
    if (!m_sharedFrontEnd) {
        m_sharedFrontEnd.reset(new dvbs::SharedFrontEnd(m_receiverS, m_receiverS2));
    }

    setEqualiser(m_settings.m_equaliser);
}

// The equaliser of the satellite receivers' front ends: the shared one, and each's own
void DVBDemodSink::setEqualiser(bool enabled)
{
    if (m_receiverS) {
        m_receiverS->setEqualiser(enabled);
    }

    if (m_receiverS2) {
        m_receiverS2->setEqualiser(enabled);
    }

    if (m_sharedFrontEnd) {
        m_sharedFrontEnd->setEqualiser(enabled);
    }
}

void DVBDemodSink::resetSatellite()
{
    if (m_sharedFrontEnd)
    {
        m_sharedFrontEnd->reset();
    }
    else if (m_receiverS && m_receiverS2)
    {
        m_receiverS->reset();
        m_receiverS2->reset();
    }
}

void DVBDemodSink::configureSatellite(double channelSampleRate, double symbolRate)
{
    if (m_sharedFrontEnd)
    {
        m_sharedFrontEnd->configure(channelSampleRate, symbolRate);
    }
    else if (m_receiverS && m_receiverS2)
    {
        m_receiverS->configure(channelSampleRate, symbolRate);
        m_receiverS2->configure(channelSampleRate, symbolRate);
    }
}

void DVBDemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    if (!m_sampleRateOK) {
        return;
    }

    const float scale = 1.0f / SDR_RX_SCALEF;

    for (SampleVector::const_iterator it = begin; it != end; ++it)
    {
        float re = it->real() * scale, im = it->imag() * scale;

        if (m_ncoShift)
        {
            // Multiplied out, as a complex multiply can be a library call checking for infinities
            const float r = m_ncoRot.real(), i = m_ncoRot.imag();
            const float shiftedRe = re * r - im * i;
            im = re * i + im * r;
            re = shiftedRe;
            m_ncoRot = dvbt::Cf(r * m_ncoStep.real() - i * m_ncoStep.imag(), r * m_ncoStep.imag() + i * m_ncoStep.real());

            if (++m_ncoCount == 1024)
            {
                m_ncoPhase = std::remainder(m_ncoPhase + 1024.0 * m_ncoStepAngle, 2.0 * M_PI);
                m_ncoRot = dvbt::Cf((float) std::cos(m_ncoPhase), (float) std::sin(m_ncoPhase));
                m_ncoCount = 0;
            }
        }

        if (m_dcRemoval)
        {
            // The leakage is the running mean (a time constant of 10 ms or so, so it follows drift but takes little
            // of the carrier there) of the samples turned back by its frequency. Subtracted before the update, as it's
            // the estimate from the samples before
            const double alpha = 100.0 / m_channelSampleRate;
            const float r = m_dcRot.real(), i = m_dcRot.imag();
            const std::complex<double> mean = m_dcMean;
            const double back = re * r + im * i, backIm = im * r - re * i;
            m_dcMean += alpha * (std::complex<double>(back, backIm) - mean);
            re -= (float) (mean.real() * r - mean.imag() * i);
            im -= (float) (mean.real() * i + mean.imag() * r);

            if (m_dcStepAngle != 0.0)
            {
                m_dcRot = dvbt::Cf(r * m_dcStep.real() - i * m_dcStep.imag(), r * m_dcStep.imag() + i * m_dcStep.real());

                if (++m_dcCount == 1024)
                {
                    m_dcPhase = std::remainder(m_dcPhase + 1024.0 * m_dcStepAngle, 2.0 * M_PI);
                    m_dcRot = dvbt::Cf((float) std::cos(m_dcPhase), (float) std::sin(m_dcPhase));
                    m_dcCount = 0;
                }
            }
        }

        m_buffer[m_bufferFill++] = dvbt::Cf(re, im);

        if (m_bufferFill == m_bufferSize) {
            processBuffer();
        }
    }
}

void DVBDemodSink::processBuffer()
{
    for (int i = 0; i < m_bufferFill; i++) {
        m_powerSum += std::norm(m_buffer[i]);
    }

    m_powerCount += m_bufferFill;
    m_rateSamples += m_bufferFill;
    m_displaySamples += m_bufferFill;

    if (DVBDemodSettings::isSatellite(m_settings.m_standard))
    {
        if (m_receiverS && m_receiverS2) {
            processSatellite();
        } else {
            m_bufferFill = 0;
        }

        flushPackets();
        return;
    }

    if (!m_receiver || !m_receiverT2)
    {
        m_bufferFill = 0;
        return;
    }

    // Automatically, both receivers search until one locks, then only it runs, until it has lost lock for a while.
    // DVB-T has no 1.7 MHz channels: DVB-T2's alone then
    const int standard = m_settings.m_standard;
    const bool autoStandard = standard == S::StandardAutoTerrestrial;
    const bool feedT = S::hasDVBT(m_settings.m_bandwidth)
        && ((standard == S::StandardDVBT) || (autoStandard && (m_active != S::StandardDVBT2)));
    const bool feedT2 = (standard == S::StandardDVBT2) || (standard == S::StandardDVBT2Lite) || (autoStandard && (m_active != S::StandardDVBT));
    dvbt::Receiver::Status tStatus;
    dvbt2::Receiver::Status t2Status;

    PROFILER_START()

    if (feedT)
    {
        m_receiver->process(m_buffer.data(), m_bufferFill);
        tStatus = m_receiver->getStatus();
    }

    PROFILER_STOP("DVBDemod sink DVB-T receiver")
    PROFILER_RESTART()

    if (feedT2)
    {
        m_receiverT2->process(m_buffer.data(), m_bufferFill);
        t2Status = m_receiverT2->getStatus();
    }

    PROFILER_STOP("DVBDemod sink DVB-T2 receiver")
    flushPackets();

    const bool tLocked = feedT && tStatus.m_tsSync;
    const bool t2Locked = feedT2 && t2Status.m_l1Valid && (t2Status.m_ofdm.m_state >= dvbt2::OFDMDemod::Symbols);

    if (autoStandard)
    {
        if (m_active == 0)
        {
            if (tLocked)
            {
                m_active = S::StandardDVBT;
                m_receiverT2->reset();
            }
            else if (t2Locked)
            {
                m_active = S::StandardDVBT2;
                m_receiver->reset();
            }
        }
        else if ((m_active == S::StandardDVBT) ? tLocked : t2Locked)
        {
            m_unlockedSamples = 0;
        }
        else if ((m_unlockedSamples += m_bufferFill) > 5 * (uint64_t) m_channelSampleRate)
        {
            m_active = 0;
            m_unlockedSamples = 0;
        }
    }

    const int shown = standard == S::StandardDVBT2Lite ? (int) S::StandardDVBT2 : !autoStandard ? standard : m_active;
    m_bufferFill = 0;

    updateRates();

    bool updateDisplay = m_displaySamples >= (uint64_t) m_channelSampleRate / 10;
    PROFILER_RESTART()

    if (updateDisplay)
    {
        if (shown == S::StandardDVBT2)
        {
            m_receiverT2->getDisplayData(m_cells, 2000, m_responseDb, 512);
            dvbt2::ImpulseResponse impulse;
            m_receiverT2->getImpulseResponse(impulse);
            m_impulseDb.swap(impulse.m_powerDb);
            m_impulseResolution = impulse.m_resolution;
            m_impulseGuard = impulse.m_guard;
        }
        else
        {
            m_receiver->getDisplayData(m_cells, 2000, m_responseDb, 512);
            dvbt::ImpulseResponse impulse;
            m_receiver->getImpulseResponse(impulse);
            m_impulseDb.swap(impulse.m_powerDb);
            m_impulseResolution = impulse.m_resolution;
            m_impulseGuard = impulse.m_guard;
        }

        m_displaySamples = 0;
    }

    QMutexLocker locker(&m_statusMutex);

    if (updateDisplay)
    {
        m_status.m_cells = m_cells;
        m_status.m_responseDb = m_responseDb;
        m_status.m_impulseDb = m_impulseDb;
        m_status.m_impulseResolution = m_impulseResolution;
        m_status.m_impulseGuard = m_impulseGuard;
    }

    if (feedT) {
        m_status.m_receiver = tStatus;
    }

    if (feedT2) {
        m_status.m_t2 = t2Status;
    }

    m_status.m_standard = shown;
    m_status.m_power = m_power;
    m_status.m_tsBitRate = m_tsBitRate;
    m_status.m_udpDatagrams = m_udp.getDatagrams();
    PROFILER_STOP("DVBDemod sink status and display data")
}

// Average power over a second or so, and TS bit rate over the last 10 seconds
void DVBDemodSink::updateRates()
{
    if ((m_rateSamples < (uint64_t) m_channelSampleRate) || (m_powerCount == 0)) {
        return;
    }

    m_power = CalcDb::dbPower(m_powerSum / m_powerCount);
    m_rateHistory.emplace_back(m_rateSamples, m_ratePackets);

    if (m_rateHistory.size() > 10) {
        m_rateHistory.pop_front();
    }

    uint64_t samples = 0, packets = 0;

    for (const auto& second : m_rateHistory)
    {
        samples += second.first;
        packets += second.second;
    }

    m_tsBitRate = packets * 188.0 * 8.0 * m_channelSampleRate / std::max((uint64_t) 1, samples);
    m_powerSum = 0.0;
    m_powerCount = 0;
    m_rateSamples = 0;
    m_ratePackets = 0;
}

// DVB-S and S2: as DVB-T and T2, automatically both search until one locks, then only it runs, until it's lost lock
// for a while. Display data is the symbols and the input's spectrum
void DVBDemodSink::processSatellite()
{
    const int standard = m_settings.m_standard;
    const bool autoRate = m_settings.m_symbolRate <= 0;

    // The symbol rate, if automatic and not found yet: the receivers wait for it
    PROFILER_START()
    const bool searched = autoRate && (m_autoRate <= 0.0) && m_rateSearch.process(m_buffer.data(), m_bufferFill);
    PROFILER_STOP("DVBDemod sink DVB-S/S2 symbol rate search")

    if (searched)
    {
        const dvbs::SymbolRateSearch::Result& result = m_rateSearch.result();
        m_signals = result.m_signals;
        m_spectrum = result.m_spectrum;
        m_spectrumBinHz = result.m_binHz;
        m_minSignalWidth = result.m_minWidth;
        m_rateSearches++;

        if (result.m_found)
        {
            qDebug("DVBDemodSink::processSatellite: symbol rate %.1f S/s (width %.0f Hz, centre %.0f Hz, line %.1f dB)",
                result.m_symbolRate, result.m_width, result.m_frequency, result.m_lineSnr);
            m_autoRate = result.m_symbolRate;
            restartAutoRelock();
            m_active = 0;
            configureSatellite(m_channelSampleRate, m_autoRate);
            configureDCRemoval();
        }
    }

    const bool rateKnown = !autoRate || (m_autoRate > 0.0);
    const bool autoStandard = standard == S::StandardAutoSatellite;
    const bool feedS = rateKnown && ((standard == S::StandardDVBS) || (autoStandard && (m_active != S::StandardDVBS2)));
    const bool feedS2 = rateKnown && ((standard == S::StandardDVBS2) || (autoStandard && (m_active != S::StandardDVBS)));
    dvbs::Receiver::Status sStatus;
    dvbs2::Receiver::Status s2Status;

    // Through their one front end, to whichever are fed (in Auto, both until one's found). Queued for the front end's
    // thread, the call waits while too much is queued: much time here, the receivers can't keep up
    if (feedS || feedS2)
    {
        PROFILER_START()
        m_sharedFrontEnd->setFeeding(feedS, feedS2);
        m_sharedFrontEnd->process(m_buffer.data(), m_bufferFill);
        PROFILER_STOP("DVBDemod sink DVB-S/S2 receivers (waiting for their front end)")
        PROFILER_RESTART()
        m_receiverS->deliverPackets();
        m_receiverS2->deliverPackets();
        PROFILER_STOP("DVBDemod sink DVB-S/S2 packets delivered")
    }

    if (feedS) {
        sStatus = m_receiverS->getStatus();
    }

    if (feedS2) {
        s2Status = m_receiverS2->getStatus();
    }

    m_satelliteLocked = (feedS && (sStatus.m_state == dvbs::Receiver::State::Locked))
        || (feedS2 && (s2Status.m_state == dvbs2::Receiver::State::Locked));

    // DVB-S with its transport stream in sync, not just a code found, and packets through Reed-Solomon since last seen:
    // a false lock (on a DVB-S2 signal, or a carrier found wrong) can find a code, and now and then sync bytes where
    // they'd be (seen: rate 1/2, agreement 0.84, on a DVB-S2 signal), which took Auto to DVB-S, the DVB-S2 receiver
    // not fed for 5 s. The counts start again from 0 when the receiver's reset
    bool sLocked = false;

    if (feedS)
    {
        const dvbt::OuterDecoder::Stats& outer = sStatus.m_fec.m_outer;
        const uint64_t goodPackets = outer.m_packets - std::min(outer.m_packets, outer.m_rsFailed);
        sLocked = (sStatus.m_state == dvbs::Receiver::State::Locked) && (goodPackets > m_sGoodPackets);
        m_sGoodPackets = goodPackets;
    }

    // DVB-S2 decoding frames, not just its headers found: on other signals (DVB-S, or one at another symbol rate), and
    // noise, something like a header is found now and then, and locked to briefly. That kept Auto on DVB-S2 (not
    // feeding the DVB-S receiver), and the symbol rate from being searched for again. The counts start again from 0
    // when the receiver's reset
    bool s2Locked = false;

    if (feedS2)
    {
        const uint64_t goodFrames = s2Status.m_fecFrames - std::min(s2Status.m_fecFrames, s2Status.m_bchFailures);
        s2Locked = goodFrames > m_s2GoodFrames;
        m_s2GoodFrames = goodFrames;
        logS2Search(s2Status, s2Locked);
    }

    if (autoStandard)
    {
        if (m_active == 0)
        {
            // The other reset in the front end's thread, before its next block: none of the one found's are dropped
            if (sLocked)
            {
                const dvbs::FECDecoder::Status& fec = sStatus.m_fec;
                qDebug("DVBDemodSink::processSatellite: Auto: DVB-S found: rate %s rotation %d agreement %.3f, packets %llu, RS corrected %llu failed %llu",
                    qPrintable(dvbt::toString(fec.m_codeRate)), fec.m_rotation, fec.m_agreement, (unsigned long long) fec.m_outer.m_packets,
                    (unsigned long long) fec.m_outer.m_rsCorrected, (unsigned long long) fec.m_outer.m_rsFailed);
                m_active = S::StandardDVBS;
                m_sharedFrontEnd->resetReceiverS2();
            }
            else if (s2Locked)
            {
                qDebug("DVBDemodSink::processSatellite: Auto: DVB-S2 found");
                m_active = S::StandardDVBS2;
                m_sharedFrontEnd->resetReceiverS();
            }
        }
        else if ((m_active == S::StandardDVBS) ? sLocked : s2Locked)
        {
            m_unlockedSamples = 0;
        }
        else if ((m_unlockedSamples += m_bufferFill) > 5 * (uint64_t) m_channelSampleRate)
        {
            m_active = 0;
            m_unlockedSamples = 0;
        }
    }

    // Automatic, and no lock at the rate found for a while: perhaps it was another signal's (one before a retune,
    // still queued), or the signal's changed
    const double wallSeconds = m_relockClock.isValid() ? std::min(m_maxRelockStep, m_relockClock.restart() * 1e-3) : 0.0;

    if (!m_relockClock.isValid()) {
        m_relockClock.start();
    }

    if (autoRate && rateKnown)
    {
        if (sLocked || s2Locked)
        {
            restartAutoRelock();
        }
        else
        {
            m_autoUnlockedSamples += m_bufferFill;
            m_autoUnlockedSeconds += wallSeconds;
            m_autoCheckSamples += m_bufferFill;
        }

        // Not locking at the rate found: it's measured again, and if it's another (the first was measured from the
        // signal before a retune, still queued, or the signal's changed), the receivers are set to it at once, rather
        // than searching at the wrong rate until the relock. Another only if measured twice running: an unclear
        // spectrum can measure one rate then another
        if (m_autoCheckSamples > m_autoCheckSeconds * m_channelSampleRate)
        {
            if ((m_autoCheckSamples - m_bufferFill <= m_autoCheckSeconds * m_channelSampleRate) && (m_autoCheckRate <= 0.0)) {
                m_rateSearch.reset(); // What's in its buffer is from long before
            }

            if (m_rateSearch.process(m_buffer.data(), m_bufferFill))
            {
                const dvbs::SymbolRateSearch::Result& result = m_rateSearch.result();
                const double previous = m_autoCheckRate;
                const bool other = result.m_found && (std::fabs(result.m_symbolRate - m_autoRate) > 0.01 * m_autoRate);
                const bool confirmed = other && (std::fabs(result.m_symbolRate - previous) <= 0.01 * result.m_symbolRate);
                m_autoCheckRate = other && !confirmed ? result.m_symbolRate : 0.0;

                // Measured again at once to confirm another, else after as long again
                if (!other || confirmed) {
                    m_autoCheckSamples = 0;
                }

                if (confirmed)
                {
                    qDebug("DVBDemodSink::processSatellite: symbol rate %.1f S/s, not %.1f as found before (width %.0f Hz, centre %.0f Hz, line %.1f dB)",
                        result.m_symbolRate, m_autoRate, result.m_width, result.m_frequency, result.m_lineSnr);
                    m_autoRate = result.m_symbolRate;
                    m_signals = result.m_signals;
                    restartAutoRelock();
                    m_active = 0;
                    configureSatellite(m_channelSampleRate, m_autoRate);
                    configureDCRemoval();
                }
            }
        }

        // Or as long in wall time: behind (samples dropped, or lost as the sample FIFO overflows: searching at a wrong
        // rate costs the most), the samples processed came much slower than real time, so a lock lost to a rate found
        // wrong was stuck for a minute or more
        // Not before DVB-S2's slowest acquisitions could lock, at low symbol rates (VL-SNR's, time slicing's), with a
        // margin for its own restart
        const double relockSeconds = feedS2 ? std::max(m_autoRelockSeconds, dvbs2::Receiver::relockSeconds(m_autoRate) + 2.0)
            : m_autoRelockSeconds;

        if ((m_autoUnlockedSamples > relockSeconds * m_channelSampleRate) || (m_autoUnlockedSeconds > relockSeconds))
        {
            // As resetReceivers(): the status isn't the old rate's while searching (the receivers aren't fed, so
            // wouldn't clear it), nor the DC removal for its bandwidth
            m_autoRate = 0.0;
            restartAutoRelock();
            m_active = 0;
            m_rateSearch.reset();
            resetSatellite();
            m_rateHistory.clear();
            configureDCRemoval();
            clearStatus();
        }
    }

    // The symbol rate set, and nothing locked for a while: the signal's measured, as Auto does, so the display can say
    // it's another (as after a retune to a transponder at another rate, with the rate set for the one before, by the user
    // or the guide). The receivers carry on at the rate set
    if (!autoRate)
    {
        if (sLocked || s2Locked)
        {
            m_fixedUnlockedSamples = 0;
            m_signalSymbolRate = 0.0;
        }
        else if ((m_fixedUnlockedSamples += m_bufferFill) > m_setRateCheckSeconds * m_channelSampleRate)
        {
            // One search, then another after as long again: searching on, a rate found each time, took the thread
            // past real time. Each started afresh: what's left in its buffer is from the last, long before (or before
            // a lock)
            if (m_fixedUnlockedSamples - m_bufferFill <= m_setRateCheckSeconds * m_channelSampleRate) {
                m_rateSearch.reset();
            }

            if (m_rateSearch.process(m_buffer.data(), m_bufferFill))
            {
                const dvbs::SymbolRateSearch::Result& result = m_rateSearch.result();
                const double set = m_settings.m_symbolRate;
                m_signalSymbolRate = result.m_found && (std::fabs(result.m_symbolRate - set) > 0.01 * set) ? result.m_symbolRate : 0.0;
                m_fixedUnlockedSamples = 0;
            }
        }
    }

    const int shown = !autoStandard ? standard : m_active != 0 ? m_active : (int) S::StandardAutoSatellite;
    m_bufferFill = 0;

    updateRates();

    const bool updateDisplay = m_displaySamples >= (uint64_t) m_channelSampleRate / 10;

    if (updateDisplay)
    {
        if ((shown == S::StandardDVBS2) || ((shown == S::StandardAutoSatellite) && !feedS)) {
            m_receiverS2->getDisplayData(m_cells, 2000, m_responseDb, 512);
        } else {
            m_receiverS->getDisplayData(m_cells, 2000, m_responseDb, 512);
        }

        m_impulseDb.clear();
        m_displaySamples = 0;
    }

    QMutexLocker locker(&m_statusMutex);

    if (updateDisplay)
    {
        m_status.m_cells = m_cells;
        m_status.m_responseDb = m_responseDb;
        m_status.m_impulseDb.clear();
    }

    if (feedS) {
        m_status.m_s = sStatus;
    }

    if (feedS2) {
        m_status.m_s2 = s2Status;
    }

    m_status.m_standard = shown;
    m_status.m_power = m_power;
    m_status.m_tsBitRate = m_tsBitRate;
    m_status.m_udpDatagrams = m_udp.getDatagrams();
    m_status.m_autoSymbolRate = !autoRate ? 0.0 : m_autoRate > 0.0 ? m_autoRate : -1.0;
    m_status.m_signalSymbolRate = !autoRate ? m_signalSymbolRate : 0.0;

    if ((m_status.m_rateSearches != m_rateSearches) || (m_status.m_bandGeneration != m_bandGeneration))
    {
        m_status.m_signals = m_signals;
        m_status.m_spectrum = m_spectrum;
        m_status.m_spectrumBinHz = m_spectrumBinHz;
        m_status.m_minSignalWidth = m_minSignalWidth;
        m_status.m_rateSearches = m_rateSearches;
        m_status.m_bandGeneration = m_bandGeneration;
    }
}

void DVBDemodSink::processPacket(const uint8_t *packet)
{
    m_ratePackets++;
    m_epg.feed(packet);
    m_packetBlock.insert(m_packetBlock.end(), packet, packet + 188);

    if (m_packetBlock.size() >= m_packetBlockSize) {
        flushPackets();
    }
}

void DVBDemodSink::flushPackets()
{
    const int count = (int) (m_packetBlock.size() / 188);

    if (count == 0) {
        return;
    }

    const uint8_t *packets = m_packetBlock.data();

    {
        QMutexLocker locker(&m_recordersMutex);

        for (auto& recorder : m_recorders)
        {
            for (int i = 0; i < count; i++) {
                recorder.second->feed(packets + i * 188);
            }
        }
    }

    if (m_settings.m_udpEnabled) {
        m_udp.writePackets(packets, count);
    }

    if (m_settings.m_videoEnabled) {
        m_tsFifo.writePackets(packets, count);
    }

    m_packetBlock.clear();
}

int DVBDemodSink::startRecording(const QString& fileName, DVBDemodRecorder::Mode mode, int serviceId, QString& error)
{
    // The file is created here, rather than in the demodulator's thread
    std::unique_ptr<DVBDemodRecorder> recorder(new DVBDemodRecorder());

    if (!recorder->start(fileName, mode, serviceId, error)) {
        return -1;
    }

    QMutexLocker locker(&m_recordersMutex);
    const int id = m_nextRecordingId++;
    m_recorders.emplace_back(id, std::move(recorder));
    return id;
}

void DVBDemodSink::stopRecording(int id)
{
    std::shared_ptr<DVBDemodRecorder> recorder;

    {
        QMutexLocker locker(&m_recordersMutex);

        for (auto it = m_recorders.begin(); it != m_recorders.end(); ++it)
        {
            if (it->first == id)
            {
                recorder.reset(it->second.release());
                m_recorders.erase(it);
                break;
            }
        }

        // Those finished since
        m_stoppingRecorders.erase(std::remove_if(m_stoppingRecorders.begin(), m_stoppingRecorders.end(), [](const std::future<void>& f) {
            return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
        }), m_stoppingRecorders.end());
    }

    if (!recorder) {
        return;
    }

    // Writing what's queued, and closing the file, in a thread of its own: not holding up the demodulator, nor the
    // caller (the GUI's thread) for as long as a slow disk takes
    try
    {
        std::future<void> stopping = std::async(std::launch::async, [recorder]() {
            recorder->stop();
            qDebug() << "DVBDemodSink::stopRecording: finished" << recorder->getStatus().m_fileName;
        });
        QMutexLocker locker(&m_recordersMutex);
        m_stoppingRecorders.push_back(std::move(stopping));
    }
    catch (const std::system_error&)
    {
        recorder->stop(); // No thread for it
    }
}

std::vector<DVBDemodSink::Recording> DVBDemodSink::getRecordings()
{
    QMutexLocker locker(&m_recordersMutex);
    std::vector<Recording> recordings;

    for (const auto& recorder : m_recorders) {
        recordings.push_back({recorder.first, recorder.second->getStatus()});
    }

    return recordings;
}

void DVBDemodSink::configureDCRemoval()
{
    // The device's centre is -inputFrequencyOffset from the channel's, after the NCO: only removed if in the channel
    const int offset = m_settings.m_inputFrequencyOffset;
    const double bandwidth = DVBDemodSettings::isSatellite(m_settings.m_standard) ? m_settings.occupiedBandwidth(m_autoRate) : m_settings.m_bandwidth;
    m_dcRemoval = (m_channelSampleRate > 0) && (std::abs(offset) < 0.49 * bandwidth);
    m_dcStepAngle = m_channelSampleRate > 0 ? 2.0 * M_PI * -offset / m_channelSampleRate : 0.0;
    m_dcStep = dvbt::Cf((float) std::cos(m_dcStepAngle), (float) std::sin(m_dcStepAngle));
    m_dcRot = dvbt::Cf(1.0f, 0.0f);
    m_dcPhase = 0.0;
    m_dcCount = 0;
    m_dcMean = std::complex<double>(0.0, 0.0);
}

void DVBDemodSink::resetReceivers()
{
    m_rateHistory.clear();
    // The power, as the TS rate, is the new signal's from now
    m_powerSum = 0.0;
    m_powerCount = 0;
    m_power = -100.0;
    m_rateSamples = 0;
    m_ratePackets = 0;

    if (m_receiver) {
        m_receiver->reset();
    }

    if (m_receiverT2) {
        m_receiverT2->reset();
    }

    resetSatellite();
    m_active = 0;
    m_unlockedSamples = 0;
    m_fixedUnlockedSamples = 0;
    m_signalSymbolRate = 0.0;

    // A symbol rate found for the signal before isn't this one's. Searched for after the samples of the signal before
    // (queued in the device and DSP engine) have passed: measured from them, the receivers searched for the new
    // signal at the old one's rate until the relock. A search under way starts again too, as its samples are from before
    if (DVBDemodSettings::isSatellite(m_settings.m_standard) && (m_settings.m_symbolRate <= 0))
    {
        if (m_autoRate > 0.0)
        {
            m_autoRate = 0.0;
            restartAutoRelock();
            configureDCRemoval();
        }

        m_rateSearch.reset(m_retuneSettleSeconds);
    }

    clearStatus();
}

// Every 2 s while DVB-S2's not decoding: what its search is doing (to see why a lock's slow on a slower computer)
void DVBDemodSink::logS2Search(const dvbs2::Receiver::Status& status, bool locked)
{
    if (locked || (status.m_state == dvbs2::Receiver::State::Locked))
    {
        m_s2LogClock.invalidate();
        return;
    }

    // From a reset (the counts start again), measured from now
    if (!m_s2LogClock.isValid() || (status.m_searchedSymbols < m_s2LogLast.m_searchedSymbols)
        || (status.m_skippedSymbols < m_s2LogLast.m_skippedSymbols) || (status.m_seconds[1] < m_s2LogLast.m_seconds[1]))
    {
        m_s2LogClock.start();
        m_s2LogLast = status;
        return;
    }

    const qint64 ms = m_s2LogClock.elapsed();

    if (ms < 2000) {
        return;
    }

    const dvbs2::Receiver::Status& last = m_s2LogLast;
    const double seconds = ms * 1e-3;
    const uint64_t searched = status.m_searchedSymbols - last.m_searchedSymbols;
    const uint64_t skipped = status.m_skippedSymbols - last.m_skippedSymbols;
    const double symbols = (double) (searched + skipped);
    qDebug("DVBDemodSink::logS2Search: %s, front end %s offset %.0f Hz clock %.1f ppm | symbols %.2f of real time, searched %.0f%% | "
        "PL frames %llu misses %llu false locks %llu sync losses %llu, restarts %llu, Es/N0 %.1f dB, PL frequency %.0f Hz | "
        "PL %.2f cores",
        status.m_state == dvbs2::Receiver::State::Frames ? "frames (no TS)" : status.m_frontEnd.m_estimating ? "measuring the spectrum" : "searching",
        status.m_frontEnd.m_estimating ? "estimating" : "running", status.m_frontEnd.m_frequencyOffset, status.m_frontEnd.m_clockOffset,
        status.m_symbolRate > 0.0 ? symbols / (status.m_symbolRate * seconds) : 0.0, symbols > 0.0 ? 100.0 * searched / symbols : 0.0,
        (unsigned long long) status.m_pl.m_frames, (unsigned long long) status.m_pl.m_headerMisses,
        (unsigned long long) status.m_pl.m_falseLocks, (unsigned long long) status.m_pl.m_syncLosses,
        (unsigned long long) status.m_restarts, status.m_pl.m_esn0, status.m_pl.m_frequency * status.m_symbolRate,
        (status.m_seconds[1] - last.m_seconds[1]) / seconds);
    m_s2LogLast = status;
    m_s2LogClock.restart();
}

unsigned int DVBDemodSink::samplesToDrop(bool drops, bool locked, unsigned int fill, unsigned int size)
{
    return drops && !locked && (fill > size / 2) ? fill - size / 4 : 0;
}

void DVBDemodSink::discontinuity()
{
    if (m_sharedFrontEnd) {
        m_sharedFrontEnd->discontinuity();
    }
}

// The holdoff after a retune (resetReceivers()) is kept by setBand(), so a band set just after one isn't searched for
// in the samples from before it
void DVBDemodSink::setRateSearchBand(double frequency, double width, bool spectrumOnly, uint64_t generation)
{
    m_rateSearch.setBand(frequency, width);
    m_rateSearch.setSpectrumOnly(spectrumOnly);
    m_bandGeneration = generation;
    m_rateSearches = 0;
    m_signals.clear();
    m_spectrum.clear();
    m_autoRate = 0.0;
    restartAutoRelock();
    m_active = 0;
    m_unlockedSamples = 0;
    resetSatellite();
    m_rateHistory.clear();
    configureDCRemoval();
    clearStatus();
}

void DVBDemodSink::clearStatus()
{
    const int standard = m_settings.m_standard;
    m_tsBitRate = 0.0;
    m_cells.clear();
    m_responseDb.clear();
    m_impulseDb.clear();
    QMutexLocker locker(&m_statusMutex);
    m_status.m_receiver = dvbt::Receiver::Status();
    m_status.m_t2 = dvbt2::Receiver::Status();
    m_status.m_s = dvbs::Receiver::Status();
    m_status.m_s2 = dvbs2::Receiver::Status();
    m_status.m_standard = standard == S::StandardDVBT2Lite ? (int) S::StandardDVBT2 : standard;
    m_status.m_tsBitRate = 0.0;
    m_status.m_power = m_power;
    // As processSatellite() sets them, so the last standard's (or rate's) "SR search" or "Wrong SR" doesn't stay
    const bool satellite = DVBDemodSettings::isSatellite(standard);
    const bool autoRate = satellite && (m_settings.m_symbolRate <= 0);
    m_status.m_autoSymbolRate = !autoRate ? 0.0 : m_autoRate > 0.0 ? m_autoRate : -1.0;
    m_status.m_signalSymbolRate = satellite && !autoRate ? m_signalSymbolRate : 0.0;
    m_status.m_cells.clear();
    m_status.m_responseDb.clear();
    m_status.m_impulseDb.clear();
}

void DVBDemodSink::getStatus(Status& status)
{
    QMutexLocker locker(&m_statusMutex);
    status = m_status;
    status.m_fifoOverflows = m_tsFifo.getOverflows();
}

void DVBDemodSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "DVBDemodSink::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset;

    if ((m_channelFrequencyOffset != channelFrequencyOffset) || (m_channelSampleRate != channelSampleRate) || force)
    {
        if (channelSampleRate > 0)
        {
            m_ncoStepAngle = -2.0 * M_PI * channelFrequencyOffset / channelSampleRate;
            m_ncoStep = dvbt::Cf((float) std::cos(m_ncoStepAngle), (float) std::sin(m_ncoStepAngle));
            m_ncoShift = channelFrequencyOffset != 0;
            m_ncoPhase = 0.0;
            m_ncoRot = dvbt::Cf(1.0f, 0.0f);
            m_ncoCount = 0;
        }
    }

    if ((m_channelSampleRate != channelSampleRate) || force)
    {
        bool ok;
        double requiredRate;

        if (DVBDemodSettings::isSatellite(m_settings.m_standard) && m_receiverS && m_receiverS2)
        {
            // The satellite receivers' matched filters take any rate from 1.2 times the symbol rate. Automatic, it's
            // searched for at the channel's rate (all the baseband's), and the receivers configured when it's found
            const double symbolRate = m_settings.m_symbolRate;

            if (symbolRate <= 0.0)
            {
                m_rateSearch.configure(channelSampleRate, channelSampleRate / m_maxSymbolRateRatio, channelSampleRate / m_minSymbolRateRatio);
                m_autoRate = 0.0;
                restartAutoRelock();
                resetSatellite();
            }
            else
            {
                // To check the rate set is the signal's, if nothing locks
                m_rateSearch.configure(channelSampleRate, channelSampleRate / m_maxSymbolRateRatio, channelSampleRate / m_minSymbolRateRatio);
                m_fixedUnlockedSamples = 0;
                m_signalSymbolRate = 0.0;
            }

            const double ratio = symbolRate > 0.0 ? channelSampleRate / symbolRate : m_minSymbolRateRatio;
            ok = (ratio >= m_minSymbolRateRatio) && (ratio <= m_maxSymbolRateRatio);
            requiredRate = m_minSymbolRateRatio * symbolRate;

            if (ok && (symbolRate > 0.0)) {
                configureSatellite(channelSampleRate, symbolRate);
            }
            else
            {
                qWarning("DVBDemodSink::applyChannelSettings: channel sample rate %d S/s should be from %.0f to %.0f S/s for %.0f S/s",
                    channelSampleRate, m_minSymbolRateRatio * symbolRate, m_maxSymbolRateRatio * symbolRate, symbolRate);
            }
        }
        else
        {
            // The receivers resample to 8 * bandwidth / 7 (131/71 MS/s for 1.7 MHz), if the rate is different
            double required = S::terrestrialSampleRate(m_settings.m_bandwidth);
            double ratio = required > 0.0 ? channelSampleRate / required : 0.0;
            ok = (ratio >= m_minRateRatio) && (ratio <= m_maxRateRatio);
            requiredRate = m_minRateRatio * required;

            if (!ok) {
                qWarning("DVBDemodSink::applyChannelSettings: channel sample rate %d S/s should be %.0f S/s, or from %.0f to %.0f S/s",
                    channelSampleRate, required, m_minRateRatio * required, m_maxRateRatio * required);
            }

            if (m_receiver && m_receiverT2)
            {
                if (ok)
                {
                    m_receiver->setInputSampleRate(channelSampleRate);
                    m_receiverT2->setInputSampleRate(channelSampleRate);
                }
                else
                {
                    m_receiver->reset();
                    m_receiverT2->reset();
                }
            }
        }

        m_sampleRateOK = ok;
        m_bufferFill = 0;
        QMutexLocker locker(&m_statusMutex);
        m_status.m_requiredRate = requiredRate;
    }

    const bool reconfigure = (m_channelSampleRate != channelSampleRate) || (m_channelFrequencyOffset != channelFrequencyOffset) || force;
    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;

    {
        QMutexLocker locker(&m_statusMutex); // As getStatus() reads them from the GUI's thread
        m_status.m_channelSampleRate = m_channelSampleRate;
        m_status.m_sampleRateOK = m_sampleRateOK;
    }

    if (reconfigure) {
        configureDCRemoval();
    }
}

void DVBDemodSink::applySettings(const DVBDemodSettings& settings, const QStringList& settingsKeys, bool force)
{
    qDebug() << "DVBDemodSink::applySettings:" << settings.getDebugString(settingsKeys, force) << " force: " << force;

    // Against the settings before: those below are changed as they're applied
    const bool bandwidthChanged = (settingsKeys.contains("bandwidth") || force) && (settings.m_bandwidth != m_settings.m_bandwidth);
    const bool dcChanged = (settingsKeys.contains("inputFrequencyOffset") && (settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset))
        || (settingsKeys.contains("bandwidth") && (settings.m_bandwidth != m_settings.m_bandwidth))
        || (settingsKeys.contains("symbolRate") && (settings.m_symbolRate != m_settings.m_symbolRate))
        || (settingsKeys.contains("standard") && (settings.m_standard != m_settings.m_standard));

    if ((settingsKeys.contains("udpEnabled") || force) && (settings.m_udpEnabled != m_settings.m_udpEnabled)) {
        m_udp.discard(); // Not sent, stale, when it's on again
    }

    // The receivers are made again only if they're for another bandwidth (forced, as on each start, they're reset)
    if (bandwidthChanged)
    {
        m_settings.m_bandwidth = settings.m_bandwidth;
        createReceiver();
        applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
    }

    if ((settingsKeys.contains("standard") && (settings.m_standard != m_settings.m_standard)) || force)
    {
        const bool satelliteChanged = DVBDemodSettings::isSatellite(settings.m_standard) != DVBDemodSettings::isSatellite(m_settings.m_standard);

        if (m_receiver) {
            m_receiver->reset();
        }

        if (m_receiverT2)
        {
            m_receiverT2->setProfile(settings.m_standard == S::StandardDVBT2Lite ? dvbt2::Profile::Lite : dvbt2::Profile::Auto);
            m_receiverT2->reset();
        }

        m_settings.m_standard = settings.m_standard;
        m_active = 0;
        m_unlockedSamples = 0;

        // Terrestrial or satellite receivers made (and the others gone), and the channel's rate is for the symbol rate
        // (or the bandwidth)
        if (satelliteChanged || force)
        {
            m_settings.m_symbolRate = settings.m_symbolRate;
            m_settings.m_codeRate = settings.m_codeRate;
            m_settings.m_rollOff = settings.m_rollOff;
            m_settings.m_isi = settings.m_isi;
            m_settings.m_plScramblingCode = settings.m_plScramblingCode;
            m_settings.m_equaliser = settings.m_equaliser;
            m_settings.m_plpId = settings.m_plpId;
            m_settings.m_lowPriority = settings.m_lowPriority;
            createReceiver();
            applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
        }
        else
        {
            resetSatellite();
        }

        clearStatus();
    }

    if (settingsKeys.contains("symbolRate") && (settings.m_symbolRate != m_settings.m_symbolRate))
    {
        m_settings.m_symbolRate = settings.m_symbolRate;
        applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
    }

    if ((settingsKeys.contains("codeRate") || force) && m_receiverS) {
        m_receiverS->setCodeRate(settings.m_codeRate);
    }

    if ((settingsKeys.contains("rollOff") || force) && m_receiverS2) {
        m_receiverS2->setRollOff(settings.m_rollOff / 100.0);
    }

    if ((settingsKeys.contains("isi") || force) && m_receiverS2) {
        m_receiverS2->setISI(settings.m_isi);
    }

    if (settingsKeys.contains("plScramblingCode") && (settings.m_plScramblingCode != m_settings.m_plScramblingCode) && m_receiverS2)
    {
        if (m_sharedFrontEnd) {
            m_sharedFrontEnd->discard(); // The receiver's reset, from this thread
        }

        m_receiverS2->setScramblingCode(settings.m_plScramblingCode);
    }

    if (settingsKeys.contains("equaliser") || force) {
        setEqualiser(settings.m_equaliser);
    }

    if ((settingsKeys.contains("plpId") || force) && m_receiverT2) {
        m_receiverT2->selectPLP(settings.m_plpId);
    }

    if ((settingsKeys.contains("lowPriority") || force) && m_receiver) {
        m_receiver->setLowPriority(settings.m_lowPriority);
    }

    if (settingsKeys.contains("udpAddress") || settingsKeys.contains("udpPort") || force) {
        m_udp.setDestination(settings.m_udpAddress, settings.m_udpPort);
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

    if (dcChanged || force) {
        configureDCRemoval();
    }
}
