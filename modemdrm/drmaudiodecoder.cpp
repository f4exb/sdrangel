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

#include "drmaudiodecoder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

#if defined(MODEMDRM_HAS_FDK_AAC)
#if __has_include(<fdk-aac/aacdecoder_lib.h>)
#include <fdk-aac/aacdecoder_lib.h>
#else
#include <aacdecoder_lib.h>
#endif
#endif

#if defined(MODEMDRM_HAS_FAAD2)
#include <neaacdec.h>
#endif

namespace modemdrm
{

namespace
{

class UnavailableAudioDecoder final : public DRMAudioDecoder
{
public:
    AudioDecoderCapabilities capabilities() const override
    {
        return {"unavailable", false, false, false};
    }

    bool configure(const SDCAudioInformation&, std::string* error) override
    {
        if (error) {
            *error = "No DRM audio codec backend is available";
        }

        return false;
    }

    bool decode(
        const std::vector<std::uint8_t>&,
        PCMFrame& output,
        std::string* error
    ) override
    {
        output = PCMFrame();
        if (error) {
            *error = "No DRM audio codec backend is available";
        }

        return false;
    }

    bool conceal(PCMFrame& output, std::string* error) override
    {
        output = PCMFrame();
        if (error) {
            *error = "No DRM audio codec backend is available";
        }

        return false;
    }

    void reset() override {}
};

#if defined(MODEMDRM_HAS_FDK_AAC)

class FDKAACAudioDecoder final : public DRMAudioDecoder
{
public:
    FDKAACAudioDecoder()
    {
        open();
    }

    ~FDKAACAudioDecoder() override
    {
        if (m_handle) {
            aacDecoder_Close(m_handle);
        }
    }

    AudioDecoderCapabilities capabilities() const override
    {
        std::array<LIB_INFO, FDK_MODULE_LAST> information {};
        FDKinitLibInfo(information.data());
        const bool queried = aacDecoder_GetLibInfo(information.data()) == 0;
        const UINT flags = queried
            ? FDKlibInfo_getCapabilities(information.data(), FDK_AACDEC)
            : 0U;
        const bool drm = (flags & CAPF_AAC_DRM_BSFORMAT) != 0U;
        const bool aac = m_handle != nullptr && drm
            && (flags & CAPF_ER_AAC_SCAL) != 0U;
        const bool xheAAC = m_handle != nullptr && drm
            && (flags & CAPF_AAC_USAC) != 0U;
        return {"fdk-aac", aac || xheAAC, aac, xheAAC};
    }

    bool configure(const SDCAudioInformation& audio, std::string* error) override
    {
        const auto fail = [this, error](const char *message) {
            m_configured = false;
            if (error) {
                *error = message;
            }

            return false;
        };
        if (!m_handle) {
            return fail("FDK-AAC DRM decoder could not be opened");
        }

        const AudioDecoderCapabilities supported = capabilities();
        if ((audio.audioCoding == 0 && !supported.aac)
            || (audio.audioCoding == 3 && !supported.xheAAC)
            || (audio.audioCoding != 0 && audio.audioCoding != 3)) {
            return fail("FDK-AAC backend does not support this DRM audio coding");
        }

        if (audio.audioCoding == 3 && audio.codecSpecificConfig.empty()) {
            return fail("DRM xHE-AAC codec-specific configuration is missing");
        }

        // Clause 6.4.3.10: a decoder without MPEG Surround support ignores the
        // MPEG Surround mode and decodes the mono/stereo core. Clearing the
        // mode keeps FDK on the core AAC object type instead of the surround
        // one; output is also capped at two channels (see open()).
        const int coderField = audio.audioCoding == 0
            ? (audio.coderField & 0x3) : audio.coderField;
        std::vector<std::uint8_t> configuration = {
            static_cast<std::uint8_t>(
                ((audio.audioCoding & 0x3) << 6)
                | ((audio.sbr ? 1 : 0) << 5)
                | ((audio.audioMode & 0x3) << 3)
                | (audio.samplingRateCode & 0x7)
            ),
            static_cast<std::uint8_t>(
                ((audio.text ? 1 : 0) << 7)
                | ((audio.enhancement ? 1 : 0) << 6)
                | ((coderField & 0x1f) << 1)
            )
        };
        configuration.insert(
            configuration.end(),
            audio.codecSpecificConfig.begin(),
            audio.codecSpecificConfig.end()
        );
        if (m_configured && configuration == m_configuration)
        {
            if (error) {
                error->clear();
            }

            return true;
        }

        m_configuration = std::move(configuration);
        UCHAR* configurationBytes = m_configuration.data();
        UINT configurationSize = static_cast<UINT>(m_configuration.size());
        m_configured = false;
        const AAC_DECODER_ERROR status = aacDecoder_ConfigRaw(
            m_handle, &configurationBytes, &configurationSize
        );
        if (status != AAC_DEC_OK) {
            return fail("FDK-AAC rejected the DRM audio configuration");
        }

        m_configured = true;
        if (error) {
            error->clear();
        }

        return true;
    }

