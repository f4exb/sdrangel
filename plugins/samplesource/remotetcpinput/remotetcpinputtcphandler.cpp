///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022-2024 Jon Beniston, M7RCE <jon@beniston.com>                //
// Copyright (C) 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#include <QDebug>
#include <cstring>

#include "device/deviceapi.h"
#include "util/message.h"
#include "maincore.h"

#include "remotetcpinputtcphandler.h"
#include "remotetcpinput.h"
#include "../../channelrx/remotetcpsink/remotetcpprotocol.h"

MESSAGE_CLASS_DEFINITION(RemoteTCPInputTCPHandler::MsgReportRemoteDevice, Message)
MESSAGE_CLASS_DEFINITION(RemoteTCPInputTCPHandler::MsgReportConnection, Message)
MESSAGE_CLASS_DEFINITION(RemoteTCPInputTCPHandler::MsgConfigureTcpHandler, Message)

RemoteTCPInputTCPHandler::RemoteTCPInputTCPHandler(SampleSinkFifo *sampleFifo, DeviceAPI *deviceAPI, ReplayBuffer<FixReal> *replayBuffer) :
    m_deviceAPI(deviceAPI),
    m_running(false),
    m_dataSocket(nullptr),
    m_tcpSocket(nullptr),
    m_webSocket(nullptr),
    m_tcpBuf(nullptr),
    m_sampleFifo(sampleFifo),
	m_replayBuffer(replayBuffer),
    m_messageQueueToInput(nullptr),
    m_messageQueueToGUI(nullptr),
    m_fillBuffer(true),
    m_timer(this),
    m_reconnectTimer(this),
    m_sdra(false),
    m_converterBuffer(nullptr),
    m_converterBufferNbSamples(0),
    m_settings(),
    m_remoteControl(true),
    m_iqOnly(false),
    m_decoder(nullptr),
    m_zOutBuf(m_zBufSize, '\0'),
    m_blacklisted(false),
    m_magsq(0.0f),
    m_magsqSum(0.0f),
    m_magsqPeak(0.0f),
    m_magsqCount(0)
{
    m_sampleFifo->setSize(5000000); // Start with large FIFO, to avoid having to resize
    m_tcpBuf = new char[m_sampleFifo->size()*2*4];
    m_timer.setInterval(50); // Previously 125, but this results in an obviously slow spectrum refresh rate
    connect(&m_reconnectTimer, SIGNAL(timeout()), this, SLOT(reconnect()));
    m_reconnectTimer.setSingleShot(true);

    // Initialise zlib decompressor
    m_zStream.zalloc = nullptr;
    m_zStream.zfree = nullptr;
    m_zStream.opaque = nullptr;
    m_zStream.avail_in = 0;
    m_zStream.next_in = nullptr;
    if (Z_OK != inflateInit(&m_zStream)) {
        qDebug() << "RemoteTCPInputTCPHandler::RemoteTCPInputTCPHandler: inflateInit failed.";
    }
}

RemoteTCPInputTCPHandler::~RemoteTCPInputTCPHandler()
{
    qDebug() << "RemoteTCPInputTCPHandler::~RemoteTCPInputTCPHandler";
    delete[] m_tcpBuf;
    if (m_converterBuffer) {
        delete[] m_converterBuffer;
    }
    cleanup();
}

void RemoteTCPInputTCPHandler::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_inputMessageQueue.clear();
    m_blacklisted = false;
}

// start() is called from DSPDeviceSourceEngine thread
// QTcpSockets need to be created on same thread they are used from, so only create it in started()
void RemoteTCPInputTCPHandler::start()
{
    QMutexLocker mutexLocker(&m_mutex);

    qDebug("RemoteTCPInputTCPHandler::start");

    if (m_running) {
        return;
    }
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    connect(thread(), SIGNAL(started()), this, SLOT(started()));
    connect(thread(), SIGNAL(finished()), this, SLOT(finished()));

    m_running = true;
}

void RemoteTCPInputTCPHandler::stop()
{
    QMutexLocker mutexLocker(&m_mutex);

    qDebug("RemoteTCPInputTCPHandler::stop");

    disconnect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

void RemoteTCPInputTCPHandler::started()
{
    QMutexLocker mutexLocker(&m_mutex);

    // Don't connectToHost until we get settings
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(processData()));

    disconnect(thread(), SIGNAL(started()), this, SLOT(started()));
}

void RemoteTCPInputTCPHandler::finished()
{
    qDebug("RemoteTCPInputTCPHandler::finished");
    QMutexLocker mutexLocker(&m_mutex);
    m_timer.stop();
    disconnect(&m_timer, SIGNAL(timeout()), this, SLOT(processData()));
    //disconnectFromHost();
    cleanup();
    disconnect(thread(), SIGNAL(finished()), this, SLOT(finished()));
    m_running = false;
}

void RemoteTCPInputTCPHandler::connectToHost(const QString& address, quint16 port, const QString& protocol)
{
    qDebug("RemoteTCPInputTCPHandler::connectToHost: connect to %s %s:%d", protocol.toStdString().c_str(), address.toStdString().c_str(), port);
    m_fillBuffer = true;
    m_readMetaData = false;
    if (protocol == "SDRangel wss")
    {
        m_webSocket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
        connect(m_webSocket, &QWebSocket::binaryFrameReceived, this, &RemoteTCPInputTCPHandler::dataReadyRead);
        connect(m_webSocket, &QWebSocket::connected, this, &RemoteTCPInputTCPHandler::connected);
        connect(m_webSocket, &QWebSocket::disconnected, this, &RemoteTCPInputTCPHandler::disconnected);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        connect(m_webSocket, &QWebSocket::errorOccurred, this, &RemoteTCPInputTCPHandler::errorOccurred);
#endif
        connect(m_webSocket, &QWebSocket::sslErrors, this, &RemoteTCPInputTCPHandler::sslErrors);
        m_webSocket->open(QUrl(QString("wss://%1:%2").arg(address).arg(port)));
        m_dataSocket = new WebSocket(m_webSocket);
    }
    else
    {
        m_tcpSocket = new QTcpSocket(this);
        connect(m_tcpSocket, SIGNAL(readyRead()), this, SLOT(dataReadyRead()));
        connect(m_tcpSocket, SIGNAL(connected()), this, SLOT(connected()));
        connect(m_tcpSocket, SIGNAL(disconnected()), this, SLOT(disconnected()));
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
        connect(m_tcpSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error), this, &RemoteTCPInputTCPHandler::errorOccurred);
#else
        connect(m_tcpSocket, &QAbstractSocket::errorOccurred, this, &RemoteTCPInputTCPHandler::errorOccurred);
#endif
        m_tcpSocket->connectToHost(address, port);
        m_dataSocket = new TCPSocket(m_tcpSocket);
    }
}

/*void RemoteTCPInputTCPHandler::disconnectFromHost()
{
    if (m_dataSocket)
    {
        qDebug() << "RemoteTCPInputTCPHandler::disconnectFromHost";
        disconnect(m_dataSocket, SIGNAL(readyRead()), this, SLOT(dataReadyRead()));
        disconnect(m_dataSocket, SIGNAL(connected()), this, SLOT(connected()));
        disconnect(m_dataSocket, SIGNAL(disconnected()), this, SLOT(disconnected()));
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
        disconnect(m_dataSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error), this, &RemoteTCPInputTCPHandler::errorOccurred);
#else
        disconnect(m_dataSocket, &QAbstractSocket::errorOccurred, this, &RemoteTCPInputTCPHandler::errorOccurred);
#endif
        //m_dataSocket->disconnectFromHost();
        cleanup();
    }
}*/

void RemoteTCPInputTCPHandler::cleanup()
{
    if (m_decoder)
    {
        FLAC__stream_decoder_delete(m_decoder);
        m_decoder = nullptr;
    }
    if (m_webSocket)
    {
        qDebug() << "RemoteTCPInputTCPHandler::cleanup: Closing and deleting web socket";
        disconnect(m_webSocket, &QWebSocket::binaryFrameReceived, this, &RemoteTCPInputTCPHandler::dataReadyRead);
        disconnect(m_webSocket, &QWebSocket::connected, this, &RemoteTCPInputTCPHandler::connected);
        disconnect(m_webSocket, &QWebSocket::disconnected, this, &RemoteTCPInputTCPHandler::disconnected);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        disconnect(m_webSocket, &QWebSocket::errorOccurred, this, &RemoteTCPInputTCPHandler::errorOccurred);
#endif
    }
    if (m_tcpSocket)
    {
        qDebug() << "RemoteTCPInputTCPHandler::cleanup: Closing and deleting TCP socket";
        // Disconnect disconnected, so don't get called recursively
        disconnect(m_tcpSocket, SIGNAL(readyRead()), this, SLOT(dataReadyRead()));
        disconnect(m_tcpSocket, SIGNAL(connected()), this, SLOT(connected()));
        disconnect(m_tcpSocket, SIGNAL(disconnected()), this, SLOT(disconnected()));
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
        disconnect(m_tcpSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error), this, &RemoteTCPInputTCPHandler::errorOccurred);
#else
        disconnect(m_tcpSocket, &QAbstractSocket::errorOccurred, this, &RemoteTCPInputTCPHandler::errorOccurred);
#endif
    }
    if (m_dataSocket)
    {
        m_dataSocket->close();
        m_dataSocket->deleteLater();
        m_dataSocket = nullptr;
    }
    if (m_webSocket)
    {
        m_webSocket->deleteLater();
        m_webSocket = nullptr;
    }
    if (m_tcpSocket)
    {
        m_tcpSocket->deleteLater();
        m_tcpSocket = nullptr;
    }
}

// Clear input buffer when settings change that invalidate the data in it
// E.g. sample rate or bit depth
void RemoteTCPInputTCPHandler::clearBuffer()
{
    if (m_dataSocket && m_readMetaData)
    {
        if (m_spyServer)
        {
            // Can't just flush buffer, otherwise we'll lose header sync
            // Read and throw away any available data
            processSpyServerData(m_dataSocket->bytesAvailable(), true);
            m_fillBuffer = true;
        }
        else
        {
            m_dataSocket->flush();
            if (!m_decoder) { // Can't throw away FLAC header
                m_dataSocket->readAll();
                m_fillBuffer = true;
            }
        }
    }
}

