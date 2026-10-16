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

#ifndef MODEMDAB_DABRECEIVER_H_
#define MODEMDAB_DABRECEIVER_H_

#include <atomic>
#include <complex>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "export.h"
#include "dabaudiodecoder.h"
#include "dabaudioframing.h"
#include "dabcoherent.h"
#include "dabensemble.h"
#include "modemdr/journaline.h"
#include "modemdr/mot.h"
#include "dabmsc.h"
#include "dabofdm.h"
#include "dabpad.h"
#include "modemdr/spi.h"
#include "dabtii.h"

namespace modemdab
{

// Counters accumulate from reset; rates are over the receiver's last second (about ten frames).
struct MODEMDAB_API DABReceiverStatus
{
    // RF
    bool locked = false;                // OFDM timing and frequency
    bool ficLocked = false;             // FIBs decoding
    float frequencyOffsetHz = 0.0f;
    float clockErrorPPM = 0.0f;
    float snrDB = 0.0f;
    float merDB = 0.0f;                 // Of the differential products (snrDB is that of the symbols)
    float cnDB = 0.0f;                  // Carrier to noise ratio, from the null symbol
    float nullDepthDB = 0.0f;           // Null symbol power relative to the other symbols
    int64_t frames = 0;
    int64_t framesLost = 0;
    int64_t fibs = 0;
    int64_t fibErrors = 0;
    float fibQuality = 0.0f;            // % of good FIBs, last second
    float ficBER = 0.0f;                // Channel bit error rate estimated from the FIC
    int cifCount = -1;
    int acquisitions = 0;               // Attempts to synchronize
    int locks = 0;                      // Times synchronized (more than once after losing it)

    // Selected service
    uint32_t sid = 0;
    bool serviceFound = false;
    bool dabPlus = false;
    int subchannelId = -1;
    int bitrate = 0;
    std::string protection;
    bool audioDecoder = false;          // The codec is available
    std::string audioBackend;
    bool audioActive = false;           // Audio decoded in the last second
    int sampleRate = 0;
    bool stereo = false;
    bool sbr = false;
    bool ps = false;
    float mscBER = 0.0f;                // Channel bit error rate of the sub-channel
    int64_t superframes = 0;
    int64_t fireCodeErrors = 0;
    int64_t rsCorrectedBytes = 0;
    int64_t rsUncorrectableRows = 0;
    int64_t audioFrames = 0;            // AUs (DAB+) or frames (DAB)
    int64_t audioFrameErrors = 0;
    float audioQuality = 0.0f;          // % of good audio frames, last second
    int64_t pcmFrames = 0;              // Decoded audio frames
    int64_t concealedFrames = 0;
    int64_t slides = 0;
    int journalineObjects = 0;
    bool website = false;               // The service has a Broadcast Website
    int64_t padCRCErrors = 0;

    // An announcement (traffic, news, alarm...) for the selected service
    int announcementType = -1;          // DABAnnouncementType bit, -1 for none
    int announcementCluster = -1;
    int announcementSubchannel = -1;
    bool announcementSwitched = false;  // The audio is the announcement's
    std::string announcementService;    // The service whose sub-channel carries it

    // Service and Programme Information
    int spiComponents = 0;              // SPI data components being decoded
    int spiServices = 0;
    int spiProgrammes = 0;
    int spiImages = 0;
};

class MODEMDAB_API DABReceiver
{
public:
    enum class Detection
    {
        Differential,   // Each symbol against the one before (for comparison)
        ThreeSymbol,    // Over windows of three symbols (the default)
        Coherent        // Three-symbol, then a second pass with the decoded data of the first as pilots
    };

    // Interleaved stereo 16-bit audio
    using AudioCallback = std::function<void(const int16_t *samples, int frames, int sampleRate)>;
    using SlideCallback = std::function<void(const modemdr::MOTObject& slide)>;

    DABReceiver();
    ~DABReceiver();

