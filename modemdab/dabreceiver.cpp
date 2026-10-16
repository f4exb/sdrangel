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
#include <cstdio>
#include <cstdlib>

#include "dabreceiver.h"

namespace modemdab
{

namespace
{

constexpr int WindowFrames = 10;            // About one second
constexpr int MaxBridgedFrames = 8;         // Longer gaps restart the service decoders
constexpr int FICLossFrames = 10;           // Frames (1 s) without a good FIB before searching again
constexpr int SlideShowApplication = 0x002;
constexpr int JournalineApplication = 0x44a;
constexpr int WebsiteApplication = 0x003;
constexpr int SPIApplication = 0x007;

} // anonymous namespace

DABReceiver::DABReceiver() :
    m_sid(0),
    m_configuredVersion(0),
    m_serviceFound(false),
    m_dabPlus(false),
    m_mp2DecoderTried(false),
    m_journalineUpdated(false),
    m_website(std::make_shared<modemdr::MOTWebsite>()),
    m_websiteUpdated(false),
    m_spi(std::make_shared<modemdr::SPIData>()),
    m_spiUpdated(false),
    m_announcementTypes(0),
    m_announcementCluster(-1),
    m_cifCount(-1),
    m_lastFrameStart(0),
    m_haveLastFrame(false),
    m_haveSlide(false),
    m_labelUpdated(false),
    m_fibWindow(WindowFrames),
    m_audioWindow(WindowFrames),
    m_ficBERWindow(WindowFrames),
    m_mscBERWindow(WindowFrames),
    m_pcmWindow(WindowFrames),
    m_windowIndex(0),
    m_ficLocked(false),
    m_ficBadFrames(0),
    m_detection(Detection::ThreeSymbol),
    m_appliedDetection(Detection::ThreeSymbol),
    m_coherentIterations(1),
    m_coherentThreshold(DABCoherentDecoder::DefaultReliabilityThreshold),
    m_appliedThreshold(-1.0f),
    m_threaded(false),
    m_stop(false),
    m_reacquireRequested(false),
    m_busy(false)
{
    m_frontEnd.setFrameCallback([this](const DABFrame& frame) { frameReady(frame); });
    m_frontEnd.setDetection(DABFrontEnd::Detection::ThreeSymbol);
}

DABReceiver::~DABReceiver()
{
    stopWorker();
}

void DABReceiver::setIterations(int iterations)
{
    // Applied by the decoder, in its own thread
    m_coherentIterations = std::max(1, iterations);
}

void DABReceiver::setReliabilityThreshold(float threshold)
{
    m_coherentThreshold = threshold;
}

void DABReceiver::setDetection(Detection detection)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_frontEnd.setDetection(detection != Detection::Differential
        ? DABFrontEnd::Detection::ThreeSymbol : DABFrontEnd::Detection::Differential);
    m_frontEnd.setExportCarriers(detection == Detection::Coherent);
    m_detection = detection;
}

void DABReceiver::setThreaded(bool threaded)
{
    if (threaded == m_threaded) {
        return;
    }
    if (threaded) {
        startWorker();
    } else {
        stopWorker();
    }
    m_threaded = threaded;
}

void DABReceiver::startWorker()
{
    m_stop = false;
    m_worker = std::thread([this]() { work(); });
}

void DABReceiver::stopWorker()
{
    if (!m_worker.joinable()) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_stop = true;
    }
    m_queueCondition.notify_all();
    m_worker.join();
    // Frames not decoded are lost (bridged when decoding resumes)
    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_frames.clear();
}

// A frame from the front end: decode it now, or queue it for the decoding thread
void DABReceiver::frameReady(const DABFrame& frame)
{
    if (!m_threaded)
    {
        onFrame(frame);
        return;
    }

    std::unique_ptr<DABFrame> copy;
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (!m_framePool.empty())
        {
            copy = std::move(m_framePool.back());
            m_framePool.pop_back();
        }
    }
    if (!copy) {
        copy.reset(new DABFrame());
    }
    *copy = frame;

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        // If decoding cannot keep up, the oldest frames go: they are bridged as lost
        while (m_frames.size() >= MaximumQueuedFrames)
        {
            m_framePool.push_back(std::move(m_frames.front()));
            m_frames.pop_front();
        }
        m_frames.push_back(std::move(copy));
    }
    m_queueCondition.notify_one();
}

void DABReceiver::work()
{
    while (true)
    {
        std::unique_ptr<DABFrame> frame;
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_queueCondition.wait(lock, [this]() { return m_stop || !m_frames.empty(); });
            if (m_stop) {
                return;
            }
            frame = std::move(m_frames.front());
            m_frames.pop_front();
            m_busy = true;
        }
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            onFrame(*frame);
        }
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            m_framePool.push_back(std::move(frame));
            m_busy = false;
        }
        m_queueCondition.notify_all();
    }
}

void DABReceiver::flush()
{
    if (m_threaded)
    {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        m_queueCondition.wait(lock, [this]() { return m_stop || (m_frames.empty() && !m_busy); });
    }
    deliver();
}

