///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022-2024 Jon Beniston, M7RCE <jon@beniston.com>                //
// Copyright (C) 2022 Jiří Pinkava <jiri.pinkava@rossum.ai>                      //
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

#include <QMutexLocker>
#include <QThread>
#include <QSslConfiguration>
#include <QSslCertificate>
#include <QSslKey>

#include "channel/channelwebapiutils.h"
#include "device/deviceapi.h"
#include "maincore.h"

#include "remotetcpsinksink.h"
#include "remotetcpsink.h"

static FLAC__StreamEncoderWriteStatus flacWriteCallback(const FLAC__StreamEncoder *encoder, const FLAC__byte buffer[], size_t bytes, uint32_t samples, uint32_t currentFrame, void *clientData)
{
    RemoteTCPSinkSink *sink = (RemoteTCPSinkSink*) clientData;

    return sink->flacWrite(encoder, buffer, bytes, samples, currentFrame);
}

RemoteTCPSinkSink::RemoteTCPSinkSink() :
    m_running(false),
    m_messageQueueToGUI(nullptr),
    m_messageQueueToChannel(nullptr),
    m_channelFrequencyOffset(0),
    m_channelSampleRate(48000),
    m_linearGain(1.0f),
    m_server(nullptr),
    m_webSocketServer(nullptr),
    m_encoder(nullptr),
    m_zStreamInitialised(false),
    m_zInBuf(m_zBufSize, '\0'),
    m_zOutBuf(m_zBufSize, '\0'),
    m_zInBufCount(0),
    m_bytesUncompressed(0),
    m_bytesCompressed(0),
    m_bytesTransmitted(0),
    m_squelchLevel(-150.0f),
    m_squelchCount(0),
    m_squelchOpen(false),
    m_squelchDelayLine(48000/2),
    m_magsq(0.0f),
    m_magsqSum(0.0f),
    m_magsqPeak(0.0f),
    m_magsqCount(0),
    m_centerFrequency(0.0),
    m_ppmCorrection(0),
    m_biasTeeEnabled(false),
    m_directSampling(false),
    m_agc(false),
    m_dcOffsetRemoval(false),
    m_iqCorrection(false),
    m_devSampleRate(0),
    m_log2Decim(0),
    m_rfBW(0),
    m_gain(),
    m_timer(this),
    m_azimuth(std::numeric_limits<float>::quiet_NaN()),
    m_elevation(std::numeric_limits<float>::quiet_NaN())
{
    qDebug("RemoteTCPSinkSink::RemoteTCPSinkSink");
    applySettings(m_settings, QStringList(), true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);

    // Get updated when position changes
    connect(&MainCore::instance()->getSettings(), &MainSettings::preferenceChanged, this, &RemoteTCPSinkSink::preferenceChanged);

    m_timer.setSingleShot(false);
    m_timer.setInterval(500);
    connect(&m_timer, &QTimer::timeout, this, &RemoteTCPSinkSink::checkDeviceSettings);
}

RemoteTCPSinkSink::~RemoteTCPSinkSink()
{
    qDebug("RemoteTCPSinkSink::~RemoteTCPSinkSink");
    disconnect(&MainCore::instance()->getSettings(), &MainSettings::preferenceChanged, this, &RemoteTCPSinkSink::preferenceChanged);
    stop();
}

void RemoteTCPSinkSink::start()
{
    qDebug("RemoteTCPSinkSink::start");

    if (m_running) {
        return;
    }

    connect(thread(), SIGNAL(started()), this, SLOT(started()));
    connect(thread(), SIGNAL(finished()), this, SLOT(finished()));

    m_running = true;
}

void RemoteTCPSinkSink::stop()
{
    qDebug("RemoteTCPSinkSink::stop");

    m_running = false;
}

void RemoteTCPSinkSink::started()
{
    QMutexLocker mutexLocker(&m_mutex);
    startServer();
    disconnect(thread(), SIGNAL(started()), this, SLOT(started()));
    m_timer.start();
}

void RemoteTCPSinkSink::finished()
{
    QMutexLocker mutexLocker(&m_mutex);
    stopServer();
    disconnect(thread(), SIGNAL(finished()), this, SLOT(finished()));
    m_timer.stop();
    m_running = false;
}

void RemoteTCPSinkSink::init()
{
}

void RemoteTCPSinkSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    QMutexLocker mutexLocker(&m_mutex);

    if (m_clients.size() > 0)
    {
        Complex ci;
        int bytes = 0;

        for (SampleVector::const_iterator it = begin; it != end; ++it)
        {
            Complex c(it->real(), it->imag());
            c *= m_nco.nextIQ();

            if (m_interpolatorDistance < 1.0f) // interpolate
            {
                while (!m_interpolator.interpolate(&m_interpolatorDistanceRemain, c, &ci))
                {
                    processOneSample(ci);
                    bytes += 2 * m_settings.m_sampleBits / 8;
                    m_interpolatorDistanceRemain += m_interpolatorDistance;
                }
            }
            else // decimate
            {
                if (m_interpolator.decimate(&m_interpolatorDistanceRemain, c, &ci))
                {
                    processOneSample(ci);
                    bytes += 2 * m_settings.m_sampleBits / 8;
                    m_interpolatorDistanceRemain += m_interpolatorDistance;
                }
            }
        }

        for (const auto client : m_clients) {
            client->flush();
        }

        QDateTime currentDateTime = QDateTime::currentDateTime();
        if (m_bwDateTime.isValid())
        {
            qint64 msecs = m_bwDateTime.msecsTo(currentDateTime) ;
            if (msecs >= 1000)
            {
                float secs = msecs / 1000.0f;
                float bw = (8 * m_bwBytes) / secs;
                float networkBW = (8 * m_bytesTransmitted) / secs;
                if (m_messageQueueToGUI) {
                    m_messageQueueToGUI->push(RemoteTCPSink::MsgReportBW::create(bw, networkBW, m_bytesUncompressed, m_bytesCompressed, m_bytesTransmitted));
                }
                m_bwDateTime = currentDateTime;
                m_bwBytes = bytes;
                m_bytesUncompressed = 0;
                m_bytesCompressed = 0;
                m_bytesTransmitted = 0;
            }
            else
            {
                m_bwBytes += bytes;
            }
        }
        else
        {
            m_bwDateTime = currentDateTime;
            m_bwBytes = bytes;
        }
    }
}

static qint32 clamp8(qint32 x)
{
    x = std::max(x, -128);
    x = std::min(x, 127);
    return x;
}

static qint32 clamp16(qint32 x)
{
    x = std::max(x, -32768);
    x = std::min(x, 32767);
    return x;
}

static qint32 clamp24(qint32 x)
{
    x = std::max(x, -8388608);
    x = std::min(x, 8388607);
    return x;
}