    void setAudioCallback(AudioCallback callback) { m_audioCallback = callback; }
    void setSlideCallback(SlideCallback callback) { m_slideCallback = callback; }
    void setSearchRange(int rangeHz) { m_frontEnd.setSearchRange(rangeHz); }
    void setBandFilter(bool enabled) { m_frontEnd.setBandFilter(enabled); }
    // Can be changed while running: the audio restarts
    void setDetection(Detection detection);
    // Coherent detection: the number of decoder aided passes (each adds about 0.4 s of delay
    // and decodes every sub-channel once more; default 1), and the reliability below which
    // decoded bits are not trusted as known symbols (see DABCoherentDecoder). Either can be
    // changed while running; the passes then start again.
    void setIterations(int iterations);
    void setReliabilityThreshold(float threshold);
    // Decode (FIC, MSC, audio and data) in a thread of its own, while the caller's thread
    // synchronizes and demodulates. Callbacks are still made in the caller's thread, from
    // process(). Off by default.
    void setThreaded(bool threaded);
    // Wait for the frames queued for the decoding thread, and make their callbacks
    void flush();

    // Forget the ensemble and synchronization (retune)
    void reset();
    // Search for the signal again, keeping the ensemble
    void reacquire();
    // Complex baseband at 2.048 MS/s
    void process(const std::complex<float> *samples, std::size_t count);

    // Select a service by SId (0 for none). Its primary audio component is decoded.
    void selectService(uint32_t sid);
    uint32_t selectedService() const { return m_sid; }

    // The accessors return copies, as decoding may be in another thread
    DABEnsemble ensemble() const;
    DABReceiverStatus status() const;
    // Carrier to noise ratio of each carrier (dB), by carrier index (k = -768..-1, then
    // 1..768), from the last frame
    std::vector<float> carrierCN() const;
    // Each returns true once after a change
    bool labelUpdated();
    DABDynamicLabel dynamicLabel() const;
    std::vector<DABTIITransmitter> tii() const;
    // TII has been looked for long enough that none found means none is transmitted
    bool tiiAnalysed() const;
    bool lastSlide(modemdr::MOTObject& slide) const;
    // Journaline objects of the selected service (from its X-PAD or a packet mode component)
    std::map<int, modemdr::JournalineObject> journaline() const;
    bool journalineUpdated();
    // The MOT Broadcast Website of the selected service (packet mode, user application 0x003):
    // the files received so far
    std::shared_ptr<const modemdr::MOTWebsite> website() const;
    bool websiteUpdated();
    // The announcement types (bits of DABAnnouncementType) the selected service's audio switches
    // to while they last, as the service supports them (FIG 0/18). Alarms are always followed
    // (TS 103 176 clause 7.3). None by default. Can be changed while running.
    void setAnnouncementTypes(uint16_t types);
    // Where the selected service can also be received
    std::vector<DABServiceLink> serviceLinks() const;
    // The ensemble's Service and Programme Information, decoded whatever service is selected
    std::shared_ptr<const modemdr::SPIData> spi() const;
    bool spiUpdated();
    // SPI files kept from before (logos, SI and PI objects of the ensemble, from spi()->images and
    // spi()->objects), used until the carousel sends them again. reset() forgets them.
    void setSPICache(const std::vector<modemdr::MOTFile>& files);

private:
    struct Window
    {
        int good = 0;
        int total = 0;
        void add(bool ok) { total++; if (ok) { good++; } }
    };

    DABFrontEnd m_frontEnd;
    DABFICDecoder m_fic;
    DABEnsemble m_ensemble;
    DABTIIDecoder m_tii;