void RemoteTCPInputTCPHandler::sendCommand(RemoteTCPProtocol::Command cmd, quint32 value)
{
    QMutexLocker mutexLocker(&m_mutex);
    quint8 request[5];

    request[0] = (quint8) cmd;
    RemoteTCPProtocol::encodeUInt32(&request[1], value);
    if (m_dataSocket)
    {
        qint64 len = m_dataSocket->write((char*)request, sizeof(request));
        if (len != sizeof(request)) {
            qDebug() << "RemoteTCPInputTCPHandler::sendCommand: Failed to write all of request:" << len;
        }
    } else {
        qDebug() << "RemoteTCPInputTCPHandler::sendCommand: No socket";
    }
}

void RemoteTCPInputTCPHandler::sendCommandFloat(RemoteTCPProtocol::Command cmd, float value)
{
    QMutexLocker mutexLocker(&m_mutex);
    quint8 request[5];

    request[0] = (quint8) cmd;
    RemoteTCPProtocol::encodeFloat(&request[1], value);
    if (m_dataSocket)
    {
        qint64 len = m_dataSocket->write((char*)request, sizeof(request));
        if (len != sizeof(request)) {
            qDebug() << "RemoteTCPInputTCPHandler::sendCommand: Failed to write all of request:" << len;
        }
    } else {
        qDebug() << "RemoteTCPInputTCPHandler::sendCommand: No socket";
    }
}

void RemoteTCPInputTCPHandler::setSampleRate(int sampleRate)
{
    sendCommand(RemoteTCPProtocol::setSampleRate, sampleRate);
}

void RemoteTCPInputTCPHandler::setCenterFrequency(quint64 frequency)
{
    sendCommand(RemoteTCPProtocol::setCenterFrequency, frequency); // FIXME: Can't support >4GHz
}

void RemoteTCPInputTCPHandler::setTunerAGC(bool agc)
{
    sendCommand(RemoteTCPProtocol::setTunerGainMode, agc);
}

void RemoteTCPInputTCPHandler::setTunerGain(int gain)
{
    sendCommand(RemoteTCPProtocol::setTunerGain, gain);
}

void RemoteTCPInputTCPHandler::setGainByIndex(int index)
{
    sendCommand(RemoteTCPProtocol::setGainByIndex, index);
}

void RemoteTCPInputTCPHandler::setFreqCorrection(int correction)
{
    sendCommand(RemoteTCPProtocol::setFrequencyCorrection, correction);
}

void RemoteTCPInputTCPHandler::setIFGain(quint16 stage, quint16 gain)
{
    sendCommand(RemoteTCPProtocol::setTunerIFGain, (stage << 16) | gain);
}

void RemoteTCPInputTCPHandler::setAGC(bool agc)
{
    sendCommand(RemoteTCPProtocol::setAGCMode, agc);
}

void RemoteTCPInputTCPHandler::setDirectSampling(bool enabled)
{
    sendCommand(RemoteTCPProtocol::setDirectSampling, enabled ? 3 : 0);
}

void RemoteTCPInputTCPHandler::setDCOffsetRemoval(bool enabled)
{
    sendCommand(RemoteTCPProtocol::setDCOffsetRemoval, enabled);
}

void RemoteTCPInputTCPHandler::setIQCorrection(bool enabled)
{
    sendCommand(RemoteTCPProtocol::setIQCorrection, enabled);
}

void RemoteTCPInputTCPHandler::setBiasTee(bool enabled)
{
    sendCommand(RemoteTCPProtocol::setBiasTee, enabled);
}

void RemoteTCPInputTCPHandler::setBandwidth(int bandwidth)
{
    sendCommand(RemoteTCPProtocol::setTunerBandwidth, bandwidth);
}

void RemoteTCPInputTCPHandler::setDecimation(int dec)
{
    sendCommand(RemoteTCPProtocol::setDecimation, dec);
}

void RemoteTCPInputTCPHandler::setChannelSampleRate(int sampleRate)
{
    sendCommand(RemoteTCPProtocol::setChannelSampleRate, sampleRate);
}

void RemoteTCPInputTCPHandler::setChannelFreqOffset(int offset)
{
    sendCommand(RemoteTCPProtocol::setChannelFreqOffset, offset);
}

void RemoteTCPInputTCPHandler::setChannelGain(int gain)
{
    sendCommand(RemoteTCPProtocol::setChannelGain, gain);
}

void RemoteTCPInputTCPHandler::setSampleBitDepth(int sampleBits)
{
    sendCommand(RemoteTCPProtocol::setSampleBitDepth, sampleBits);
}

void RemoteTCPInputTCPHandler::setSquelchEnabled(bool enabled)
{
    sendCommand(RemoteTCPProtocol::setIQSquelchEnabled, (quint32) enabled);
}

void RemoteTCPInputTCPHandler::setSquelch(float squelch)
{
    sendCommandFloat(RemoteTCPProtocol::setIQSquelch, squelch);
}

void RemoteTCPInputTCPHandler::setSquelchGate(float squelchGate)
{
    sendCommandFloat(RemoteTCPProtocol::setIQSquelchGate, squelchGate);
}

void RemoteTCPInputTCPHandler::sendMessage(const QString& callsign, const QString& text, bool broadcast)
{
    QMutexLocker mutexLocker(&m_mutex);

    if (m_dataSocket)
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

        len = m_dataSocket->write(&cmd[0], sizeof(cmd));
        if (len != sizeof(cmd)) {
            qDebug() << "RemoteTCPInputTCPHandler::set: Failed to write all of message header:" << len;
        }
        len = m_dataSocket->write(bytes.data(), bytes.size());
        if (len != bytes.size()) {
            qDebug() << "RemoteTCPInputTCPHandler::set: Failed to write all of message:" << len;
        }
        m_dataSocket->flush();
        qDebug() << "sendMessage" << text;
    } else {
        qDebug() << "RemoteTCPInputTCPHandler::sendMessage: No socket";
    }
}

void RemoteTCPInputTCPHandler::spyServerConnect()
{
    QMutexLocker mutexLocker(&m_mutex);

    quint8 request[8+4+9];
    SpyServerProtocol::encodeUInt32(&request[0], 0);
    SpyServerProtocol::encodeUInt32(&request[4], 4+9);
    SpyServerProtocol::encodeUInt32(&request[8], SpyServerProtocol::ProtocolID);
    memcpy(&request[8+4], "SDRangel", 9);
    if (m_dataSocket)
    {
        m_dataSocket->write((char*)request, sizeof(request));
        m_dataSocket->flush();
    }
}

void RemoteTCPInputTCPHandler::spyServerSet(int setting, int value)
{
    QMutexLocker mutexLocker(&m_mutex);

    quint8 request[8+8];
    SpyServerProtocol::encodeUInt32(&request[0], 2);
    SpyServerProtocol::encodeUInt32(&request[4], 8);
    SpyServerProtocol::encodeUInt32(&request[8], setting);
    SpyServerProtocol::encodeUInt32(&request[12], value);
    if (m_dataSocket)
    {
        m_dataSocket->write((char*)request, sizeof(request));
        m_dataSocket->flush();
    }
}

void RemoteTCPInputTCPHandler::spyServerSetIQFormat(int sampleBits)
{
    quint32 format;

    if (sampleBits == 8) {
        format = 1;
    } else if (sampleBits == 16) {
        format = 2;
    } else if (sampleBits == 24) {
        format = 3;
    } else if (sampleBits == 32) {
        format = 4; // This is float
    } else {
        qDebug() << "RemoteTCPInputTCPHandler::spyServerSetIQFormat: Unsupported value" << sampleBits;
        format = 1;
    }
    spyServerSet(SpyServerProtocol::setIQFormat, format);
}

void RemoteTCPInputTCPHandler::spyServerSetStreamIQ()
{
    spyServerSetIQFormat(m_settings.m_sampleBits);
    spyServerSet(SpyServerProtocol::setStreamingMode, 1);       // Stream IQ only
    spyServerSet(SpyServerProtocol::setStreamingEnabled, 1);    // Enable streaming
}