// Make the callbacks the decoding thread queued, in the caller's thread
void DABReceiver::deliver()
{
    std::deque<DABPCM> pcm;
    std::deque<modemdr::MOTObject> slides;
    {
        std::lock_guard<std::mutex> lock(m_outputMutex);
        pcm.swap(m_pcmOutput);
        slides.swap(m_slideOutput);
    }
    for (const DABPCM& p : pcm)
    {
        if (m_audioCallback) {
            m_audioCallback(p.samples.data(), p.frames(), p.sampleRate);
        }
    }
    for (const modemdr::MOTObject& s : slides)
    {
        if (m_slideCallback) {
            m_slideCallback(s);
        }
    }
}

void DABReceiver::emitPCM(const DABPCM& pcm)
{
    if (!m_threaded)
    {
        if (m_audioCallback) {
            m_audioCallback(pcm.samples.data(), pcm.frames(), pcm.sampleRate);
        }
        return;
    }
    std::lock_guard<std::mutex> lock(m_outputMutex);
    m_pcmOutput.push_back(pcm);
    // A caller that stops processing does not collect audio: keep about 2 s
    while (m_pcmOutput.size() > 100) {
        m_pcmOutput.pop_front();
    }
}

void DABReceiver::emitSlide(const modemdr::MOTObject& slide)
{
    if (!m_threaded)
    {
        if (m_slideCallback) {
            m_slideCallback(slide);
        }
        return;
    }
    std::lock_guard<std::mutex> lock(m_outputMutex);
    m_slideOutput.push_back(slide);
    while (m_slideOutput.size() > 4) {
        m_slideOutput.pop_front();
    }
}

DABEnsemble DABReceiver::ensemble() const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_ensemble;
}

DABDynamicLabel DABReceiver::dynamicLabel() const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_pad.label();
}

std::vector<DABTIITransmitter> DABReceiver::tii() const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_tii.transmitters();
}

bool DABReceiver::tiiAnalysed() const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_tii.analysed();
}

bool DABReceiver::lastSlide(modemdr::MOTObject& slide) const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    if (m_haveSlide) {
        slide = m_lastSlide;
    }
    return m_haveSlide;
}

std::map<int, modemdr::JournalineObject> DABReceiver::journaline() const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_journaline.objects();
}

void DABReceiver::reset()
{
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        while (!m_frames.empty())
        {
            m_framePool.push_back(std::move(m_frames.front()));
            m_frames.pop_front();
        }
    }
    {
        std::lock_guard<std::mutex> lock(m_outputMutex);
        m_pcmOutput.clear();
        m_slideOutput.clear();
    }
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_reacquireRequested = false;
    m_frontEnd.reset();
    m_ensemble.reset();
    m_tii.reset();
    for (auto& pass : m_coherent) {
        pass->reset();
    }
    m_cifCount = -1;
    m_haveLastFrame = false;
    m_ficLocked = false;
    m_ficBadFrames = 0;
    m_status = DABReceiverStatus();
    for (int i = 0; i < WindowFrames; i++)
    {
        m_fibWindow[i] = Window();
        m_audioWindow[i] = Window();
        m_ficBERWindow[i] = std::make_pair(0, 0);
        m_mscBERWindow[i] = std::make_pair(0, 0);
        m_pcmWindow[i] = 0;
    }
    clearService();
    m_spiComponents.clear();
    m_spiCache.clear();
    m_spi = std::make_shared<modemdr::SPIData>();
    m_spiUpdated = true;
    m_configuredVersion = m_ensemble.version();
}

void DABReceiver::reacquire()
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_frontEnd.reacquire();
    m_haveLastFrame = false;
}

void DABReceiver::process(const std::complex<float> *samples, std::size_t count)
{
    // The decoder asks for this when the FIC is lost; the front end is the caller's
    if (m_reacquireRequested.exchange(false)) {
        m_frontEnd.reacquire();
    }
    m_frontEnd.process(samples, count);
    if (m_threaded) {
        deliver();
    }
}

void DABReceiver::selectService(uint32_t sid)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    if (sid != m_sid)
    {
        m_sid = sid;
        clearService();
        configureService();
    }
}

bool DABReceiver::journalineUpdated()
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    bool updated = m_journalineUpdated;
    m_journalineUpdated = false;
    return updated;
}

bool DABReceiver::labelUpdated()
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    bool updated = m_labelUpdated;
    m_labelUpdated = false;
    return updated;
}

std::vector<float> DABReceiver::carrierCN() const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_carrierCN;
}

