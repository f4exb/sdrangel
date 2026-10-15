///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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

#ifndef INCLUDE_DRMDEMODSINK_H
#define INCLUDE_DRMDEMODSINK_H

#include <atomic>
#include <complex>
#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <QByteArray>
#include <QMutex>
#include <QString>

#include "audio/audiofifo.h"
#include "dsp/channelsamplesink.h"
#include "dsp/interpolator.h"
#include "dsp/nco.h"
#include "modemdrm/drmaudiodecoder.h"
#include "modemdrm/drmaudioframing.h"
#include "modemdrm/drmfacacquisition.h"
#include "modemdrm/drmmsc.h"
#include "modemdrm/drmreceiver.h"
#include "modemdrm/drmresampler.h"
#include "modemdrm/drmsdc.h"
#include "modemdrm/drmsyncstatemachine.h"

#include "drmdemodsettings.h"

class ChannelAPI;

struct DRMAFSStatus
{
    int type = -1;
    bool listVersion = false;
    int extension = -1;
    bool synchronousMultiplex = false;
    bool baseLayer = false;
    bool appliesToAllServices = false;
    bool regionOrScheduleRestricted = false;
    int serviceRestrictionField = -1;
    int regionScheduleField = -1;
    std::vector<std::uint16_t> frequencyCodes;
};

struct DRMServiceStatus
{
    std::uint32_t serviceIdentifier = 0;
    int shortIdentifier = -1;
    bool audioService = false;
    bool conditionalAccess = false;
    int facLanguageCode = -1;
    int facServiceDescriptor = -1;
    QString label;
    QString language;
    QString country;
    int streamId = -1;
    int audioCoding = -1;
    int audioMode = -1;
    int samplingRateCode = -1;
    bool sbr = false;
    bool textFlag = false;
};

struct DRMDemodStatus
{
    QString audioDecoderBackend;
    QStringList capabilities;
    QString syncState = "stopped";
    quint64 epoch = 0;
    bool facValid = false;
    bool sdcValid = false;
    bool mscReady = false;
    bool audioActive = false;
    qint64 facAgeMs = -1;
    qint64 sdcAgeMs = -1;
    qint64 audioAgeMs = -1;
    qint64 broadcastTimeAgeMs = -1;
    quint64 validFACBlocks = 0;
    quint64 invalidFACBlocks = 0;
    quint64 validSDCBlocks = 0;
    quint64 invalidSDCBlocks = 0;
    quint64 skippedSDCBlocks = 0;
    char mode = '?';
    int spectrumInverted = -1;        // detected orientation: 1 mirrored (I/Q swapped), -1 until locked
    int occupancy = -1;
    int effectiveSearchRangeHz = 0;
    int internalSampleRate = 0;       // modem rate after resampling
    double inputFilterLowHz = 0.0;    // fR-relative band-filter passband
    double inputFilterHighHz = 0.0;
    double carrierOffsetHz = 0.0;
    bool carrierOffsetAvailable = false;   // false until a FAC lock measures it
    double sampleClockErrorPPM = 0.0;
    bool sampleClockErrorAvailable = false; // false until enough coherent frames
    bool mscMERAvailable = false;
    double mscMERDB = 0.0;
    quint64 mscMERCellCount = 0;
    qint64 selectedServiceIdentifier = -1;
    quint64 captureWindows = 0;
    quint64 audioDroppedSamples = 0;
    quint64 inputDroppedSamples = 0; // Lifetime RF samples rejected by the input FIFO.
    int processedSuperframes = 0;
    int retainedSamples = 0;
    int decodedMultiplexFrames = 0;
    int invalidMultiplexFrames = 0;
    int parsedAudioSuperframes = 0;
    int invalidAudioSuperframes = 0;
    int decodedPCMFrames = 0;
    int concealedPCMFrames = 0;
    int decoderAudioSampleRate = 0;
    int outputAudioSampleRate = 0;
    int audioChannels = 0;
    std::vector<std::uint32_t> serviceIdentifiers;
    std::vector<DRMServiceStatus> services;
    bool textMessagePresent = false;
    QString textMessage;
    int invalidTextSegments = 0;
    // The selected service's MOT slideshow (SDC entity 5): whether it has
    // one, the latest image, how many images have arrived and the packets.
    bool slideshowPresent = false;
    QByteArray slideshowImage;
    QString slideshowName;
    QString slideshowMimeType;
    quint64 slideshowImages = 0;
    quint64 slideshowUpdates = 0; // counts images and clears, never reset
    qint64 slideshowAgeMs = -1;
    quint64 slideshowValidPackets = 0;
    quint64 slideshowInvalidPackets = 0;
    // The selected service's MOT Broadcast Website: whether it has one, its
    // files so far (a shared snapshot, replaced when a file arrives or the
    // directory changes; websiteUpdates counts the changes) and the packets.
    bool websitePresent = false;
    std::shared_ptr<const modemdrm::MOTWebsite> website;
    quint64 websiteUpdates = 0;
    quint64 websiteValidPackets = 0;
    quint64 websiteInvalidPackets = 0;
    // The selected service is a data service, without audio.
    bool dataService = false;
    // The multiplex's Journaline services by short Id, each its objects by
    // object Id (0 is the root menu); services sharing a component share the
    // objects. Replaced only when an object changes; journalineUpdates counts
    // the changes.
    using JournalineObjects = std::map<int, modemdrm::JournalineObject>;
    std::map<int, std::shared_ptr<const JournalineObjects>> journaline;
    quint64 journalineUpdates = 0;
    // The multiplex's programme guide (SPI): a shared snapshot, replaced when
    // it changes; spiUpdates counts the changes.
    std::shared_ptr<const modemdr::SPIData> spi;
    quint64 spiUpdates = 0;
    QString broadcastTimeUTC;
    bool localTimeOffsetPresent = false;
    int localTimeOffsetMinutes = 0;
    int afsIndex = -1;
    std::vector<DRMAFSStatus> alternativeFrequencies;
    QString error;
};

