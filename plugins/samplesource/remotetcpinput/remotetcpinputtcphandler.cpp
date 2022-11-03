///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2022 Jon Beniston, M7RCE                                        //
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

#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "device/deviceapi.h"

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
    m_tcpBuf = new char[m_sampleFifo->size()*2*4];
    m_timer.setInterval(125);
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
    if (m_dataSocket)
    {
        m_dataSocket->flush();
        m_dataSocket->readAll();
        m_fillBuffer = true;
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

void RemoteTCPInputTCPHandler::applySettings(const RemoteTCPInputSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "RemoteTCPInputTCPHandler::applySettings: "
                << "force: " << force
                << settings.getDebugString(settingsKeys, force);
    QMutexLocker mutexLocker(&m_mutex);

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
        // Can't do this while running
        if (!m_running && settingsKeys.contains("channelSampleRate") && settings.m_channelSampleRate > (qint32)m_sampleFifo->size())
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
    if (m_settings.m_overrideRemoteSettings)
    {
        // Force settings to be sent to remote device
        applySettings(m_settings, QList<QString>(), true);
    }
    if (m_messageQueueToGUI)
    {
        MsgReportConnection *msg = MsgReportConnection::create(true);
        m_messageQueueToGUI->push(msg);
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

    if (!m_readMetaData)
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
                    bytesRead = m_dataSocket->read((char *)&metaData[4], RemoteTCPProtocol::m_rtl0MetaDataSize-4);

                    RemoteTCPProtocol::Device tuner = (RemoteTCPProtocol::Device)RemoteTCPProtocol::extractUInt32(&metaData[4]);
                    if (m_messageQueueToGUI) {
                        m_messageQueueToGUI->push(MsgReportRemoteDevice::create(tuner, protocol));
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
                    bytesRead = m_dataSocket->read((char *)&metaData[4], RemoteTCPProtocol::m_sdraMetaDataSize-4);

                    RemoteTCPProtocol::Device device = (RemoteTCPProtocol::Device)RemoteTCPProtocol::extractUInt32(&metaData[4]);
                    if (m_messageQueueToGUI) {
                        m_messageQueueToGUI->push(MsgReportRemoteDevice::create(device, protocol));
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
            }
            m_readMetaData = true;
        }
    }
}

// QTimer::timeout isn't guarenteed to be called on every timeout, so we need to look at the system clock
void RemoteTCPInputTCPHandler::processData()
{
    QMutexLocker mutexLocker(&m_mutex);
    if (m_dataSocket && (m_dataSocket->state() == QAbstractSocket::ConnectedState))
    {
        int sampleRate = m_settings.m_channelSampleRate;
        int bytesPerSample = m_settings.m_sampleBits / 8;
        int bytesPerSecond = sampleRate * 2 * bytesPerSample;

        if (m_dataSocket->bytesAvailable() < (0.1f * m_settings.m_preFill * bytesPerSecond))
        {
            qDebug() << "RemoteTCPInputTCPHandler::processData: Buffering!";
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
                qDebug() << "Buffer primed bytesAvailable:" << m_dataSocket->bytesAvailable();
                m_fillBuffer = false;
                m_prevDateTime = QDateTime::currentDateTime();
                factor = 6.0f / 8.0f;
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
            if (m_dataSocket->bytesAvailable() >= requiredSamples*2*bytesPerSample)
            {
                m_dataSocket->read(&m_tcpBuf[0], requiredSamples*2*bytesPerSample);
                convert(requiredSamples);
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

    if ((m_settings.m_sampleBits == 32) && (SDR_RX_SAMP_SZ == 24))
    {
        m_sampleFifo->write(reinterpret_cast<quint8*>(m_tcpBuf), nbSamples*sizeof(Sample));
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