DABReceiverStatus DABReceiver::status() const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    DABReceiverStatus status = m_status;
    int good = 0;
    int total = 0;
    int audioGood = 0;
    int audioTotal = 0;
    int64_t ficErrors = 0;
    int64_t ficBits = 0;
    int64_t mscErrors = 0;
    int64_t mscBits = 0;
    int pcm = 0;

    for (int i = 0; i < WindowFrames; i++)
    {
        good += m_fibWindow[i].good;
        total += m_fibWindow[i].total;
        audioGood += m_audioWindow[i].good;
        audioTotal += m_audioWindow[i].total;
        ficErrors += m_ficBERWindow[i].first;
        ficBits += m_ficBERWindow[i].second;
        mscErrors += m_mscBERWindow[i].first;
        mscBits += m_mscBERWindow[i].second;
        pcm += m_pcmWindow[i];
    }
    status.locked = m_frontEnd.state() == DABFrontEnd::State::Locked;
    status.ficLocked = m_ficLocked;
    status.fibQuality = total > 0 ? 100.0f * good / total : 0.0f;
    status.audioQuality = audioTotal > 0 ? 100.0f * audioGood / audioTotal : 0.0f;
    status.ficBER = ficBits > 0 ? static_cast<float>(ficErrors) / ficBits : 0.0f;
    status.mscBER = mscBits > 0 ? static_cast<float>(mscErrors) / mscBits : 0.0f;
    status.audioActive = pcm > 0;
    status.acquisitions = m_frontEnd.acquisitionAttempts();
    status.locks = m_frontEnd.locks();
    status.padCRCErrors = m_pad.crcErrors();
    status.journalineObjects = static_cast<int>(m_journaline.objects().size());
    return status;
}

void DABReceiver::nextWindow()
{
    m_windowIndex = (m_windowIndex + 1) % WindowFrames;
    m_fibWindow[m_windowIndex] = Window();
    m_audioWindow[m_windowIndex] = Window();
    m_ficBERWindow[m_windowIndex] = std::make_pair(0, 0);
    m_mscBERWindow[m_windowIndex] = std::make_pair(0, 0);
    m_pcmWindow[m_windowIndex] = 0;
}

void DABReceiver::onFrame(const DABFrame& frame)
{
    nextWindow();

    m_status.frames++;
    m_status.frequencyOffsetHz = frame.frequencyOffsetHz;
    m_status.clockErrorPPM = frame.clockErrorPPM;
    m_status.snrDB = frame.snrDB;
    m_status.merDB = frame.merDB;
    m_status.cnDB = frame.cnDB;
    m_status.nullDepthDB = frame.nullDepthDB;
    m_carrierCN = frame.carrierCN;

    // Frames the front end could not deliver (lost lock) are bridged with erasures
    if (m_haveLastFrame)
    {
        const double gap = static_cast<double>(frame.startSample - m_lastFrameStart) / ModeI::FrameSize;
        const int missing = static_cast<int>(std::lround(gap)) - 1;
        if (missing > 0) {
            lostFrames(missing);
        }
    }
    m_lastFrameStart = frame.startSample;
    m_haveLastFrame = true;

    // FIC
    std::array<std::array<uint8_t, ModeI::FIBBytes>, ModeI::FIBsPerCIF> fibs;
    std::array<bool, ModeI::FIBsPerCIF> valid;
    const Detection detection = m_detection;
    if (detection != m_appliedDetection)
    {
        // The audio sub-channel is fed by the coherent pass or straight from the first, so
        // starts again
        m_appliedDetection = detection;
        for (auto& pass : m_coherent) {
            pass->reset();
        }
        if (m_audioSubchannel.configured()) {
            m_audioSubchannel.configure(m_audioSubchannel.subchannel());
        }
    }
    const bool coherent = m_appliedDetection == Detection::Coherent;
    std::array<std::vector<uint8_t>, ModeI::CIFsPerFrame> ficCoded;
    std::array<bool, ModeI::CIFsPerFrame> ficValid{};
    bool cifCountFound = false;
    int goodFIBs = 0;

    m_ensemble.cifCountUpdated();   // Only a FIG 0/0 of this frame counts

    for (int block = 0; block < ModeI::CIFsPerFrame; block++)
    {
        int bitErrors;
        int codedBits;
        m_fic.decodeBlock(&frame.ficSoft[block * ModeI::FICBlockBits], fibs, valid, bitErrors, codedBits);
        m_ficBERWindow[m_windowIndex].first += bitErrors;
        m_ficBERWindow[m_windowIndex].second += codedBits;

        for (int f = 0; f < ModeI::FIBsPerCIF; f++)
        {
            m_fibWindow[m_windowIndex].add(valid[f]);
            m_status.fibs++;
            if (valid[f])
            {
                goodFIBs++;
                m_ensemble.processFIB(fibs[f].data());
            }
            else
            {
                m_status.fibErrors++;
            }
        }
        if (coherent && valid[0] && valid[1] && valid[2])
        {
            ficCoded[block] = DABConvolutional::encode(m_fic.lastBits(), DABTables::ficProfile());
            ficValid[block] = true;
        }
        // FIG 0/0 is in the first FIB of the first CIF of the frame
        if ((block == 0) && m_ensemble.cifCountUpdated())
        {
            m_cifCount = m_ensemble.lastCIFCount();
            cifCountFound = true;
        }
    }
    if (!cifCountFound && (m_cifCount >= 0)) {
        m_cifCount = (m_cifCount + ModeI::CIFsPerFrame) % ModeI::CIFCountModulus;
    }
    m_status.cifCount = m_cifCount;

    if (goodFIBs > 0)
    {
        m_ficLocked = true;
        m_ficBadFrames = 0;
    }
    else if (++m_ficBadFrames >= FICLossFrames)
    {
        // The timing or frequency is wrong
        if (std::getenv("MODEMDAB_DEBUG")) {
            std::fprintf(stderr, "receiver: no good FIB for %d frames, reacquire\n", FICLossFrames);
        }
        m_ficLocked = false;
        m_ficBadFrames = 0;
        m_haveLastFrame = false;
        m_reacquireRequested = true;
        return;
    }

    // TII is in the null symbol of frames starting at CIF counts 0 modulo 8
    if ((m_cifCount >= 0) && ((m_cifCount % 8) == 0)) {
        m_tii.push(frame.nullSpectrum);
    }

    if (m_ensemble.version() != m_configuredVersion)
    {
        configureSPI();
        configureService();
    }

    // MSC
    for (int i = 0; i < ModeI::CIFsPerFrame; i++)
    {
        if (m_cifCount >= 0) {
            m_ensemble.cif((m_cifCount + i) % ModeI::CIFCountModulus);
        }
        processCIF(&frame.mscSoft[i * ModeI::CIFBits]);
    }

    // Decoder aided second pass: every sub-channel is decoded, and the frames whose symbols
    // are then known are demodulated again for the service's audio
    if (coherent && (m_cifCount >= 0))
    {
        const int iterations = m_coherentIterations;
        const float threshold = m_coherentThreshold;
        if ((static_cast<int>(m_coherent.size()) != iterations) || (threshold != m_appliedThreshold))
        {
            // New passes: the audio they feed starts again too
            m_coherent.clear();
            for (int i = 0; i < iterations; i++)
            {
                m_coherent.emplace_back(new DABCoherentDecoder());
                m_coherent.back()->setReliabilityThreshold(threshold);
            }
            m_appliedThreshold = threshold;
            if (m_audioSubchannel.configured()) {
                m_audioSubchannel.configure(m_audioSubchannel.subchannel());
            }
        }

        const std::vector<DABSubchannel> subchannels = m_ensemble.subchannels();
        std::vector<DABCoherentDecoder::Output> outputs;

        // Each pass takes the frames the one before demodulated again
        for (std::size_t p = 0; p < m_coherent.size(); p++)
        {
            DABCoherentDecoder& pass = *m_coherent[p];
            pass.setSubchannels(subchannels);
            std::vector<DABCoherentDecoder::Output> next;
            DABCoherentDecoder::Output output;
            if (p == 0)
            {
                pass.push(frame, m_cifCount, ficCoded, ficValid);
                while (pass.pop(output)) {
                    next.push_back(std::move(output));
                }
            }
            else
            {
                for (const auto& input : outputs)
                {
                    pass.push(input.frame, input.cifCount, input.ficCoded, input.ficValid);
                    while (pass.pop(output)) {
                        next.push_back(std::move(output));
                    }
                }
            }
            outputs.swap(next);
        }
        for (const auto& output : outputs)
        {
            for (int i = 0; i < ModeI::CIFsPerFrame; i++) {
                audioCIF(&output.frame.mscSoft[i * ModeI::CIFBits]);
            }
        }
    }
}