class DRMDemodSink : public ChannelSampleSink
{
public:
    DRMDemodSink();

    void feed(
        const SampleVector::const_iterator& begin,
        const SampleVector::const_iterator& end
    ) override;

    void start();
    // Offline EOF drain: process complete tracked superframes without padding
    // input or discarding the partial tail. Does not stop/reset RF or audio.
    void flush();
    // Ask decoding in progress on the worker thread to return early, so that
    // stop() need not wait for an acquisition search; start() clears it.
    void requestAbort() { m_abort = true; }
    void stop();
    // keepQueuedAudio: a seek straight after an EOF drain (File Input's loop)
    // keeps the drained audio queued for playback.
    void reset(bool retune, bool keepQueuedAudio = false);
    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void applySettings(
        const DRMDemodSettings& settings,
        const QStringList& settingsKeys,
        bool force = false
    );
    void applyAudioSampleRate(int sampleRate);
    int getAudioSampleRate() const;
    AudioFifo *getAudioFifo() { return &m_audioFifo; }
    void setAudioFifoLabel(const QString& label) { m_audioFifo.setLabel(label); }
    // Callable from any thread without waiting for a decode step: returns the
    // live status when the worker is idle, else the snapshot published at the
    // end of the last feed.
    DRMDemodStatus getStatus() const;
    void getMagSqLevels(double& average, double& peak, int& samples);
    // The DRM band's power, averaged over about half a second (not consumed);
    // false until measured since the last start, stop, reset or retune.
    bool getChannelPowerDB(double& db) const;
    // The channel whose "demod" data pipes (Demod Analyzer, MCP audio capture)
    // receive the output audio.
    void setChannel(ChannelAPI *channel) { m_channel = channel; }
    // Internal rate the modem runs at: 192 kHz when the channel carries it,
    // else 48 kHz (modes A-D only). DRM symbols are integral at both.
    int getInternalSampleRate() const;

    static int internalSampleRateFor(int channelSampleRate);

private:
    friend class DRMDemodSinkTest;
    friend class DRMDemodBasebandTest;

