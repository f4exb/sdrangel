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
    m_converterBuffer(nullptr),
    m_converterBufferNbSamples(0),
    m_mutex(QMutex::Recursive),
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
    RemoteTCPProtocol::encodeUInt32(&request[1], enabled);
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

void RemoteTCPInputTCPHandler::applySettings(const RemoteTCPInputSettings& settings, bool force)
{
    qDebug() << "RemoteTCPInputTCPHandler::applySettings: "
                << "force: " << force
                << "m_dataAddress: " << settings.m_dataAddress
                << "m_dataPort: " << settings.m_dataPort
                << "m_devSampleRate: " << settings.m_devSampleRate
                << "m_channelSampleRate: " << settings.m_channelSampleRate;
    QMutexLocker mutexLocker(&m_mutex);

    if ((settings.m_centerFrequency != m_settings.m_centerFrequency) || force) {
        setCenterFrequency(settings.m_centerFrequency);
    }
    if ((settings.m_loPpmCorrection != m_settings.m_loPpmCorrection) || force) {
        setFreqCorrection(settings.m_loPpmCorrection);
    }
    if ((settings.m_dcBlock != m_settings.m_dcBlock) || force) {
        setDCOffsetRemoval(settings.m_dcBlock);
    }
    if ((settings.m_iqCorrection != m_settings.m_iqCorrection) || force) {
        setIQCorrection(settings.m_iqCorrection);
    }
    if ((settings.m_biasTee != m_settings.m_biasTee) || force) {
        setBiasTee(settings.m_biasTee);
    }
    if ((settings.m_directSampling != m_settings.m_directSampling) || force) {
        setDirectSampling(settings.m_directSampling);
    }
    if ((settings.m_log2Decim != m_settings.m_log2Decim) || force) {
        setDecimation(settings.m_log2Decim);
    }
    if ((settings.m_devSampleRate != m_settings.m_devSampleRate) || force) {
        setSampleRate(settings.m_devSampleRate);
    }
    if ((settings.m_agc != m_settings.m_agc) || force) {
        setAGC(settings.m_agc);
    }
    if (force) {
        setTunerAGC(1); // The SDRangel RTLSDR driver always has tuner gain as manual
    }
    if ((settings.m_gain[0] != m_settings.m_gain[0]) || force) {
        setTunerGain(settings.m_gain[0]);
    }
    for (int i = 1; i < 3; i++)
    {
        if ((settings.m_gain[i] != m_settings.m_gain[i]) || force) {
            setIFGain(i, settings.m_gain[i]);
        }
    }
    if ((settings.m_rfBW != m_settings.m_rfBW) || force) {
        setBandwidth(settings.m_rfBW);
    }
    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || force) {
        setChannelFreqOffset(settings.m_inputFrequencyOffset);
    }
    if ((settings.m_channelGain != m_settings.m_channelGain) || force) {
        setChannelGain(settings.m_channelGain);
    }
    if ((settings.m_channelSampleRate != m_settings.m_channelSampleRate) || force)
    {
        // Resize FIFO to give us 1 second
        // Can't do this while running
        if (!m_running && settings.m_channelSampleRate > (qint32)m_sampleFifo->size())
        {
            qDebug() << "RemoteTCPInputTCPHandler::applySettings: Resizing sample FIFO from " << m_sampleFifo->size() << "to" << settings.m_channelSampleRate;
            m_sampleFifo->setSize(settings.m_channelSampleRate);
            delete[] m_tcpBuf;
            m_tcpBuf = new char[m_sampleFifo->size()*2*4];
            m_fillBuffer = true; // So we reprime FIFO
        }
        setChannelSampleRate(settings.m_channelSampleRate);
        clearBuffer();
    }
    if ((settings.m_sampleBits != m_settings.m_sampleBits) || force)
    {
        setSampleBitDepth(settings.m_sampleBits);
        clearBuffer();
    }

    // Don't use force, as disconnect can cause rtl_tcp to quit
    if ((settings.m_dataPort != m_settings.m_dataPort) || (settings.m_dataAddress != m_settings.m_dataAddress) || (m_dataSocket == nullptr))
    {
        disconnectFromHost();
        connectToHost(settings.m_dataAddress, settings.m_dataPort);
    }

    m_settings = settings;
}