    uint32_t m_sid;
    uint32_t m_configuredVersion;
    DABService m_service;
    bool m_serviceFound;
    DABSubchannelDecoder m_audioSubchannel;
    bool m_dabPlus;
    DABPlusFramer m_dabPlusFramer;
    DABMP2Framer m_mp2Framer;
    DABPlusParameters m_aacParameters;
    std::unique_ptr<DABAudioDecoder> m_audioDecoder;
    bool m_mp2DecoderTried;
    DABPADDecoder m_pad;
    modemdr::MOTDecoder m_mot;
    // Packet mode data components of the service: SlideShow and Journaline
    struct DataComponent
    {
        int application = 0;    // User application type
        int address = -1;
        bool dataGroups = true;
        DABSubchannelDecoder subchannel;
        DABPacketDecoder packets;
    };
    std::vector<std::unique_ptr<DataComponent>> m_dataComponents;
    modemdr::MOTDecoder m_packetMOT;
    modemdr::JournalineDecoder m_journaline;
    bool m_journalineUpdated;
    modemdr::MOTDirectoryDecoder m_websiteDecoder;
    std::shared_ptr<const modemdr::MOTWebsite> m_website;
    bool m_websiteUpdated;
    // The ensemble's SPI components
    struct SPIComponent
    {
        int address = -1;
        bool dataGroups = true;
        DABSubchannelDecoder subchannel;
        DABPacketDecoder packets;
        modemdr::SPIDecoder decoder;
    };
    static constexpr std::size_t MaximumSPIComponents = 4;
    std::vector<std::unique_ptr<SPIComponent>> m_spiComponents;
    std::map<std::string, modemdr::MOTFile> m_spiCache;
    std::shared_ptr<const modemdr::SPIData> m_spi;
    bool m_spiUpdated;
    // Announcements
    std::atomic<uint16_t> m_announcementTypes;
    int m_announcementCluster;              // Being followed, -1 for none

    int m_cifCount;
    int64_t m_lastFrameStart;
    bool m_haveLastFrame;

    AudioCallback m_audioCallback;
    SlideCallback m_slideCallback;
    modemdr::MOTObject m_lastSlide;
    bool m_haveSlide;
    bool m_labelUpdated;

    // Status
    DABReceiverStatus m_status;
    std::vector<float> m_carrierCN;
    std::vector<Window> m_fibWindow;
    std::vector<Window> m_audioWindow;
    std::vector<std::pair<int, int>> m_ficBERWindow;
    std::vector<std::pair<int, int>> m_mscBERWindow;
    std::vector<int> m_pcmWindow;
    int m_windowIndex;
    bool m_ficLocked;
    int m_ficBadFrames;

    void onFrame(const DABFrame& frame);
    void lostFrames(int count);
    void processCIF(const float *cif);
    void audioCIF(const float *cif);
    void configureService();
    void clearService();
    int announcementSubchannel();
    void configureSPI();
    void spiLogicalFrame(SPIComponent& component, const std::vector<uint8_t>& frame, bool lost);
    void rebuildSPI();
    void audioLogicalFrame(const std::vector<uint8_t>& frame, bool lost);
    void decodeAudio(const uint8_t *data, int size, bool valid);
    void outputPCM(const DABPCM& pcm);
    void dataLogicalFrame(DataComponent& component, const std::vector<uint8_t>& frame, bool lost);
    void journaline(std::vector<std::vector<uint8_t>>& dataGroups);
    void website(std::vector<std::vector<uint8_t>>& dataGroups);
    void slides(modemdr::MOTDecoder& decoder, std::vector<std::vector<uint8_t>>& dataGroups);
    void nextWindow();

    std::atomic<Detection> m_detection;     // As set
    Detection m_appliedDetection;           // As the decoder uses it
    std::vector<std::unique_ptr<DABCoherentDecoder>> m_coherent;   // One per iteration
    std::atomic<int> m_coherentIterations;
    std::atomic<float> m_coherentThreshold;
    float m_appliedThreshold;

    // Decoding thread
    static constexpr std::size_t MaximumQueuedFrames = 20;     // About 2 s
    bool m_threaded;
    std::thread m_worker;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCondition;
    std::deque<std::unique_ptr<DABFrame>> m_frames;
    std::vector<std::unique_ptr<DABFrame>> m_framePool;
    bool m_stop;
    mutable std::mutex m_stateMutex;        // Everything the decoding uses
    std::mutex m_outputMutex;
    std::deque<DABPCM> m_pcmOutput;         // Callbacks waiting for the caller's thread
    std::deque<modemdr::MOTObject> m_slideOutput;
    std::atomic<bool> m_reacquireRequested;
    bool m_busy;                            // Decoding a frame (m_queueMutex)

    void frameReady(const DABFrame& frame);
    void work();
    void startWorker();
    void stopWorker();
    void deliver();
    void emitPCM(const DABPCM& pcm);
    void emitSlide(const modemdr::MOTObject& slide);
};

} // namespace modemdab

#endif // MODEMDAB_DABRECEIVER_H_