void RemoteTCPSinkSink::processOneSample(Complex &ci)
{
    // Apply gain
    ci = ci * m_linearGain;

    // Calculate channel power
    Real re = ci.real();
    Real im = ci.imag();
    Real magsq = (re*re + im*im) / (SDR_RX_SCALED*SDR_RX_SCALED);
    m_movingAverage(magsq);
    m_magsq = m_movingAverage.asDouble();
    m_magsqSum += magsq;
    m_magsqPeak = std::max<double>(magsq, m_magsqPeak);
    m_magsqCount++;

    // Squelch
    if (m_settings.m_squelchEnabled)
    {
        // Convert gate time from seconds to samples
        int squelchGate = m_settings.m_squelchGate * m_channelSampleRate;

        m_squelchDelayLine.write(ci);

        if (m_magsq < m_squelchLevel)
        {
            if (m_squelchCount > 0) {
                m_squelchCount--;
            }
        }
        else
        {
            m_squelchCount = squelchGate;
        }
        m_squelchOpen = m_squelchCount > 0;

        if (m_squelchOpen) {
            ci = m_squelchDelayLine.readBack(squelchGate);
        } else {
            ci = 0.0;
        }
    }

    if (!m_settings.m_iqOnly && (m_settings.m_compression == RemoteTCPSinkSettings::FLAC))
    {
        // Compress using FLAC
        FLAC__int32 iqBuf[2];

        if (m_settings.m_sampleBits == 8)
        {
            iqBuf[0] = (qint32) (ci.real() / 65536.0f);
            iqBuf[1] = (qint32) (ci.imag() / 65536.0f);
            iqBuf[0] = clamp8(iqBuf[0]);
            iqBuf[1] = clamp8(iqBuf[1]);
        }
        else if (m_settings.m_sampleBits == 16)
        {
            iqBuf[0] = (qint32) (ci.real() / 256.0f);
            iqBuf[1] = (qint32) (ci.imag() / 256.0f);
            iqBuf[0] = clamp16(iqBuf[0]);
            iqBuf[1] = clamp16(iqBuf[1]);
        }
        else if (m_settings.m_sampleBits == 24)
        {
            iqBuf[0] = (qint32) ci.real();
            iqBuf[1] = (qint32) ci.imag();
            iqBuf[0] = clamp24(iqBuf[0]);
            iqBuf[1] = clamp24(iqBuf[1]);
        }
        else
        {
            iqBuf[0] = (qint32) ci.real();
            iqBuf[1] = (qint32) ci.imag();
        }
        int bytes = 2 * m_settings.m_sampleBits / 8;
        m_bytesUncompressed += bytes;

        if (m_encoder && !FLAC__stream_encoder_process_interleaved(m_encoder, iqBuf, 1)) { // Number of samples in one channel
            qDebug() << "RemoteTCPSinkSink::processOneSample: FLAC failed to encode:" << FLAC__stream_encoder_get_state(m_encoder);
        }
    }
    else
    {
        quint8 iqBuf[4*2];

        if (m_settings.m_sampleBits == 8)
        {
            // Transmit data as per rtl_tcp - Interleaved unsigned 8-bit IQ
            iqBuf[0] = clamp8((qint32) (ci.real() / 65536.0f)) + 128;
            iqBuf[1] = clamp8((qint32) (ci.imag() / 65536.0f)) + 128;
        }
        else if (m_settings.m_sampleBits == 16)
        {
            // Interleaved little-endian signed 16-bit IQ
            qint32 i, q;
            i = clamp16((qint32) (ci.real() / 256.0f));
            q = clamp16((qint32) (ci.imag() / 256.0f));
            iqBuf[1] = (i >> 8) & 0xff;
            iqBuf[0] = i & 0xff;
            iqBuf[3] = (q >> 8) & 0xff;
            iqBuf[2] = q & 0xff;
        }
        else if (m_settings.m_sampleBits == 24)
        {
            // Interleaved little-endian signed 24-bit IQ
            qint32 i, q;
            i = clamp24((qint32) ci.real());
            q = clamp24((qint32) ci.imag());
            iqBuf[2] = (i >> 16) & 0xff;
            iqBuf[1] = (i >> 8) & 0xff;
            iqBuf[0] = i & 0xff;
            iqBuf[5] = (q >> 16) & 0xff;
            iqBuf[4] = (q >> 8) & 0xff;
            iqBuf[3] = q & 0xff;
        }
        else
        {
            // Interleaved little-endian signed 32-bit IQ
            qint32 i, q;
            i = (qint32) ci.real();
            q = (qint32) ci.imag();
            iqBuf[3] = (i >> 24) & 0xff;
            iqBuf[2] = (i >> 16) & 0xff;
            iqBuf[1] = (i >> 8) & 0xff;
            iqBuf[0] = i & 0xff;
            iqBuf[7] = (q >> 24) & 0xff;
            iqBuf[6] = (q >> 16) & 0xff;
            iqBuf[5] = (q >> 8) & 0xff;
            iqBuf[4] = q & 0xff;
        }

        int bytes = 2 * m_settings.m_sampleBits / 8;
        m_bytesUncompressed += bytes;

        if (!m_settings.m_iqOnly && (m_settings.m_compression == RemoteTCPSinkSettings::ZLIB))
        {
            if (m_zStreamInitialised)
            {
                // Store in block buffer
                memcpy(&m_zInBuf.data()[m_zInBufCount], iqBuf, bytes);
                m_zInBufCount += bytes;

                if (m_zInBufCount >= m_settings.m_blockSize)
                {
                    // Compress using zlib
                    m_zStream.next_in = (Bytef *) m_zInBuf.data();
                    m_zStream.avail_in = m_zInBufCount;
                    m_zStream.next_out = (Bytef *) m_zOutBuf.data();
                    m_zStream.avail_out = m_zOutBuf.size();
                    int ret = deflate(&m_zStream, Z_FINISH);

                    if (ret == Z_STREAM_END) {
                        deflateReset(&m_zStream);
                    } else if (ret != Z_OK) {
                        qDebug() << "RemoteTCPSinkSink::processOneSample: Failed to deflate" << ret;
                    }
                    if (m_zStream.avail_in != 0) {
                        qDebug() << "RemoteTCPSinkSink::processOneSample: Data still in input buffer";
                    }
                    int compressedBytes = m_zOutBuf.size() - m_zStream.avail_out;

                    //qDebug() << "zlib ret" << ret << "m_settings.m_blockSize" << m_settings.m_blockSize << "m_zInBufCount" << m_zInBufCount << "compressedBytes" << compressedBytes << "avail_in" << m_zStream.avail_in << "avail_out" << m_zStream.avail_out << " % " << round(100.0 * compressedBytes / (float) m_zInBufCount );

                    m_zInBufCount = 0;

                    // Send to clients

                    int clients = std::min((int) m_clients.size(), m_settings.m_maxClients);
                    char header[1+4];

                    header[0] = (char) RemoteTCPProtocol::dataIQzlib;
                    RemoteTCPProtocol::encodeUInt32((quint8 *) &header[1], compressedBytes);

                    for (int i = 0; i < clients; i++)
                    {
                        m_clients[i]->write(header, sizeof(header));
                        m_bytesTransmitted += sizeof(header);
                        m_clients[i]->write((const char *)m_zOutBuf.data(), compressedBytes);
                        m_bytesTransmitted += compressedBytes;
                    }
                    m_bytesCompressed += sizeof(header) + compressedBytes;
                }
            }
        }
        else
        {
            // Send uncompressed
            int clients = std::min((int) m_clients.size(), m_settings.m_maxClients);

            for (int i = 0; i < clients; i++)
            {
                m_clients[i]->write((const char *)iqBuf, bytes);
                m_bytesTransmitted += bytes;
            }
        }
    }
}

void RemoteTCPSinkSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "RemoteTCPSinkSink::applyChannelSettings:"
        << " channelSampleRate: " << channelSampleRate
        << " channelFrequencyOffset: " << channelFrequencyOffset;

    if ((m_channelFrequencyOffset != channelFrequencyOffset) ||
        (m_channelSampleRate != channelSampleRate) || force)
    {
        m_nco.setFreq(-channelFrequencyOffset, channelSampleRate);
    }

    if ((m_channelSampleRate != channelSampleRate) || force)
    {
        m_interpolator.create(16, channelSampleRate, m_settings.m_channelSampleRate / 2.0);
        m_interpolatorDistance = (Real) channelSampleRate / (Real) m_settings.m_channelSampleRate;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;

    m_squelchDelayLine.resize(m_settings.m_squelchGate * m_channelSampleRate + 1);
}