void DABReceiver::audioCIF(const float *cif)
{
    std::vector<uint8_t> logicalFrame;

    if (m_audioSubchannel.configured() && m_audioSubchannel.push(cif, logicalFrame))
    {
        m_mscBERWindow[m_windowIndex].first += m_audioSubchannel.lastBitErrors();
        m_mscBERWindow[m_windowIndex].second += m_audioSubchannel.lastComparedBits();
        audioLogicalFrame(logicalFrame, false);
    }
}

void DABReceiver::lostFrames(int count)
{
    m_status.framesLost += count;
    if (m_appliedDetection == Detection::Coherent)
    {
        // The second pass starts again; so does the audio it feeds
        for (auto& pass : m_coherent) {
            pass->reset();
        }
        if (m_audioSubchannel.configured()) {
            m_audioSubchannel.configure(m_audioSubchannel.subchannel());
        }
    }
    if (m_cifCount >= 0) {
        m_cifCount = (m_cifCount + count * ModeI::CIFsPerFrame) % ModeI::CIFCountModulus;
    }

    if (count > MaxBridgedFrames)
    {
        // Too long to conceal: start the service decoders again
        if (m_audioSubchannel.configured()) {
            m_audioSubchannel.configure(m_audioSubchannel.subchannel());
        }
        for (auto& component : m_dataComponents)
        {
            component->subchannel.configure(component->subchannel.subchannel());
            component->packets.frameLost();
        }
        for (auto& component : m_spiComponents)
        {
            component->subchannel.configure(component->subchannel.subchannel());
            component->packets.frameLost();
        }
        m_dabPlusFramer.reset();
        m_mp2Framer.reset();
        return;
    }
    for (int i = 0; i < count * ModeI::CIFsPerFrame; i++) {
        processCIF(nullptr);
    }
}