    bool decode(
        const std::vector<std::uint8_t>& codecFrame,
        PCMFrame& output,
        std::string* error
    ) override
    {
        output = PCMFrame();
        if (!m_configured || !m_handle)
        {
            if (error) {
                *error = "DRM AAC decoder is not configured";
            }

            return false;
        }

        if (codecFrame.empty()
            || codecFrame.size() > std::numeric_limits<UINT>::max())
        {
            if (error) {
                *error = "Invalid DRM AAC codec frame";
            }

            return false;
        }

        UCHAR* input = const_cast<UCHAR*>(codecFrame.data());
        UINT inputSize = static_cast<UINT>(codecFrame.size());
        UINT bytesValid = inputSize;
        AAC_DECODER_ERROR status = aacDecoder_Fill(
            m_handle, &input, &inputSize, &bytesValid
        );
        if (status != AAC_DEC_OK || bytesValid != 0U)
        {
            if (error) {
                *error = "FDK-AAC did not accept the complete DRM frame";
            }

            return false;
        }

        return decodeBufferedFrame(0U, output, error);
    }

    bool conceal(PCMFrame& output, std::string* error) override
    {
        output = PCMFrame();
        if (!m_configured || !m_handle)
        {
            if (error) {
                *error = "DRM AAC decoder is not configured";
            }

            return false;
        }

        const bool decoded = decodeBufferedFrame(AACDEC_CONCEAL, output, error);
        output.concealed = decoded;
        return decoded;
    }

    void reset() override
    {
        const std::vector<std::uint8_t> configuration = m_configuration;
        open();
        m_configured = false;
        if (!m_handle || configuration.empty()) {
            return;
        }

        m_configuration = configuration;
        UCHAR* bytes = m_configuration.data();
        UINT size = static_cast<UINT>(m_configuration.size());
        m_configured = aacDecoder_ConfigRaw(m_handle, &bytes, &size) == AAC_DEC_OK;
    }

private:
    void open()
    {
        if (m_handle) {
            aacDecoder_Close(m_handle);
        }

        m_handle = aacDecoder_Open(TT_DRM, 2U);
        // SDRangel's audio path is mono/stereo: have FDK downmix any
        // multichannel (MPEG Surround) output rather than rejecting it.
        if (m_handle) {
            aacDecoder_SetParam(m_handle, AAC_PCM_MAX_OUTPUT_CHANNELS, 2);
        }
    }