void RemoteTCPInputTCPHandler::connected()
{
    QMutexLocker mutexLocker(&m_mutex);
    qDebug() << "RemoteTCPInputTCPHandler::connected";
    if (m_settings.m_overrideRemoteSettings)
    {
        // Force settings to be sent to remote device
        applySettings(m_settings, true);
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
                    bytesRead = m_dataSocket->read((char *)&metaData[4], RemoteTCPProtocol::m_rtl0MetaDataSize-4);

                    RemoteTCPProtocol::Device tuner = (RemoteTCPProtocol::Device)RemoteTCPProtocol::extractUInt32(&metaData[4]);
                    if (m_messageQueueToGUI) {
                        m_messageQueueToGUI->push(MsgReportRemoteDevice::create(tuner, protocol));
                    }
                    if (m_settings.m_sampleBits != 8)
                    {
                        RemoteTCPInputSettings& settings = m_settings;
                        settings.m_sampleBits = 8;
                        if (m_messageQueueToInput) {
                            m_messageQueueToInput->push(RemoteTCPInput::MsgConfigureRemoteTCPInput::create(settings));
                        }
                        if (m_messageQueueToGUI) {
                            m_messageQueueToGUI->push(RemoteTCPInput::MsgConfigureRemoteTCPInput::create(settings));
                        }
                    }
                }
                else if (protocol == "SDRA")
                {
                    bytesRead = m_dataSocket->read((char *)&metaData[4], RemoteTCPProtocol::m_sdraMetaDataSize-4);

                    RemoteTCPProtocol::Device device = (RemoteTCPProtocol::Device)RemoteTCPProtocol::extractUInt32(&metaData[4]);
                    if (m_messageQueueToGUI) {
                        m_messageQueueToGUI->push(MsgReportRemoteDevice::create(device, protocol));
                    }
                    if (!m_settings.m_overrideRemoteSettings)
                    {
                        // Update local settings to match remote
                        RemoteTCPInputSettings& settings = m_settings;
                        settings.m_centerFrequency = RemoteTCPProtocol::extractUInt64(&metaData[8]);
                        settings.m_loPpmCorrection = RemoteTCPProtocol::extractUInt32(&metaData[16]);
                        quint32 flags = RemoteTCPProtocol::extractUInt32(&metaData[20]);
                        settings.m_biasTee = flags & 1;
                        settings.m_directSampling = (flags >> 1) & 1;
                        settings.m_agc = (flags >> 2) & 1;
                        settings.m_dcBlock = (flags >> 3) & 1;
                        settings.m_iqCorrection = (flags >> 4) & 1;
                        settings.m_devSampleRate = RemoteTCPProtocol::extractUInt32(&metaData[24]);
                        settings.m_log2Decim = RemoteTCPProtocol::extractUInt32(&metaData[28]);
                        settings.m_gain[0] = RemoteTCPProtocol::extractInt16(&metaData[32]);
                        settings.m_gain[1] = RemoteTCPProtocol::extractInt16(&metaData[34]);
                        settings.m_gain[2] = RemoteTCPProtocol::extractInt16(&metaData[36]);
                        settings.m_rfBW = RemoteTCPProtocol::extractUInt32(&metaData[40]);
                        settings.m_inputFrequencyOffset = RemoteTCPProtocol::extractUInt32(&metaData[44]);
                        settings.m_channelGain = RemoteTCPProtocol::extractUInt32(&metaData[48]);
                        settings.m_channelSampleRate = RemoteTCPProtocol::extractUInt32(&metaData[52]);
                        settings.m_sampleBits = RemoteTCPProtocol::extractUInt32(&metaData[56]);
                        if (settings.m_channelSampleRate != (settings.m_devSampleRate >> settings.m_log2Decim)) {
                            settings.m_channelDecimation = true;
                        }
                        if (m_messageQueueToInput) {
                            m_messageQueueToInput->push(RemoteTCPInput::MsgConfigureRemoteTCPInput::create(settings));
                        }
                        if (m_messageQueueToGUI) {
                            m_messageQueueToGUI->push(RemoteTCPInput::MsgConfigureRemoteTCPInput::create(settings));
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
        qint8 *in = (qint8 *)m_tcpBuf;
        qint16 *out = (qint16 *)m_converterBuffer;

        for (int is = 0; is < nbSamples*2; is++) {
            out[is] = (((qint16)in[is]) - 128);
        }

        m_sampleFifo->write(reinterpret_cast<quint8*>(out), nbSamples*sizeof(Sample));
    }
    else if ((m_settings.m_sampleBits == 8) && (SDR_RX_SAMP_SZ == 24))
    {
        qint8 *in = (qint8 *)m_tcpBuf;
        qint32 *out = (qint32 *)m_converterBuffer;

        for (int is = 0; is < nbSamples*2; is++) {
            out[is] = (((qint32)in[is]) - 128) << 8; // Only shift by 8, rather than 16, to match levels of native driver
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
        applySettings(notif.getSettings(), notif.getForce());
        return true;
    }
    else
    {
        return false;
    }
}