void DABReceiver::processCIF(const float *cif)
{
    std::vector<uint8_t> logicalFrame;

    // In coherent mode the audio comes from the second pass
    if (m_appliedDetection != Detection::Coherent) {
        audioCIF(cif);
    }
    for (auto& component : m_dataComponents)
    {
        if (component->subchannel.push(cif, logicalFrame)) {
            dataLogicalFrame(*component, logicalFrame, component->subchannel.lastFrameDamaged());
        }
    }
    for (auto& component : m_spiComponents)
    {
        if (component->subchannel.push(cif, logicalFrame)) {
            spiLogicalFrame(*component, logicalFrame, component->subchannel.lastFrameDamaged());
        }
    }
}

void DABReceiver::clearService()
{
    m_serviceFound = false;
    m_service = DABService();
    m_audioSubchannel.configure(DABSubchannel());
    m_dataComponents.clear();
    m_dabPlusFramer.reset();
    m_mp2Framer.reset();
    m_audioDecoder.reset();
    m_mp2DecoderTried = false;
    m_aacParameters = DABPlusParameters();
    m_pad.reset();
    m_mot.reset();
    m_packetMOT.reset();
    m_journaline.reset();
    m_journalineUpdated = true;
    m_websiteDecoder.reset();
    m_website = std::make_shared<modemdr::MOTWebsite>(m_websiteDecoder.website());
    m_websiteUpdated = true;
    m_status.website = false;
    m_haveSlide = false;
    m_lastSlide = modemdr::MOTObject();
    m_labelUpdated = true;
    m_announcementCluster = -1;
    m_status.announcementType = -1;
    m_status.announcementCluster = -1;
    m_status.announcementSubchannel = -1;
    m_status.announcementSwitched = false;
    m_status.announcementService.clear();

    m_status.sid = m_sid;
    m_status.serviceFound = false;
    m_status.dabPlus = false;
    m_status.subchannelId = -1;
    m_status.bitrate = 0;
    m_status.protection.clear();
    m_status.audioDecoder = false;
    m_status.audioBackend.clear();
    m_status.sampleRate = 0;
    m_status.stereo = false;
    m_status.sbr = false;
    m_status.ps = false;
    m_status.superframes = 0;
    m_status.fireCodeErrors = 0;
    m_status.rsCorrectedBytes = 0;
    m_status.rsUncorrectableRows = 0;
    m_status.audioFrames = 0;
    m_status.audioFrameErrors = 0;
    m_status.pcmFrames = 0;
    m_status.concealedFrames = 0;
    m_status.slides = 0;
}

void DABReceiver::configureService()
{
    m_configuredVersion = m_ensemble.version();
    if (m_sid == 0) {
        return;
    }

    DABService service;
    if (!m_ensemble.service(m_sid, service))
    {
        if (m_serviceFound) {
            clearService();
        }
        return;
    }

    // The primary audio component, else the first audio component
    const DABServiceComponent *audio = nullptr;
    for (const auto& component : service.components)
    {
        if (component.isAudio() && (!audio || (component.primary && !audio->primary))) {
            audio = &component;
        }
    }

    DABSubchannel subchannel;
    if (!audio || !m_ensemble.subchannel(audio->subchannelId, subchannel))
    {
        m_service = service;
        return;
    }

    // An announcement: its sub-channel's audio for as long as it lasts
    bool dabPlus = audio->isDABPlus();
    const int announcement = announcementSubchannel();
    DABSubchannel target;
    const int announcementASCTy = m_ensemble.subchannelASCTy(announcement);
    m_status.announcementSwitched = false;
    if ((announcement >= 0) && (announcement != subchannel.id) && (announcementASCTy >= 0)
        && m_ensemble.subchannel(announcement, target))
    {
        subchannel = target;
        dabPlus = announcementASCTy == 63;
        m_status.announcementSwitched = true;
    }

    const bool changed = !m_audioSubchannel.configured() || !(m_audioSubchannel.subchannel() == subchannel)
        || (m_dabPlus != dabPlus);

    m_service = service;
    m_serviceFound = true;
    if (changed)
    {
        m_audioSubchannel.configure(subchannel);
        m_dabPlus = dabPlus;
        m_dabPlusFramer.reset();
        m_mp2Framer.reset();
        m_audioDecoder.reset();
        m_mp2DecoderTried = false;
        m_aacParameters = DABPlusParameters();
        m_pad.reset();
        m_mot.reset();
    }

    // SlideShow and Journaline in the X-PAD
    for (const auto& app : audio->userApplications)
    {
        if ((app.type == SlideShowApplication) && app.xpad) {
            m_pad.setMOTApplicationType(app.xpadAppType);
        } else if ((app.type == JournalineApplication) && app.xpad) {
            m_pad.setJournalineApplicationType(app.xpadAppType);
        }
    }

    // SlideShow and Journaline in packet mode components of the service
    struct Wanted
    {
        int application;
        DABSubchannel subchannel;
        int address;
        bool dataGroups;
        std::vector<uint8_t> data;
    };
    std::vector<Wanted> wanted;
    for (const auto& component : service.components)
    {
        DABSubchannel dataSubchannel;
        if ((component.transport != DABServiceComponent::PacketData) || !m_ensemble.subchannel(component.subchannelId, dataSubchannel)) {
            continue;
        }
        for (const auto& app : component.userApplications)
        {
            if ((app.type == SlideShowApplication) || (app.type == JournalineApplication) || (app.type == WebsiteApplication)) {
                wanted.push_back({app.type, dataSubchannel, component.packetAddress, component.dataGroups, app.data});
            }
        }
    }
    bool same = wanted.size() == m_dataComponents.size();
    for (std::size_t i = 0; same && (i < wanted.size()); i++)
    {
        const DataComponent& c = *m_dataComponents[i];
        same = (c.application == wanted[i].application) && (c.subchannel.subchannel() == wanted[i].subchannel)
            && (c.address == wanted[i].address) && (c.dataGroups == wanted[i].dataGroups);
    }
    if (!same)
    {
        m_dataComponents.clear();
        for (const Wanted& w : wanted)
        {
            std::unique_ptr<DataComponent> c(new DataComponent());
            c->application = w.application;
            c->address = w.address;
            c->dataGroups = w.dataGroups;
            c->subchannel.configure(w.subchannel);
            m_dataComponents.push_back(std::move(c));
            if (w.application == WebsiteApplication)
            {
                // The user application data lists the website's profiles, up to a 0x00
                // (TS 101 498-1 clause 5.5.1)
                const auto end = std::find(w.data.begin(), w.data.end(), 0U);
                m_websiteDecoder.setProfiles(std::vector<uint8_t>(w.data.begin(), end));
            }
        }
        m_packetMOT.reset();
    }
    m_status.website = std::any_of(wanted.begin(), wanted.end(), [](const Wanted& w) { return w.application == WebsiteApplication; });

    m_status.sid = m_sid;
    m_status.serviceFound = true;
    m_status.dabPlus = m_dabPlus;
    m_status.subchannelId = subchannel.id;
    m_status.bitrate = subchannel.bitrate;
    m_status.protection = subchannel.protection();
    m_status.audioDecoder = m_dabPlus ? DABAudioDecoder::aacAvailable() : DABAudioDecoder::mp2Available();
}

