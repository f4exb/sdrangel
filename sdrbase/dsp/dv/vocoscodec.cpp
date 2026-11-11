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

#include <cstdlib>
#include <map>
#include <mutex>
#include <tuple>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLibrary>

#include "vocoscodec.h"

const char *const VocosCodec::ModelFiles[3] = { "encodec_encoder.bin", "encodec_codebooks.bin", "vocos_decoder.bin" };
const char *const VocosCodec::ModelSha256[3] = {
    "7960bf7cf1af0b002bd20f6712b9bff0f2a57bd1bbd4b4d3bc758987f784e4ae",
    "4adfcc7a3b5670310dc9d2c227bfa6773b4b6de36bf4709507b0a1c82926b57d",
    "b373ace31e114cfb7f95b2d96da374c987fbfb9168a13905b3a58dbd99556e5b"
};

// The C interface of vocoscodec/c_api.h, resolved at run time
struct VocosCodec::Library
{
    typedef int (*AbiVersionFn)(void);
    typedef const char *(*Text)(void);
    typedef void *(*EncoderCreate)(const char*, int);
    typedef int (*Encode)(void*, const int16_t*, uint8_t*, int);
    typedef void (*Op)(void*);
    typedef void *(*DecoderCreate)(const char*, int, int);
    typedef int (*Query)(void*);
    typedef int (*Decode)(void*, const uint8_t*, int, int16_t*, int);
    typedef int (*DecodePackets)(void*, const uint8_t* const*, const int*, int, int16_t*, int);
    typedef int (*DecodeLost)(void*, int16_t*, int);

    QLibrary m_library;
    bool m_loaded = false;
    std::string m_error;
    AbiVersionFn m_abiVersion = nullptr;
    Text m_version = nullptr;
    Text m_builtModelDirectory = nullptr;
    Text m_lastError = nullptr;
    EncoderCreate m_encoderCreate = nullptr;
    Encode m_encode = nullptr;
    Op m_encoderReset = nullptr;
    Op m_encoderDestroy = nullptr;
    DecoderCreate m_decoderCreate = nullptr;
    Query m_decoderDelay = nullptr;
    Decode m_decode = nullptr;
    DecodePackets m_decodePackets = nullptr;
    DecodeLost m_decodeLost = nullptr;
    DecodeLost m_decoderFlush = nullptr;
    Query m_decoderIsComfortNoise = nullptr;
    Op m_decoderReset = nullptr;
    Op m_decoderDestroy = nullptr;

