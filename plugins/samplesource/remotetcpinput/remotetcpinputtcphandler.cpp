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

#include <QUdpSocket>
#include <QDebug>

#include "device/deviceapi.h"
#include "util/message.h"

#include "remotetcpinputtcphandler.h"
#include "remotetcpinput.h"
#include "../../channelrx/remotetcpsink/remotetcpprotocol.h"

MESSAGE_CLASS_DEFINITION(RemoteTCPInputTCPHandler::MsgReportRemoteDevice, Message)
MESSAGE_CLASS_DEFINITION(RemoteTCPInputTCPHandler::MsgReportConnection, Message)
MESSAGE_CLASS_DEFINITION(RemoteTCPInputTCPHandler::MsgConfigureTcpHandler, Message)

RemoteTCPInputTCPHandler::RemoteTCPInputTCPHandler(SampleSinkFifo *sampleFifo, DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_running(false),
    m_dataSocket(nullptr),
    m_tcpBuf(nullptr),
    m_sampleFifo(sampleFifo),
    m_messageQueueToGUI(0),
    m_fillBuffer(true),
    m_timer(this),
    m_reconnectTimer(this),
    m_sdra(false),
    m_converterBuffer(nullptr),
    m_converterBufferNbSamples(0),
    m_settings()
{
    m_sampleFifo->setSize(5000000); // Start with large FIFO, to avoid having to resize
    m_tcpBuf = new char[m_sampleFifo->size()*2*4];
    m_timer.setInterval(50); // Previously 125, but this results in an obviously slow spectrum refresh rate
    connect(&m_reconnectTimer, SIGNAL(timeout()), this, SLOT(reconnect()));
    m_reconnectTimer.setSingleShot(true);
}

RemoteTCPInputTCPHandler::~RemoteTCPInputTCPHandler()
{
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
    m_timer.start();

    disconnect(thread(), SIGNAL(started()), this, SLOT(started()));
}

void RemoteTCPInputTCPHandler::finished()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_timer.stop();
    disconnect(&m_timer, SIGNAL(timeout()), this, SLOT(processData()));
    disconnectFromHost();
    disconnect(thread(), SIGNAL(finished()), this, SLOT(finished()));
    m_running = false;
}

void RemoteTCPInputTCPHandler::connectToHost(const QString& address, quint16 port)
{
    qDebug("RemoteTCPInputTCPHandler::connectToHost: connect to %s:%d", address.toStdString().c_str(), port);
    m_dataSocket = new QTcpSocket(this);
    m_fillBuffer = true;
    m_readMetaData = false;
    connect(m_dataSocket, SIGNAL(readyRead()), this, SLOT(dataReadyRead()));
    connect(m_dataSocket, SIGNAL(connected()), this, SLOT(connected()));
    connect(m_dataSocket, SIGNAL(disconnected()), this, SLOT(disconnected()));
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    connect(m_dataSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error), this, &RemoteTCPInputTCPHandler::errorOccurred);
#else
    connect(m_dataSocket, &QAbstractSocket::errorOccurred, this, &RemoteTCPInputTCPHandler::errorOccurred);
#endif
    m_dataSocket->connectToHost(address, port);
}

void RemoteTCPInputTCPHandler::disconnectFromHost()
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
        m_dataSocket->disconnectFromHost();
        cleanup();
    }
}

void RemoteTCPInputTCPHandler::cleanup()
{
    if (m_dataSocket)
    {
        m_dataSocket->deleteLater();
        m_dataSocket = nullptr;
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
            m_dataSocket->readAll();
            m_fillBuffer = true;
        }
    }
}

void RemoteTCPInputTCPHandler::setSampleRate(int sampleRate)
{
    QMutexLocker mutexLocker(&m_mutex);

    quint8 request[5];
    request[0] = RemoteTCPProtocol::setSampleRate;
    RemoteTCPProtocol::encodeUInt32(&request[1], sampleRate);
    if (m_dataSocket) {
        m_dataSocket->write((char*)request, sizeof(request));
    }
}