void DABReceiver::audioLogicalFrame(const std::vector<uint8_t>& frame, bool lost)
{
    const int size = static_cast<int>(frame.size());

    if (m_dabPlus)
    {
        DABPlusSuperframe superframe;
        if (!m_dabPlusFramer.push(lost ? nullptr : frame.data(), size, superframe)) {
            return;
        }

        m_status.superframes = m_dabPlusFramer.superframes();
        m_status.fireCodeErrors = m_dabPlusFramer.fireCodeErrors();
        m_status.rsCorrectedBytes = m_dabPlusFramer.rsCorrectedBytes();
        m_status.rsUncorrectableRows = m_dabPlusFramer.rsUncorrectableRows();

        const DABPlusParameters& p = superframe.parameters;
        if ((p.numAUs > 0) && (!m_audioDecoder || (p != m_aacParameters)))
        {
            m_aacParameters = p;
            m_audioDecoder = DABAudioDecoder::createAAC(p);
            m_status.sampleRate = p.outputSampleRate();
            m_status.stereo = p.stereo || p.ps;
            m_status.sbr = p.sbr;
            m_status.ps = p.ps;
            m_status.audioBackend = m_audioDecoder ? m_audioDecoder->backend() : "";
        }

        for (const DABAccessUnit& au : superframe.accessUnits)
        {
            m_status.audioFrames++;
            m_audioWindow[m_windowIndex].add(au.valid);
            if (!au.valid) {
                m_status.audioFrameErrors++;
            }

            if (au.valid)
            {
                const uint8_t *pad;
                int padLength;
                if (DABPlusFramer::extractPAD(au.data, pad, padLength))
                {
                    m_pad.process(pad + padLength - 2, padLength - 2, pad + padLength - 2);
                    if (m_pad.labelUpdated()) {
                        m_labelUpdated = true;
                    }
                    slides(m_mot, m_pad.motDataGroups());
                    journaline(m_pad.journalineDataGroups());
                }
            }
            decodeAudio(au.data.data(), static_cast<int>(au.data.size()), au.valid);
        }
    }
    else
    {
        DABMP2Frame mp2;
        if (!m_mp2Framer.push(lost ? nullptr : frame.data(), size, m_audioSubchannel.subchannel().bitrate, mp2)) {
            return;
        }

        m_status.audioFrames++;
        m_audioWindow[m_windowIndex].add(mp2.valid);
        if (!mp2.valid) {
            m_status.audioFrameErrors++;
        }

        if (!m_audioDecoder && !m_mp2DecoderTried)
        {
            m_audioDecoder = DABAudioDecoder::createMP2();
            m_mp2DecoderTried = true;
            m_status.audioBackend = m_audioDecoder ? m_audioDecoder->backend() : "";
        }
        if (mp2.valid)
        {
            m_status.sampleRate = mp2.sampleRate;
            m_status.stereo = mp2.channels == 2;
            m_status.sbr = false;
            m_status.ps = false;

            const int frameSize = static_cast<int>(mp2.data.size());
            const uint8_t *fpad = &mp2.data[frameSize - 2];
            const int available = std::min(196, frameSize - 2 - mp2.scfCRCLength - 8);
            if (available > 0)
            {
                m_pad.process(fpad - mp2.scfCRCLength, available, fpad);
                if (m_pad.labelUpdated()) {
                    m_labelUpdated = true;
                }
                slides(m_mot, m_pad.motDataGroups());
                journaline(m_pad.journalineDataGroups());
            }
        }
        decodeAudio(mp2.data.data(), static_cast<int>(mp2.data.size()), mp2.valid);
    }
}