    Library()
    {
        // Beside the executable, in the library directory of a Linux install, then the loader's paths
        QStringList candidates;
        const QString appDir = QCoreApplication::instance() ? QCoreApplication::applicationDirPath() : QString();

        if (!appDir.isEmpty())
        {
            candidates << appDir + "/vocoscodec";
            candidates << appDir + "/../lib/sdrangel/vocoscodec";
            candidates << appDir + "/../lib64/sdrangel/vocoscodec";
            candidates << appDir + "/../lib/vocoscodec";
        }

#ifdef VOCOSCODEC_LIBRARY_PATH
        candidates << QString::fromUtf8(VOCOSCODEC_LIBRARY_PATH);
#endif
        candidates << "vocoscodec";

        for (const QString& candidate : candidates)
        {
            m_library.setFileName(candidate);

            if (m_library.load()) {
                break;
            }

            m_error = m_library.errorString().toStdString();
        }

        if (!m_library.isLoaded())
        {
            m_error = "V1 codec library (vocoscodec) not found: " + m_error;
            qWarning("VocosCodec::Library: %s", m_error.c_str());
            return;
        }

        m_abiVersion = (AbiVersionFn) m_library.resolve("vocoscodec_abi_version");
        m_version = (Text) m_library.resolve("vocoscodec_version");
        m_builtModelDirectory = (Text) m_library.resolve("vocoscodec_built_model_directory");
        m_lastError = (Text) m_library.resolve("vocoscodec_last_error");
        m_encoderCreate = (EncoderCreate) m_library.resolve("vocoscodec_encoder_create");
        m_encode = (Encode) m_library.resolve("vocoscodec_encode");
        m_encoderReset = (Op) m_library.resolve("vocoscodec_encoder_reset");
        m_encoderDestroy = (Op) m_library.resolve("vocoscodec_encoder_destroy");
        m_decoderCreate = (DecoderCreate) m_library.resolve("vocoscodec_decoder_create");
        m_decoderDelay = (Query) m_library.resolve("vocoscodec_decoder_delay_samples");
        m_decode = (Decode) m_library.resolve("vocoscodec_decode");
        m_decodePackets = (DecodePackets) m_library.resolve("vocoscodec_decode_packets");
        m_decodeLost = (DecodeLost) m_library.resolve("vocoscodec_decode_lost");
        m_decoderFlush = (DecodeLost) m_library.resolve("vocoscodec_decoder_flush");
        m_decoderIsComfortNoise = (Query) m_library.resolve("vocoscodec_decoder_is_comfort_noise");
        m_decoderReset = (Op) m_library.resolve("vocoscodec_decoder_reset");
        m_decoderDestroy = (Op) m_library.resolve("vocoscodec_decoder_destroy");

        if (!m_abiVersion || !m_version || !m_builtModelDirectory || !m_lastError || !m_encoderCreate || !m_encode
            || !m_encoderReset || !m_encoderDestroy || !m_decoderCreate || !m_decoderDelay || !m_decode || !m_decodePackets
            || !m_decodeLost || !m_decoderFlush || !m_decoderIsComfortNoise || !m_decoderReset || !m_decoderDestroy)
        {
            m_error = "V1 codec library: missing symbols in " + m_library.fileName().toStdString();
            qWarning("VocosCodec::Library: %s", m_error.c_str());
            m_library.unload();
            return;
        }

        if (m_abiVersion() != AbiVersion)
        {
            m_error = "V1 codec library: interface version " + std::to_string(m_abiVersion()) + ", expected " + std::to_string(AbiVersion);
            qWarning("VocosCodec::Library: %s", m_error.c_str());
            m_library.unload();
            return;
        }

        m_loaded = true;
        m_error.clear();
        qDebug("VocosCodec::Library: loaded %s (version %s)", qPrintable(m_library.fileName()), m_version());
    }

    std::string lastError() const { return m_lastError ? std::string(m_lastError()) : std::string(); }
};

VocosCodec::Library *VocosCodec::library()
{
    static Library *lib = nullptr;
    static std::once_flag once;
    std::call_once(once, []() { lib = new Library(); });
    return lib;
}

bool VocosCodec::isAvailable()
{
    return library()->m_loaded;
}

std::string VocosCodec::getError()
{
    return library()->m_error;
}

std::string VocosCodec::getVersion()
{
    return library()->m_loaded ? std::string(library()->m_version()) : std::string("not loaded");
}

// ---- Packets (the normative layout: 10-bit indices, MSB first, coarse codebook first) ---------

int VocosCodec::packetBytes(int codebooks)
{
    return ((codebooks >= 1) && (codebooks <= MaxCodebooks)) ? (codebooks * BitsPerCodebook + 7) / 8 : 0;
}

int VocosCodec::packetCodebooks(int bytes)
{
    for (int n = 1; n <= MaxCodebooks; n++)
    {
        if (packetBytes(n) == bytes) {
            return n;
        }
    }

    return 0;
}

int VocosCodec::truncate(uint8_t *packet, int bytes, int codebooks)
{
    const int from = packetCodebooks(bytes);

    if (!packet || (from == 0) || (codebooks < 1) || (codebooks > from)) {
        return 0;
    }

    const int size = packetBytes(codebooks);
    const int bits = codebooks * BitsPerCodebook;

    if (bits % 8) {
        packet[size - 1] &= (uint8_t) (0xFF << (8 - bits % 8));   // the bits past the last kept index
    }

    return size;
}