void RemoteTCPInputTCPHandler::setCenterFrequency(quint64 frequency)
{
    QMutexLocker mutexLocker(&m_mutex);

    quint8 request[5];
    request[0] = RemoteTCPProtocol::setCenterFrequency;
    RemoteTCPProtocol::encodeUInt32(&request[1], frequency);
    if (m_dataSocket) {
        m_dataSocket->write((char*)request, sizeof(request));
    }
}

void RemoteTCPInputTCPHandler::setTunerAGC(bool agc)
{
    QMutexLocker mutexLocker(&m_mutex);

    quint8 request[5];
    request[0] = RemoteTCPProtocol::setTunerGainMode;
    RemoteTCPProtocol::encodeUInt32(&request[1], agc);
    if (m_dataSocket) {
        m_dataSocket->write((char*)request, sizeof(request));
    }
}

void RemoteTCPInputTCPHandler::setTunerGain(int gain)
{
    QMutexLocker mutexLocker(&m_mutex);

    quint8 request[5];
    request[0] = RemoteTCPProtocol::setTunerGain;
    RemoteTCPProtocol::encodeUInt32(&request[1], gain);
    if (m_dataSocket) {
        m_dataSocket->write((char*)request, sizeof(request));
    }
}

void RemoteTCPInputTCPHandler::setGainByIndex(int index)
{
    QMutexLocker mutexLocker(&m_mutex);

    quint8 request[5];
    request[0] = RemoteTCPProtocol::setGainByIndex;
    RemoteTCPProtocol::encodeUInt32(&request[1], index);
    if (m_dataSocket) {
        m_dataSocket->write((char*)request, sizeof(request));
    }
}

void RemoteTCPInputTCPHandler::setFreqCorrection(int correction)
{
    QMutexLocker mutexLocker(&m_mutex);

    quint8 request[5];
    request[0] = RemoteTCPProtocol::setFrequencyCorrection;
    RemoteTCPProtocol::encodeUInt32(&request[1], correction);
    if (m_dataSocket) {
        m_dataSocket->write((char*)request, sizeof(request));
    }
}

void RemoteTCPInputTCPHandler::setIFGain(quint16 stage, quint16 gain)
{
    QMutexLocker mutexLocker(&m_mutex);

    quint8 request[5];
    request[0] = RemoteTCPProtocol::setTunerIFGain;
    RemoteTCPProtocol::encodeUInt32(&request[1], (stage << 16) | gain);
    if (m_dataSocket) {
        m_dataSocket->write((char*)request, sizeof(request));
    }
}

void RemoteTCPInputTCPHandler::setAGC(bool agc)
{
    QMutexLocker mutexLocker(&m_mutex);

    quint8 request[5];
    request[0] = RemoteTCPProtocol::setAGCMode;
    RemoteTCPProtocol::encodeUInt32(&request[1], agc);
    if (m_dataSocket) {
        m_dataSocket->write((char*)request, sizeof(request));
    }
}

void RemoteTCPInputTCPHandler::setDirectSampling(bool enabled)
{
    QMutexLocker mutexLocker(&m_mutex);

    quint8 request[5];
    request[0] = RemoteTCPProtocol::setDirectSampling;
    RemoteTCPProtocol::encodeUInt32(&request[1], enabled ? 3 : 0);
    if (m_dataSocket) {
        m_dataSocket->write((char*)request, sizeof(request));
    }
}

void RemoteTCPInputTCPHandler::setDCOffsetRemoval(bool enabled)
{
    QMutexLocker mutexLocker(&m_mutex);

    quint8 request[5];
    request[0] = RemoteTCPProtocol::setDCOffsetRemoval;
    RemoteTCPProtocol::encodeUInt32(&request[1], enabled);
    if (m_dataSocket) {
        m_dataSocket->write((char*)request, sizeof(request));
    }
}

void RemoteTCPInputTCPHandler::setIQCorrection(bool enabled)
{
    QMutexLocker mutexLocker(&m_mutex);

    quint8 request[5];
    request[0] = RemoteTCPProtocol::setIQCorrection;
    RemoteTCPProtocol::encodeUInt32(&request[1], enabled);
    if (m_dataSocket) {
        m_dataSocket->write((char*)request, sizeof(request));
    }
}