void DABReceiver::decodeAudio(const uint8_t *data, int size, bool valid)
{
    if (!m_audioDecoder) {
        return;
    }

    DABPCM pcm;
    if (valid) {
        m_audioDecoder->decode(data, size, pcm);
    } else {
        m_audioDecoder->conceal(pcm);
    }
    outputPCM(pcm);
}

void DABReceiver::outputPCM(const DABPCM& pcm)
{
    if (pcm.samples.empty()) {
        return;
    }
    if (pcm.concealed)
    {
        m_status.concealedFrames++;
    }
    else
    {
        m_status.pcmFrames++;
        m_pcmWindow[m_windowIndex]++;
    }
    emitPCM(pcm);
}

void DABReceiver::dataLogicalFrame(DataComponent& component, const std::vector<uint8_t>& frame, bool lost)
{
    if (lost)
    {
        component.packets.frameLost();
        return;
    }

    std::vector<std::vector<uint8_t>> dataGroups;
    component.packets.push(frame.data(), static_cast<int>(frame.size()), component.address, component.dataGroups, dataGroups);
    if (!component.dataGroups) {
        return;
    }
    if (component.application == SlideShowApplication) {
        slides(m_packetMOT, dataGroups);
    } else if (component.application == JournalineApplication) {
        journaline(dataGroups);
    } else if (component.application == WebsiteApplication) {
        website(dataGroups);
    }
}

void DABReceiver::setAnnouncementTypes(uint16_t types)
{
    m_announcementTypes = types;
    // Applied at the next ensemble change, or now
    std::lock_guard<std::mutex> lock(m_stateMutex);
    configureService();
}

// The announcement to follow (TS 103 176 clause 7.3): an alarm, whenever the ensemble signals
// alarms; else one in a cluster the service belongs to, of a type it supports and is wanted. One
// in progress is not interrupted, except by an alarm. Returns its sub-channel, or -1 for none.
int DABReceiver::announcementSubchannel()
{
    const auto& announcements = m_ensemble.announcements();
    const DABAnnouncementSupport support = m_ensemble.announcementSupport(m_sid);
    const uint16_t wanted = m_announcementTypes;
    int cluster = -1;

    const auto followed = [&](int c) {
        const auto it = announcements.find(c);
        if (it == announcements.end()) {
            return false;
        }
        if (c == 0xff) {
            return m_ensemble.info().alarm;
        }
        return (std::find(support.clusters.begin(), support.clusters.end(), c) != support.clusters.end())
            && (it->second.flags & support.flags & wanted) != 0;
    };

    if (followed(0xff)) {
        cluster = 0xff;
    }
    else if ((m_announcementCluster >= 0) && followed(m_announcementCluster)) {
        cluster = m_announcementCluster;
    }
    else
    {
        for (int c : support.clusters)
        {
            if (followed(c))
            {
                cluster = c;
                break;
            }
        }
    }

    m_announcementCluster = cluster;
    if (cluster < 0)
    {
        m_status.announcementType = -1;
        m_status.announcementCluster = -1;
        m_status.announcementSubchannel = -1;
        m_status.announcementService.clear();
        return -1;
    }
    const DABAnnouncementSwitch& announcement = announcements.at(cluster);
    m_status.announcementType = announcement.type();
    m_status.announcementCluster = cluster;
    m_status.announcementSubchannel = announcement.subchannelId;
    m_status.announcementService = m_ensemble.subchannelServiceLabel(announcement.subchannelId);
    return announcement.subchannelId;
}