void RemoteTCPSinkSink::applySettings(const RemoteTCPSinkSettings& settings, const QStringList& settingsKeys, bool force, bool restartRequired)
{
    bool initFLAC = false;
    bool initZLib = false;

    QMutexLocker mutexLocker(&m_mutex);

    qDebug() << "RemoteTCPSinkSink::applySettings:" << settings.getDebugString(settingsKeys, force) << " force: " << force;

    if (settingsKeys.contains("gain") || force)
    {
        m_linearGain = powf(10.0f, settings.m_gain/20.0f);
    }

    if (settingsKeys.contains("channelSampleRate") || force)
    {
        m_interpolator.create(16, m_channelSampleRate, settings.m_channelSampleRate / 2.0);
        m_interpolatorDistance = (Real) m_channelSampleRate / (Real) settings.m_channelSampleRate;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }

    // Update time limit for connected clients
    if (settingsKeys.contains("timeLimit") && (m_settings.m_timeLimit != settings.m_timeLimit))
    {
        if (settings.m_timeLimit > 0)
        {
            // Set new timelimit
            for (int i = 0; i < m_timers.size(); i++) {
                m_timers[i]->setInterval(settings.m_timeLimit * 60 * 1000);
            }
            // Start timers if they weren't previously started
            if (m_settings.m_timeLimit == 0)
            {
                for (int i = 0; i < std::min((int) m_timers.size(), m_settings.m_maxClients); i++) {
                    m_timers[i]->start();
                }
            }
        }
        else
        {
            // Stop any existing timers
            for (int i = 0; i < m_timers.size(); i++) {
                m_timers[i]->stop();
            }
        }
    }

    if ((settingsKeys.contains("compressionLevel") && (settings.m_compressionLevel != m_settings.m_compressionLevel))
        || (settingsKeys.contains("compression") && (settings.m_compression != m_settings.m_compression))
        || (settingsKeys.contains("sampleBits")  && (settings.m_sampleBits != m_settings.m_sampleBits))
        || (settingsKeys.contains("blockSize")  && (settings.m_blockSize != m_settings.m_blockSize))
        || (settingsKeys.contains("channelSampleRate")  && (settings.m_channelSampleRate != m_settings.m_channelSampleRate))
        || force)
    {
        initFLAC = true;
    }

    if ((settingsKeys.contains("compressionLevel") && (settings.m_compressionLevel != m_settings.m_compressionLevel))
        || (settingsKeys.contains("compression") && (settings.m_compression != m_settings.m_compression))
        || force)
    {
        initZLib = true;
    }

    if (settingsKeys.contains("squelch") || force)
    {
        m_squelchLevel = std::pow(10.0, settings.m_squelch / 10.0);
        m_movingAverage.reset();
        m_squelchCount = 0;
    }

    if (settingsKeys.contains("squelchGate") || force) {
        m_squelchDelayLine.resize(settings.m_squelchGate * m_channelSampleRate + 1);
    }

    // Do clients need to reconnect to get these updated settings?
    // settingsKeys will be empty if force is set
    bool restart = (settingsKeys.contains("dataAddress") && (m_settings.m_dataAddress != settings.m_dataAddress))
                || (settingsKeys.contains("dataPort") && (m_settings.m_dataPort != settings.m_dataPort))
                || (settingsKeys.contains("certificate") && (m_settings.m_certificate != settings.m_certificate))
                || (settingsKeys.contains("key") && (m_settings.m_key!= settings.m_key))
                || (settingsKeys.contains("sampleBits") && (m_settings.m_sampleBits != settings.m_sampleBits))
                || (settingsKeys.contains("protocol") && (m_settings.m_protocol != settings.m_protocol))
                || (settingsKeys.contains("compression") && (m_settings.m_compression != settings.m_compression))
                || (settingsKeys.contains("remoteControl") && (m_settings.m_remoteControl != settings.m_remoteControl))
                || initFLAC
                || restartRequired
                   ;

    if (!restart && (m_settings.m_protocol != RemoteTCPSinkSettings::RTL0) && !m_settings.m_iqOnly)
    {
        // Forward settings to clients if they've changed
        if ((settingsKeys.contains("channelSampleRate") || force) && (settings.m_channelSampleRate != m_settings.m_channelSampleRate)) {
            sendCommand(RemoteTCPProtocol::setChannelSampleRate, settings.m_channelSampleRate);
        }
        if ((settingsKeys.contains("inputFrequencyOffset") || force) && (settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset)) {
            sendCommand(RemoteTCPProtocol::setChannelFreqOffset, settings.m_inputFrequencyOffset);
        }
        if ((settingsKeys.contains("gain") || force) && (settings.m_gain != m_settings.m_gain)) {
            sendCommand(RemoteTCPProtocol::setChannelGain, settings.m_gain);
        }
        if ((settingsKeys.contains("sampleBits") || force) && (settings.m_sampleBits != m_settings.m_sampleBits)) {
            sendCommand(RemoteTCPProtocol::setSampleBitDepth, settings.m_sampleBits);
        }
        if ((settingsKeys.contains("squelchEnabled") || force) && (settings.m_squelchEnabled != m_settings.m_squelchEnabled)) {
            sendCommand(RemoteTCPProtocol::setIQSquelchEnabled, (quint32) settings.m_squelchEnabled);
        }
        if ((settingsKeys.contains("squelch") || force) && (settings.m_squelch != m_settings.m_squelch))  {
            sendCommandFloat(RemoteTCPProtocol::setIQSquelch, settings.m_squelch);
        }
        if ((settingsKeys.contains("squelchGate") || force) && (settings.m_squelchGate != m_settings.m_squelchGate))  {
            sendCommandFloat(RemoteTCPProtocol::setIQSquelchGate, settings.m_squelchGate);
        }
        // m_remoteControl rather than restart?
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

    if (m_running && (restart || force)) {
        startServer();
    }

    if (initFLAC && (m_settings.m_compression == RemoteTCPSinkSettings::FLAC))
    {
        if (m_encoder)
        {
            // Delete existing decoder
            FLAC__stream_encoder_finish(m_encoder);
            FLAC__stream_encoder_delete(m_encoder);
            m_encoder = nullptr;
            m_flacHeader.clear();
        }

        // Create FLAC encoder
        FLAC__StreamEncoderInitStatus init_status;
        m_encoder = FLAC__stream_encoder_new();
        if (m_encoder)
        {
            const int maxSampleRate = 176400; // Spec says max is 655350, but doesn't seem to work
            FLAC__bool ok = true;

            ok &= FLAC__stream_encoder_set_verify(m_encoder, false);
	        ok &= FLAC__stream_encoder_set_compression_level(m_encoder, m_settings.m_compressionLevel);
	        ok &= FLAC__stream_encoder_set_channels(m_encoder, 2);
	        ok &= FLAC__stream_encoder_set_bits_per_sample(m_encoder, m_settings.m_sampleBits);
            // We'll get FLAC__STREAM_ENCODER_INIT_STATUS_NOT_STREAMABLE if we use the real sample rate
            if (m_settings.m_channelSampleRate < maxSampleRate) {
	            ok &= FLAC__stream_encoder_set_sample_rate(m_encoder, m_settings.m_channelSampleRate);
            } else {
	            ok &= FLAC__stream_encoder_set_sample_rate(m_encoder, maxSampleRate);
            }
	        ok &= FLAC__stream_encoder_set_total_samples_estimate(m_encoder, 0);
            //ok &= FLAC__stream_encoder_set_do_mid_side_stereo(m_encoder, false);
            // FLAC__MAX_BLOCK_SIZE is 65536
            // However, FLAC__format_blocksize_is_subset says anything over 16384 is not streamable
            // Also, if sampleRate <= 48000, then max block size is 4608
            if (FLAC__format_blocksize_is_subset(m_settings.m_blockSize, m_settings.m_channelSampleRate)) {
                ok &= FLAC__stream_encoder_set_blocksize(m_encoder, m_settings.m_blockSize);
            } else {
                ok &= FLAC__stream_encoder_set_blocksize(m_encoder, 4096);
            }
            if (ok)
            {
                init_status = FLAC__stream_encoder_init_stream(m_encoder, flacWriteCallback, nullptr, nullptr, nullptr, this);
                if (init_status != FLAC__STREAM_ENCODER_INIT_STATUS_OK)
                {
			        qDebug() << "RemoteTCPSinkSink::applySettings: Error initializing FLAC encoder:" << FLAC__StreamEncoderInitStatusString[init_status];
                    FLAC__stream_encoder_delete(m_encoder);
                    m_encoder = nullptr;
                }
            }
            else
            {
                qDebug() << "RemoteTCPSinkSink::applySettings: Failed to configure FLAC encoder";
                FLAC__stream_encoder_delete(m_encoder);
                m_encoder = nullptr;
            }
        }
        else
        {
            qDebug() << "RemoteTCPSinkSink::applySettings: Failed to allocate FLAC encoder";
        }

        m_bytesUncompressed = 0;
        m_bytesCompressed = 0;
    }

    if (initZLib && (m_settings.m_compression == RemoteTCPSinkSettings::ZLIB))
    {
        // Intialise zlib compression
        m_zStream.zalloc = nullptr;
        m_zStream.zfree = nullptr;
        m_zStream.opaque = nullptr;
        m_zStream.data_type = Z_BINARY;
        int windowBits = log2(m_settings.m_blockSize);

        if (Z_OK == deflateInit2(&m_zStream, m_settings.m_compressionLevel, Z_DEFLATED, windowBits, 9, Z_DEFAULT_STRATEGY))
        {
            m_zStreamInitialised = true;
        }
        else
        {
            qDebug() << "RemoteTCPSinkSink::applySettings: deflateInit failed";
            m_zStreamInitialised = false;
        }

        m_bytesUncompressed = 0;
        m_bytesCompressed = 0;
    }

}

void RemoteTCPSinkSink::startServer()
{
    stopServer();

    if (m_settings.m_protocol == RemoteTCPSinkSettings::SDRA_WSS)
    {
#ifndef QT_NO_OPENSSL
        QSslConfiguration sslConfiguration;
        qDebug() << "RemoteTCPSinkSink::startServer: SSL config: " << m_settings.m_certificate << m_settings.m_key;
        if (m_settings.m_certificate.isEmpty())
        {
            QString msg = "RemoteTCPSink requires an SSL certificate in order to use wss protocol";
            qWarning() << msg;
            if (m_messageQueueToGUI) {
                m_messageQueueToGUI->push(RemoteTCPSink::MsgError::create(msg));
            }
            return;
        }
        if (m_settings.m_certificate.isEmpty())
        {
            QString msg = "RemoteTCPSink requires an SSL key in order to use wss protocol";
            qWarning() << msg;
            if (m_messageQueueToGUI) {
                m_messageQueueToGUI->push(RemoteTCPSink::MsgError::create(msg));
            }
            return;
        }
        QFile certFile(m_settings.m_certificate);
        if (!certFile.open(QIODevice::ReadOnly))
        {
            QString msg = QString("RemoteTCPSink failed to open certificate %1: %2").arg(m_settings.m_certificate).arg(certFile.errorString());
            qWarning() << msg;
            if (m_messageQueueToGUI) {
                m_messageQueueToGUI->push(RemoteTCPSink::MsgError::create(msg));
            }
            return;
        }
        QFile keyFile(m_settings.m_key);
        if (!keyFile.open(QIODevice::ReadOnly))
        {
            QString msg = QString("RemoteTCPSink failed to open key %1: %2").arg(m_settings.m_key).arg(keyFile.errorString());
            qWarning() << msg;
            if (m_messageQueueToGUI) {
                m_messageQueueToGUI->push(RemoteTCPSink::MsgError::create(msg));
            }
            return;
        }
        QSslCertificate certificate(&certFile, QSsl::Pem);
        QSslKey sslKey(&keyFile, QSsl::Rsa, QSsl::Pem);
        certFile.close();
        keyFile.close();
        sslConfiguration.setPeerVerifyMode(QSslSocket::VerifyNone);
        sslConfiguration.setLocalCertificate(certificate);
        sslConfiguration.setPrivateKey(sslKey);

        m_webSocketServer = new QWebSocketServer(QStringLiteral("Remote TCP Sink"),
                                                 QWebSocketServer::SecureMode,
                                                 this);
        m_webSocketServer->setSslConfiguration(sslConfiguration);

        QHostAddress address(m_settings.m_dataAddress);

#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
        m_webSocketServer->setSupportedSubprotocols({"binary"}); // Chrome wont connect without this - "Sent non-empty 'Sec-WebSocket-Protocol' header but no response was received"
#endif
        if (!m_webSocketServer->listen(address, m_settings.m_dataPort))
        {
            QString msg = QString("RemoteTCPSink failed to listen on %1 port %2: %3").arg(m_settings.m_dataAddress).arg(m_settings.m_dataPort).arg(m_webSocketServer->errorString());
            qWarning() << msg;
            if (m_messageQueueToGUI) {
                m_messageQueueToGUI->push(RemoteTCPSink::MsgError::create(msg));
            }
        }
        else
        {
            qInfo() << "RemoteTCPSink listening on" << m_settings.m_dataAddress << "port" << m_settings.m_dataPort;
            connect(m_webSocketServer, &QWebSocketServer::newConnection, this, &RemoteTCPSinkSink::acceptWebConnection);
            connect(m_webSocketServer, &QWebSocketServer::sslErrors, this, &RemoteTCPSinkSink::onSslErrors);
        }
#else
        QString msg = "RemoteTCPSink unable to use wss protocol as SSL is not supported";
        qWarning() << msg;
        if (m_messageQueueToGUI) {
            m_messageQueueToGUI->push(RemoteTCPSink::MsgError::create(msg));
        }
#endif
    }
    else
    {
        m_server = new QTcpServer(this);
        if (!m_server->listen(QHostAddress(m_settings.m_dataAddress), m_settings.m_dataPort))
        {
            QString msg = QString("RemoteTCPSink failed to listen on %1 port %2: %3").arg(m_settings.m_dataAddress).arg(m_settings.m_dataPort).arg(m_webSocketServer->errorString());
            qWarning() << msg;
            if (m_messageQueueToGUI) {
                m_messageQueueToGUI->push(RemoteTCPSink::MsgError::create(msg));
            }
        }
        else
        {
            qInfo() << "RemoteTCPSink listening on" << m_settings.m_dataAddress << "port" << m_settings.m_dataPort;
            connect(m_server, &QTcpServer::newConnection, this, &RemoteTCPSinkSink::acceptTCPConnection);
        }
    }
}

void RemoteTCPSinkSink::stopServer()
{
    // Close connections to any existing clients
    while (m_clients.size() > 0) {
        m_clients[0]->close(); // This results in disconnected() being called, where we delete and remove from m_clients
    }

    // Close server sockets
    if (m_server)
    {
        qDebug() << "RemoteTCPSinkSink::stopServer: Closing old socket server";
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
    }
    if (m_webSocketServer)
    {
        qDebug() << "RemoteTCPSinkSink::stopServer: Closing old web socket server";
        m_webSocketServer->close();
        m_webSocketServer->deleteLater();
        m_webSocketServer = nullptr;
    }
}

RemoteTCPProtocol::Device RemoteTCPSinkSink::getDevice()
{
    DeviceAPI *deviceAPI = MainCore::instance()->getDevice(m_deviceIndex);
    if (deviceAPI)
    {
        QString id = deviceAPI->getHardwareId();
        QHash<QString, RemoteTCPProtocol::Device> map = {
            {"Airspy", RemoteTCPProtocol::AIRSPY},
            {"AirspyHF", RemoteTCPProtocol::AIRSPY_HF},
            {"AudioInput", RemoteTCPProtocol::AUDIO_INPUT},
            {"BladeRF1", RemoteTCPProtocol::BLADE_RF1},
            {"BladeRF2", RemoteTCPProtocol::BLADE_RF2},
            {"FCDPro", RemoteTCPProtocol::FCD_PRO},
            {"FCDProPlus", RemoteTCPProtocol::FCD_PRO_PLUS},
            {"FileInput", RemoteTCPProtocol::FILE_INPUT},
            {"HackRF", RemoteTCPProtocol::HACK_RF},
            {"KiwiSDR", RemoteTCPProtocol::KIWI_SDR},
            {"LimeSDR", RemoteTCPProtocol::LIME_SDR},
            {"LocalInput", RemoteTCPProtocol::LOCAL_INPUT},
            {"Perseus", RemoteTCPProtocol::PERSEUS},
            {"PlutoSDR", RemoteTCPProtocol::PLUTO_SDR},
            {"RemoteInput", RemoteTCPProtocol::REMOTE_INPUT},
            {"RemoteTCPInput", RemoteTCPProtocol::REMOTE_TCP_INPUT},
            {"SDRplay1", RemoteTCPProtocol::SDRPLAY_1},
            {"SigMFFileInput", RemoteTCPProtocol::SIGMF_FILE_INPUT},
            {"SoapySDR", RemoteTCPProtocol::SOAPY_SDR},
            {"TestSource", RemoteTCPProtocol::TEST_SOURCE},
            {"USRP", RemoteTCPProtocol::USRP},
            {"XTRX", RemoteTCPProtocol::XTRX},
        };
        if (map.contains(id))
        {
            return map.value(id);
        }
        else if (id == "SDRplayV3")
        {
            QString deviceType;
            if (ChannelWebAPIUtils::getDeviceReportValue(m_deviceIndex, "deviceType", deviceType))
            {
                QHash<QString, RemoteTCPProtocol::Device> sdrplayMap = {
                    {"RSP1", RemoteTCPProtocol::SDRPLAY_V3_RSP1},
                    {"RSP1A", RemoteTCPProtocol::SDRPLAY_V3_RSP1A},
                    {"RSP1B", RemoteTCPProtocol::SDRPLAY_V3_RSP1B},
                    {"RSP2", RemoteTCPProtocol::SDRPLAY_V3_RSP2},
                    {"RSPduo", RemoteTCPProtocol::SDRPLAY_V3_RSPDUO},
                    {"RSPdx", RemoteTCPProtocol::SDRPLAY_V3_RSPDX},
                };
                if (sdrplayMap.contains(deviceType)) {
                    return sdrplayMap.value(deviceType);
                }
                qDebug() << "RemoteTCPSinkSink::getDevice: Unknown SDRplayV3 deviceType: " << deviceType;
            }
            else
            {
                qDebug() << "RemoteTCPSinkSink::getDevice: Failed to get deviceType for SDRplayV3";
            }
        }
        else if (id == "RTLSDR")
        {
            QString tunerType;
            if (ChannelWebAPIUtils::getDeviceReportValue(m_deviceIndex, "tunerType", tunerType))
            {
                QHash<QString, RemoteTCPProtocol::Device> rtlsdrMap = {
                    {"E4000", RemoteTCPProtocol::RTLSDR_E4000},
                    {"FC0012", RemoteTCPProtocol::RTLSDR_FC0012},
                    {"FC0013", RemoteTCPProtocol::RTLSDR_FC0013},
                    {"FC2580", RemoteTCPProtocol::RTLSDR_FC2580},
                    {"R820T", RemoteTCPProtocol::RTLSDR_R820T},
                    {"R828D", RemoteTCPProtocol::RTLSDR_R828D},
                };
                if (rtlsdrMap.contains(tunerType)) {
                    return rtlsdrMap.value(tunerType);
                }
                qDebug() << "RemoteTCPSinkSink::getDevice: Unknown RTLSDR tunerType: " << tunerType;
            }
            else
            {
                qDebug() << "RemoteTCPSinkSink::getDevice: Failed to get tunerType for RTLSDR";
            }
        }
    }
    return RemoteTCPProtocol::UNKNOWN;
}

void RemoteTCPSinkSink::acceptWebConnection()
{
     QMutexLocker mutexLocker(&m_mutex);
     QWebSocket *client = m_webSocketServer->nextPendingConnection();

    connect(client, &QWebSocket::binaryMessageReceived, this, &RemoteTCPSinkSink::processCommand);
    connect(client, &QWebSocket::disconnected, this, &RemoteTCPSinkSink::disconnected);
    qDebug() << "RemoteTCPSinkSink::acceptWebConnection: client connected";

    // https://bugreports.qt.io/browse/QTBUG-125874
    QTimer::singleShot(200, this, [this, client] () {
        QMutexLocker mutexLocker(&m_mutex);
        m_clients.append(new WebSocket(client));
        acceptConnection(m_clients.last());
    });
}

void RemoteTCPSinkSink::acceptTCPConnection()
{
    QMutexLocker mutexLocker(&m_mutex);
    QTcpSocket *client = m_server->nextPendingConnection();

    connect(client, &QIODevice::readyRead, this, &RemoteTCPSinkSink::processCommand);
    connect(client, &QTcpSocket::disconnected, this, &RemoteTCPSinkSink::disconnected);
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    connect(client, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error), this, &RemoteTCPSinkSink::errorOccurred);
#else
    connect(client, &QAbstractSocket::errorOccurred, this, &RemoteTCPSinkSink::errorOccurred);
#endif
    qDebug() << "RemoteTCPSinkSink::acceptTCPConnection: client connected";

    QTimer::singleShot(200, this, [this, client] () {
        QMutexLocker mutexLocker(&m_mutex);
        m_clients.append(new TCPSocket(client));
        acceptConnection(m_clients.last());
    });
}