    static constexpr int CaptureWindowSeconds = 6;
    static constexpr int AudioBufferSeconds = CaptureWindowSeconds + 2;
    static constexpr int WideInternalSampleRate = 192000;
    static constexpr int NarrowInternalSampleRate = 48000;
    // Modes A-D decode 1.2 s of audio per superframe step (mode E 0.4 s),
    // and the next step needs another 1.6 s of input after acquisition (a
    // frame of context): a margin keeps playout from running dry between steps.
    static constexpr int DefaultAudioPreRollMilliseconds = 800;
    // Queued audio kept beyond the pre-roll: one step and a margin.
    static constexpr int MaximumQueuedAudioBeyondPreRollMilliseconds = 1600;
    static constexpr int MaximumQueuedAudioBeyondPreRollModeEMilliseconds = 800;

    struct PublishedStatus
    {
        DRMDemodStatus status;
        bool locked = false;
        std::chrono::steady_clock::time_point lastFAC{};
        std::chrono::steady_clock::time_point lastSDC{};
        std::chrono::steady_clock::time_point lastDecodedPCM{};
        std::chrono::steady_clock::time_point lastBroadcastTime{};
        std::chrono::steady_clock::time_point lastSlide{};
    };

    // m_mutex serializes the worker's decoder state. getStatus() and
    // getMagSqLevels() never wait on it: they use m_statusMutex/m_magMutex.
    mutable QMutex m_mutex;
    mutable QMutex m_statusMutex;
    mutable QMutex m_magMutex;
    PublishedStatus m_published;
    DRMDemodSettings m_settings;
    modemdrm::SyncStateMachine m_syncState;
    DRMDemodStatus m_status;
    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    int m_internalSampleRate;
    // fR-relative band the channelizer and resampler preserve.
    double m_inputBandLowHz = 0.0;
    double m_inputBandHighHz = 0.0;
    double m_magSqSum = 0.0;
    double m_magSqPeak = 0.0;
    int m_magSqSamples = 0;
    double m_channelPower = -1.0; // under m_magMutex; negative until measured
    NCO m_nco;
    modemdrm::FractionalResampler m_resampler;
    modemdrm::BandFilter m_bandFilter;
    std::vector<std::complex<float>> m_resampled;
    std::vector<std::complex<float>> m_filtered;
    std::uint64_t m_receivedSamples = 0; // channel-rate samples since the last reset
    AudioFifo m_audioFifo;
    ChannelAPI *m_channel = nullptr;
    std::vector<qint16> m_demodSamples;
    Interpolator m_audioInterpolator;
    int m_audioSampleRate;
    std::unique_ptr<modemdrm::DRMAudioDecoder> m_audioDecoder;
    int m_decoderAudioSampleRate = 0;
    int m_decoderAudioChannels = 0;
    Real m_audioInterpolatorDistance = 1.0f;
    Real m_audioInterpolatorDistanceRemain = 0.0f;
    std::uint64_t m_audioSamplesWritten = 0;
    // Silence queued ahead of the first audio after each audio start in modes
    // A-D, and when the output has run dry (0 disables it, as the FIFO-exact
    // tests do).
    int m_audioPreRollMilliseconds = DefaultAudioPreRollMilliseconds;
    std::uint64_t m_audioTrimmedSamples = 0; // dropped by trimQueuedAudio, for tests
    bool m_audioPreRollPending = true; // set at each audio start (service, epoch, output rate)
    modemdrm::SDCData m_currentSDC;
    modemdrm::FACServiceCache m_facServiceCache;
    bool m_currentSDCValid = false;
    int m_lastReconfigurationIndex = -1;
    int m_lastServiceCountCode = -1;
    modemdrm::FACAcquisitionResult m_trackedFAC;
    std::size_t m_trackingPrefixSamples = 0;
    // A successful cold search whose first superframe has not yet been fully
    // captured: resumed, not searched again, once enough samples arrive.
    modemdrm::FACAcquisitionResult m_pendingFAC;
    std::size_t m_pendingCaptureSamples = 0;
    // Minimum capture before retrying a step that consumed nothing.
    std::size_t m_captureRetrySamples = 0;
    // Mode, occupancy and offset of the lock just lost, tried first on reacquisition.
    modemdrm::FACSearchOptions m_reacquireHint;
    std::atomic_bool m_abort{false};
    modemdrm::SampleClockState m_sampleClock;
    modemdrm::CarrierNoiseHistory m_carrierNoise;
    std::unique_ptr<modemdrm::MSCCellDeinterleaver> m_mscCellDeinterleaver;
    std::string m_mscConfigurationKey;
    // The decoded service's audio configuration, and the short id it was for.
    std::string m_audioConfigurationKey;
    int m_audioKeyShortId = -1;
    // Superframe steps taken, and the step whose MSC last entered the
    // deinterleaver: its frames may only be followed by the next step's.
    std::uint64_t m_superframeSequence = 0;
    std::uint64_t m_mscSuperframeSequence = 0;
    modemdrm::ModeEAudioSuperframeAssembler m_modeEAudioAssembler;
    modemdrm::XHEAACSuperframeParser m_xheAudioParser;
    modemdrm::DRMTextMessageDecoder m_textMessageDecoder;
    modemdrm::DRMSlideshowDecoder m_slideshowDecoder;
    // The service identifiers of the station whose data is kept, and after a
    // lost lock those found since, until it is known to be the same station
    std::set<std::uint32_t> m_stationServices;
    std::set<std::uint32_t> m_relockServices;
    bool m_stationCheck = false;
    modemdrm::DRMWebsiteDecoder m_websiteDecoder;
    modemdrm::DRMJournalineDecoder m_journalineDecoder;
    modemdrm::DRMSPIDecoder m_spiDecoder;
    // The published Journaline objects of each component, by stream and packet Id
    struct JournalineSnapshot
    {
        std::uint64_t revision = 0;
        std::shared_ptr<const DRMDemodStatus::JournalineObjects> objects;
    };
    std::map<std::pair<int, int>, JournalineSnapshot> m_journalineSnapshots;
    std::vector<std::complex<float>> m_capture;
    int m_coldCaptureSeconds = 2;
    using Clock = std::chrono::steady_clock;
    Clock::time_point m_lastFAC{};
    Clock::time_point m_lastSDC{};
    Clock::time_point m_lastDecodedPCM{};
    Clock::time_point m_lastSlide{};
    Clock::time_point m_lastBroadcastTime{};

