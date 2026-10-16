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

#ifndef MODEMDRM_DRMRECEIVER_H_
#define MODEMDRM_DRMRECEIVER_H_

#include <complex>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "export.h"
#include "drmaudiodecoder.h"
#include "drmdata.h"
#include "drmjournaline.h"
#include "drmspi.h"
#include "drmfacacquisition.h"
#include "drmsdc.h"

namespace modemdrm
{

class DRMTextMessageDecoder;
class MSCCellDeinterleaver;
class ModeEAudioSuperframeAssembler;
class XHEAACSuperframeParser;

struct MODEMDRM_API ReceiverMSCReport
{
    bool mscReady = false;
    int processedSuperframes = 0;
    int decodedMultiplexFrames = 0;
    int invalidMultiplexFrames = 0;
    int parsedAudioSuperframes = 0;
    int invalidAudioSuperframes = 0;
    int decodedPCMFrames = 0;
    int concealedPCMFrames = 0;
    bool mscMERAvailable = false;
    double mscMERDB = 0.0;
    std::size_t mscMERCellCount = 0;
    std::int64_t selectedServiceIdentifier = -1;
    int selectedShortId = -1;
    int selectedStreamId = -1;
    bool audioCodecAvailable = false;
    // Non-fatal service/codec failure; RF and multiplex processing continue.
    std::string audioError;
    bool textMessageUpdated = false;
    bool clearTextMessage = false;
    std::string textMessage;
    int invalidTextSegments = 0;
    // The selected service's MOT slideshow component (SDC entity 5), when it
    // has one and a slideshow decoder was given: the objects completed in
    // this call and the packet counts.
    bool slideshowPresent = false;
    std::vector<MOTObject> slideshowObjects;
    int slideshowValidPackets = 0;
    int slideshowInvalidPackets = 0;
    // The selected service's MOT Broadcast Website component, when it has one
    // and a website decoder was given: whether the website changed in this
    // call, and the packet counts.
    bool websitePresent = false;
    bool websiteUpdated = false;
    int websiteValidPackets = 0;
    int websiteInvalidPackets = 0;
    // The selected service is a data service (no audio is decoded).
    bool dataService = false;
    // The slideshow and website decoders were moved from another service's
    // data to the selected one's, before decoding it; and the text decoder.
    bool serviceDataReset = false;
    bool textMessageReset = false;
    // A Journaline object of the multiplex changed (journalineDecoder given).
    bool journalineUpdated = false;
    // The multiplex's programme guide changed (spiDecoder given).
    bool spiUpdated = false;
    double sampleClockErrorPPM = 0.0;
    std::size_t consumedSamples = 0;
};

// Caller-owned sample-clock estimate carried between decodeMSC calls. Each
// call measures the pilot phase slope between consecutive frames; keeping the
// estimate lets a one-superframe call (three A-D frames) steer its FFT windows
// from its first frame. Reset it with the RF epoch.
struct MODEMDRM_API SampleClockState
{
    bool haveDrift = false;
    int observations = 0;
    double driftPerFrame = 0.0; // samples per transmission frame
};

// Caller-owned per-carrier noise carried between decodeMSC calls: the latest
// measurement of each MSC carrier (a carrier deep in a fade has too few
// usable cells to measure in one superframe and keeps its last value for up
// to ten superframes, erased ones included). It resets itself when the mode,
// occupancy or MSC constellation changes; reset it with the RF epoch.
struct MODEMDRM_API CarrierNoiseHistory
{
    int mode = -1;
    int occupancy = -1;
    int mscMode = -1;
    std::vector<double> noise; // per carrier from the lowest; negative until measured
    std::vector<int> age; // superframes since each carrier was last measured
    void reset() { mode = occupancy = mscMode = -1; noise.clear(); age.clear(); }
};

class MODEMDRM_API DRMReceiver
{
public:
    using PCMCallback = std::function<void(const PCMFrame&)>;

    // decodeMSC demodulates every requested frame before decoding, so one
    // call is bounded (about 150 s of modes A-D or 50 s of mode E). Longer
    // offline spans are decoded in successive calls with persistent state.
    static constexpr int MaximumSuperframesPerCall = 128;

    // Returns the number of complete transmission superframes beginning at
    // the first FAC-identified frame that fit in the supplied sample span.
    static int maximumCompleteSuperframes(
        std::size_t sampleCount,
        int sampleRate,
        const FACAcquisitionResult& fac
    );

    // Tries successive transmission superframes beginning at the first frame
    // identified by FAC. CRC-valid blocks are accumulated across the AFS-index
    // cycle so entities distributed between SDC blocks form one current
    // configuration. At least one CRC-valid block is required for success.
    static bool decodeSDC(
        const std::complex<float>* samples,
        std::size_t sampleCount,
        int sampleRate,
        const FACAcquisitionResult& fac,
        SDCData& sdc,
        int maximumAttempts = 4,
        std::string* error = nullptr,
        std::vector<std::uint8_t>* decodedBits = nullptr
    );

    // Decodes a bounded number of transmission superframes. A count of zero
    // consumes every complete superframe in the supplied span, which must not
    // exceed MaximumSuperframesPerCall. Success means
    // processing completed, including interleaver warmup; report.mscReady
    // indicates whether any deinterleaved multiplex frame became available.
    // The 24-bit
    // selectedServiceIdentifier is the stable user-facing service selector;
    // -1 selects the first supported audio service. The FAC short identifier
    // and SDC stream identifier remain internal mappings.
    static bool decodeMSC(
        const std::complex<float>* samples,
        std::size_t sampleCount,
        int sampleRate,
        const FACAcquisitionResult& fac,
        const SDCData& sdc,
        ReceiverMSCReport& report,
        std::int64_t selectedServiceIdentifier = -1,
        int superframeCount = 3,
        const PCMCallback& pcmCallback = PCMCallback(),
        std::string* error = nullptr,
        DRMTextMessageDecoder* textMessageDecoder = nullptr,
        DRMAudioDecoder* audioDecoder = nullptr,
        MSCCellDeinterleaver* cellDeinterleaver = nullptr,
        ModeEAudioSuperframeAssembler* modeEAudioAssembler = nullptr,
        std::vector<std::vector<std::uint8_t>>* decodedFrames = nullptr,
        XHEAACSuperframeParser* xheAudioParser = nullptr,
        // Advance an already configured transmission epoch with zero-reliability
        // cells for a known missing superframe. No RF/control decoding is done.
        bool erasedTransmission = false,
        // Caller-owned epoch cache, updated only from fresh FAC observations.
        // Required for continuous selection across rotating service announcements.
        const FACServiceCache* serviceCache = nullptr,
        SampleClockState* sampleClock = nullptr,
        CarrierNoiseHistory* carrierNoise = nullptr,
        // Caller-owned decoder for the selected service's MOT slideshow,
        // carried between calls; reset it when the service changes.
        DRMSlideshowDecoder* slideshowDecoder = nullptr,
        // Caller-owned decoder for the multiplex's Journaline services,
        // carried between calls; reset it with the RF epoch.
        DRMJournalineDecoder* journalineDecoder = nullptr,
        // Caller-owned decoder for the selected service's MOT Broadcast
        // Website, carried between calls; reset it when the service changes.
        DRMWebsiteDecoder* websiteDecoder = nullptr,
        // Caller-owned decoder for the multiplex's programme guide (SPI),
        // carried between calls; reset it with the RF epoch.
        DRMSPIDecoder* spiDecoder = nullptr
    );
};

} // namespace modemdrm

#endif // MODEMDRM_DRMRECEIVER_H_