// Decode every SPI component of the ensemble (packet mode, user application 0x007)
void DABReceiver::configureSPI()
{
    struct Wanted
    {
        DABSubchannel subchannel;
        int address;
        bool dataGroups;
    };
    std::vector<Wanted> wanted;

    for (const DABService& service : m_ensemble.services())
    {
        for (const auto& component : service.components)
        {
            DABSubchannel subchannel;
            if ((component.transport != DABServiceComponent::PacketData) || !m_ensemble.subchannel(component.subchannelId, subchannel)) {
                continue;
            }
            const bool spi = std::any_of(component.userApplications.begin(), component.userApplications.end(),
                [](const DABUserApplication& app) { return (app.type == SPIApplication) && !app.xpad; });
            const bool known = std::any_of(wanted.begin(), wanted.end(), [&](const Wanted& w) {
                return (w.subchannel.id == subchannel.id) && (w.address == component.packetAddress);
            });
            if (spi && !known && (wanted.size() < MaximumSPIComponents)) {
                wanted.push_back({subchannel, component.packetAddress, component.dataGroups});
            }
        }
    }

    bool same = wanted.size() == m_spiComponents.size();
    for (std::size_t i = 0; same && (i < wanted.size()); i++)
    {
        const SPIComponent& c = *m_spiComponents[i];
        same = (c.subchannel.subchannel() == wanted[i].subchannel) && (c.address == wanted[i].address)
            && (c.dataGroups == wanted[i].dataGroups);
    }
    if (same) {
        return;
    }

    // Decoded information is kept by component, so a component signalled again keeps its own
    std::vector<std::unique_ptr<SPIComponent>> components;
    for (const Wanted& w : wanted)
    {
        std::unique_ptr<SPIComponent> c;
        for (auto& old : m_spiComponents)
        {
            if (old && (old->subchannel.subchannel() == w.subchannel) && (old->address == w.address))
            {
                c = std::move(old);
                break;
            }
        }
        if (!c)
        {
            c.reset(new SPIComponent());
            c->subchannel.configure(w.subchannel);
        }
        c->address = w.address;
        c->dataGroups = w.dataGroups;
        components.push_back(std::move(c));
    }
    m_spiComponents = std::move(components);
    m_status.spiComponents = static_cast<int>(m_spiComponents.size());
}

void DABReceiver::spiLogicalFrame(SPIComponent& component, const std::vector<uint8_t>& frame, bool lost)
{
    if (lost)
    {
        component.packets.frameLost();
        return;
    }

    std::vector<std::vector<uint8_t>> dataGroups;
    component.packets.push(frame.data(), static_cast<int>(frame.size()), component.address, component.dataGroups, dataGroups);
    if (!component.dataGroups) {
        return;
    }

    bool changed = false;
    for (const auto& dataGroup : dataGroups) {
        changed = component.decoder.push(dataGroup) || changed;
    }
    if (!changed) {
        return;
    }

    rebuildSPI();
}

// The information of every component's files and the cached ones together: a file received
// replaces a cached one of the same name
void DABReceiver::rebuildSPI()
{
    std::map<std::string, modemdr::MOTFile> files = m_spiCache;
    for (const auto& c : m_spiComponents)
    {
        for (const auto& file : c->decoder.files()) {
            files[file.first] = file.second;
        }
    }
    m_spi = modemdr::SPIDecoder::decode(files);
    m_spiUpdated = true;
    m_status.spiServices = static_cast<int>(m_spi->services.size());
    int programmes = 0;
    for (const auto& schedule : m_spi->schedules) {
        programmes += static_cast<int>(schedule.second.size());
    }
    m_status.spiProgrammes = programmes;
    m_status.spiImages = static_cast<int>(m_spi->images.size());
}

std::vector<DABServiceLink> DABReceiver::serviceLinks() const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_sid ? m_ensemble.serviceLinks(m_sid) : std::vector<DABServiceLink>();
}

void DABReceiver::setSPICache(const std::vector<modemdr::MOTFile>& files)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_spiCache.clear();
    for (const modemdr::MOTFile& file : files)
    {
        if (!file.name.empty() && file.data) {
            m_spiCache[file.name] = file;
        }
    }
    rebuildSPI();
}

std::shared_ptr<const modemdr::SPIData> DABReceiver::spi() const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_spi;
}

bool DABReceiver::spiUpdated()
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    bool updated = m_spiUpdated;
    m_spiUpdated = false;
    return updated;
}

void DABReceiver::website(std::vector<std::vector<uint8_t>>& dataGroups)
{
    bool changed = false;
    for (const auto& dataGroup : dataGroups) {
        changed = m_websiteDecoder.push(dataGroup) || changed;
    }
    dataGroups.clear();
    if (changed)
    {
        // A snapshot: its files' data is shared
        m_website = std::make_shared<modemdr::MOTWebsite>(m_websiteDecoder.website());
        m_websiteUpdated = true;
    }
}

std::shared_ptr<const modemdr::MOTWebsite> DABReceiver::website() const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_website;
}

bool DABReceiver::websiteUpdated()
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    bool updated = m_websiteUpdated;
    m_websiteUpdated = false;
    return updated;
}

void DABReceiver::journaline(std::vector<std::vector<uint8_t>>& dataGroups)
{
    for (const auto& dataGroup : dataGroups)
    {
        if (m_journaline.push(dataGroup)) {
            m_journalineUpdated = true;
        }
    }
    dataGroups.clear();
}

void DABReceiver::slides(modemdr::MOTDecoder& decoder, std::vector<std::vector<uint8_t>>& dataGroups)
{
    std::vector<modemdr::MOTObject> objects;

    for (const auto& dataGroup : dataGroups) {
        decoder.push(dataGroup, objects);
    }
    dataGroups.clear();

    for (modemdr::MOTObject& object : objects)
    {
        // SlideShow images (JPEG or PNG)
        if ((object.contentType != 2) || object.body.empty()) {
            continue;
        }
        m_status.slides++;
        m_lastSlide = std::move(object);
        m_haveSlide = true;
        emitSlide(m_lastSlide);
    }
}

} // namespace modemdab