// ---- Models ---------------------------------------------------------------------------------

bool VocosCodec::hasModels(const std::string& directory)
{
    if (directory.empty()) {
        return false;
    }

    const QDir dir(QString::fromStdString(directory));
    return dir.exists(ModelFiles[0]) && dir.exists(ModelFiles[1]) && dir.exists(ModelFiles[2]);
}

bool VocosCodec::verifyModels(const std::string& directory, std::string& error)
{
    // The files are 70 MB: hashed once per directory, again only when one of them changes
    static std::mutex mutex;
    static std::map<std::string, std::tuple<std::string, bool, std::string>> cache;   // directory -> (stamp, ok, error)
    const QDir dir(QString::fromStdString(directory));
    std::string stamp;

    for (const char *file : ModelFiles)
    {
        const QFileInfo info(dir.filePath(file));
        stamp += std::to_string(info.exists() ? info.size() : -1) + ":" + std::to_string(info.exists() ? info.lastModified().toMSecsSinceEpoch() : 0) + ";";
    }

    std::lock_guard<std::mutex> lock(mutex);
    auto it = cache.find(directory);

    if ((it != cache.end()) && (std::get<0>(it->second) == stamp))
    {
        error = std::get<2>(it->second);
        return std::get<1>(it->second);
    }

    bool ok = true;
    error.clear();

    for (int i = 0; ok && (i < 3); i++)
    {
        QFile file(dir.filePath(ModelFiles[i]));

        if (!file.open(QIODevice::ReadOnly))
        {
            error = std::string("V1 model file ") + ModelFiles[i] + " missing in " + directory;
            ok = false;
            break;
        }

        QCryptographicHash hash(QCryptographicHash::Sha256);
        hash.addData(&file);
        const std::string digest = hash.result().toHex().toStdString();

        if (digest != ModelSha256[i])
        {
            // Another model set is another codec family (3.1.1): refused, never substituted
            error = std::string("V1 model file ") + ModelFiles[i] + " in " + directory + " is not V1's (SHA-256 "
                + digest.substr(0, 16) + "..., expected " + std::string(ModelSha256[i]).substr(0, 16) + "...)";
            ok = false;
        }
    }

    cache[directory] = std::make_tuple(stamp, ok, error);
    return ok;
}

std::string VocosCodec::defaultModelDirectory()
{
    const char *env = getenv("VOCOSCODEC_MODEL_DIR");

    if (env && env[0] && hasModels(env)) {
        return env;
    }

    const QString appDir = QCoreApplication::instance() ? QCoreApplication::applicationDirPath() : QString();

    if (!appDir.isEmpty())
    {
        for (const QString& candidate : { appDir + "/vocoscodec", appDir + "/../share/sdrangel/vocoscodec" })
        {
            const std::string directory = QFileInfo(candidate).absoluteFilePath().toStdString();

            if (hasModels(directory)) {
                return directory;
            }
        }
    }

#ifdef VOCOSCODEC_CONFIGURED_MODEL_DIR
    if (hasModels(VOCOSCODEC_CONFIGURED_MODEL_DIR)) {
        return VOCOSCODEC_CONFIGURED_MODEL_DIR;
    }
#endif

    if (library()->m_loaded)
    {
        const std::string built = library()->m_builtModelDirectory();

        if (hasModels(built)) {
            return built;
        }
    }

    return std::string();
}

bool VocosCodec::modelsFor(const std::string& requested, std::string& directory, std::string& error)
{
    if (!isAvailable())
    {
        error = getError();
        return false;
    }

    directory = requested.empty() ? defaultModelDirectory() : requested;

    if (directory.empty())
    {
        error = std::string("V1 models not found (") + ModelFiles[0] + ", " + ModelFiles[1] + ", " + ModelFiles[2] + ")";
        return false;
    }

    return verifyModels(directory, error);
}

// ---- Encoder ---------------------------------------------------------------------------------

VocosCodec::Encoder::Encoder() :
    m_encoder(nullptr),
    m_codebooks(0)
{
}