    bool decodeBufferedFrame(
        UINT flags,
        PCMFrame& output,
        std::string* error
    )
    {
        std::vector<INT_PCM>& pcm = m_pcm;
        const AAC_DECODER_ERROR status = aacDecoder_DecodeFrame(
            m_handle, pcm.data(), static_cast<INT>(pcm.size()), flags
        );
        if (!IS_OUTPUT_VALID(status))
        {
            if (error) {
                *error = "FDK-AAC failed to decode the DRM audio frame";
            }

            return false;
        }

        const CStreamInfo* stream = aacDecoder_GetStreamInfo(m_handle);
        if (!stream || stream->sampleRate <= 0 || stream->frameSize <= 0
            || stream->numChannels < 1 || stream->numChannels > 2)
        {
            if (error) {
                *error = "FDK-AAC returned invalid DRM PCM parameters";
            }

            return false;
        }

        const std::size_t sampleCount = static_cast<std::size_t>(stream->frameSize)
            * static_cast<std::size_t>(stream->numChannels);
        if (sampleCount > pcm.size())
        {
            if (error) {
                *error = "FDK-AAC DRM PCM frame exceeds the output buffer";
            }

            return false;
        }

        output.sampleRate = stream->sampleRate;
        output.channels = stream->numChannels;
        output.concealed = status != AAC_DEC_OK || (flags & AACDEC_CONCEAL) != 0U;
        output.samples.reserve(sampleCount);
        for (std::size_t i = 0; i < sampleCount; ++i)
        {
            const auto value = std::clamp<long long>(
                static_cast<long long>(pcm[i]),
                std::numeric_limits<std::int16_t>::min(),
                std::numeric_limits<std::int16_t>::max()
            );
            output.samples.push_back(static_cast<std::int16_t>(value));
        }

        return true;
    }

    HANDLE_AACDECODER m_handle = nullptr;
    std::vector<INT_PCM> m_pcm = std::vector<INT_PCM>(32768U); // reused decode output
    std::vector<std::uint8_t> m_configuration;
    bool m_configured = false;
};

#endif

#if defined(MODEMDRM_HAS_FAAD2)

// AAC (with SBR and parametric stereo) through the DRM build of FAAD2
// (faad_drm), which has no xHE-AAC decoder. FAAD2 checks each frame's CRC
// itself. It has no concealment, so a lost frame fades the previous one out,
// and further lost frames are silent.
class FAAD2AudioDecoder final : public DRMAudioDecoder
{
public:
    ~FAAD2AudioDecoder() override
    {
        close();
    }

    AudioDecoderCapabilities capabilities() const override
    {
        const bool aac = (NeAACDecGetCapabilities() & ERROR_RESILIENCE_CAP) != 0U;
        return {"faad2", aac, aac, false};
    }

    bool configure(const SDCAudioInformation& audio, std::string* error) override
    {
        const auto fail = [this, error](const char *message) {
            close();
            if (error) {
                *error = message;
            }

            return false;
        };

        if (audio.audioCoding != 0) {
            return fail("FAAD2 backend does not support this DRM audio coding");
        }

        const unsigned long sampleRate = coreSampleRate(audio.samplingRateCode);

        if (sampleRate == 0U) {
            return fail("FAAD2 backend does not support this DRM AAC sampling rate");
        }

        const unsigned char channels = channelMode(audio);

        if (m_handle && (sampleRate == m_sampleRate) && (channels == m_channels))
        {
            if (error) {
                error->clear();
            }

            return true;
        }

        m_sampleRate = sampleRate;
        m_channels = channels;

        if (!open()) {
            return fail("FAAD2 rejected the DRM audio configuration");
        }

        if (error) {
            error->clear();
        }

        return true;
    }