void RemoteTCPInputTCPHandler::setBiasTee(bool enabled)
{
    QMutexLocker mutexLocker(&m_mutex);

    quint8 request[5];
    request[0] = RemoteTCPProtocol::setBiasTee;
    RemoteTCPProtocol::encodeUInt32(&request[1], enabled);
    if (m_dataSocket) {
        m_dataSocket->write((char*)request, sizeof(request));
    }
}

void RemoteTCPInputTCPHandler::setBandwidth(int bandwidth)
{
    QMutexLocker mutexLocker(&m_mutex);

    quint8 request[5];
    request[0] = RemoteTCPProtocol::setTunerBandwidth;
    RemoteTCPProtocol::encodeUInt32(&request[1], bandwidth);
    if (m_dataSocket) {
        m_dataSocket->write((char*)request, sizeof(request));
    }
}

void RemoteTCPInputTCPHandler::setDecimation(int dec)
{
    QMutexLocker mutexLocker(&m_mutex);

    quint8 request[5];
    request[0] = RemoteTCPProtocol::setDecimation;
    RemoteTCPProtocol::encodeUInt32(&request[1], dec);
    if (m_dataSocket) {
        m_dataSocket->write((char*)request, sizeof(request));
    }
}

void RemoteTCPInputTCPHandler::setChannelSampleRate(int sampleRate)
{
    QMutexLocker mutexLocker(&m_mutex);

    quint8 request[5];
    request[0] = RemoteTCPProtocol::setChannelSampleRate;
    RemoteTCPProtocol::encodeUInt32(&request[1], sampleRate);
    if (m_dataSocket) {
        m_dataSocket->write((char*)request, sizeof(request));
    }
}

void RemoteTCPInputTCPHandler::setChannelFreqOffset(int offset)
{
    QMutexLocker mutexLocker(&m_mutex);

    quint8 request[5];
    request[0] = RemoteTCPProtocol::setChannelFreqOffset;
    RemoteTCPProtocol::encodeUInt32(&request[1], offset);
    if (m_dataSocket) {
        m_dataSocket->write((char*)request, sizeof(request));
    }
}

void RemoteTCPInputTCPHandler::setChannelGain(int gain)
{
    QMutexLocker mutexLocker(&m_mutex);

    quint8 request[5];
    request[0] = RemoteTCPProtocol::setChannelGain;
    RemoteTCPProtocol::encodeUInt32(&request[1], gain);
    if (m_dataSocket) {
        m_dataSocket->write((char*)request, sizeof(request));
    }
}

