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

#include "androidsdrdriverinputtcphandler.h"
#include "androidsdrdriverinput.h"
#include "../../channelrx/remotetcpsink/remotetcpprotocol.h"

MESSAGE_CLASS_DEFINITION(AndroidSDRDriverInputTCPHandler::MsgReportRemoteDevice, Message)
MESSAGE_CLASS_DEFINITION(AndroidSDRDriverInputTCPHandler::MsgReportConnection, Message)
MESSAGE_CLASS_DEFINITION(AndroidSDRDriverInputTCPHandler::MsgConfigureTcpHandler, Message)

AndroidSDRDriverInputTCPHandler::AndroidSDRDriverInputTCPHandler(SampleSinkFifo *sampleFifo, DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_running(false),
    m_dataSocket(nullptr),
    m_tcpBuf(nullptr),
    m_sampleFifo(sampleFifo),
    m_messageQueueToGUI(0),
    m_fillBuffer(true),
    m_reconnectTimer(this),
    m_rsp0(false),
    m_converterBuffer(nullptr),
    m_converterBufferNbSamples(0),
    m_settings()
{
    m_tcpBuf = new char[m_sampleFifo->size()*2*4];
    connect(&m_reconnectTimer, SIGNAL(timeout()), this, SLOT(reconnect()));
    m_reconnectTimer.setSingleShot(true);
}

AndroidSDRDriverInputTCPHandler::~AndroidSDRDriverInputTCPHandler()
{
    delete[] m_tcpBuf;
    if (m_converterBuffer) {
        delete[] m_converterBuffer;
    }
    cleanup();
}

void AndroidSDRDriverInputTCPHandler::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_inputMessageQueue.clear();
}

void AndroidSDRDriverInputTCPHandler::start()
{
    QMutexLocker mutexLocker(&m_mutex);

    qDebug("AndroidSDRDriverInputTCPHandler::start");

    if (m_running) {
        return;
    }
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    connect(thread(), SIGNAL(started()), this, SLOT(started()));
    connect(thread(), SIGNAL(finished()), this, SLOT(finished()));

    m_running = true;
}