    bool decode(
        const std::vector<std::uint8_t>& codecFrame,
        PCMFrame& output,
        std::string* error
    ) override
    {
        output = PCMFrame();

        if (!m_handle)
        {
            if (error) {
                *error = "DRM AAC decoder is not configured";
            }

            return false;
        }

        if (codecFrame.empty() || (codecFrame.size() > std::numeric_limits<unsigned long>::max()))
        {
            if (error) {
                *error = "Invalid DRM AAC codec frame";
            }

            return false;
        }

        // The frame is the transmitted CRC byte followed by the access unit,
        // which is what the DRM mode of FAAD2 reads.
        NeAACDecFrameInfo information {};
        const auto *pcm = static_cast<const std::int16_t*>(NeAACDecDecode(m_handle, &information,
            const_cast<unsigned char*>(codecFrame.data()), static_cast<unsigned long>(codecFrame.size())));

        if (!pcm || (information.error != 0U))
        {
            if (error)
            {
                *error = "FAAD2 failed to decode the DRM audio frame: ";
                *error += NeAACDecGetErrorMessage(information.error);
            }

            return false;
        }

        // The first frame only primes the filter bank and gives no samples:
        // give a frame of silence, so that every reset keeps the timeline
        if (information.samples == 0U)
        {
            output = silence();

            if ((information.samplerate > 0U) && (information.channels > 0U))
            {
                output.sampleRate = static_cast<int>(information.samplerate);
                output.channels = std::min<int>(information.channels, 2);
                output.samples.assign(static_cast<std::size_t>(frameLength() * output.channels), 0);
            }

            output.concealed = true; // no decoded audio in it

            if (error) {
                error->clear();
            }

            return true;
        }

        if ((information.channels < 1U) || (information.channels > 2U) || (information.samplerate == 0U)
            || ((information.samples % information.channels) != 0U))
        {
            if (error) {
                *error = "FAAD2 returned invalid DRM PCM parameters";
            }

            return false;
        }

        output.sampleRate = static_cast<int>(information.samplerate);
        output.channels = information.channels;
        output.samples.assign(pcm, pcm + information.samples);
        m_last = output;
        m_concealedFrames = 0;

        if (error) {
            error->clear();
        }

        return true;
    }

    bool conceal(PCMFrame& output, std::string* error) override
    {
        output = PCMFrame();

        if (!m_handle)
        {
            if (error) {
                *error = "DRM AAC decoder is not configured";
            }

            return false;
        }

        output.concealed = true;

        if (m_last.samples.empty()) {
            // Nothing decoded since the reset: a frame of silence
            output = silence();
        }
        else if (m_concealedFrames++ == 0)
        {
            // Fade out over the last frame played backwards, which starts
            // from the sample just output rather than jumping back to its first
            const std::size_t channels = static_cast<std::size_t>(m_last.channels);
            const std::size_t frames = m_last.samples.size() / channels;
            output.sampleRate = m_last.sampleRate;
            output.channels = m_last.channels;
            output.samples.resize(m_last.samples.size());

            for (std::size_t frame = 0; frame < frames; ++frame)
            {
                const double gain = 1.0 - static_cast<double>(frame + 1U) / static_cast<double>(frames);

                for (std::size_t channel = 0; channel < channels; ++channel)
                {
                    output.samples[frame * channels + channel] = static_cast<std::int16_t>(
                        std::lround(m_last.samples[(frames - 1U - frame) * channels + channel] * gain));
                }
            }
        }
        else
        {
            output.sampleRate = m_last.sampleRate;
            output.channels = m_last.channels;
            output.samples.assign(m_last.samples.size(), 0);
        }

        output.concealed = true;

        if (error) {
            error->clear();
        }

        return true;
    }

    void reset() override
    {
        if (m_handle) {
            open();
        }
    }

private:
    // Output samples per frame: 960 of the core, doubled by SBR
    int frameLength() const
    {
        return m_channels >= DRMCH_SBR_MONO ? 1920 : 960;
    }

    // A frame of silence in the configured format (FAAD2 always outputs stereo)
    PCMFrame silence() const
    {
        PCMFrame frame;
        frame.sampleRate = static_cast<int>(m_sampleRate * (m_channels >= DRMCH_SBR_MONO ? 2U : 1U));
        frame.channels = 2;
        frame.samples.assign(static_cast<std::size_t>(frameLength() * frame.channels), 0);
        frame.concealed = true;
        return frame;
    }

    // The AAC core sampling rate, which SBR doubles
    static unsigned long coreSampleRate(int samplingRateCode)
    {
        switch (samplingRateCode)
        {
        case 1:
            return 12000U;
        case 3:
            return 24000U;
        case 5:
            return 48000U;
        default:
            return 0U;
        }
    }