void RemoteTCPInputTCPHandler::applySettings(const RemoteTCPInputSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "RemoteTCPInputTCPHandler::applySettings: "
                << "force: " << force
                << settings.getDebugString(settingsKeys, force);
    QMutexLocker mutexLocker(&m_mutex);

    if (m_spyServer)
    {
        if (settingsKeys.contains("centerFrequency") || force) {
            spyServerSet(SpyServerProtocol::setCenterFrequency, settings.m_centerFrequency);
        }
        if ((settings.m_channelSampleRate != m_settings.m_channelSampleRate) || force)
        {
            // Resize FIFO to give us 1 second
            if ((settingsKeys.contains("channelSampleRate") || force) && (settings.m_channelSampleRate > (qint32)m_sampleFifo->size()))
            {
                qDebug() << "RemoteTCPInputTCPHandler::applySettings: Resizing sample FIFO from " << m_sampleFifo->size() << "to" << settings.m_channelSampleRate;
                m_sampleFifo->setSize(settings.m_channelSampleRate);
                delete[] m_tcpBuf;
                m_tcpBuf = new char[m_sampleFifo->size()*2*4];
                m_fillBuffer = true; // So we reprime FIFO
            }
            // Protocol only seems to allow changing decimation
            //spyServerSet(SpyServerProtocol::???, settings.m_channelSampleRate);
            clearBuffer();
        }
        if (settingsKeys.contains("sampleBits") || force)
        {
            spyServerSetIQFormat(settings.m_sampleBits);
            clearBuffer();
        }
        if (settingsKeys.contains("log2Decim") || force)
        {
            spyServerSet(SpyServerProtocol::setIQDecimation, settings.m_log2Decim);
            clearBuffer();
        }
        if (settingsKeys.contains("gain[0]") || force)
        {
            spyServerSet(SpyServerProtocol::setGain, settings.m_gain[0] / 10); // Convert 10ths dB to index
        }
    }
    else
    {
        if (settingsKeys.contains("centerFrequency") || force) {
            setCenterFrequency(settings.m_centerFrequency);
        }
        if (settingsKeys.contains("loPpmCorrection") || force) {
            setFreqCorrection(settings.m_loPpmCorrection);
        }
        if (settingsKeys.contains("dcBlock") || force) {
            if (m_sdra) {
                setDCOffsetRemoval(settings.m_dcBlock);
            }
        }
        if (settingsKeys.contains("iqCorrection") || force) {
            if (m_sdra) {
                setIQCorrection(settings.m_iqCorrection);
            }
        }
        if (settingsKeys.contains("biasTee") || force) {
            setBiasTee(settings.m_biasTee);
        }
        if (settingsKeys.contains("directSampling") || force) {
            setDirectSampling(settings.m_directSampling);
        }
        if (settingsKeys.contains("log2Decim") || force) {
            if (m_sdra) {
                setDecimation(settings.m_log2Decim);
            }
        }
        if (settingsKeys.contains("devSampleRate") || force) {
            setSampleRate(settings.m_devSampleRate);
        }
        if (settingsKeys.contains("agc") || force) {
            setAGC(settings.m_agc);
        }
        if (force) {
            setTunerAGC(1); // The SDRangel RTLSDR driver always has tuner gain as manual
        }
        if (settingsKeys.contains("gain[0]") || force) {
            setTunerGain(settings.m_gain[0]);
        }
        for (int i = 1; i < 3; i++)
        {
            if (settingsKeys.contains(QString("gain[%1]").arg(i)) || force) {
                setIFGain(i, settings.m_gain[i]);
            }
        }
        if (settingsKeys.contains("rfBW") || force) {
            setBandwidth(settings.m_rfBW);
        }
        if (settingsKeys.contains("inputFrequencyOffset") || force) {
            if (m_sdra) {
                setChannelFreqOffset(settings.m_inputFrequencyOffset);
            }
        }
        if (settingsKeys.contains("channelGain") || force) {
            if (m_sdra) {
                setChannelGain(settings.m_channelGain);
            }
        }
        if ((settings.m_channelSampleRate != m_settings.m_channelSampleRate) || force)
        {
            // Resize FIFO to give us 1 second
            if ((settingsKeys.contains("channelSampleRate") || force) && (settings.m_channelSampleRate > (qint32)m_sampleFifo->size()))
            {
                qDebug() << "RemoteTCPInputTCPHandler::applySettings: Resizing sample FIFO from " << m_sampleFifo->size() << "to" << settings.m_channelSampleRate;
                m_sampleFifo->setSize(settings.m_channelSampleRate);
                delete[] m_tcpBuf;
                m_tcpBuf = new char[m_sampleFifo->size()*2*4];
                m_fillBuffer = true; // So we reprime FIFO
            }
            if (m_sdra) {
                setChannelSampleRate(settings.m_channelSampleRate);
            }
            clearBuffer();
        }
        if (settingsKeys.contains("sampleBits") || force)
        {
            if (m_sdra) {
                setSampleBitDepth(settings.m_sampleBits);
            }
            clearBuffer();
        }
        if (settingsKeys.contains("squelchEnabled") || force)
        {
            if (m_sdra) {
                setSquelchEnabled(settings.m_squelchEnabled);
            }
        }
        if (settingsKeys.contains("squelch") || force)
        {
            if (m_sdra) {
                setSquelch(settings.m_squelch);
            }
        }
        if (settingsKeys.contains("squelchGate") || force)
        {
            if (m_sdra) {
                setSquelchGate(settings.m_squelchGate);
            }
        }
    }

    if (m_dataSocket) {
        m_dataSocket->flush(); // Apparently needed for WebAssembly with proxy
    }

    // Don't use force, as disconnect can cause rtl_tcp to quit
    if (settingsKeys.contains("dataAddress")
        || settingsKeys.contains("dataPort")
        || ((m_dataSocket == nullptr) && !m_blacklisted))
    {
        //disconnectFromHost();
        cleanup();
        connectToHost(settings.m_dataAddress, settings.m_dataPort, settings.m_protocol);
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

static FLAC__StreamDecoderReadStatus flacReadCallback(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *clientData)
{
    RemoteTCPInputTCPHandler *handler = (RemoteTCPInputTCPHandler *) clientData;

    return handler->flacRead(decoder, buffer, bytes);
}

static FLAC__StreamDecoderWriteStatus flacWriteCallback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 *const buffer[], void *clientData)
{
    RemoteTCPInputTCPHandler *handler = (RemoteTCPInputTCPHandler *) clientData;

    return handler->flacWrite(decoder, frame, buffer);
}

static void flacErrorCallback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *clientData)
{
    RemoteTCPInputTCPHandler *handler = (RemoteTCPInputTCPHandler *) clientData;

    return handler->flacError(decoder, status);
}