VocosCodec::Encoder::~Encoder()
{
    close();
}

bool VocosCodec::Encoder::open(int codebooks, const std::string& modelDirectory)
{
    close();
    std::string directory;

    if (!modelsFor(modelDirectory, directory, m_error)) {
        return false;
    }

    m_encoder = library()->m_encoderCreate(directory.c_str(), codebooks);

    if (!m_encoder)
    {
        m_error = "V1 encoder: " + library()->lastError() + " (models in " + directory + ")";
        return false;
    }

    m_codebooks = codebooks;
    m_error.clear();
    return true;
}

void VocosCodec::Encoder::close()
{
    if (m_encoder)
    {
        library()->m_encoderDestroy(m_encoder);
        m_encoder = nullptr;
    }

    m_codebooks = 0;
}

int VocosCodec::Encoder::encode(const int16_t *pcm, uint8_t *packet, int capacity)
{
    if (!m_encoder) {
        return 0;
    }

    const int n = library()->m_encode(m_encoder, pcm, packet, capacity);

    if (n == 0) {
        m_error = "V1 encode: " + library()->lastError();
    }

    return n;
}

void VocosCodec::Encoder::reset()
{
    if (m_encoder) {
        library()->m_encoderReset(m_encoder);
    }
}

// ---- Decoder ---------------------------------------------------------------------------------

VocosCodec::Decoder::Decoder() :
    m_decoder(nullptr)
{
}

VocosCodec::Decoder::~Decoder()
{
    close();
}

bool VocosCodec::Decoder::open(const std::string& modelDirectory, int lookahead, bool silence)
{
    close();
    std::string directory;

    if (!modelsFor(modelDirectory, directory, m_error)) {
        return false;
    }

    m_decoder = library()->m_decoderCreate(directory.c_str(), lookahead, silence ? 1 : 0);

    if (!m_decoder)
    {
        m_error = "V1 decoder: " + library()->lastError() + " (models in " + directory + ")";
        return false;
    }

    m_error.clear();
    return true;
}

void VocosCodec::Decoder::close()
{
    if (m_decoder)
    {
        library()->m_decoderDestroy(m_decoder);
        m_decoder = nullptr;
    }
}

int VocosCodec::Decoder::delaySamples() const
{
    return m_decoder ? library()->m_decoderDelay(m_decoder) : 0;
}

int VocosCodec::Decoder::decode(const uint8_t *packet, int packetBytes, int16_t *pcm, int capacity)
{
    if (!m_decoder) {
        return 0;
    }

    const int n = library()->m_decode(m_decoder, packet, packetBytes, pcm, capacity);

    if (n == 0) {
        m_error = "V1 decode: " + library()->lastError();
    }

    return n;
}

int VocosCodec::Decoder::decodePackets(const uint8_t *const *packets, const int *packetBytes, int count, int16_t *pcm, int capacity)
{
    if (!m_decoder) {
        return 0;
    }

    const int n = library()->m_decodePackets(m_decoder, packets, packetBytes, count, pcm, capacity);

    if (n == 0) {
        m_error = "V1 decode: " + library()->lastError();
    }

    return n;
}

int VocosCodec::Decoder::decodeLost(int16_t *pcm, int capacity)
{
    if (!m_decoder) {
        return 0;
    }

    const int n = library()->m_decodeLost(m_decoder, pcm, capacity);

    if (n == 0) {
        m_error = "V1 concealment: " + library()->lastError();
    }

    return n;
}

int VocosCodec::Decoder::flush(int16_t *pcm, int capacity)
{
    return m_decoder ? library()->m_decoderFlush(m_decoder, pcm, capacity) : 0;
}

bool VocosCodec::Decoder::isComfortNoise() const
{
    return m_decoder && (library()->m_decoderIsComfortNoise(m_decoder) != 0);
}

void VocosCodec::Decoder::reset()
{
    if (m_decoder) {
        library()->m_decoderReset(m_decoder);
    }
}