FLAC__StreamEncoderWriteStatus RemoteTCPSinkSink::flacWrite(const FLAC__StreamEncoder *encoder, const FLAC__byte buffer[], size_t bytes, uint32_t samples, uint32_t currentFrame)
{
    (void) encoder;

    char header[1+4];

    // Save FLAC header for clients that connect later
    if ((currentFrame == 0) && (samples == 0))
    {
        m_flacHeader.append((const char *) buffer, bytes);

        // Write complete header to all clients
        if (m_flacHeader.size() == m_flacHeaderSize)
        {
            header[0] = (char) RemoteTCPProtocol::dataIQFLAC;
            RemoteTCPProtocol::encodeUInt32((quint8 *) &header[1], m_flacHeader.size());

            for (auto client : m_clients)
            {
                client->write(header, sizeof(header));
                client->write(m_flacHeader.constData(), m_flacHeader.size());
                m_bytesTransmitted += sizeof(header) + m_flacHeader.size();
                client->flush();
            }
        }
    }
    else
    {
        // Send compressed IQ data to max number of clients
        header[0] = (char) RemoteTCPProtocol::dataIQFLAC;
        RemoteTCPProtocol::encodeUInt32((quint8 *) &header[1], bytes);
        int clients = std::min((int) m_clients.size(), m_settings.m_maxClients);
        for (int i = 0; i < clients; i++)
        {
            Socket *client = m_clients[i];
            client->write(header, sizeof(header));
            client->write((const char *) buffer, bytes);
            m_bytesTransmitted += sizeof(header) + bytes;
            client->flush();
        }
    }
    m_bytesCompressed += sizeof(header) + bytes;

    return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

void RemoteTCPSinkSink::acceptConnection(Socket *client)
{
    if (m_settings.m_protocol == RemoteTCPSinkSettings::RTL0)
    {
        quint8 metaData[RemoteTCPProtocol::m_rtl0MetaDataSize] = {'R', 'T', 'L', '0'};
        RemoteTCPProtocol::encodeUInt32(&metaData[4], getDevice()); // Tuner ID
        RemoteTCPProtocol::encodeUInt32(&metaData[8], 1); // Gain stages
        client->write((const char *)metaData, sizeof(metaData));
        m_bytesTransmitted += sizeof(metaData);
        client->flush();
    }
    else
    {
        quint8 metaData[RemoteTCPProtocol::m_sdraMetaDataSize] = {'S', 'D', 'R', 'A'};
        RemoteTCPProtocol::encodeUInt32(&metaData[4], getDevice());

        // Send device/channel settings, so they can be displayed in the remote GUI
        ChannelWebAPIUtils::getCenterFrequency(m_deviceIndex, m_centerFrequency);
        ChannelWebAPIUtils::getLOPpmCorrection(m_deviceIndex, m_ppmCorrection);
        ChannelWebAPIUtils::getDevSampleRate(m_deviceIndex, m_devSampleRate);
        ChannelWebAPIUtils::getSoftDecim(m_deviceIndex, m_log2Decim);
        for (int i = 0; i < 4; i++) {
            ChannelWebAPIUtils::getGain(m_deviceIndex, i, m_gain[i]);
        }
        ChannelWebAPIUtils::getRFBandwidth(m_deviceIndex, m_rfBW);
        ChannelWebAPIUtils::getBiasTee(m_deviceIndex, m_biasTeeEnabled);
        ChannelWebAPIUtils::getDeviceSetting(m_deviceIndex, "noModMode", m_directSampling);
        ChannelWebAPIUtils::getAGC(m_deviceIndex, m_agc);
        ChannelWebAPIUtils::getDCOffsetRemoval(m_deviceIndex, m_dcOffsetRemoval);
        ChannelWebAPIUtils::getIQCorrection(m_deviceIndex, m_iqCorrection);

        quint32 flags =   ((!m_settings.m_iqOnly) << 7)
                        | (m_settings.m_remoteControl << 6)
                        | (m_settings.m_squelchEnabled << 5)
                        | (m_iqCorrection << 4)
                        | (m_dcOffsetRemoval << 3)
                        | (m_agc << 2)
                        | (m_directSampling << 1)
                        | m_biasTeeEnabled;

        RemoteTCPProtocol::encodeUInt64(&metaData[8], (quint64)m_centerFrequency);
        RemoteTCPProtocol::encodeUInt32(&metaData[16], m_ppmCorrection);
        RemoteTCPProtocol::encodeUInt32(&metaData[20], flags);
        RemoteTCPProtocol::encodeUInt32(&metaData[24], m_devSampleRate);
        RemoteTCPProtocol::encodeUInt32(&metaData[28], m_log2Decim);
        RemoteTCPProtocol::encodeInt16(&metaData[32], m_gain[0]);
        RemoteTCPProtocol::encodeInt16(&metaData[34], m_gain[1]);
        RemoteTCPProtocol::encodeInt16(&metaData[36], m_gain[2]);
        RemoteTCPProtocol::encodeInt16(&metaData[38], m_gain[3]);
        RemoteTCPProtocol::encodeUInt32(&metaData[40], m_rfBW);
        RemoteTCPProtocol::encodeInt32(&metaData[44], m_settings.m_inputFrequencyOffset);
        RemoteTCPProtocol::encodeUInt32(&metaData[48], m_settings.m_gain);
        RemoteTCPProtocol::encodeUInt32(&metaData[52], m_settings.m_channelSampleRate);
        RemoteTCPProtocol::encodeUInt32(&metaData[56], m_settings.m_sampleBits);
        RemoteTCPProtocol::encodeUInt32(&metaData[60], 1); // Protocol revision. 0=64 byte meta data, 1=128 byte meta data
        RemoteTCPProtocol::encodeFloat(&metaData[64], m_settings.m_squelch);
        RemoteTCPProtocol::encodeFloat(&metaData[68], m_settings.m_squelchGate);
        // Send API port? Not accessible via MainCore

        client->write((const char *)metaData, sizeof(metaData));
        m_bytesTransmitted += sizeof(metaData);
        client->flush();

        // Inform client if they are in a queue
        if (!m_settings.m_iqOnly && (m_clients.size() > m_settings.m_maxClients)) {
            sendQueuePosition(client, m_clients.size() - m_settings.m_maxClients);
        }

        // Send existing FLAC header to new client
        if (!m_settings.m_iqOnly && (m_settings.m_compression == RemoteTCPSinkSettings::FLAC) && (m_flacHeader.size() == m_flacHeaderSize))
        {
            char header[1+4];

            header[0] = (char) RemoteTCPProtocol::dataIQFLAC;
            RemoteTCPProtocol::encodeUInt32((quint8 *) &header[1], m_flacHeader.size());
            client->write(header, sizeof(header));
            client->write(m_flacHeader.constData(), m_flacHeader.size());
            m_bytesTransmitted += sizeof(header) + m_flacHeader.size();
            client->flush();
        }

        // Send position / direction of antenna
        sendPosition();
        if (m_settings.m_isotropic) {
            sendDirection(true, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN());
        } else if (m_settings.m_rotator == "None") {
            sendDirection(false, m_settings.m_azimuth, m_settings.m_elevation);
        } else {
            sendRotatorDirection(true);
        }
    }

    // Create timer to disconnect client after timelimit reached
    QTimer *timer = new QTimer();
    timer->setSingleShot(true);
    timer->callOnTimeout(this, [this, client] () {
        qDebug() << "Disconnecting" << client->peerAddress() << "as time limit reached";
        if (m_settings.m_compression) {
            sendTimeLimit(client);
        }
        client->close();
    });
    if (m_settings.m_timeLimit > 0)
    {
        timer->setInterval(m_settings.m_timeLimit * 60 * 1000);
        // Only start timer if we will receive data immediately
        if (m_clients.size() <= m_settings.m_maxClients) {
            timer->start();
        }
    }
    m_timers.append(timer);

    // Close connection if blacklisted
    for (const auto& ip : m_settings.m_ipBlacklist)
    {
        QHostAddress address(ip);
        if (address == client->peerAddress())
        {
            qDebug() << "Disconnecting" << client->peerAddress() << "as blacklisted";
            if (m_settings.m_compression) {
                sendBlacklisted(client);
            }
            client->close();
            break;
        }
    }

    m_messageQueueToChannel->push(RemoteTCPSink::MsgReportConnection::create(m_clients.size(), client->peerAddress(), client->peerPort()));
    if (m_messageQueueToGUI) {
        m_messageQueueToGUI->push(RemoteTCPSink::MsgReportConnection::create(m_clients.size(), client->peerAddress(), client->peerPort()));
    }
}

void RemoteTCPSinkSink::disconnected()
{
    QMutexLocker mutexLocker(&m_mutex);
    qDebug() << "RemoteTCPSinkSink::disconnected";
    QObject *object = sender();
    QMutableListIterator<Socket *> itr(m_clients);

    // Remove from list of clients
    int idx = 0;
    while (itr.hasNext())
    {
        Socket *socket = itr.next();
        if (socket->socket() == object)
        {
            itr.remove();
            delete m_timers.takeAt(idx);
            m_messageQueueToChannel->push(RemoteTCPSink::MsgReportDisconnect::create(m_clients.size(), socket->peerAddress(), socket->peerPort()));
            if (m_messageQueueToGUI) {
                m_messageQueueToGUI->push(RemoteTCPSink::MsgReportDisconnect::create(m_clients.size(), socket->peerAddress(), socket->peerPort()));
            }
            socket->deleteLater();
            break;
        }
        else
        {
            idx++;
        }
    }

    // Start timer for next waiting client
    if ((idx < m_settings.m_maxClients) && (m_settings.m_timeLimit > 0))
    {
        int newActiveIdx = m_settings.m_maxClients - 1;
        if (newActiveIdx < m_clients.size()) {
            m_timers[newActiveIdx]->start();
        }
    }

    // Update other clients waiting with current queue position
    for (int i = m_settings.m_maxClients; i < m_clients.size(); i++) {
        sendQueuePosition(m_clients[i], i - m_settings.m_maxClients + 1);
    }
}

#ifndef QT_NO_OPENSSL
void RemoteTCPSinkSink::onSslErrors(const QList<QSslError> &errors)
{
    qWarning() << "RemoteTCPSinkSink::onSslErrors: " << errors;
}
#endif

void RemoteTCPSinkSink::errorOccurred(QAbstractSocket::SocketError socketError)
{
    qDebug() << "RemoteTCPSinkSink::errorOccurred: " << socketError;
    /*if (m_msgQueueToFeature)
    {
        RemoteTCPSinkSink::MsgReportWorker *msg = RemoteTCPSinkSink::MsgReportWorker::create(m_socket.errorString() + " " + socketError);
        m_msgQueueToFeature->push(msg);
    }*/
}

Socket *RemoteTCPSinkSink::getSocket(QObject *object) const
{
    for (const auto client : m_clients)
    {
        if (client->socket() == object) {
            return client;
        }
    }

    return nullptr;
}

void RemoteTCPSinkSink::processCommand()
{
    QMutexLocker mutexLocker(&m_mutex);
    Socket *client = getSocket(sender());
    RemoteTCPSinkSettings settings = m_settings;
    quint8 cmd[5];

    while (client && (client->bytesAvailable() >= (qint64)sizeof(cmd)))
    {
        int len = client->read((char *)cmd, sizeof(cmd));

        if (len == sizeof(cmd))
        {
            if (cmd[0] == RemoteTCPProtocol::sendMessage)
            {
                quint32 msgLen = RemoteTCPProtocol::extractUInt32(&cmd[1]);
                try
                {
                    char *buf = new char[msgLen];
                    len = client->read((char *)buf, msgLen);
                    if (len == (int) msgLen)
                    {
                        bool broadcast = (bool) buf[0];
                        int i;
                        for (i = 1; i < (int) msgLen; i++)
                        {
                            if (buf[i] == '\0') {
                                break;
                            }
                        }
                        QString callsign = QString::fromUtf8(&buf[1]);
                        QString text = QString::fromUtf8(&buf[i+1]);

                        if (m_messageQueueToGUI) {
                            m_messageQueueToGUI->push(RemoteTCPSink::MsgSendMessage::create(client->peerAddress(), client->peerPort(), callsign, text, broadcast));
                        }
                    }
                    else
                    {
                        qDebug() << "RemoteTCPSinkSink::processCommand: sendMessage: Failed to read" << msgLen;
                    }
                    delete[] buf;
                }
                catch(std::bad_alloc&)
                {
                    qDebug() << "RemoteTCPSinkSink::processCommand: sendMessage - Failed to allocate" << msgLen;
                }
            }
            else if (!m_settings.m_remoteControl)
            {
                qDebug() << "RemoteTCPSinkSink::processCommand: Ignoring command from client as remote control disabled";
            }
            else
            {
                switch (cmd[0])
                {
                case RemoteTCPProtocol::setCenterFrequency:
                {
                    quint64 centerFrequency = RemoteTCPProtocol::extractUInt32(&cmd[1]);
                    qDebug() << "RemoteTCPSinkSink::processCommand: set center frequency " << centerFrequency;
                    ChannelWebAPIUtils::setCenterFrequency(m_deviceIndex, (double)centerFrequency);
                    break;
                }
                case RemoteTCPProtocol::setSampleRate:
                {
                    int sampleRate = RemoteTCPProtocol::extractUInt32(&cmd[1]);
                    qDebug() << "RemoteTCPSinkSink::processCommand: set sample rate " << sampleRate;
                    ChannelWebAPIUtils::setDevSampleRate(m_deviceIndex, sampleRate);
                    if (m_settings.m_protocol == RemoteTCPSinkSettings::RTL0)
                    {
                        // Match channel sample rate with device sample rate for RTL0 protocol
                        ChannelWebAPIUtils::setSoftDecim(m_deviceIndex, 0);
                        settings.m_channelSampleRate = sampleRate;
                        if (m_messageQueueToGUI) {
                            m_messageQueueToGUI->push(RemoteTCPSink::MsgConfigureRemoteTCPSink::create(settings, {"channelSampleRate"}, false));
                        }
                        if (m_messageQueueToChannel) {
                            m_messageQueueToChannel->push(RemoteTCPSink::MsgConfigureRemoteTCPSink::create(settings, {"channelSampleRate"}, false));
                        }
                    }
                    break;
                }
                case RemoteTCPProtocol::setTunerGainMode:
                    // SDRangel's rtlsdr sample source always has this fixed as 1, so nothing to do
                    break;
                case RemoteTCPProtocol::setTunerGain: // gain is gain in 10th of a dB
                {
                    int gain = RemoteTCPProtocol::extractUInt32(&cmd[1]);
                    qDebug() << "RemoteTCPSinkSink::processCommand: set gain " << gain;
                    ChannelWebAPIUtils::setGain(m_deviceIndex, 0, gain);
                    break;
                }
                case RemoteTCPProtocol::setFrequencyCorrection:
                {
                    int ppm = RemoteTCPProtocol::extractUInt32(&cmd[1]);
                    qDebug() << "RemoteTCPSinkSink::processCommand: set LO ppm correction " << ppm;
                    ChannelWebAPIUtils::setLOPpmCorrection(m_deviceIndex, ppm);
                    break;
                }
                case RemoteTCPProtocol::setTunerIFGain:
                {
                    int v = RemoteTCPProtocol::extractUInt32(&cmd[1]);
                    int gain = (int)(qint16)(v & 0xffff);
                    int stage = (v >> 16) & 0xffff;
                    ChannelWebAPIUtils::setGain(m_deviceIndex, stage, gain);
                    break;
                }
                case RemoteTCPProtocol::setAGCMode:
                {
                    int agc = RemoteTCPProtocol::extractUInt32(&cmd[1]);
                    qDebug() << "RemoteTCPSinkSink::processCommand: set AGC " << agc;
                    ChannelWebAPIUtils::setAGC(m_deviceIndex, agc);
                    break;
                }
                case RemoteTCPProtocol::setDirectSampling:
                {
                    int ds = RemoteTCPProtocol::extractUInt32(&cmd[1]);
                    qDebug() << "RemoteTCPSinkSink::processCommand: set direct sampling " << ds;
                    ChannelWebAPIUtils::patchDeviceSetting(m_deviceIndex, "noModMode", ds);  // RTLSDR only
                    break;
                }
                case RemoteTCPProtocol::setBiasTee:
                {
                    int biasTee = RemoteTCPProtocol::extractUInt32(&cmd[1]);
                    qDebug() << "RemoteTCPSinkSink::processCommand: set bias tee " << biasTee;
                    ChannelWebAPIUtils::setBiasTee(m_deviceIndex, biasTee);
                    break;
                }
                case RemoteTCPProtocol::setTunerBandwidth:
                {
                    int rfBW = RemoteTCPProtocol::extractUInt32(&cmd[1]);
                    qDebug() << "RemoteTCPSinkSink::processCommand: set tuner bandwidth " << rfBW;
                    ChannelWebAPIUtils::setRFBandwidth(m_deviceIndex, rfBW);
                    break;
                }
                case RemoteTCPProtocol::setDecimation:
                {
                    int dec = RemoteTCPProtocol::extractUInt32(&cmd[1]);
                    qDebug() << "RemoteTCPSinkSink::processCommand: set decimation " << dec;
                    ChannelWebAPIUtils::setSoftDecim(m_deviceIndex, dec);
                    break;
                }
                case RemoteTCPProtocol::setDCOffsetRemoval:
                {
                    int dc = RemoteTCPProtocol::extractUInt32(&cmd[1]);
                    qDebug() << "RemoteTCPSinkSink::processCommand: set DC offset removal " << dc;
                    ChannelWebAPIUtils::setDCOffsetRemoval(m_deviceIndex, dc);
                    break;
                }
                case RemoteTCPProtocol::setIQCorrection:
                {
                    int iq = RemoteTCPProtocol::extractUInt32(&cmd[1]);
                    qDebug() << "RemoteTCPSinkSink::processCommand: set IQ correction " << iq;
                    ChannelWebAPIUtils::setIQCorrection(m_deviceIndex, iq);
                    break;
                }
                case RemoteTCPProtocol::setChannelSampleRate:
                {
                    int channelSampleRate = RemoteTCPProtocol::extractUInt32(&cmd[1]);
                    qDebug() << "RemoteTCPSinkSink::processCommand: set channel sample rate " << channelSampleRate;
                    bool restartRequired;
                    if (channelSampleRate <= m_settings.m_maxSampleRate)
                    {
                        settings.m_channelSampleRate = channelSampleRate;
                        restartRequired = false;
                    }
                    else
                    {
                        settings.m_channelSampleRate = m_settings.m_maxSampleRate;
                        restartRequired = true; // Need to restart so client gets max sample rate
                    }
                    if (m_messageQueueToGUI) {
                        m_messageQueueToGUI->push(RemoteTCPSink::MsgConfigureRemoteTCPSink::create(settings, {"channelSampleRate"}, false, restartRequired));
                    }
                    if (m_messageQueueToChannel) {
                        m_messageQueueToChannel->push(RemoteTCPSink::MsgConfigureRemoteTCPSink::create(settings, {"channelSampleRate"}, false, restartRequired));
                    }
                    break;
                }
                case RemoteTCPProtocol::setChannelFreqOffset:
                {
                    int offset = (int)RemoteTCPProtocol::extractUInt32(&cmd[1]);
                    qDebug() << "RemoteTCPSinkSink::processCommand: set channel input frequency offset " << offset;
                    settings.m_inputFrequencyOffset = offset;
                    if (m_messageQueueToGUI) {
                        m_messageQueueToGUI->push(RemoteTCPSink::MsgConfigureRemoteTCPSink::create(settings, {"inputFrequencyOffset"}, false));
                    }
                    if (m_messageQueueToChannel) {
                        m_messageQueueToChannel->push(RemoteTCPSink::MsgConfigureRemoteTCPSink::create(settings, {"inputFrequencyOffset"}, false));
                    }
                    break;
                }
                case RemoteTCPProtocol::setChannelGain:
                {
                    int gain = (int)RemoteTCPProtocol::extractUInt32(&cmd[1]);
                    qDebug() << "RemoteTCPSinkSink::processCommand: set channel gain " << gain;
                    settings.m_gain = gain;
                    if (m_messageQueueToGUI) {
                        m_messageQueueToGUI->push(RemoteTCPSink::MsgConfigureRemoteTCPSink::create(settings, {"gain"}, false));
                    }
                    if (m_messageQueueToChannel) {
                        m_messageQueueToChannel->push(RemoteTCPSink::MsgConfigureRemoteTCPSink::create(settings, {"gain"}, false));
                    }
                    break;
                }
                case RemoteTCPProtocol::setSampleBitDepth:
                {
                    int bits = RemoteTCPProtocol::extractUInt32(&cmd[1]);
                    qDebug() << "RemoteTCPSinkSink::processCommand: set sample bit depth " << bits;
                    settings.m_sampleBits = bits;
                    if (m_messageQueueToGUI) {
                        m_messageQueueToGUI->push(RemoteTCPSink::MsgConfigureRemoteTCPSink::create(settings, {"sampleBits"}, false));
                    }
                    if (m_messageQueueToChannel) {
                        m_messageQueueToChannel->push(RemoteTCPSink::MsgConfigureRemoteTCPSink::create(settings, {"sampleBits"}, false));
                    }
                    break;
                }
                case RemoteTCPProtocol::setIQSquelchEnabled:
                {
                    bool enabled = (bool) RemoteTCPProtocol::extractUInt32(&cmd[1]);
                    qDebug() << "RemoteTCPSinkSink::processCommand: set IQ squelch enabled " << enabled;
                    settings.m_squelchEnabled = enabled;
                    if (m_messageQueueToGUI) {
                        m_messageQueueToGUI->push(RemoteTCPSink::MsgConfigureRemoteTCPSink::create(settings, {"squelchEnabled"}, false));
                    }
                    if (m_messageQueueToChannel) {
                        m_messageQueueToChannel->push(RemoteTCPSink::MsgConfigureRemoteTCPSink::create(settings, {"squelchEnabled"}, false));
                    }
                    break;
                }
                case RemoteTCPProtocol::setIQSquelch:
                {
                    float squelch = RemoteTCPProtocol::extractFloat(&cmd[1]);
                    qDebug() << "RemoteTCPSinkSink::processCommand: set IQ squelch " << squelch;
                    settings.m_squelch = squelch;
                    if (m_messageQueueToGUI) {
                        m_messageQueueToGUI->push(RemoteTCPSink::MsgConfigureRemoteTCPSink::create(settings, {"squelch"}, false));
                    }
                    if (m_messageQueueToChannel) {
                        m_messageQueueToChannel->push(RemoteTCPSink::MsgConfigureRemoteTCPSink::create(settings, {"squelch"}, false));
                    }
                    break;
                }
                case RemoteTCPProtocol::setIQSquelchGate:
                {
                    float squelchGate = RemoteTCPProtocol::extractFloat(&cmd[1]);
                    qDebug() << "RemoteTCPSinkSink::processCommand: set IQ squelch gate " << squelchGate;
                    settings.m_squelchGate = squelchGate;
                    if (m_messageQueueToGUI) {
                        m_messageQueueToGUI->push(RemoteTCPSink::MsgConfigureRemoteTCPSink::create(settings, {"squelchGate"}, false));
                    }
                    if (m_messageQueueToChannel) {
                        m_messageQueueToChannel->push(RemoteTCPSink::MsgConfigureRemoteTCPSink::create(settings, {"squelchGate"}, false));
                    }
                    break;
                }
                default:
                    qDebug() << "RemoteTCPSinkSink::processCommand: unknown command " << cmd[0];
                    break;
                }
            }
        }
        else
        {
            qDebug() << "RemoteTCPSinkSink::processCommand: read only " << len << " bytes - Expecting " << sizeof(cmd);
        }
    }
}

void RemoteTCPSinkSink::sendCommand(RemoteTCPProtocol::Command cmdId, quint32 value)
{
    QMutexLocker mutexLocker(&m_mutex);
    quint8 cmd[5];

    cmd[0] = (quint8) cmdId;
    RemoteTCPProtocol::encodeUInt32(&cmd[1], value);

    for (const auto client : m_clients)
    {
        qint64 len = client->write((char *) cmd, sizeof(cmd));
        if (len != sizeof(cmd)) {
            qDebug() << "RemoteTCPSinkSink::sendCommand: Failed to write all of message:" << len;
        }
        m_bytesTransmitted += sizeof(cmd);
        client->flush();
    }
}

void RemoteTCPSinkSink::sendCommandFloat(RemoteTCPProtocol::Command cmdId, float value)
{
    QMutexLocker mutexLocker(&m_mutex);
    quint8 cmd[5];

    cmd[0] = (quint8) cmdId;
    RemoteTCPProtocol::encodeFloat(&cmd[1], value);

    for (const auto client : m_clients)
    {
        qint64 len = client->write((char *) cmd, sizeof(cmd));
        if (len != sizeof(cmd)) {
            qDebug() << "RemoteTCPSinkSink::sendCommand: Failed to write all of message:" << len;
        }
        m_bytesTransmitted += sizeof(cmd);
        client->flush();
    }
}

void RemoteTCPSinkSink::sendMessage(QHostAddress address, quint16 port, const QString& callsign, const QString& text, bool broadcast)
{
    qint64 len;
    char cmd[1+4+1];
    QByteArray callsignBytes = callsign.toUtf8();
    QByteArray textBytes = text.toUtf8();
    QByteArray bytes;

    bytes.append(callsignBytes);
    bytes.append('\0');
    bytes.append(textBytes);
    bytes.append('\0');

    cmd[0] = (char) RemoteTCPProtocol::sendMessage;
    RemoteTCPProtocol::encodeUInt32((quint8*) &cmd[1], bytes.size() + 1);
    cmd[5] = (char) broadcast;

    for (const auto client : m_clients)
    {
        bool addressMatch = (address == client->peerAddress()) && (port == client->peerPort());
        if ((broadcast && !addressMatch) || (!broadcast && addressMatch))
        {
            len = client->write(cmd, sizeof(cmd));
            if (len != sizeof(cmd)) {
                 qDebug() << "RemoteTCPSinkSink::sendMessage: Failed to write all of message header:" << len;
            }
            len = client->write(bytes.data(), bytes.size());
            if (len != bytes.size()) {
                 qDebug() << "RemoteTCPSinkSink::sendMessage: Failed to write all of message:" << len;
            }
            m_bytesTransmitted += sizeof(cmd) + bytes.size();
            client->flush();
            qDebug() << "RemoteTCPSinkSink::sendMessage:" << client->peerAddress() << client->peerPort() << text;
        }
    }
}

void RemoteTCPSinkSink::sendQueuePosition(Socket *client, int position)
{
    QString callsign = MainCore::instance()->getSettings().getStationName();

    sendMessage(client->peerAddress(), client->peerPort(), callsign, QString("Server busy. You are number %1 in the queue.").arg(position), false);
}

void RemoteTCPSinkSink::sendBlacklisted(Socket *client)
{
    char cmd[1+4];

    cmd[0] = (char) RemoteTCPProtocol::sendBlacklistedMessage;
    RemoteTCPProtocol::encodeUInt32((quint8*) &cmd[1], 0);

    qint64 len = client->write(cmd, sizeof(cmd));
    if (len != sizeof(cmd)) {
        qDebug() << "RemoteTCPSinkSink::sendBlacklisted: Failed to write all of message:" << len;
    }
    m_bytesTransmitted += sizeof(cmd);
    client->flush();
}

void RemoteTCPSinkSink::sendTimeLimit(Socket *client)
{
    QString callsign = MainCore::instance()->getSettings().getStationName();

    sendMessage(client->peerAddress(), client->peerPort(), callsign, "Time limit reached.", false);
}

void RemoteTCPSinkSink::sendPosition(float latitude, float longitude, float altitude)
{
    char msg[1+4+4+4+4];
    msg[0] = (char) RemoteTCPProtocol::dataPosition;
    RemoteTCPProtocol::encodeUInt32((quint8 *) &msg[1], 4+4+4);
    RemoteTCPProtocol::encodeFloat((quint8 *) &msg[1+4], latitude);
    RemoteTCPProtocol::encodeFloat((quint8 *) &msg[1+4+4], longitude);
    RemoteTCPProtocol::encodeFloat((quint8 *) &msg[1+4+4+4], altitude);

    int clients = std::min((int) m_clients.size(), m_settings.m_maxClients);
    for (int i = 0; i < clients; i++)
    {
        Socket *client = m_clients[i];
        client->write(msg, sizeof(msg));
        m_bytesTransmitted += sizeof(msg);
        client->flush();
    }
}

void RemoteTCPSinkSink::sendDirection(bool isotropic, float azimuth, float elevation)
{
    char msg[1+4+4+4+4];
    msg[0] = (char) RemoteTCPProtocol::dataDirection;
    RemoteTCPProtocol::encodeUInt32((quint8 *) &msg[1], 4+4+4);
    RemoteTCPProtocol::encodeUInt32((quint8 *) &msg[1+4], isotropic);
    RemoteTCPProtocol::encodeFloat((quint8 *) &msg[1+4+4], azimuth);
    RemoteTCPProtocol::encodeFloat((quint8 *) &msg[1+4+4+4], elevation);

    int clients = std::min((int) m_clients.size(), m_settings.m_maxClients);
    for (int i = 0; i < clients; i++)
    {
        Socket *client = m_clients[i];
        client->write(msg, sizeof(msg));
        m_bytesTransmitted += sizeof(msg);
        client->flush();
    }
}

void RemoteTCPSinkSink::sendPosition()
{
    float latitude = MainCore::instance()->getSettings().getLatitude();
    float longitude = MainCore::instance()->getSettings().getLongitude();
    float altitude = MainCore::instance()->getSettings().getAltitude();

    // Use device postion in preference to My Position
    ChannelWebAPIUtils::getDevicePosition(m_deviceIndex, latitude, longitude, altitude);

    sendPosition(latitude, longitude, altitude);
}

void RemoteTCPSinkSink::sendRotatorDirection(bool force)
{
    unsigned int rotatorFeatureSetIndex;
    unsigned int rotatorFeatureIndex;

    if (MainCore::getFeatureIndexFromId(m_settings.m_rotator, rotatorFeatureSetIndex, rotatorFeatureIndex))
    {
        double azimuth;
        double elevation;

        if (ChannelWebAPIUtils::getFeatureReportValue(rotatorFeatureSetIndex, rotatorFeatureIndex, "currentAzimuth", azimuth)
            && ChannelWebAPIUtils::getFeatureReportValue(rotatorFeatureSetIndex, rotatorFeatureIndex, "currentElevation", elevation))
        {
            if (force || ((azimuth != m_azimuth) || (elevation != m_elevation)))
            {
                sendDirection(false, (float) azimuth, (float) elevation);
                m_azimuth = azimuth;
                m_elevation = elevation;
            }
        }
    }
}

void RemoteTCPSinkSink::preferenceChanged(int elementType)
{
    Preferences::ElementType pref = (Preferences::ElementType)elementType;

    if ((pref == Preferences::Latitude) || (pref == Preferences::Longitude) || (pref == Preferences::Altitude)) {
        sendPosition();
    }
}

// Poll for changes to device settings - FIXME: Need a signal from DeviceAPI - reverseAPI?
void RemoteTCPSinkSink::checkDeviceSettings()
{
    if ((m_settings.m_protocol != RemoteTCPSinkSettings::RTL0) && !m_settings.m_iqOnly)
    {
        // Forward device settings to clients if they've changed

        double centerFrequency;
        if (ChannelWebAPIUtils::getCenterFrequency(m_deviceIndex, centerFrequency))
        {
            if (centerFrequency != m_centerFrequency)
            {
                m_centerFrequency = centerFrequency;
                sendCommand(RemoteTCPProtocol::setCenterFrequency, m_centerFrequency);
            }
        }

        int ppmCorrection;
        if (ChannelWebAPIUtils::getLOPpmCorrection(m_deviceIndex, ppmCorrection))
        {
            if (ppmCorrection != m_ppmCorrection)
            {
                m_ppmCorrection = ppmCorrection;
                sendCommand(RemoteTCPProtocol::setFrequencyCorrection, m_ppmCorrection);
            }
        }

        int biasTeeEnabled;
        if (ChannelWebAPIUtils::getBiasTee(m_deviceIndex, biasTeeEnabled))
        {
            if (biasTeeEnabled != m_biasTeeEnabled)
            {
                m_biasTeeEnabled = biasTeeEnabled;
                sendCommand(RemoteTCPProtocol::setBiasTee, m_biasTeeEnabled);
            }
        }

        int directSampling;
        if (ChannelWebAPIUtils::getDeviceSetting(m_deviceIndex, "noModMode", directSampling))
        {
            if (directSampling != m_directSampling)
            {
                m_directSampling = directSampling;
                sendCommand(RemoteTCPProtocol::setDirectSampling, m_directSampling);
            }
        }

        int agc;
        if (ChannelWebAPIUtils::getAGC(m_deviceIndex, agc))
        {
            if (agc != m_agc)
            {
                m_agc = agc;
                sendCommand(RemoteTCPProtocol::setAGCMode, m_agc);
            }
        }

        int dcOffsetRemoval;
        if (ChannelWebAPIUtils::getDCOffsetRemoval(m_deviceIndex, dcOffsetRemoval))
        {
            if (dcOffsetRemoval != m_dcOffsetRemoval)
            {
                m_dcOffsetRemoval = dcOffsetRemoval;
                sendCommand(RemoteTCPProtocol::setDCOffsetRemoval, m_dcOffsetRemoval);
            }
        }

        int iqCorrection;
        if (ChannelWebAPIUtils::getIQCorrection(m_deviceIndex, iqCorrection))
        {
            if (iqCorrection != m_iqCorrection)
            {
                m_iqCorrection = iqCorrection;
                sendCommand(RemoteTCPProtocol::setIQCorrection, m_iqCorrection);
            }
        }

        qint32 devSampleRate;
        if (ChannelWebAPIUtils::getDevSampleRate(m_deviceIndex, devSampleRate))
        {
            if (devSampleRate != m_devSampleRate)
            {
                m_devSampleRate = devSampleRate;
                sendCommand(RemoteTCPProtocol::setSampleRate, m_devSampleRate);
            }
        }

        qint32 log2Decim;
        if (ChannelWebAPIUtils::getSoftDecim(m_deviceIndex, log2Decim))
        {
            if (log2Decim != m_log2Decim)
            {
                m_log2Decim = log2Decim;
                sendCommand(RemoteTCPProtocol::setDecimation, m_log2Decim);
            }
        }


        qint32 rfBW;
        if (ChannelWebAPIUtils::getRFBandwidth(m_deviceIndex, rfBW))
        {
            if (rfBW != m_rfBW)
            {
                m_rfBW = rfBW;
                sendCommand(RemoteTCPProtocol::setTunerBandwidth, m_rfBW);
            }
        }

        for (int i = 0; i < 4; i++)
        {
            qint32 gain;
            if (ChannelWebAPIUtils::getGain(m_deviceIndex, i, gain))
            {
                if (gain != m_gain[i])
                {
                    m_gain[i] = gain;
                    if (i == 0)
                    {
                        sendCommand(RemoteTCPProtocol::setTunerGain, gain);
                    }
                    else
                    {
                        int v = (gain & 0xffff) | (i << 16);
                        sendCommand(RemoteTCPProtocol::setTunerIFGain, v);
                    }
                }
            }
        }

        if (!m_settings.m_isotropic && !m_settings.m_rotator.isEmpty() && (m_settings.m_rotator != "None")) {
            sendRotatorDirection(false);
        }

    }
}