    // Audio mode 0 is mono, 1 parametric stereo (with SBR only) and 2 stereo
    static unsigned char channelMode(const SDCAudioInformation& audio)
    {
        if (!audio.sbr) {
            return audio.audioMode == 2 ? DRMCH_STEREO : DRMCH_MONO;
        }

        switch (audio.audioMode)
        {
        case 1:
            return DRMCH_SBR_PS_STEREO;
        case 2:
            return DRMCH_SBR_STEREO;
        default:
            return DRMCH_SBR_MONO;
        }
    }

    bool open()
    {
        // NeAACDecInitDRM closes the decoder it is given and opens a new one
        if (!m_handle) {
            m_handle = NeAACDecOpen();
        }

        if (!m_handle || (NeAACDecInitDRM(&m_handle, m_sampleRate, m_channels) != 0))
        {
            close();
            return false;
        }

        NeAACDecConfigurationPtr configuration = NeAACDecGetCurrentConfiguration(m_handle);
        configuration->outputFormat = FAAD_FMT_16BIT;
        NeAACDecSetConfiguration(m_handle, configuration);
        m_last = PCMFrame();
        m_concealedFrames = 0;
        return true;
    }

    void close()
    {
        if (m_handle)
        {
            NeAACDecClose(m_handle);
            m_handle = nullptr;
        }

        m_last = PCMFrame();
        m_concealedFrames = 0;
    }

    NeAACDecHandle m_handle = nullptr;
    unsigned long m_sampleRate = 0;
    unsigned char m_channels = 0;
    PCMFrame m_last;            // The frame a lost one fades out from
    int m_concealedFrames = 0;
};

#endif

#if defined(MODEMDRM_HAS_FAAD2) && defined(MODEMDRM_HAS_FDK_AAC)

// AAC through FAAD2 and xHE-AAC through FDK AAC: each configuration goes to
// the backend for its audio coding.
class RoutingAudioDecoder final : public DRMAudioDecoder
{
public:
    AudioDecoderCapabilities capabilities() const override
    {
        const AudioDecoderCapabilities aac = m_aac.capabilities();
        const AudioDecoderCapabilities xheAAC = m_xheAAC.capabilities();
        return {aac.backend + "+" + xheAAC.backend, aac.aac || xheAAC.xheAAC, aac.aac, xheAAC.xheAAC};
    }

    bool configure(const SDCAudioInformation& audio, std::string* error) override
    {
        DRMAudioDecoder *backend = audio.audioCoding == 3
            ? static_cast<DRMAudioDecoder*>(&m_xheAAC) : static_cast<DRMAudioDecoder*>(&m_aac);

        if (m_active && (m_active != backend)) {
            m_active->reset();
        }

        m_active = backend;
        return backend->configure(audio, error);
    }

    bool decode(const std::vector<std::uint8_t>& codecFrame, PCMFrame& output, std::string* error) override
    {
        if (!m_active) {
            return notConfigured(output, error);
        }

        return m_active->decode(codecFrame, output, error);
    }

    bool conceal(PCMFrame& output, std::string* error) override
    {
        if (!m_active) {
            return notConfigured(output, error);
        }

        return m_active->conceal(output, error);
    }

    void reset() override
    {
        m_aac.reset();
        m_xheAAC.reset();
    }

private:
    static bool notConfigured(PCMFrame& output, std::string* error)
    {
        output = PCMFrame();

        if (error) {
            *error = "DRM audio decoder is not configured";
        }

        return false;
    }

    FAAD2AudioDecoder m_aac;
    FDKAACAudioDecoder m_xheAAC;
    DRMAudioDecoder *m_active = nullptr;
};

#endif

} // namespace

std::unique_ptr<DRMAudioDecoder> DRMAudioDecoder::create()
{
#if defined(MODEMDRM_HAS_FAAD2) && defined(MODEMDRM_HAS_FDK_AAC)
    return std::make_unique<RoutingAudioDecoder>();
#elif defined(MODEMDRM_HAS_FAAD2)
    return std::make_unique<FAAD2AudioDecoder>();
#elif defined(MODEMDRM_HAS_FDK_AAC)
    return std::make_unique<FDKAACAudioDecoder>();
#else
    return std::make_unique<UnavailableAudioDecoder>();
#endif
}

} // namespace modemdrm