    std::size_t processCapture();
    void drainCapture();
    std::size_t captureTargetSamples() const;
    std::size_t retainCapture(const modemdrm::FACAcquisitionResult& fac, std::size_t end);
    void processPCMFrame(const modemdrm::PCMFrame& pcm);
    void accumulateMSCReport(const modemdrm::ReceiverMSCReport& report, bool processed = true);
    void appendAudioSample(const Complex& sample, AudioVector& output) const;
    void trimQueuedAudio();
    void silenceQueuedAudio();
    void dropQueuedSilence();
    static QString imageMimeType(int contentSubtype);
    void publishJournaline();
    void clearJournaline();
    void clearSPI();
    void clearServiceData();
    void clearServiceDataStatus();
    void observeStation(std::uint32_t serviceIdentifier, int serviceCount);
    void dataFramesLost();
    void clearStationData();
    static void enrichServices(std::vector<DRMServiceStatus>& services, const modemdrm::SDCData& sdc);
    static std::string mscConfigurationKey(const modemdrm::FACAcquisitionResult& fac, const modemdrm::SDCData& sdc);
    // The audio configuration of one service (shortId), or of all of them (-1).
    static std::string audioConfigurationKey(const modemdrm::SDCData& sdc, int shortId);
    void clearServiceStatus(bool keepQueuedAudio = false);
    void updateSyncStatus();
    void observeFAC(const modemdrm::FACAcquisitionResult& fac);
    void clearDownstreamStatus(bool resetCaptureWindows = false, bool keepQueuedAudio = false);
    void resetInputChain();
    void updateInputBand();
    bool spectrumUsable(modemdrm::Mode mode, int occupancy, bool spectralInversion) const;
    modemdrm::FACSearchOptions searchOptions() const;
    void rememberLock();
    PublishedStatus snapshot() const;
    static DRMDemodStatus statusFrom(const PublishedStatus& published);
    void publishStatus();
};

#endif // INCLUDE_DRMDEMODSINK_H