void AndroidSDRDriverInputTCPHandler::stop()
{
    QMutexLocker mutexLocker(&m_mutex);

    qDebug("AndroidSDRDriverInputTCPHandler::stop");

    disconnect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

void AndroidSDRDriverInputTCPHandler::started()
{
    QMutexLocker mutexLocker(&m_mutex);

    disconnect(thread(), SIGNAL(started()), this, SLOT(started()));
}

void AndroidSDRDriverInputTCPHandler::finished()
{
    QMutexLocker mutexLocker(&m_mutex);
    disconnectFromHost();
    disconnect(thread(), SIGNAL(finished()), this, SLOT(finished()));
    m_running = false;
}

void AndroidSDRDriverInputTCPHandler::connectToHost(const QString& address, quint16 port)
{
    qDebug("AndroidSDRDriverInputTCPHandler::connectToHost: connect to %s:%d", address.toStdString().c_str(), port);
    m_dataSocket = new QTcpSocket(this);
    m_fillBuffer = true;
    m_readMetaData = false;
    connect(m_dataSocket, SIGNAL(readyRead()), this, SLOT(dataReadyRead()));
    connect(m_dataSocket, SIGNAL(connected()), this, SLOT(connected()));
    connect(m_dataSocket, SIGNAL(disconnected()), this, SLOT(disconnected()));
    connect(m_dataSocket, &QAbstractSocket::errorOccurred, this, &AndroidSDRDriverInputTCPHandler::errorOccurred);
    m_dataSocket->connectToHost(address, port);
}

void AndroidSDRDriverInputTCPHandler::disconnectFromHost()
{
    if (m_dataSocket)
    {
        qDebug() << "AndroidSDRDriverInputTCPHandler::disconnectFromHost";
        disconnect(m_dataSocket, SIGNAL(readyRead()), this, SLOT(dataReadyRead()));
        disconnect(m_dataSocket, SIGNAL(connected()), this, SLOT(connected()));
        disconnect(m_dataSocket, SIGNAL(disconnected()), this, SLOT(disconnected()));
        disconnect(m_dataSocket, &QAbstractSocket::errorOccurred, this, &AndroidSDRDriverInputTCPHandler::errorOccurred);
        m_dataSocket->disconnectFromHost();
        cleanup();
    }
}

void AndroidSDRDriverInputTCPHandler::cleanup()
{
    if (m_dataSocket)
    {
        m_dataSocket->deleteLater();
        m_dataSocket = nullptr;
    }
}

// Clear input buffer when settings change that invalidate the data in it
// E.g. sample rate or bit depth
void AndroidSDRDriverInputTCPHandler::clearBuffer()
{
    if (m_dataSocket)
    {
        m_dataSocket->flush();
        m_dataSocket->readAll();
        m_fillBuffer = true;
    }
}

void AndroidSDRDriverInputTCPHandler::setSampleRate(int sampleRate)
{
    QMutexLocker mutexLocker(&m_mutex);

    quint8 request[5];
    request[0] = RemoteTCPProtocol::setSampleRate;
    RemoteTCPProtocol::encodeUInt32(&request[1], sampleRate);
    if (m_dataSocket) {
        m_dataSocket->write((char*)request, sizeof(request));
    }
}

void AndroidSDRDriverInputTCPHandler::setCenterFrequency(quint64 frequency)
{
    QMutexLocker mutexLocker(&m_mutex);

    quint8 request[5];
    request[0] = RemoteTCPProtocol::setCenterFrequency;
    RemoteTCPProtocol::encodeUInt32(&request[1], frequency);
    if (m_dataSocket) {
        m_dataSocket->write((char*)request, sizeof(request));
    }
}

void AndroidSDRDriverInputTCPHandler::setTunerAGC(bool agc)
{
    QMutexLocker mutexLocker(&m_mutex);

    quint8 request[5];
    request[0] = RemoteTCPProtocol::setTunerGainMode;
    RemoteTCPProtocol::encodeUInt32(&request[1], agc);
    if (m_dataSocket) {
        m_dataSocket->write((char*)request, sizeof(request));
    }
}

void AndroidSDRDriverInputTCPHandler::setTunerGain(int gain)
{
    QMutexLocker mutexLocker(&m_mutex);

    quint8 request[5];
    request[0] = RemoteTCPProtocol::setTunerGain;
    RemoteTCPProtocol::encodeUInt32(&request[1], gain);
    if (m_dataSocket) {
        m_dataSocket->write((char*)request, sizeof(request));
    }
}

void AndroidSDRDriverInputTCPHandler::setFreqCorrection(int correction)
{
    QMutexLocker mutexLocker(&m_mutex);

    quint8 request[5];
    request[0] = RemoteTCPProtocol::setFrequencyCorrection;
    RemoteTCPProtocol::encodeUInt32(&request[1], correction);
    if (m_dataSocket) {
        m_dataSocket->write((char*)request, sizeof(request));
    }
}

void AndroidSDRDriverInputTCPHandler::setIFGain(quint16 stage, quint16 gain)
{
    QMutexLocker mutexLocker(&m_mutex);

    quint8 request[5];
    request[0] = RemoteTCPProtocol::setTunerIFGain;
    RemoteTCPProtocol::encodeUInt32(&request[1], (stage << 16) | gain);
    if (m_dataSocket) {
        m_dataSocket->write((char*)request, sizeof(request));
    }
}

void AndroidSDRDriverInputTCPHandler::setAGC(bool agc)
{
    QMutexLocker mutexLocker(&m_mutex);

    quint8 request[5];
    request[0] = RemoteTCPProtocol::setAGCMode;
    RemoteTCPProtocol::encodeUInt32(&request[1], agc);
    if (m_dataSocket) {
        m_dataSocket->write((char*)request, sizeof(request));
    }
}

void AndroidSDRDriverInputTCPHandler::setDirectSampling(bool enabled)
{
    QMutexLocker mutexLocker(&m_mutex);

    quint8 request[5];
    request[0] = RemoteTCPProtocol::setDirectSampling;
    RemoteTCPProtocol::encodeUInt32(&request[1], enabled);
    if (m_dataSocket) {
        m_dataSocket->write((char*)request, sizeof(request));
    }
}

void AndroidSDRDriverInputTCPHandler::setDCOffsetRemoval(bool enabled)
{
    QMutexLocker mutexLocker(&m_mutex);

    quint8 request[5];
    request[0] = RemoteTCPProtocol::setDCOffsetRemoval;
    RemoteTCPProtocol::encodeUInt32(&request[1], enabled);
    if (m_dataSocket) {
        m_dataSocket->write((char*)request, sizeof(request));
    }
}

void AndroidSDRDriverInputTCPHandler::setIQCorrection(bool enabled)
{
    QMutexLocker mutexLocker(&m_mutex);

    quint8 request[5];
    request[0] = RemoteTCPProtocol::setIQCorrection;
    RemoteTCPProtocol::encodeUInt32(&request[1], enabled);
    if (m_dataSocket) {
        m_dataSocket->write((char*)request, sizeof(request));
    }
}

void AndroidSDRDriverInputTCPHandler::setBiasTee(bool enabled)
{
    QMutexLocker mutexLocker(&m_mutex);

    quint8 request[5];
    request[0] = RemoteTCPProtocol::setBiasTee;
    RemoteTCPProtocol::encodeUInt32(&request[1], enabled);
    if (m_dataSocket) {
        m_dataSocket->write((char*)request, sizeof(request));
    }
}

void AndroidSDRDriverInputTCPHandler::setBandwidth(int bandwidth)
{
    QMutexLocker mutexLocker(&m_mutex);

    quint8 request[5];
    request[0] = RemoteTCPProtocol::setTunerBandwidth;
    RemoteTCPProtocol::encodeUInt32(&request[1], bandwidth);
    if (m_dataSocket) {
        m_dataSocket->write((char*)request, sizeof(request));
    }
}

void AndroidSDRDriverInputTCPHandler::setDecimation(int dec)
{
    QMutexLocker mutexLocker(&m_mutex);

    quint8 request[5];
    request[0] = RemoteTCPProtocol::setDecimation;
    RemoteTCPProtocol::encodeUInt32(&request[1], dec);
    if (m_dataSocket) {
        m_dataSocket->write((char*)request, sizeof(request));
    }
}

void AndroidSDRDriverInputTCPHandler::setChannelSampleRate(int sampleRate)
{
    QMutexLocker mutexLocker(&m_mutex);

    quint8 request[5];
    request[0] = RemoteTCPProtocol::setChannelSampleRate;
    RemoteTCPProtocol::encodeUInt32(&request[1], sampleRate);
    if (m_dataSocket) {
        m_dataSocket->write((char*)request, sizeof(request));
    }
}

void AndroidSDRDriverInputTCPHandler::setSampleBitDepth(int sampleBits)
{
    QMutexLocker mutexLocker(&m_mutex);

    quint8 request[5];
    request[0] = RemoteTCPProtocol::setSampleBitDepth;
    RemoteTCPProtocol::encodeUInt32(&request[1], sampleBits);
    if (m_dataSocket) {
        m_dataSocket->write((char*)request, sizeof(request));
    }
}

void AndroidSDRDriverInputTCPHandler::setAndroidGainByPercentage(int gain)
{
    QMutexLocker mutexLocker(&m_mutex);

    quint8 request[5];
    request[0] = RemoteTCPProtocol::androidGainByPercentage;
    RemoteTCPProtocol::encodeUInt32(&request[1], gain);
    if (m_dataSocket) {
        m_dataSocket->write((char*)request, sizeof(request));
    }
}

void AndroidSDRDriverInputTCPHandler::setAndroidEnable16BitSigned(bool enable)
{
    QMutexLocker mutexLocker(&m_mutex);

    quint8 request[5];
    request[0] = RemoteTCPProtocol::androidEnable16BitSigned;
    RemoteTCPProtocol::encodeUInt32(&request[1], enable);
    if (m_dataSocket) {
        m_dataSocket->write((char*)request, sizeof(request));
    }
}

void AndroidSDRDriverInputTCPHandler::rspSetAGC(bool agc)
{
    QMutexLocker mutexLocker(&m_mutex);

    quint8 request[5];
    request[0] = RemoteTCPProtocol::rspSetAGC;
    RemoteTCPProtocol::encodeUInt32(&request[1], agc);
    if (m_dataSocket) {
        m_dataSocket->write((char*)request, sizeof(request));
    }
}

void AndroidSDRDriverInputTCPHandler::rspSetIfGainR(int gain)
{
    QMutexLocker mutexLocker(&m_mutex);

    quint8 request[5];
    request[0] = RemoteTCPProtocol::rspSetIfGainR;
    RemoteTCPProtocol::encodeUInt32(&request[1], gain);
    if (m_dataSocket) {
        m_dataSocket->write((char*)request, sizeof(request));
    }
}

void AndroidSDRDriverInputTCPHandler::rspSetLNAState(int state)
{
    QMutexLocker mutexLocker(&m_mutex);

    quint8 request[5];
    request[0] = RemoteTCPProtocol::rspSetLNAState;
    RemoteTCPProtocol::encodeUInt32(&request[1], state);
    if (m_dataSocket) {
        m_dataSocket->write((char*)request, sizeof(request));
    }
}

void AndroidSDRDriverInputTCPHandler::applySettings(const AndroidSDRDriverInputSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "AndroidSDRDriverInputTCPHandler::applySettings: "
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
        setDCOffsetRemoval(settings.m_dcBlock);
    }
    if (settingsKeys.contains("iqCorrection") || force) {
        setIQCorrection(settings.m_iqCorrection);
    }
    if (settingsKeys.contains("biasTee") || force) {
        setBiasTee(settings.m_biasTee);
    }
    if (settingsKeys.contains("directSampling") || force) {
        setDirectSampling(settings.m_directSampling);
    }
    if (settingsKeys.contains("devSampleRate") || force) {
        setSampleRate(settings.m_devSampleRate);
    }
    if (settingsKeys.contains("agc") || force)
    {
        if (m_rsp0) {
            rspSetAGC(settings.m_agc);
        } else {
            setAGC(settings.m_agc);
        }
    }
    if (force)
    {
        if (!m_rsp0) {
            setTunerAGC(1); // The SDRangel RTLSDR driver always has tuner gain as manual
        }
    }
    if (settingsKeys.contains("gain[0]") || force)
    {
        if (m_rsp0) {
            rspSetLNAState(settings.m_gain[0] / 10);
        } else {
            setTunerGain(settings.m_gain[0]);
        }
    }
    for (int i = 1; i < 2; i++)
    {
        if (settingsKeys.contains(QString("gain[%1]").arg(i)) || force)
        {
            if (m_rsp0 && (i == 1)) {
                rspSetIfGainR(settings.m_gain[i] / -10);
            } else {
                setIFGain(i, settings.m_gain[i]);
            }
        }
    }
    if (settingsKeys.contains("rfBW") || force) {
        setBandwidth(settings.m_rfBW);
    }
    if (settingsKeys.contains("sampleBits") || force)
    {
        if (m_rsp0) {
            setAndroidEnable16BitSigned(settings.m_sampleBits == 16);
        } else {
            setSampleBitDepth(settings.m_sampleBits);
        }
        clearBuffer();
    }

    // Don't use force, as disconnect can cause rtl_tcp to quit
    if (settingsKeys.contains("dataPort") || (m_dataSocket == nullptr))
    {
        disconnectFromHost();
        connectToHost("127.0.0.1", settings.m_dataPort);
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

void AndroidSDRDriverInputTCPHandler::connected()
{
    QMutexLocker mutexLocker(&m_mutex);
    qDebug() << "AndroidSDRDriverInputTCPHandler::connected";
    // Force settings to be sent to remote device
    applySettings(m_settings, QList<QString>(), true);
    if (m_messageQueueToGUI)
    {
        MsgReportConnection *msg = MsgReportConnection::create(true);
        m_messageQueueToGUI->push(msg);
    }
}

void AndroidSDRDriverInputTCPHandler::reconnect()
{
    QMutexLocker mutexLocker(&m_mutex);
    if (!m_dataSocket) {
        connectToHost("127.0.0.1", m_settings.m_dataPort);
    }
}

void AndroidSDRDriverInputTCPHandler::disconnected()
{
    QMutexLocker mutexLocker(&m_mutex);
    qDebug() << "AndroidSDRDriverInputTCPHandler::disconnected";
    cleanup();
    if (m_messageQueueToGUI)
    {
        MsgReportConnection *msg = MsgReportConnection::create(false);
        m_messageQueueToGUI->push(msg);
    }
    // Try to reconnect
    m_reconnectTimer.start(500);
}

void AndroidSDRDriverInputTCPHandler::errorOccurred(QAbstractSocket::SocketError socketError)
{
    qDebug() << "AndroidSDRDriverInputTCPHandler::errorOccurred: " << socketError;
    cleanup();
    if (m_messageQueueToGUI)
    {
        MsgReportConnection *msg = MsgReportConnection::create(false);
        m_messageQueueToGUI->push(msg);
    }
    // Try to reconnect
    m_reconnectTimer.start(500);
}

void AndroidSDRDriverInputTCPHandler::dataReadyRead()
{
    QMutexLocker mutexLocker(&m_mutex);

    if (!m_readMetaData)
    {
        quint8 metaData[RemoteTCPProtocol::m_rtl0MetaDataSize];
        if (m_dataSocket->bytesAvailable() >= (qint64)sizeof(metaData))
        {
            qint64 bytesRead = m_dataSocket->read((char *)&metaData[0], 4);
            if (bytesRead == 4)
            {
                // Read first 4 bytes which indicate which protocol is in use.
                char protochars[5];
                memcpy(protochars, metaData, 4);
                protochars[4] = '\0';
                QString protocol(protochars);

                qDebug() << "RemoteTCPInputTCPHandler::dataReadyRead: Protocol: " << QByteArray((char *)metaData, 4).toHex() << " - " << protocol;

                if (protocol == "RTL0")
                {
                    m_rsp0 = false;
                    bytesRead = m_dataSocket->read((char *)&metaData[4], RemoteTCPProtocol::m_rtl0MetaDataSize-4);
                    RemoteTCPProtocol::Device tuner = (RemoteTCPProtocol::Device)RemoteTCPProtocol::extractUInt32(&metaData[4]);
                    if (m_messageQueueToGUI) {
                        m_messageQueueToGUI->push(MsgReportRemoteDevice::create(tuner, protocol));
                    }
                    // Set default gain to something reasonable
                    if (m_settings.m_gain[0] == 0)
                    {
                        AndroidSDRDriverInputSettings& settings = m_settings;
                        if (tuner == RemoteTCPProtocol::RTLSDR_E4000) {
                            settings.m_gain[0] = 290.0;
                        } else {
                            settings.m_gain[0] = 297.0;
                        }
                        QList<QString> settingsKeys{"gain[0]"};
                        if (m_messageQueueToInput) {
                            m_messageQueueToInput->push(AndroidSDRDriverInput::MsgConfigureAndroidSDRDriverInput::create(settings, settingsKeys));
                        }
                        if (m_messageQueueToGUI) {
                            m_messageQueueToGUI->push(AndroidSDRDriverInput::MsgConfigureAndroidSDRDriverInput::create(settings, settingsKeys));
                        }
                    }
                }
                else if (protocol == "MIR0")
                {
                    // Android RTLplay driver uses MIR0 protocol, but tuner type is 0 so no way of knowing what device is used
                    m_rsp0 = true;
                    bytesRead = m_dataSocket->read((char *)&metaData[4], RemoteTCPProtocol::m_rtl0MetaDataSize-4);
                    if (m_messageQueueToGUI) {
                        m_messageQueueToGUI->push(MsgReportRemoteDevice::create(RemoteTCPProtocol::SDRPLAY_V3_RSPDUO, protocol));
                    }
                    // Switch to 16-bit
                    // If we don't do this straight away, it doesn't seem reliable
                    setAndroidEnable16BitSigned(true);
                    if (m_settings.m_sampleBits != 16)
                    {
                        AndroidSDRDriverInputSettings& settings = m_settings;
                        settings.m_sampleBits = 16;
                        QList<QString> settingsKeys{"sampleBits"};
                        if (m_messageQueueToInput) {
                            m_messageQueueToInput->push(AndroidSDRDriverInput::MsgConfigureAndroidSDRDriverInput::create(settings, settingsKeys));
                        }
                        if (m_messageQueueToGUI) {
                            m_messageQueueToGUI->push(AndroidSDRDriverInput::MsgConfigureAndroidSDRDriverInput::create(settings, settingsKeys));
                        }
                    }
                    // Set default gains to something reasonable
                    if ((m_settings.m_gain[0] == 0) && (m_settings.m_gain[1] == 0))
                    {
                        AndroidSDRDriverInputSettings& settings = m_settings;
                        settings.m_gain[0] = 40;
                        settings.m_gain[1] = -400;
                        QList<QString> settingsKeys{"gain[0]", "gain[1]"};
                        if (m_messageQueueToInput) {
                            m_messageQueueToInput->push(AndroidSDRDriverInput::MsgConfigureAndroidSDRDriverInput::create(settings, settingsKeys));
                        }
                        if (m_messageQueueToGUI) {
                            m_messageQueueToGUI->push(AndroidSDRDriverInput::MsgConfigureAndroidSDRDriverInput::create(settings, settingsKeys));
                        }
                    }
                }
                else
                {
                    qDebug() << "AndroidSDRDriverInputTCPHandler::dataReadyRead: Unknown protocol: " << QByteArray((char *)metaData, 4).toHex() << " - " << protocol;
                }
            }
            else
            {
                qDebug() << "AndroidSDRDriverInputTCPHandler::dataReadyRead: Failed to read protocol ID";
            }
            m_readMetaData = true;
        }
        else
        {
            qDebug() << "AndroidSDRDriverInputTCPHandler::dataReadyRead: Not enough metadata";
        }
    }
    else
    {
        int bytesPerSample = m_settings.m_sampleBits / 8;

        unsigned int remaining = m_sampleFifo->size() - m_sampleFifo->fill();
        int requiredSamples = (int)std::min((unsigned int)(m_dataSocket->bytesAvailable()/(2*bytesPerSample)), remaining);

        if (requiredSamples >= 0)
        {
            m_dataSocket->read(&m_tcpBuf[0], requiredSamples*2*bytesPerSample);
            convert(requiredSamples);
        }
    }
}

// The following code assumes host is little endian
void AndroidSDRDriverInputTCPHandler::convert(int nbSamples)
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
            if (m_rsp0) {
                out[is] = in[is] << 12;
            } else {
                out[is] = in[is] << 8;
            }
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
        qWarning("AndroidSDRDriverInputTCPHandler::convert: unexpected sample size in stream: %d bits", (int) m_settings.m_sampleBits);
    }
}

void AndroidSDRDriverInputTCPHandler::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

bool AndroidSDRDriverInputTCPHandler::handleMessage(const Message& cmd)
{
    if (MsgConfigureTcpHandler::match(cmd))
    {
        qDebug() << "AndroidSDRDriverInputTCPHandler::handleMessage: MsgConfigureTcpHandler";
        MsgConfigureTcpHandler& notif = (MsgConfigureTcpHandler&) cmd;
        applySettings(notif.getSettings(), notif.getSettingsKeys(), notif.getForce());
        return true;
    }
    else
    {
        return false;
    }
}