FLAC__StreamDecoderReadStatus RemoteTCPInputTCPHandler::flacRead(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes)
{
    (void) decoder;

    qsizetype bytesRequested = *bytes;
    qsizetype bytesRead = std::min(bytesRequested, (qsizetype) m_compressedData.size());

    memcpy(buffer, m_compressedData.constData(), bytesRead);
    m_compressedData.remove(0, bytesRead);

    if (bytesRead == 0)
    {
        qDebug() << "RemoteTCPInputTCPHandler::flacRead: Decoder will hang if we can't return data";
        abort();
    }

    *bytes = (size_t) bytesRead;
    return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

FIFO::FIFO(qsizetype elements)
{
    m_data.resize(elements);
    clear();
}

qsizetype FIFO::write(quint8 *data, qsizetype elements)
{
    qsizetype writeCount = std::min(elements, m_data.size() - m_fill);
    qsizetype remaining = m_data.size() - m_writePtr;
    qsizetype len2 = writeCount - remaining;

    //qDebug() << "write" << write << remaining << len2;

    if (len2 < 0)
    {
        std::memcpy(&m_data.data()[m_writePtr], data, writeCount);
        m_writePtr += writeCount;
    }
    else if (len2 == 0)
    {
        std::memcpy(&m_data.data()[m_writePtr], data, writeCount);
        m_writePtr = 0;
    }
    else
    {
        std::memcpy(&m_data.data()[m_writePtr], data, remaining);
        std::memcpy(&m_data.data()[0], &data[remaining], len2);
        m_writePtr = len2;
    }

    m_fill += writeCount;

    return writeCount;
}

qsizetype FIFO::read(quint8 *data, qsizetype elements)
{
    qsizetype readCount = std::min(elements, m_fill);
    qsizetype remaining = m_data.size() - m_readPtr;
    qsizetype len2 = readCount - remaining;

   // qDebug() << "read" << read << remaining << len2;

    if (len2 < 0)
    {
        std::memcpy(data, &m_data.data()[m_readPtr], readCount);
        m_readPtr += readCount;
    }
    else if (len2 == 0)
    {
        std::memcpy(data, &m_data.data()[m_readPtr], readCount);
        m_readPtr = 0;
    }
    else
    {
        std::memcpy(&data[0], &m_data.data()[m_readPtr], remaining);
        std::memcpy(&data[remaining], &m_data.data()[0], len2);
        m_readPtr = len2;
    }

    m_fill -= readCount;

    return readCount;
}

qsizetype FIFO::readPtr(quint8 **data, qsizetype elements)
{
    *data = (quint8 *) &m_data.data()[m_readPtr];

    return std::min(elements, m_data.size() - m_readPtr);
}

void FIFO::read(qsizetype elements)
{
    m_readPtr = (m_readPtr + elements) % m_data.size();
    m_fill -= elements;
    if (m_fill < 0)
    {
        qDebug() << "FIFO::read: Underrun";
        m_fill = 0;
    }
}

void FIFO::resize(qsizetype elements)
{
    m_data.resize(elements);
    m_data.squeeze();
}

void FIFO::clear()
{
    m_writePtr = 0;
    m_readPtr = 0;
    m_fill = 0;
}

void RemoteTCPInputTCPHandler::calcPower(const Sample *iq, int nbSamples)
{
    for (int i = 0; i < nbSamples; i++)
    {
        Real re = iq[i].real();// SDR_RX_SCALED;
        Real im = iq[i].imag();// SDR_RX_SCALED;

        Real magsq = (re*re + im*im) / (SDR_RX_SCALED*SDR_RX_SCALED);
        m_movingAverage(magsq);
        m_magsq = m_movingAverage.asDouble();
        m_magsqSum += magsq;
        m_magsqPeak = std::max<double>(magsq, m_magsqPeak);
        m_magsqCount++;
    }
}

FLAC__StreamDecoderWriteStatus RemoteTCPInputTCPHandler::flacWrite(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 *const buffer[])
{
    (void) decoder;

    m_uncompressedFrames++;

    int nbSamples = frame->header.blocksize;
    if (nbSamples > (int) m_converterBufferNbSamples)
    {
        if (m_converterBuffer) {
            delete[] m_converterBuffer;
        }
        m_converterBuffer = new int32_t[nbSamples*2];
    }

    // Convert and interleave samples and output to FIFO
    if ((frame->header.bits_per_sample == 8) && (SDR_RX_SAMP_SZ == 24) && (frame->header.channels == 2))
    {
        qint32 *out = (qint32 *)m_converterBuffer;
        const qint32 *inI = buffer[0];
        const qint32 *inQ = buffer[1];

        for (int i = 0; i < nbSamples; i++)
        {
            *out++ = *inI++ << 16;
            *out++ = *inQ++ << 16;
        }
        m_uncompressedData.write(reinterpret_cast<quint8*>(m_converterBuffer), nbSamples*sizeof(Sample));
    }
    else if ((frame->header.bits_per_sample == 16) && (SDR_RX_SAMP_SZ == 24) && (frame->header.channels == 2))
    {
        qint32 *out = (qint32 *)m_converterBuffer;
        const qint32 *inI = buffer[0];
        const qint32 *inQ = buffer[1];

        for (int i = 0; i < nbSamples; i++)
        {
            *out++ = *inI++ << 8;
            *out++ = *inQ++ << 8;
        }
        m_uncompressedData.write(reinterpret_cast<quint8*>(m_converterBuffer), nbSamples*sizeof(Sample));
    }
    else if ((frame->header.bits_per_sample == 24) && (SDR_RX_SAMP_SZ == 24) && (frame->header.channels == 2))
    {
        qint32 *out = (qint32 *)m_converterBuffer;
        const qint32 *inI = buffer[0];
        const qint32 *inQ = buffer[1];

        for (int i = 0; i < nbSamples; i++)
        {
            *out++ = *inI++;
            *out++ = *inQ++;
        }
        m_uncompressedData.write(reinterpret_cast<quint8*>(m_converterBuffer), nbSamples*sizeof(Sample));
    }
    else if ((frame->header.bits_per_sample == 32) && (SDR_RX_SAMP_SZ == 24) && (frame->header.channels == 2))
    {
        qint32 *out = (qint32 *)m_converterBuffer;
        const qint32 *inI = buffer[0];
        const qint32 *inQ = buffer[1];

        for (int i = 0; i < nbSamples; i++)
        {
            *out++ = *inI++;
            *out++ = *inQ++;
        }
        m_uncompressedData.write(reinterpret_cast<quint8*>(m_converterBuffer), nbSamples*sizeof(Sample));
    }
    else
    {
        qDebug() << "RemoteTCPInputTCPHandler::flacWrite: Unsupported format";
    }

    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}


// Convert from zlib uncompressed network format to Samples, to uncompressed data FIFO
void RemoteTCPInputTCPHandler::processDecompressedZlibData(const char *inBuf, int nbSamples)
{
    // Ensure conversion buffer is large enough - FIXME: Don't use this buffer - just write in to FIFO
    if (nbSamples > (int) m_converterBufferNbSamples)
    {
        if (m_converterBuffer) {
            delete[] m_converterBuffer;
        }
        m_converterBuffer = new int32_t[nbSamples*2];
    }

    // Convert from network format to Sample
    if ((m_settings.m_sampleBits == 8) && (SDR_RX_SAMP_SZ == 16))
    {
        const quint8 *in = (const quint8 *) inBuf;
        qint16 *out = (qint16 *) m_converterBuffer;

        for (int is = 0; is < nbSamples*2; is++) {
            out[is] = (((qint16)in[is]) - 128) << 8;
        }
    }
    else if ((m_settings.m_sampleBits == 8) && (SDR_RX_SAMP_SZ == 24))
    {
        const quint8 *in = (const quint8 *) inBuf;
        qint32 *out = (qint32 *) m_converterBuffer;

        for (int is = 0; is < nbSamples*2; is++) {
            out[is] = (((qint32)in[is]) - 128) << 16;
        }
    }
    else if ((m_settings.m_sampleBits == 16) && (SDR_RX_SAMP_SZ == 16))
    {
        const qint16 *in = (const qint16 *) inBuf;
        qint32 *out = (qint32 *) m_converterBuffer;

        for (int is = 0; is < nbSamples*2; is++) {
            out[is] = in[is];
        }
    }
    else if ((m_settings.m_sampleBits == 16) && (SDR_RX_SAMP_SZ == 24))
    {
        const qint16 *in = (const qint16 *) inBuf;
        qint32 *out = (qint32 *) m_converterBuffer;

        for (int is = 0; is < nbSamples*2; is++) {
            out[is] = in[is] << 8;
        }
    }
    else if ((m_settings.m_sampleBits == 24) && (SDR_RX_SAMP_SZ == 24))
    {
        const quint8 *in = (const quint8 *) inBuf;
        qint32 *out = (qint32 *) m_converterBuffer;

        for (int is = 0; is < nbSamples*2; is++) {
            out[is] = (((in[3*is+2] << 16) | (in[3*is+1] << 8) | in[3*is]) << 8) >> 8;
        }
    }
    else if ((m_settings.m_sampleBits == 24) && (SDR_RX_SAMP_SZ == 16))
    {
        const quint8 *in = (const quint8 *) inBuf;
        qint16 *out = (qint16 *) m_converterBuffer;

        for (int is = 0; is < nbSamples*2; is++) {
            out[is] = (in[3*is+2] << 8) | in[3*is+1];
        }
    }
    else if ((m_settings.m_sampleBits == 32) && (SDR_RX_SAMP_SZ == 16))
    {
        const qint32 *in = (const qint32 *) inBuf;
        qint16 *out = (qint16 *) m_converterBuffer;

        for (int is = 0; is < nbSamples*2; is++) {
            out[is] = in[is] >> 8;
        }
    }
    else if ((m_settings.m_sampleBits == 32) && (SDR_RX_SAMP_SZ == 24))
    {
        const qint32 *in = (const qint32 *) inBuf;
        qint32 *out = (qint32 *) m_converterBuffer;

        for (int is = 0; is < nbSamples*2; is++) {
            out[is] = in[is];
        }
    }
    else // invalid size
    {
        qWarning("RemoteTCPInputTCPHandler::convert: unexpected sample size in stream: %d bits", (int) m_settings.m_sampleBits);
    }

    m_uncompressedData.write(reinterpret_cast<quint8*>(m_converterBuffer), nbSamples*sizeof(Sample));
}

void RemoteTCPInputTCPHandler::flacError(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status)
{
    (void) decoder;

    qDebug() << "RemoteTCPInputTCPHandler::flacError: Error:" << status;
}

void RemoteTCPInputTCPHandler::connected()
{
    QMutexLocker mutexLocker(&m_mutex);
    qDebug() << "RemoteTCPInputTCPHandler::connected";
    if (m_messageQueueToGUI)
    {
        MsgReportConnection *msg = MsgReportConnection::create(true);
        m_messageQueueToGUI->push(msg);
    }
    m_spyServer = m_settings.m_protocol == "Spy Server";
    m_state = HEADER;
    m_sdra = false;
    m_remoteControl = true;
    m_iqOnly = true;
    if (m_spyServer) {
        spyServerConnect();
    }
    // Start calls to processData
    m_timer.start();
}

void RemoteTCPInputTCPHandler::reconnect()
{
    QMutexLocker mutexLocker(&m_mutex);
    if (!m_dataSocket) {
        connectToHost(m_settings.m_dataAddress, m_settings.m_dataPort, m_settings.m_protocol);
    }
}

void RemoteTCPInputTCPHandler::disconnected()
{
    QMutexLocker mutexLocker(&m_mutex);
    qDebug() << "RemoteTCPInputTCPHandler::disconnected";
    cleanup();
    if (m_messageQueueToGUI)
    {
        MsgReportConnection *msg = MsgReportConnection::create(false);
        m_messageQueueToGUI->push(msg);
    }
    if (!m_blacklisted)
    {
        // Try to reconnect immediately - it may just be server settings changed
        m_reconnectTimer.start(1);
    }
    else
    {
        // Stop device so we don't try to reconnect
        RemoteTCPInput::MsgStartStop *msg = RemoteTCPInput::MsgStartStop::create(false);
        m_messageQueueToInput->push(msg);
    }
}

void RemoteTCPInputTCPHandler::errorOccurred(QAbstractSocket::SocketError socketError)
{
    QMutexLocker mutexLocker(&m_mutex);
    qDebug() << "RemoteTCPInputTCPHandler::errorOccurred: " << socketError;

    // For RemoteHostClosedError, disconnected() will be called afterwards, so don't try to reconnect here
    // We try to reconnect here, for errors such as ConnectionRefusedError
    if (socketError != QAbstractSocket::RemoteHostClosedError)
    {
        cleanup();
        if (m_messageQueueToGUI)
        {
            MsgReportConnection *msg = MsgReportConnection::create(false);
            m_messageQueueToGUI->push(msg);
        }
        // Try to reconnect
        m_reconnectTimer.start(500);
    }
}

void RemoteTCPInputTCPHandler::sslErrors(const QList<QSslError> &errors)
{
    qDebug() << "RemoteTCPInputTCPHandler::sslErrors: " << errors;
    m_webSocket->ignoreSslErrors(); // FIXME: Add a setting whether to do this?
}

void RemoteTCPInputTCPHandler::dataReadyRead()
{
    QMutexLocker mutexLocker(&m_mutex);

    if (!m_readMetaData && !m_spyServer) {
        processMetaData();
    } else if (!m_readMetaData && m_spyServer) {
        processSpyServerMetaData();
    }

    if (m_readMetaData && !m_iqOnly) {
        processCommands();
    }
}

void RemoteTCPInputTCPHandler::processMetaData()
{
    quint8 metaData[RemoteTCPProtocol::m_sdraMetaDataSize];
    if (m_dataSocket->bytesAvailable() >= (qint64)sizeof(metaData))
    {
        qint64 bytesRead = m_dataSocket->read((char *)&metaData[0], 4);
        if (bytesRead == 4)
        {
            // Read first 4 bytes which indicate which protocol is in use
            // RTL0 or SDRA
            char protochars[5];
            memcpy(protochars, metaData, 4);
            protochars[4] = '\0';
            QString protocol(protochars);

            if (protocol == "RTL0")
            {
                m_sdra = false;
                m_spyServer = false;
                bytesRead = m_dataSocket->read((char *)&metaData[4], RemoteTCPProtocol::m_rtl0MetaDataSize-4);

                m_device = (RemoteTCPProtocol::Device)RemoteTCPProtocol::extractUInt32(&metaData[4]);
                if (m_messageQueueToGUI) {
                    m_messageQueueToGUI->push(MsgReportRemoteDevice::create(m_device, protocol, false, true));
                }
                if (m_settings.m_sampleBits != 8)
                {
                    RemoteTCPInputSettings& settings = m_settings;
                    settings.m_sampleBits = 8;
                    sendSettings(settings, {"sampleBits"});
                }
            }
            else if (protocol == "SDRA")
            {
                m_sdra = true;
                m_spyServer = false;
                bytesRead = m_dataSocket->read((char *)&metaData[4], RemoteTCPProtocol::m_sdraMetaDataSize-4);

                m_device = (RemoteTCPProtocol::Device)RemoteTCPProtocol::extractUInt32(&metaData[4]);
                quint32 protocolRevision = RemoteTCPProtocol::extractUInt32(&metaData[60]);
                quint32 flags = RemoteTCPProtocol::extractUInt32(&metaData[20]);
                if (protocolRevision >= 1)
                {
                    m_iqOnly = !(bool) ((flags >> 7) & 1);
                    m_remoteControl = (bool) ((flags >> 6) & 1);
                }
                else
                {
                    m_iqOnly = true;
                    m_remoteControl = true;
                }
                if (m_messageQueueToGUI) {
                    m_messageQueueToGUI->push(MsgReportRemoteDevice::create(m_device, protocol, m_iqOnly, m_remoteControl));
                }
                if (!m_settings.m_overrideRemoteSettings || !m_remoteControl)
                {
                    // Update local settings to match remote
                    RemoteTCPInputSettings& settings = m_settings;
                    QList<QString> settingsKeys;
                    settings.m_centerFrequency = RemoteTCPProtocol::extractUInt64(&metaData[8]);
                    settingsKeys.append("centerFrequency");
                    settings.m_loPpmCorrection = RemoteTCPProtocol::extractUInt32(&metaData[16]);
                    settingsKeys.append("loPpmCorrection");
                    settings.m_biasTee = flags & 1;
                    settingsKeys.append("biasTee");
                    settings.m_directSampling = (flags >> 1) & 1;
                    settingsKeys.append("directSampling");
                    settings.m_agc = (flags >> 2) & 1;
                    settingsKeys.append("agc");
                    settings.m_dcBlock = (flags >> 3) & 1;
                    settingsKeys.append("dcBlock");
                    settings.m_iqCorrection = (flags >> 4) & 1;
                    settingsKeys.append("iqCorrection");
                    settings.m_devSampleRate = RemoteTCPProtocol::extractUInt32(&metaData[24]);
                    settingsKeys.append("devSampleRate");
                    settings.m_log2Decim = RemoteTCPProtocol::extractUInt32(&metaData[28]);
                    settingsKeys.append("log2Decim");
                    settings.m_gain[0] = RemoteTCPProtocol::extractInt16(&metaData[32]);
                    settings.m_gain[1] = RemoteTCPProtocol::extractInt16(&metaData[34]);
                    settings.m_gain[2] = RemoteTCPProtocol::extractInt16(&metaData[36]);
                    settingsKeys.append("gain[0]");
                    settingsKeys.append("gain[1]");
                    settingsKeys.append("gain[2]");
                    settings.m_rfBW = RemoteTCPProtocol::extractUInt32(&metaData[40]);
                    settingsKeys.append("rfBW");
                    settings.m_inputFrequencyOffset = RemoteTCPProtocol::extractUInt32(&metaData[44]);
                    settingsKeys.append("inputFrequencyOffset");
                    settings.m_channelGain = RemoteTCPProtocol::extractUInt32(&metaData[48]);
                    settingsKeys.append("channelGain");
                    settings.m_channelSampleRate = RemoteTCPProtocol::extractUInt32(&metaData[52]);
                    settingsKeys.append("channelSampleRate");
                    settings.m_sampleBits = RemoteTCPProtocol::extractUInt32(&metaData[56]);
                    settingsKeys.append("sampleBits");
                    if (settings.m_channelSampleRate != (settings.m_devSampleRate >> settings.m_log2Decim))
                    {
                        settings.m_channelDecimation = true;
                        settingsKeys.append("channelDecimation");
                    }
                    if (protocolRevision >= 1)
                    {
                        settings.m_squelchEnabled = (flags >> 5) & 1;
                        settingsKeys.append("squelchEnabled");
                        settings.m_squelch =  RemoteTCPProtocol::extractFloat(&metaData[64]);
                        settingsKeys.append("squelch");
                        settings.m_squelchGate =  RemoteTCPProtocol::extractFloat(&metaData[68]);
                        settingsKeys.append("squelchGate");
                    }
                    sendSettings(settings, settingsKeys);
                }

                if (!m_iqOnly)
                {
                    qDebug() << "RemoteTCPInputTCPHandler: Compression enabled";
                    // Create FLAC decoder for IQ decompression
                    m_decoder = FLAC__stream_decoder_new();
                    m_remainingSamples = 0;
                    m_compressedFrames = 0;
                    m_uncompressedFrames = 0;

                    int bytesPerSecond = m_settings.m_channelSampleRate * 2 * sizeof(Sample);
                    int fifoSize = 2 * m_settings.m_preFill * bytesPerSecond;
                    m_uncompressedData.resize(fifoSize);
                    m_uncompressedData.clear();

                    if (m_decoder)
                    {
                        FLAC__StreamDecoderInitStatus initStatus;
                        initStatus = FLAC__stream_decoder_init_stream(m_decoder, flacReadCallback, nullptr, nullptr, nullptr, nullptr, flacWriteCallback, nullptr, flacErrorCallback, this);
                        if (initStatus != FLAC__STREAM_DECODER_INIT_STATUS_OK)
                        {
                            qDebug() << "RemoteTCPInputTCPHandler: Failed to init FLAC decoder: " << initStatus;
                        }
                    }
                    else
                    {
                        qDebug() << "RemoteTCPInputTCPHandler: Failed to allocate FLAC decoder";
                    }
                }
                else
                {
                    qDebug() << "RemoteTCPInputTCPHandler: Compression disabled";
                }
            }
            else
            {
                qDebug() << "RemoteTCPInputTCPHandler::dataReadyRead: Unknown protocol: " << protocol;
                m_dataSocket->close();
            }
            if (m_settings.m_overrideRemoteSettings && m_remoteControl)
            {
                // Force settings to be sent to remote device (this needs to be after m_sdra is determined above)
                applySettings(m_settings, QList<QString>(), true);
            }
        }
        m_readMetaData = true;
    }
}

void RemoteTCPInputTCPHandler::processSpyServerMetaData()
{
    bool done = false;

    while (!done)
    {
        if (m_state == HEADER)
        {
            if (m_dataSocket->bytesAvailable() >= (qint64)sizeof(SpyServerProtocol::Header))
            {
                qint64 bytesRead = m_dataSocket->read((char *)&m_spyServerHeader, sizeof(SpyServerProtocol::Header));
                if (bytesRead == sizeof(SpyServerProtocol::Header)) {
                    m_state = DATA;
                } else {
                    qDebug() << "RemoteTCPInputTCPHandler::processSpyServerMetaData: Failed to read:" << bytesRead << "/" << sizeof(SpyServerProtocol::Header);
                }
            }
            else
            {
                done = true;
            }
        }
        else if (m_state == DATA)
        {
            if (m_dataSocket->bytesAvailable() >= m_spyServerHeader.m_size)
            {
                qint64 bytesRead = m_dataSocket->read(&m_tcpBuf[0], m_spyServerHeader.m_size);
                if (bytesRead == m_spyServerHeader.m_size)
                {
                    if (m_spyServerHeader.m_message == SpyServerProtocol::DeviceMessage)
                    {
                        processSpyServerDevice((SpyServerProtocol::Device *) &m_tcpBuf[0]);
                        m_state = HEADER;
                    }
                    else if (m_spyServerHeader.m_message == SpyServerProtocol::StateMessage)
                    {
                        // This call can result in applySettings() calling clearBuffer() then processSpyServerData()
                        processSpyServerState((SpyServerProtocol::State *) &m_tcpBuf[0], true);
                        spyServerSetStreamIQ();

                        m_state = HEADER;
                        m_readMetaData = true;
                        done = true;
                    }
                    else
                    {
                        qDebug() << "RemoteTCPInputTCPHandler::processSpyServerMetaData: Unexpected message type" << m_spyServerHeader.m_message;
                        m_state = HEADER;
                    }
                }
                else
                {
                    qDebug() << "RemoteTCPInputTCPHandler::processSpyServerMetaData: Failed to read:" << bytesRead << "/" << m_spyServerHeader.m_size;
                }
            }
            else
            {
                done = true;
            }
        }
    }
}

void RemoteTCPInputTCPHandler::processSpyServerDevice(const SpyServerProtocol::Device* ssDevice)
{
    qDebug() << "RemoteTCPInputTCPHandler::processSpyServerDevice:"
        << "device:" << ssDevice->m_device
        << "serial:" << ssDevice->m_serial
        << "sampleRate:" << ssDevice->m_sampleRate
        << "decimationStages:" << ssDevice->m_decimationStages
        << "maxGainIndex:" << ssDevice->m_maxGainIndex
        << "minFrequency:" << ssDevice->m_minFrequency
        << "maxFrequency:" << ssDevice->m_maxFrequency
        << "sampleBits:" << ssDevice->m_sampleBits
        << "minDecimation:" << ssDevice->m_minDecimation;

    switch (ssDevice->m_device)
    {
    case 1:
        m_device = RemoteTCPProtocol::AIRSPY;
        break;
    case 2:
        m_device = RemoteTCPProtocol::AIRSPY_HF;
        break;
    case 3:
        m_device = ssDevice->m_maxGainIndex == 14
            ? RemoteTCPProtocol::RTLSDR_E4000
            : RemoteTCPProtocol::RTLSDR_R820T;
        break;
    default:
        m_device = RemoteTCPProtocol::UNKNOWN;
        break;
    }
    if (m_messageQueueToGUI) {
        m_messageQueueToGUI->push(MsgReportRemoteDevice::create(m_device, "Spy Server", false, true, ssDevice->m_maxGainIndex));
    }

    RemoteTCPInputSettings& settings = m_settings;
    QList<QString> settingsKeys{};
    // We can't change sample rate, so always have to update local setting to match
    m_settings.m_devSampleRate = settings.m_devSampleRate = ssDevice->m_sampleRate;
    settingsKeys.append("devSampleRate");
    // Make sure decimation setting is at least the minimum
    if (!m_settings.m_overrideRemoteSettings || (settings.m_log2Decim < (int) ssDevice->m_minDecimation))
    {
        m_settings.m_log2Decim = settings.m_log2Decim = ssDevice->m_minDecimation;
        settingsKeys.append("log2Decim");
    }
    sendSettings(settings, settingsKeys);
}

void RemoteTCPInputTCPHandler::processSpyServerState(const SpyServerProtocol::State* ssState, bool initial)
{
    qDebug() << "RemoteTCPInputTCPHandler::processSpyServerState: "
        << "initial:" << initial
        << "controllable:" << ssState->m_controllable
        << "gain:" << ssState->m_gain
        << "deviceCenterFrequency:" << ssState->m_deviceCenterFrequency
        << "iqCenterFrequency:" << ssState->m_iqCenterFrequency;

    if (initial && ssState->m_controllable && m_settings.m_overrideRemoteSettings)
    {
        // Force client settings to be sent to server
        applySettings(m_settings, QList<QString>(), true);
    }
    else
    {
        // Update client settings with that from server
        RemoteTCPInputSettings& settings = m_settings;
        QList<QString> settingsKeys;

        if (m_settings.m_centerFrequency != ssState->m_iqCenterFrequency)
        {
            settings.m_centerFrequency = ssState->m_iqCenterFrequency;
            settingsKeys.append("centerFrequency");
        }
        if (m_settings.m_gain[0] != (qint32) ssState->m_gain)
        {
            settings.m_gain[0] = ssState->m_gain;
            settingsKeys.append("gain[0]");
        }
        if (settingsKeys.size() > 0)
        {
            sendSettings(settings, settingsKeys);
        }
    }
}

void RemoteTCPInputTCPHandler::processSpyServerData(int requiredBytes, bool clear)
{
    if (!m_readMetaData) {
        return;
    }

    bool done = false;

    while (!done)
    {
        if (m_state == HEADER)
        {
            if (m_dataSocket->bytesAvailable() >= (qint64) sizeof(SpyServerProtocol::Header))
            {
                qint64 bytesRead = m_dataSocket->read((char *) &m_spyServerHeader, sizeof(SpyServerProtocol::Header));
                if (bytesRead == sizeof(SpyServerProtocol::Header)) {
                    m_state = DATA;
                } else {
                    qDebug() << "RemoteTCPInputTCPHandler::processSpyServerData: Failed to read:" << bytesRead << "/" << sizeof(SpyServerProtocol::Header);
                }
            }
            else
            {
                done = true;
            }
        }
        else if (m_state == DATA)
        {
            int bytes;

            if ((m_spyServerHeader.m_message >= SpyServerProtocol::IQ8MMessage) && (m_spyServerHeader.m_message <= SpyServerProtocol::IQ32Message)) {
                bytes = std::min(requiredBytes, (int) m_spyServerHeader.m_size);
            } else {
                bytes = m_spyServerHeader.m_size;
            }

            if (m_dataSocket->bytesAvailable() >= bytes)
            {
                qint64 bytesRead = m_dataSocket->read(&m_tcpBuf[0], bytes);
                if (bytesRead == bytes)
                {
                    if ((m_spyServerHeader.m_message >= SpyServerProtocol::IQ8MMessage) && (m_spyServerHeader.m_message <= SpyServerProtocol::IQ32Message))
                    {
                        if (!clear)
                        {
                            const int bytesPerIQPair = 2 * m_settings.m_sampleBits / 8;
                            processUncompressedData(&m_tcpBuf[0], bytesRead / bytesPerIQPair);
                        }
                        m_spyServerHeader.m_size -= bytesRead;
                        requiredBytes -= bytesRead;
                        if (m_spyServerHeader.m_size == 0) {
                            m_state = HEADER;
                        }
                        if (requiredBytes <= 0) {
                            done = true;
                        }
                    }
                    else if (m_spyServerHeader.m_message == SpyServerProtocol::StateMessage)
                    {
                        processSpyServerState((SpyServerProtocol::State *) &m_tcpBuf[0], false);
                        m_state = HEADER;
                    }
                    else
                    {
                        qDebug() << "RemoteTCPInputTCPHandler::processSpyServerData: Skipping unsupported message";
                        m_state = HEADER;
                    }
                }
                else
                {
                    qDebug() << "RemoteTCPInputTCPHandler::processSpyServerData: Failed to read:" << bytesRead << "/" << bytes;
                }
            }
            else
            {
                done = true;
            }
        }
    }
}

void RemoteTCPInputTCPHandler::sendSettings(const RemoteTCPInputSettings& settings, const QStringList& settingsKeys)
{
    if (m_messageQueueToInput) {
        m_messageQueueToInput->push(RemoteTCPInput::MsgConfigureRemoteTCPInput::create(settings, settingsKeys));
    }
    if (m_messageQueueToGUI) {
        m_messageQueueToGUI->push(RemoteTCPInput::MsgConfigureRemoteTCPInput::create(settings, settingsKeys));
    }
}

void RemoteTCPInputTCPHandler::processCommands()
{
    bool done = false;

    while (!done)
    {
        if (m_state == HEADER)
        {
            if (m_dataSocket->bytesAvailable() >= 5)
            {
                quint8 buf[5];
                qint64 bytesRead = m_dataSocket->read((char *) buf, sizeof(buf));

                if (bytesRead == sizeof(buf))
                {
                    m_command = (RemoteTCPProtocol::Command) buf[0];

                    switch (m_command)
                    {
                    case RemoteTCPProtocol::setCenterFrequency:
                    {
                        quint32 centerFrequency = (quint32) RemoteTCPProtocol::extractUInt32(&buf[1]);
                        if (centerFrequency != m_settings.m_centerFrequency)
                        {
                            RemoteTCPInputSettings settings = m_settings;
                            settings.m_centerFrequency = centerFrequency;
                            sendSettings(settings, {"centerFrequency"});
                        }
                        break;
                    }
                    case RemoteTCPProtocol::setSampleRate:
                    {
                        int devSampleRate = RemoteTCPProtocol::extractInt32(&buf[1]);
                        if (devSampleRate != m_settings.m_devSampleRate)
                        {
                            RemoteTCPInputSettings settings = m_settings;
                            settings.m_devSampleRate = devSampleRate;
                            sendSettings(settings, {"devSampleRate"});
                        }
                        break;
                    }
                    case RemoteTCPProtocol::setTunerGainMode:
                    {
                        // Currently fixed as 1
                    }
                    case RemoteTCPProtocol::setTunerGain:
                    {
                        int gain = RemoteTCPProtocol::extractUInt32(&buf[1]);
                        if (gain != m_settings.m_gain[0])
                        {
                            RemoteTCPInputSettings settings = m_settings;
                            settings.m_gain[0] = gain;
                            sendSettings(settings, {"gain[0]"});
                        }
                        break;
                    }
                    case RemoteTCPProtocol::setFrequencyCorrection:
                    {
                        qint32 loPpmCorrection = (qint32) RemoteTCPProtocol::extractUInt32(&buf[1]);
                        if (loPpmCorrection != m_settings.m_loPpmCorrection)
                        {
                            RemoteTCPInputSettings settings = m_settings;
                            settings.m_loPpmCorrection = loPpmCorrection;
                            sendSettings(settings, {"loPpmCorrection"});
                        }
                        break;
                    }
                    case RemoteTCPProtocol::setTunerIFGain:
                    {
                        int v = RemoteTCPProtocol::extractUInt32(&buf[1]);
                        int gain = (int)(qint16)(v & 0xffff);
                        int stage = (v >> 16) & 0xffff;
                        if ((stage < RemoteTCPInputSettings::m_maxGains) && (gain != m_settings.m_gain[stage]))
                        {
                            RemoteTCPInputSettings settings = m_settings;
                            settings.m_gain[stage] = gain;
                            sendSettings(settings, {QString("gain[%1]").arg(stage)});
                        }
                        break;
                    }
                    case RemoteTCPProtocol::setAGCMode:
                    {
                        bool agc = (bool) RemoteTCPProtocol::extractUInt32(&buf[1]);
                        if (agc != m_settings.m_agc)
                        {
                            RemoteTCPInputSettings settings = m_settings;
                            settings.m_agc = agc;
                            sendSettings(settings, {"agc"});
                        }
                        break;
                    }
                    case RemoteTCPProtocol::setDirectSampling:
                    {
                        bool directSampling = (bool) RemoteTCPProtocol::extractUInt32(&buf[1]);
                        if (directSampling != m_settings.m_directSampling)
                        {
                            RemoteTCPInputSettings settings = m_settings;
                            settings.m_directSampling = directSampling;
                            sendSettings(settings, {"directSampling"});
                        }
                        break;
                    }
                    case RemoteTCPProtocol::setBiasTee:
                    {
                        bool biasTee = (bool) RemoteTCPProtocol::extractUInt32(&buf[1]);
                        if (biasTee != m_settings.m_biasTee)
                        {
                            RemoteTCPInputSettings settings = m_settings;
                            settings.m_biasTee = biasTee;
                            sendSettings(settings, {"biasTee"});
                        }
                        break;
                    }
                    case RemoteTCPProtocol::setTunerBandwidth:
                    {
                        int rfBW = RemoteTCPProtocol::extractInt32(&buf[1]);
                        if (rfBW != m_settings.m_rfBW)
                        {
                            RemoteTCPInputSettings settings = m_settings;
                            settings.m_rfBW = rfBW;
                            sendSettings(settings, {"rfBW"});
                        }
                        break;
                    }
                    case RemoteTCPProtocol::setDecimation:
                    {
                        int log2Decim = RemoteTCPProtocol::extractInt32(&buf[1]);
                        if (log2Decim != m_settings.m_log2Decim)
                        {
                            RemoteTCPInputSettings settings = m_settings;
                            settings.m_log2Decim = log2Decim;
                            sendSettings(settings, {"log2Decim"});
                        }
                        break;
                    }
                    case RemoteTCPProtocol::setDCOffsetRemoval:
                    {
                        bool dcBlock = (bool) RemoteTCPProtocol::extractUInt32(&buf[1]);
                        if (dcBlock != m_settings.m_dcBlock)
                        {
                            RemoteTCPInputSettings settings = m_settings;
                            settings.m_dcBlock = dcBlock;
                            sendSettings(settings, {"dcBlock"});
                        }
                        break;
                    }
                    case RemoteTCPProtocol::setIQCorrection:
                    {
                        bool iqCorrection = (bool) RemoteTCPProtocol::extractUInt32(&buf[1]);
                        if (iqCorrection != m_settings.m_iqCorrection)
                        {
                            RemoteTCPInputSettings settings = m_settings;
                            settings.m_iqCorrection = iqCorrection;
                            sendSettings(settings, {"iqCorrection"});
                        }
                        break;
                    }
                    case RemoteTCPProtocol::setChannelSampleRate:
                    {
                        qint32 channelSampleRate = RemoteTCPProtocol::extractInt32(&buf[1]);
                        if (channelSampleRate != m_settings.m_channelSampleRate)
                        {
                            RemoteTCPInputSettings settings = m_settings;
                            settings.m_channelSampleRate = channelSampleRate;
                            sendSettings(settings, {"channelSampleRate"});
                        }
                        break;
                    }
                    case RemoteTCPProtocol::setChannelFreqOffset:
                    {
                        qint32 inputFrequencyOffset = (qint32) RemoteTCPProtocol::extractUInt32(&buf[1]);
                        if (inputFrequencyOffset != m_settings.m_inputFrequencyOffset)
                        {
                            RemoteTCPInputSettings settings = m_settings;
                            settings.m_inputFrequencyOffset = inputFrequencyOffset;
                            sendSettings(settings, {"inputFrequencyOffset"});
                        }
                        break;
                    }
                    case RemoteTCPProtocol::setChannelGain:
                    {
                        qint32 channelGain = RemoteTCPProtocol::extractInt32(&buf[1]);
                        if (channelGain != m_settings.m_channelGain)
                        {
                            RemoteTCPInputSettings settings = m_settings;
                            settings.m_channelGain = channelGain;
                            sendSettings(settings, {"channelGain"});
                        }
                        break;
                    }
                    case RemoteTCPProtocol::setSampleBitDepth:
                    {
                        qint32 sampleBits = RemoteTCPProtocol::extractInt32(&buf[1]);
                        if (sampleBits != m_settings.m_sampleBits)
                        {
                            RemoteTCPInputSettings settings = m_settings;
                            settings.m_sampleBits = sampleBits;
                            sendSettings(settings, {"sampleBits"});
                        }
                        break;
                    }
                    case RemoteTCPProtocol::setIQSquelchEnabled:
                    {
                        bool squelchEnabled = (bool) RemoteTCPProtocol::extractUInt32(&buf[1]);
                        if (squelchEnabled != m_settings.m_squelchEnabled)
                        {
                            RemoteTCPInputSettings settings = m_settings;
                            settings.m_squelchEnabled = squelchEnabled;
                            sendSettings(settings, {"squelchEnabled"});
                        }
                        break;
                    }
                    case RemoteTCPProtocol::setIQSquelch:
                    {
                        float squelch = RemoteTCPProtocol::extractFloat(&buf[1]);
                        if (squelch != m_settings.m_squelch)
                        {
                            RemoteTCPInputSettings settings = m_settings;
                            settings.m_squelch = squelch;
                            sendSettings(settings, {"squelch"});
                        }
                        break;
                    }
                    case RemoteTCPProtocol::setIQSquelchGate:
                    {
                        float squelchGate = RemoteTCPProtocol::extractFloat(&buf[1]);
                        if (squelchGate != m_settings.m_squelchGate)
                        {
                            RemoteTCPInputSettings settings = m_settings;
                            settings.m_squelchGate = squelchGate;
                            sendSettings(settings, {"squelchGate"});
                        }
                        break;
                    }
                    default:
                        m_commandLength = RemoteTCPProtocol::extractUInt32(&buf[1]);
                        m_state = DATA;
                    }
                }
                else
                {
                    qDebug() << "RemoteTCPInputTCPHandler::processCommands: Failed to read:" << bytesRead << "/" << sizeof(buf);
                }
            }
            else
            {
                done = true;
            }
        }
        if (m_state == DATA)
        {
            if (m_dataSocket->bytesAvailable() >= m_commandLength)
            {
                try
                {
                    switch (m_command)
                    {


                    case RemoteTCPProtocol::dataIQ:
                    {
                        break;
                    }

                    case RemoteTCPProtocol::dataIQFLAC:
                    {
                        qsizetype s = m_compressedData.size();
                        m_compressedData.resize(s + m_commandLength);
                        qint64 bytesRead = m_dataSocket->read(&m_compressedData.data()[s], m_commandLength);
                        m_compressedFrames++;
                        if (bytesRead == m_commandLength)
                        {
                            // FLAC encoder writes out 4 (fLaC), 38 (STREAMINFO), 51 (?) byte headers, that are transmitted as one command block,
                            // then each command block will be a complete audio block (first two bytes will be 0xfff8)
                            // FLAC__stream_decoder_process_single will keep calling the read callback until it's decoded one metadata or audio block
                            // so we need to make sure there's enough data that it will be able to return

                            bool decodeDone = false;

                            while (!decodeDone)
                            {
                                //qDebug() << "m_compressedFrames" << m_compressedFrames << "m_uncompressedFrames" << m_uncompressedFrames;
                                if (m_compressedFrames - 1 > m_uncompressedFrames)
                                {
                                    if (!FLAC__stream_decoder_process_single(m_decoder))
                                    {
                                        qDebug() << "FLAC decode failed";
                                        decodeDone = true;
                                    }
                                }
                                else
                                {
                                    decodeDone = true;
                                }
                            }
                        }
                        else
                        {
                            qDebug() << "RemoteTCPInputTCPHandler::processCommands: Failed to read:" << bytesRead << "/" << m_commandLength;
                        }
                        break;
                    }

                    case RemoteTCPProtocol::dataIQzlib:
                    {
                        if (m_commandLength > (quint32) m_compressedData.size()) {
                            m_compressedData.resize(m_commandLength);
                        }
                        qint64 bytesRead = m_dataSocket->read(m_compressedData.data(), m_commandLength);
                        if (bytesRead == m_commandLength)
                        {
                            // Decompressing using zlib
                            m_zStream.next_in = (Bytef *) m_compressedData.data();
                            m_zStream.avail_in = m_commandLength;
                            m_zStream.next_out = (Bytef *) m_zOutBuf.data();
                            m_zStream.avail_out = m_zOutBuf.size();

                            int ret = inflate(&m_zStream, Z_NO_FLUSH);

                            if (ret == Z_STREAM_END) {
                                inflateReset(&m_zStream);
                                // Convert and write to uncompressed data FIFO
                                int uncompressedBytes = m_zOutBuf.size() - m_zStream.avail_out;
                                int nbSamples = uncompressedBytes / 2 / (m_settings.m_sampleBits / 8);
                                processDecompressedZlibData(m_zOutBuf.data(), nbSamples);
                            } else if (ret == Z_NEED_DICT) {
                                qDebug() << "zlib needs dict to inflate";
                            } else if (ret == Z_DATA_ERROR) {
                                qDebug() << "zlib data error";
                            } else if (ret == Z_MEM_ERROR) {
                                qDebug() << "zlib mem error";
                            } else {
                                qDebug() << "Unexpected zlib return value" << ret;
                            }
                        }
                        else
                        {
                            qDebug() << "RemoteTCPInputTCPHandler::processCommands: Failed to read:" << bytesRead << "/" << m_commandLength;
                        }
                        break;
                    }

                    case RemoteTCPProtocol::dataPosition:
                    {
                        char pos[4+4+4];
                        qint64 bytesRead = m_dataSocket->read(pos, m_commandLength);
                        if (bytesRead == m_commandLength)
                        {
                            float latitude = RemoteTCPProtocol::extractFloat((const quint8 *) &pos[0]);
                            float longitude = RemoteTCPProtocol::extractFloat((const quint8 *) &pos[4]);
                            float altitude = RemoteTCPProtocol::extractFloat((const quint8 *) &pos[8]);
                            qDebug() << "RemoteTCPInputTCPHandler::processCommands: Position " << latitude << longitude << altitude;
                            if (m_messageQueueToInput) {
                                m_messageQueueToInput->push(RemoteTCPInput::MsgReportPosition::create(latitude, longitude, altitude));
                            }
                        }
                        else
                        {
                            qDebug() << "RemoteTCPInputTCPHandler::processCommands: Failed to read:" << bytesRead << "/" << m_commandLength;
                        }
                        break;
                    }

                    case RemoteTCPProtocol::dataDirection:
                    {
                        char dir[4+4+4];
                        qint64 bytesRead = m_dataSocket->read(dir, m_commandLength);
                        if (bytesRead == m_commandLength)
                        {
                            float isotropic = RemoteTCPProtocol::extractUInt32((const quint8 *) &dir[0]);
                            float azimuth = RemoteTCPProtocol::extractFloat((const quint8 *) &dir[4]);
                            float elevation = RemoteTCPProtocol::extractFloat((const quint8 *) &dir[8]);
                            qDebug() << "RemoteTCPInputTCPHandler::processCommands: Direction " << isotropic << azimuth << elevation;
                            if (m_messageQueueToInput) {
                                m_messageQueueToInput->push(RemoteTCPInput::MsgReportDirection::create(isotropic, azimuth, elevation));
                            }
                        }
                        else
                        {
                            qDebug() << "RemoteTCPInputTCPHandler::processCommands: Failed to read:" << bytesRead << "/" << m_commandLength;
                        }
                        break;
                    }

                    case RemoteTCPProtocol::sendMessage:
                    {
                        char *buf = new char[m_commandLength];
                        qint64 bytesRead = m_dataSocket->read(buf, m_commandLength);

                        if (bytesRead == m_commandLength)
                        {
                            bool broadcast = (bool) buf[0];
                            int i;
                            for (i = 1; i < (int) m_commandLength; i++)
                            {
                                if (buf[i] == '\0') {
                                    break;
                                }
                            }
                            QString callsign = QString::fromUtf8(&buf[1]);
                            QString text = QString::fromUtf8(&buf[i+1]);

                            qDebug() << "RemoteTCPInputTCPHandler::processCommands: Message " << m_dataSocket->peerAddress() << m_dataSocket->peerPort() << callsign << broadcast << text;
                            if (m_messageQueueToGUI) {
                                m_messageQueueToGUI->push(RemoteTCPInput::MsgSendMessage::create(callsign, text, broadcast));
                            }
                        }
                        else
                        {
                            qDebug() << "RemoteTCPInputTCPHandler::processCommands: Failed to read:" << bytesRead << "/" << m_commandLength;
                        }
                        delete[] buf;
                        break;
                    }

                    case RemoteTCPProtocol::sendBlacklistedMessage:
                    {
                        qDebug() << "RemoteTCPInputTCPHandler::processCommands: Disconnecting as blacklisted";
                        if (m_messageQueueToGUI) {
                            m_messageQueueToGUI->push(RemoteTCPInput::MsgSendMessage::create("", "Disconnecting as IP address is blacklisted", false));
                        }
                        m_blacklisted = true;
                        qDebug() << "set m_blacklisted" << m_blacklisted;
                        break;
                    }

                    default:
                    {
                        qDebug() << "RemoteTCPInputTCPHandler::processCommands: Unknown command" << m_command;
                        char *buf = new char[m_commandLength];
                        m_dataSocket->read(buf, m_commandLength);
                        delete[] buf;
                        break;
                    }
                    }
                }
                catch(std::bad_alloc&)
                {
                    qDebug() << "RemoteTCPInputTCPHandler::processCommands: Failed to allocate memory";
                    done = true;
                }
                m_state = HEADER;
            }
            else
            {
                done = true;
            }
        }
    }
}

// QTimer::timeout isn't guaranteed to be called on every timeout, so we need to look at the system clock
void RemoteTCPInputTCPHandler::processData()
{
    QMutexLocker mutexLocker(&m_mutex);

    if (m_dataSocket && m_dataSocket->isConnected())
    {
        int sampleRate = m_settings.m_channelSampleRate;
        int bytesPerIQPair = m_iqOnly ?  (2 * m_settings.m_sampleBits / 8) : (2 * sizeof(Sample));
        int bytesPerSecond = sampleRate * bytesPerIQPair;

        qint64 bytesAvailable = m_iqOnly ? m_dataSocket->bytesAvailable() : m_uncompressedData.fill();

        if ((bytesAvailable < (0.1f * m_settings.m_preFill * bytesPerSecond)) && !m_fillBuffer)
        {
            qDebug() << "RemoteTCPInputTCPHandler::processData: Buffering - bytesAvailable:" << bytesAvailable;
            m_fillBuffer = true;
        }

        // Report buffer usage
        // QTcpSockets buffer size should be unlimited - we pretend here it's twice as big as the point we start reading from it
        if (m_messageQueueToGUI)
        {
            qint64 size = std::max(bytesAvailable, (qint64)(m_settings.m_preFill * bytesPerSecond));
            RemoteTCPInput::MsgReportTCPBuffer *report = RemoteTCPInput::MsgReportTCPBuffer::create(
                                                            bytesAvailable, size, bytesAvailable / (float)bytesPerSecond,
                                                            m_sampleFifo->fill(),  m_sampleFifo->size(), m_sampleFifo->fill() / (float)bytesPerSecond
                                                            );
            m_messageQueueToGUI->push(report);
        }

        float factor = 0.0f;
        // Prime buffer, before we start reading
        if (m_fillBuffer)
        {
            if (bytesAvailable >= m_settings.m_preFill * bytesPerSecond)
            {
                qDebug() << "RemoteTCPInputTCPHandler::processData: Buffer primed - bytesAvailable:" << bytesAvailable;
                m_fillBuffer = false;
                m_prevDateTime = QDateTime::currentDateTime();
                factor = 1.0f / 4.0f; // If this is too high, samples can just be dropped downstream
            }
        }
        else
        {
            QDateTime currentDateTime = QDateTime::currentDateTime();
            factor = m_prevDateTime.msecsTo(currentDateTime) / 1000.0f; // FIXME: Close skew.. Actual sample rate may differ
            m_prevDateTime = currentDateTime;
        }

        unsigned int remaining = m_sampleFifo->size() - m_sampleFifo->fill();
        unsigned int maxRequired = (unsigned int) (factor * sampleRate);
        int requiredSamples = (int)std::min(maxRequired, remaining);
        int overflow = maxRequired - requiredSamples;
        if (overflow > 0) {
            qDebug() << "Not enough space in FIFO:" << overflow << maxRequired;
        }

        if (!m_fillBuffer)
        {
            if (!m_iqOnly)
            {
                processDecompressedData(requiredSamples);
            }
            else if (!m_spyServer)
            {
                if (m_dataSocket->bytesAvailable() >= requiredSamples*bytesPerIQPair)
                {
                    // rtl_tcp stream is just IQ samples
                    m_dataSocket->read(&m_tcpBuf[0], requiredSamples*bytesPerIQPair);
                    processUncompressedData(&m_tcpBuf[0], requiredSamples);
                }
            }
            else
            {
                // SpyServer stream is packetized, into a header and body, with multiple packet types
                int requiredBytes = requiredSamples*bytesPerIQPair;
                processSpyServerData(requiredBytes, false);
            }
        }
    }
}

// Copy from decompressed FIFO to replay buffer and sample FIFO
void RemoteTCPInputTCPHandler::processDecompressedData(int requiredSamples)
{
    qint64 requiredBytes = requiredSamples * sizeof(Sample);

    m_replayBuffer->lock();

    while ((requiredBytes > 0) && !m_uncompressedData.empty())
    {
        quint8 *uncompressedPtr;
        qsizetype uncompressedBytes = m_uncompressedData.readPtr(&uncompressedPtr, requiredSamples * sizeof(Sample));
        qsizetype uncompressedSamples = 2 * uncompressedBytes / sizeof(Sample);

        // Save data to replay buffer
	    bool replayEnabled = m_replayBuffer->getSize() > 0;
	    if (replayEnabled) {
		    m_replayBuffer->write((FixReal *) uncompressedPtr, (unsigned int) uncompressedSamples);
        }

        const FixReal *buf = (FixReal *) uncompressedPtr;
        qint32 remaining = uncompressedSamples;

        while (remaining > 0)
        {
            qint32 len;

            // Choose between live data or replayed data
		    if (replayEnabled && m_replayBuffer->useReplay()) {
			    len = m_replayBuffer->read(remaining, buf);
		    } else {
			    len = remaining;
		    }
            remaining -= len;

            calcPower(reinterpret_cast<const Sample*>(buf), len / 2);

            m_sampleFifo->write(reinterpret_cast<const quint8 *>(buf), len * sizeof(FixReal));
        }

        m_uncompressedData.read(uncompressedBytes);
        requiredBytes -= uncompressedBytes;
    }

    m_replayBuffer->unlock();
}

// Convert from uncompressed network format to Samples, then copy to replay buffer and sample FIFO
// The following code assumes host is little endian
void RemoteTCPInputTCPHandler::processUncompressedData(const char *inBuf, int nbSamples)
{
    // Ensure conversion buffer is large enough
    if (nbSamples > (int) m_converterBufferNbSamples)
    {
        if (m_converterBuffer) {
            delete[] m_converterBuffer;
        }
        m_converterBuffer = new int32_t[nbSamples*2];
    }

    // Convert from network format to Sample
    if ((m_settings.m_sampleBits == 32) && (SDR_RX_SAMP_SZ == 24) && m_spyServer)
    {
        const float *in = (const float *) inBuf;
        qint32 *out = (qint32 *) m_converterBuffer;

        for (int is = 0; is < nbSamples*2; is++) {
            out[is] = (qint32)(in[is] * SDR_RX_SCALEF);
        }
    }
    else if ((m_settings.m_sampleBits == 32) && (SDR_RX_SAMP_SZ == 16) && m_spyServer)
    {
        const float *in = (const float *) inBuf;
        qint16 *out = (qint16 *) m_converterBuffer;

        for (int is = 0; is < nbSamples*2; is++) {
            out[is] = (qint16)(in[is] * SDR_RX_SCALEF);
        }
    }
    else if ((m_settings.m_sampleBits == 8) && (SDR_RX_SAMP_SZ == 16))
    {
        const quint8 *in = (const quint8 *) inBuf;
        qint16 *out = (qint16 *) m_converterBuffer;

        for (int is = 0; is < nbSamples*2; is++) {
            out[is] = (((qint16)in[is]) - 128) << 8;
        }
    }
    else if ((m_settings.m_sampleBits == 8) && (SDR_RX_SAMP_SZ == 24))
    {
        const quint8 *in = (const quint8 *) inBuf;
        qint32 *out = (qint32 *) m_converterBuffer;

        for (int is = 0; is < nbSamples*2; is++) {
            out[is] = (((qint32)in[is]) - 128) << 16;
        }
    }
    else if ((m_settings.m_sampleBits == 24) && (SDR_RX_SAMP_SZ == 24))
    {
        const quint8 *in = (const quint8 *) inBuf;
        qint32 *out = (qint32 *) m_converterBuffer;

        for (int is = 0; is < nbSamples*2; is++) {
            out[is] = (((in[3*is+2] << 16) | (in[3*is+1] << 8) | in[3*is]) << 8) >> 8;
        }
    }
    else if ((m_settings.m_sampleBits == 24) && (SDR_RX_SAMP_SZ == 16))
    {
        const quint8 *in = (const quint8 *) inBuf;
        qint16 *out = (qint16 *) m_converterBuffer;

        for (int is = 0; is < nbSamples*2; is++) {
            out[is] = (in[3*is+2] << 8) | in[3*is+1];
        }
    }
    else if ((m_settings.m_sampleBits == 16) && (SDR_RX_SAMP_SZ == 24))
    {
        const qint16 *in = (const qint16 *) inBuf;
        qint32 *out = (qint32 *) m_converterBuffer;

        for (int is = 0; is < nbSamples*2; is++) {
            out[is] = in[is] << 8;
        }
    }
    else if ((m_settings.m_sampleBits == 32) && (SDR_RX_SAMP_SZ == 16))
    {
        const qint32 *in = (const qint32 *) inBuf;
        qint16 *out = (qint16 *) m_converterBuffer;

        for (int is = 0; is < nbSamples*2; is++) {
            out[is] = in[is] >> 8;
        }
    }
    else if ((m_settings.m_sampleBits == 32) && (SDR_RX_SAMP_SZ == 24))
    {
        const qint32 *in = (const qint32 *) inBuf;
        qint32 *out = (qint32 *) m_converterBuffer;

        for (int is = 0; is < nbSamples*2; is++) {
            out[is] = in[is];
        }
    }
    else // invalid size
    {
        qWarning("RemoteTCPInputTCPHandler::convert: unexpected sample size in stream: %d bits", (int) m_settings.m_sampleBits);
    }

    qint32 len = nbSamples*2;

    // Save data to replay buffer
	m_replayBuffer->lock();
	bool replayEnabled = m_replayBuffer->getSize() > 0;
	if (replayEnabled) {
		m_replayBuffer->write((const FixReal *) m_converterBuffer, len);
    }

    const FixReal *buf = (const FixReal *) m_converterBuffer;
	qint32 remaining = len;

    while (remaining > 0)
    {
        // Choose between live data or replayed data
		if (replayEnabled && m_replayBuffer->useReplay()) {
			len = m_replayBuffer->read(remaining, buf);
		} else {
			len = remaining;
		}
		remaining -= len;

        calcPower(reinterpret_cast<const Sample*>(buf), len / 2);

        m_sampleFifo->write(reinterpret_cast<const quint8*>(buf), len * sizeof(FixReal));
    }

    m_replayBuffer->unlock();
}

void RemoteTCPInputTCPHandler::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

bool RemoteTCPInputTCPHandler::handleMessage(const Message& cmd)
{
    if (MsgConfigureTcpHandler::match(cmd))
    {
        qDebug() << "RemoteTCPInputTCPHandler::handleMessage: MsgConfigureTcpHandler";
        const MsgConfigureTcpHandler& notif = (const MsgConfigureTcpHandler&) cmd;
        applySettings(notif.getSettings(), notif.getSettingsKeys(), notif.getForce());
        return true;
    }
    else if (RemoteTCPInput::MsgSendMessage::match(cmd))
    {
        const RemoteTCPInput::MsgSendMessage& msg = (const RemoteTCPInput::MsgSendMessage&) cmd;

        sendMessage(MainCore::instance()->getSettings().getStationName(), msg.getText(), msg.getBroadcast());

        return true;
    }
    else
    {
        return false;
    }
}