void RemoteTCPInputTCPHandler::setSampleBitDepth(int sampleBits)
{
    QMutexLocker mutexLocker(&m_mutex);

    quint8 request[5];
    request[0] = RemoteTCPProtocol::setSampleBitDepth;
    RemoteTCPProtocol::encodeUInt32(&request[1], sampleBits);
    if (m_dataSocket) {
        m_dataSocket->write((char*)request, sizeof(request));
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
    if (m_dataSocket) {
        m_dataSocket->write((char*)request, sizeof(request));
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
    if (m_dataSocket) {
        m_dataSocket->write((char*)request, sizeof(request));
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
    }

    // Don't use force, as disconnect can cause rtl_tcp to quit
    if (settingsKeys.contains("dataAddress") || settingsKeys.contains("dataPort") || (m_dataSocket == nullptr))
    {
        disconnectFromHost();
        connectToHost(settings.m_dataAddress, settings.m_dataPort);
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
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
    if (m_spyServer) {
        spyServerConnect();
    }
}

void RemoteTCPInputTCPHandler::reconnect()
{
    QMutexLocker mutexLocker(&m_mutex);
    if (!m_dataSocket) {
        connectToHost(m_settings.m_dataAddress, m_settings.m_dataPort);
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
    // Try to reconnect
    m_reconnectTimer.start(500);
}

void RemoteTCPInputTCPHandler::errorOccurred(QAbstractSocket::SocketError socketError)
{
    qDebug() << "RemoteTCPInputTCPHandler::errorOccurred: " << socketError;
    cleanup();
    if (m_messageQueueToGUI)
    {
        MsgReportConnection *msg = MsgReportConnection::create(false);
        m_messageQueueToGUI->push(msg);
    }
    // Try to reconnect
    m_reconnectTimer.start(500);
}

void RemoteTCPInputTCPHandler::dataReadyRead()
{
    QMutexLocker mutexLocker(&m_mutex);

    if (!m_readMetaData && !m_spyServer)
    {
        processMetaData();
    }
    else if (!m_readMetaData && m_spyServer)
    {
        processSpyServerMetaData();
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
                    m_messageQueueToGUI->push(MsgReportRemoteDevice::create(m_device, protocol));
                }
                if (m_settings.m_sampleBits != 8)
                {
                    RemoteTCPInputSettings& settings = m_settings;
                    settings.m_sampleBits = 8;
                    QList<QString> settingsKeys{"sampleBits"};
                    if (m_messageQueueToInput) {
                        m_messageQueueToInput->push(RemoteTCPInput::MsgConfigureRemoteTCPInput::create(settings, settingsKeys));
                    }
                    if (m_messageQueueToGUI) {
                        m_messageQueueToGUI->push(RemoteTCPInput::MsgConfigureRemoteTCPInput::create(settings, settingsKeys));
                    }
                }
            }
            else if (protocol == "SDRA")
            {
                m_sdra = true;
                m_spyServer = false;
                bytesRead = m_dataSocket->read((char *)&metaData[4], RemoteTCPProtocol::m_sdraMetaDataSize-4);

                m_device = (RemoteTCPProtocol::Device)RemoteTCPProtocol::extractUInt32(&metaData[4]);
                if (m_messageQueueToGUI) {
                    m_messageQueueToGUI->push(MsgReportRemoteDevice::create(m_device, protocol));
                }
                if (!m_settings.m_overrideRemoteSettings)
                {
                    // Update local settings to match remote
                    RemoteTCPInputSettings& settings = m_settings;
                    QList<QString> settingsKeys;
                    settings.m_centerFrequency = RemoteTCPProtocol::extractUInt64(&metaData[8]);
                    settingsKeys.append("centerFrequency");
                    settings.m_loPpmCorrection = RemoteTCPProtocol::extractUInt32(&metaData[16]);
                    settingsKeys.append("loPpmCorrection");
                    quint32 flags = RemoteTCPProtocol::extractUInt32(&metaData[20]);
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
                    if (m_messageQueueToInput) {
                        m_messageQueueToInput->push(RemoteTCPInput::MsgConfigureRemoteTCPInput::create(settings, settingsKeys));
                    }
                    if (m_messageQueueToGUI) {
                        m_messageQueueToGUI->push(RemoteTCPInput::MsgConfigureRemoteTCPInput::create(settings, settingsKeys));
                    }
                }
            }
            else
            {
                qDebug() << "RemoteTCPInputTCPHandler::dataReadyRead: Unknown protocol: " << protocol;
            }
            if (m_settings.m_overrideRemoteSettings)
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
        m_messageQueueToGUI->push(MsgReportRemoteDevice::create(m_device, "Spy Server", ssDevice->m_maxGainIndex));
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
    if (m_messageQueueToInput) {
        m_messageQueueToInput->push(RemoteTCPInput::MsgConfigureRemoteTCPInput::create(settings, settingsKeys));
    }
    if (m_messageQueueToGUI) {
        m_messageQueueToGUI->push(RemoteTCPInput::MsgConfigureRemoteTCPInput::create(settings, settingsKeys));
    }
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
            if (m_messageQueueToInput) {
                m_messageQueueToInput->push(RemoteTCPInput::MsgConfigureRemoteTCPInput::create(settings, settingsKeys));
            }
            if (m_messageQueueToGUI) {
                m_messageQueueToGUI->push(RemoteTCPInput::MsgConfigureRemoteTCPInput::create(settings, settingsKeys));
            }
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
                            convert(bytesRead / bytesPerIQPair);
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

// QTimer::timeout isn't guaranteed to be called on every timeout, so we need to look at the system clock
void RemoteTCPInputTCPHandler::processData()
{
    QMutexLocker mutexLocker(&m_mutex);
    if (m_dataSocket && (m_dataSocket->state() == QAbstractSocket::ConnectedState))
    {
        int sampleRate = m_settings.m_channelSampleRate;
        int bytesPerIQPair = 2 * m_settings.m_sampleBits / 8;
        int bytesPerSecond = sampleRate * bytesPerIQPair;

        if (m_dataSocket->bytesAvailable() < (0.1f * m_settings.m_preFill * bytesPerSecond))
        {
            qDebug() << "RemoteTCPInputTCPHandler::processData: Buffering - bytesAvailable:" << m_dataSocket->bytesAvailable();
            m_fillBuffer = true;
        }

        // Report buffer usage
        // QTcpSockets buffer size should be unlimited - we pretend here it's twice as big as the point we start reading from it
        if (m_messageQueueToGUI)
        {
            qint64 size = std::max(m_dataSocket->bytesAvailable(), (qint64)(m_settings.m_preFill * bytesPerSecond));
            RemoteTCPInput::MsgReportTCPBuffer *report = RemoteTCPInput::MsgReportTCPBuffer::create(
                                                            m_dataSocket->bytesAvailable(), size, m_dataSocket->bytesAvailable() / (float)bytesPerSecond,
                                                            m_sampleFifo->fill(),  m_sampleFifo->size(), m_sampleFifo->fill() / (float)bytesPerSecond
                                                            );
            m_messageQueueToGUI->push(report);
        }

        float factor = 0.0f;
        // Prime buffer, before we start reading
        if (m_fillBuffer)
        {
            if (m_dataSocket->bytesAvailable() >= m_settings.m_preFill * bytesPerSecond)
            {
                qDebug() << "RemoteTCPInputTCPHandler::processData: Buffer primed - bytesAvailable:" << m_dataSocket->bytesAvailable();
                m_fillBuffer = false;
                m_prevDateTime = QDateTime::currentDateTime();
                factor = 1.0f / 4.0f; // If this is too high, samples can just be dropped downstream
            }
        }
        else
        {
            QDateTime currentDateTime = QDateTime::currentDateTime();
            factor = m_prevDateTime.msecsTo(currentDateTime) / 1000.0f;
            m_prevDateTime = currentDateTime;
        }

        unsigned int remaining = m_sampleFifo->size() - m_sampleFifo->fill();
        int requiredSamples = (int)std::min((unsigned int)(factor * sampleRate), remaining);

        if (!m_fillBuffer)
        {
            if (!m_spyServer)
            {
                // rtl_tcp/SDRA stream is just IQ samples
                if (m_dataSocket->bytesAvailable() >= requiredSamples*bytesPerIQPair)
                {
                    m_dataSocket->read(&m_tcpBuf[0], requiredSamples*bytesPerIQPair);
                    convert(requiredSamples);
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

// The following code assumes host is little endian
void RemoteTCPInputTCPHandler::convert(int nbSamples)
{
    if (nbSamples > (int) m_converterBufferNbSamples)
    {
        if (m_converterBuffer) {
            delete[] m_converterBuffer;
        }
        m_converterBuffer = new int32_t[nbSamples*2];
    }

    if ((m_settings.m_sampleBits == 32) && (SDR_RX_SAMP_SZ == 24) && !m_spyServer)
    {
        m_sampleFifo->write(reinterpret_cast<quint8*>(m_tcpBuf), nbSamples*sizeof(Sample));
    }
    else if ((m_settings.m_sampleBits == 32) && (SDR_RX_SAMP_SZ == 24) && m_spyServer)
    {
        float *in = (float *)m_tcpBuf;
        qint32 *out = (qint32 *)m_converterBuffer;

        for (int is = 0; is < nbSamples*2; is++) {
            out[is] = (qint32)(in[is] * SDR_RX_SCALEF);
        }

        m_sampleFifo->write(reinterpret_cast<quint8*>(out), nbSamples*sizeof(Sample));
    }
    else if ((m_settings.m_sampleBits == 32) && (SDR_RX_SAMP_SZ == 16) && m_spyServer)
    {
        float *in = (float *)m_tcpBuf;
        qint16 *out = (qint16 *)m_converterBuffer;

        for (int is = 0; is < nbSamples*2; is++) {
            out[is] = (qint16)(in[is] * SDR_RX_SCALEF);
        }

        m_sampleFifo->write(reinterpret_cast<quint8*>(out), nbSamples*sizeof(Sample));
    }
    else if ((m_settings.m_sampleBits == 8) && (SDR_RX_SAMP_SZ == 16))
    {
        quint8 *in = (quint8 *)m_tcpBuf;
        qint16 *out = (qint16 *)m_converterBuffer;

        for (int is = 0; is < nbSamples*2; is++) {
            out[is] = (((qint16)in[is]) - 128) << 8;
        }

        m_sampleFifo->write(reinterpret_cast<quint8*>(out), nbSamples*sizeof(Sample));
    }
    else if ((m_settings.m_sampleBits == 8) && (SDR_RX_SAMP_SZ == 24))
    {
        quint8 *in = (quint8 *)m_tcpBuf;
        qint32 *out = (qint32 *)m_converterBuffer;

        for (int is = 0; is < nbSamples*2; is++) {
            out[is] = (((qint32)in[is]) - 128) << 16;
        }

        m_sampleFifo->write(reinterpret_cast<quint8*>(out), nbSamples*sizeof(Sample));
    }
    else if ((m_settings.m_sampleBits == 24) && (SDR_RX_SAMP_SZ == 24))
    {
        quint8 *in = (quint8 *)m_tcpBuf;
        qint32 *out = (qint32 *)m_converterBuffer;

        for (int is = 0; is < nbSamples*2; is++) {
            out[is] = (((in[3*is+2] << 16) | (in[3*is+1] << 8) | in[3*is]) << 8) >> 8;
        }

        m_sampleFifo->write(reinterpret_cast<quint8*>(out), nbSamples*sizeof(Sample));
    }
    else if ((m_settings.m_sampleBits == 24) && (SDR_RX_SAMP_SZ == 16))
    {
        quint8 *in = (quint8 *)m_tcpBuf;
        qint16 *out = (qint16 *)m_converterBuffer;

        for (int is = 0; is < nbSamples*2; is++) {
            out[is] = (in[3*is+2] << 8) | in[3*is+1];
        }

        m_sampleFifo->write(reinterpret_cast<quint8*>(out), nbSamples*sizeof(Sample));
    }
    else if ((m_settings.m_sampleBits == 16) && (SDR_RX_SAMP_SZ == 24))
    {
        qint16 *in = (qint16 *)m_tcpBuf;
        qint32 *out = (qint32 *)m_converterBuffer;

        for (int is = 0; is < nbSamples*2; is++) {
            out[is] = in[is] << 8;
        }

        m_sampleFifo->write(reinterpret_cast<quint8*>(out), nbSamples*sizeof(Sample));
    }
    else if ((m_settings.m_sampleBits == 32) && (SDR_RX_SAMP_SZ == 16))
    {
        qint32 *in = (qint32 *)m_tcpBuf;
        qint16 *out = (qint16 *)m_converterBuffer;

        for (int is = 0; is < nbSamples*2; is++) {
            out[is] = in[is] >> 8;
        }

        m_sampleFifo->write(reinterpret_cast<quint8*>(out), nbSamples*sizeof(Sample));
    }
    else // invalid size
    {
        qWarning("RemoteTCPInputTCPHandler::convert: unexpected sample size in stream: %d bits", (int) m_settings.m_sampleBits);
    }
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
        MsgConfigureTcpHandler& notif = (MsgConfigureTcpHandler&) cmd;
        applySettings(notif.getSettings(), notif.getSettingsKeys(), notif.getForce());
        return true;
    }
    else
    {
        return false;
    }
}
