///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#include <QMutexLocker>
#include <QThread>

#include "channel/channelwebapiutils.h"
#include "dsp/hbfilterchainconverter.h"
#include "device/deviceapi.h"
#include "util/timeutil.h"
#include "maincore.h"

#include "remotetcpsinksink.h"
#include "remotetcpsink.h"
#include "remotetcpsinkbaseband.h"

RemoteTCPSinkSink::RemoteTCPSinkSink() :
        m_running(false),
        m_messageQueueToGUI(nullptr),
        m_messageQueueToChannel(nullptr),
        m_channelFrequencyOffset(0),
        m_channelSampleRate(48000),
        m_linearGain(1.0f),
        m_server(nullptr)
{
    qDebug("RemoteTCPSinkSink::RemoteTCPSinkSink");
    applySettings(m_settings, QStringList(), true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

RemoteTCPSinkSink::~RemoteTCPSinkSink()
{
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
}

void RemoteTCPSinkSink::finished()
{
    QMutexLocker mutexLocker(&m_mutex);
    stopServer();
    disconnect(thread(), SIGNAL(finished()), this, SLOT(finished()));
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


        if (m_bwDateTime.isValid())
        {
            QDateTime currentDateTime = QDateTime::currentDateTime();
            qint64 msecs = m_bwDateTime.msecsTo(currentDateTime) ;
            if (msecs > 1000)
            {
                float bw = (8*m_bwBytes)/(msecs/1000.0f);
                if (m_messageQueueToGUI) {
                    m_messageQueueToGUI->push(RemoteTCPSink::MsgReportBW::create(bw));
                }
                m_bwDateTime = currentDateTime;
                m_bwBytes = bytes;
            }
            else
            {
                m_bwBytes += bytes;
            }
        }
        else
        {
            m_bwDateTime = QDateTime::currentDateTime();
            m_bwBytes = bytes;
        }
    }
}

void RemoteTCPSinkSink::processOneSample(Complex &ci)
{
    if (m_settings.m_sampleBits == 8)
    {
        // Transmit data as per rtl_tcp - Interleaved unsigned 8-bit IQ
        quint8 iqBuf[2];
        iqBuf[0] = ((int)(ci.real() / SDR_RX_SCALEF * 256.0f * m_linearGain)) + 128;
        iqBuf[1] = ((int)(ci.imag() / SDR_RX_SCALEF * 256.0f * m_linearGain)) + 128;
        for (auto client : m_clients) {
            client->write((const char *)iqBuf, sizeof(iqBuf));
        }
    }
    else if (m_settings.m_sampleBits == 16)
    {
        // Interleaved little-endian signed 16-bit IQ
        quint8 iqBuf[2*2];
        qint32 i, q;
        i = ((qint32)(ci.real() / SDR_RX_SCALEF * 65536.0f * m_linearGain));
        q = ((qint32)(ci.imag() / SDR_RX_SCALEF * 65536.0f * m_linearGain));
        iqBuf[1] = (i >> 8) & 0xff;
        iqBuf[0] = i & 0xff;
        iqBuf[3] = (q >> 8) & 0xff;
        iqBuf[2] = q & 0xff;
        for (auto client : m_clients) {
            client->write((const char *)iqBuf, sizeof(iqBuf));
        }
    }
    else if (m_settings.m_sampleBits == 24)
    {
        // Interleaved little-endian signed 24-bit IQ
        quint8 iqBuf[3*2];
        qint32 i, q;
        i = ((qint32)(ci.real() * m_linearGain));
        q = ((qint32)(ci.imag() * m_linearGain));
        iqBuf[2] = (i >> 16) & 0xff;
        iqBuf[1] = (i >> 8) & 0xff;
        iqBuf[0] = i & 0xff;
        iqBuf[5] = (q >> 16) & 0xff;
        iqBuf[4] = (q >> 8) & 0xff;
        iqBuf[3] = q & 0xff;
        for (auto client : m_clients) {
            client->write((const char *)iqBuf, sizeof(iqBuf));
        }
    }
    else
    {
        // Interleaved little-endian signed 32-bit IQ
        quint8 iqBuf[4*2];
        qint32 i, q;
        i = ((qint32)(ci.real() * m_linearGain));
        q = ((qint32)(ci.imag() * m_linearGain));
        iqBuf[3] = (i >> 24) & 0xff;
        iqBuf[2] = (i >> 16) & 0xff;
        iqBuf[1] = (i >> 8) & 0xff;
        iqBuf[0] = i & 0xff;
        iqBuf[7] = (q >> 24) & 0xff;
        iqBuf[6] = (q >> 16) & 0xff;
        iqBuf[5] = (q >> 8) & 0xff;
        iqBuf[4] = q & 0xff;
        for (auto client : m_clients) {
            client->write((const char *)iqBuf, sizeof(iqBuf));
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
}

void RemoteTCPSinkSink::applySettings(const RemoteTCPSinkSettings& settings, const QStringList& settingsKeys, bool force, bool remoteChange)
{
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

    // Do clients need to reconnect to get these updated settings?
    bool restart = (settingsKeys.contains("dataAddress") && (m_settings.m_dataAddress != settings.m_dataAddress))
                || (settingsKeys.contains("dataPort") && (m_settings.m_dataPort != settings.m_dataPort))
                || (settingsKeys.contains("sampleBits") && (m_settings.m_sampleBits != settings.m_sampleBits))
                || (settingsKeys.contains("protocol") && (m_settings.m_protocol != settings.m_protocol))
                || (   !remoteChange
                    && (settingsKeys.contains("channelSampleRate") && (m_settings.m_channelSampleRate != settings.m_channelSampleRate))
                   );

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

    if (m_running && restart) {
        startServer();
    }
}

void RemoteTCPSinkSink::startServer()
{
    stopServer();

    m_server = new QTcpServer(this);
    if (!m_server->listen(QHostAddress(m_settings.m_dataAddress), m_settings.m_dataPort))
    {
        qCritical() << "RemoteTCPSink failed to listen on" << m_settings.m_dataAddress << "port" << m_settings.m_dataPort;
        // FIXME: Report to GUI?
    }
    else
    {
        qInfo() << "RemoteTCPSink listening on" << m_settings.m_dataAddress << "port" << m_settings.m_dataPort;
        connect(m_server, &QTcpServer::newConnection, this, &RemoteTCPSinkSink::acceptConnection);
    }
}

void RemoteTCPSinkSink::stopServer()
{
    for (auto client : m_clients)
    {
        qDebug() << "RemoteTCPSinkSink::stopServer: Closing connection to client";
        client->close();
        delete client;
    }
    if (m_clients.size() > 0)
    {
        if (m_messageQueueToGUI) {
            m_messageQueueToGUI->push(RemoteTCPSink::MsgReportConnection::create(0));
        }
        m_clients.clear();
    }

    if (m_server)
    {
        qDebug() << "RemoteTCPSinkSink::stopServer: Closing old server";
        m_server->close();
        delete m_server;
        m_server = nullptr;
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

void RemoteTCPSinkSink::acceptConnection()
{
    QMutexLocker mutexLocker(&m_mutex);
    QTcpSocket *client = m_server->nextPendingConnection();

    if (!client) {
        qDebug() << "RemoteTCPSinkSink::acceptConnection: client is nullptr";
        return;
    }
    m_clients.append(client);

    connect(client, &QIODevice::readyRead, this, &RemoteTCPSinkSink::processCommand);
    connect(client, SIGNAL(disconnected()), this, SLOT(disconnected()));
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    connect(client, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error), this, &RemoteTCPSinkSink::errorOccurred);
#else
    connect(client, &QAbstractSocket::errorOccurred, this, &RemoteTCPSinkSink::errorOccurred);
#endif
    qDebug() << "RemoteTCPSinkSink::acceptConnection: client connected";
    if (m_settings.m_protocol == RemoteTCPSinkSettings::RTL0)
    {
        quint8 metaData[RemoteTCPProtocol::m_rtl0MetaDataSize] = {'R', 'T', 'L', '0'};
        RemoteTCPProtocol::encodeUInt32(&metaData[4], getDevice()); // Tuner ID
        RemoteTCPProtocol::encodeUInt32(&metaData[8], 1); // Gain stages
        client->write((const char *)metaData, sizeof(metaData));
    }
    else
    {
        quint8 metaData[RemoteTCPProtocol::m_sdraMetaDataSize] = {'S', 'D', 'R', 'A'};
        RemoteTCPProtocol::encodeUInt32(&metaData[4], getDevice());
        // Send device/channel settings, so they can be displayed in the remote GUI

        double centerFrequency = 0.0;
        qint32 ppmCorrection = 0;
        quint32 flags = 0;
        int biasTeeEnabled = false;
        int directSampling = false;
        int agc = false;
        int dcOffsetRemoval = false;
        int iqCorrection = false;
        qint32 devSampleRate = 0;
        qint32 log2Decim = 0;
        qint32 gain[4] = {0, 0, 0, 0};
        qint32 rfBW = 0;

        ChannelWebAPIUtils::getCenterFrequency(m_deviceIndex, centerFrequency);
        ChannelWebAPIUtils::getLOPpmCorrection(m_deviceIndex, ppmCorrection);
        ChannelWebAPIUtils::getDevSampleRate(m_deviceIndex, devSampleRate);
        ChannelWebAPIUtils::getSoftDecim(m_deviceIndex, log2Decim);
        for (int i = 0; i < 4; i++) {
            ChannelWebAPIUtils::getGain(m_deviceIndex, i, gain[i]);
        }
        ChannelWebAPIUtils::getRFBandwidth(m_deviceIndex, rfBW);
        ChannelWebAPIUtils::getBiasTee(m_deviceIndex, biasTeeEnabled);
        ChannelWebAPIUtils::getDeviceSetting(m_deviceIndex, "noModMode", directSampling);
        ChannelWebAPIUtils::getAGC(m_deviceIndex, agc);
        ChannelWebAPIUtils::getDCOffsetRemoval(m_deviceIndex, dcOffsetRemoval);
        ChannelWebAPIUtils::getIQCorrection(m_deviceIndex, iqCorrection);
        flags =   (iqCorrection << 4)
                | (dcOffsetRemoval << 3)
                | (agc << 2)
                | (directSampling << 1)
                | biasTeeEnabled;

        RemoteTCPProtocol::encodeUInt64(&metaData[8], (quint64)centerFrequency);
        RemoteTCPProtocol::encodeUInt32(&metaData[16], ppmCorrection);
        RemoteTCPProtocol::encodeUInt32(&metaData[20], flags);
        RemoteTCPProtocol::encodeUInt32(&metaData[24], devSampleRate);
        RemoteTCPProtocol::encodeUInt32(&metaData[28], log2Decim);
        RemoteTCPProtocol::encodeInt16(&metaData[32], gain[0]);
        RemoteTCPProtocol::encodeInt16(&metaData[34], gain[1]);
        RemoteTCPProtocol::encodeInt16(&metaData[36], gain[2]);
        RemoteTCPProtocol::encodeInt16(&metaData[38], gain[3]);
        RemoteTCPProtocol::encodeUInt32(&metaData[40], rfBW);
        RemoteTCPProtocol::encodeInt32(&metaData[44], m_settings.m_inputFrequencyOffset);
        RemoteTCPProtocol::encodeUInt32(&metaData[48], m_settings.m_gain);
        RemoteTCPProtocol::encodeUInt32(&metaData[52], m_settings.m_channelSampleRate);
        RemoteTCPProtocol::encodeUInt32(&metaData[56], m_settings.m_sampleBits);
        // Send API port? Not accessible via MainCore

        client->write((const char *)metaData, sizeof(metaData));
    }
    if (m_messageQueueToGUI) {
        m_messageQueueToGUI->push(RemoteTCPSink::MsgReportConnection::create(m_clients.size()));
    }
}

void RemoteTCPSinkSink::disconnected()
{
    QMutexLocker mutexLocker(&m_mutex);
    qDebug() << "RemoteTCPSinkSink::disconnected";
    QTcpSocket *client = (QTcpSocket*)sender();
    client->deleteLater();
    m_clients.removeAll(client);
    if (m_messageQueueToGUI) {
        m_messageQueueToGUI->push(RemoteTCPSink::MsgReportConnection::create(m_clients.size()));
    }
}

void RemoteTCPSinkSink::errorOccurred(QAbstractSocket::SocketError socketError)
{
    qDebug() << "RemoteTCPSinkSink::errorOccurred: " << socketError;
    /*if (m_msgQueueToFeature)
    {
        RemoteTCPSinkSink::MsgReportWorker *msg = RemoteTCPSinkSink::MsgReportWorker::create(m_socket.errorString() + " " + socketError);
        m_msgQueueToFeature->push(msg);
    }*/
}

void RemoteTCPSinkSink::processCommand()
{
    QMutexLocker mutexLocker(&m_mutex);
    QTcpSocket *client = (QTcpSocket*)sender();
    RemoteTCPSinkSettings settings = m_settings;

    quint8 cmd[5];
    while (client && (client->bytesAvailable() >= (qint64)sizeof(cmd)))
    {
        int len = client->read((char *)cmd, sizeof(cmd));
        if (len == sizeof(cmd))
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
                settings.m_channelSampleRate = channelSampleRate;
                if (m_messageQueueToGUI) {
                    m_messageQueueToGUI->push(RemoteTCPSink::MsgConfigureRemoteTCPSink::create(settings, {"channelSampleRate"}, false, true));
                }
                if (m_messageQueueToChannel) {
                    m_messageQueueToChannel->push(RemoteTCPSink::MsgConfigureRemoteTCPSink::create(settings, {"channelSampleRate"}, false, true));
                }
                break;
            }
            case RemoteTCPProtocol::setChannelFreqOffset:
            {
                int offset = (int)RemoteTCPProtocol::extractUInt32(&cmd[1]);
                qDebug() << "RemoteTCPSinkSink::processCommand: set channel input frequency offset " << offset;
                settings.m_inputFrequencyOffset = offset;
                if (m_messageQueueToGUI) {
                    m_messageQueueToGUI->push(RemoteTCPSink::MsgConfigureRemoteTCPSink::create(settings, {"inputFrequencyOffset"}, false, true));
                }
                if (m_messageQueueToChannel) {
                    m_messageQueueToChannel->push(RemoteTCPSink::MsgConfigureRemoteTCPSink::create(settings, {"inputFrequencyOffset"}, false, true));
                }
                break;
            }
            case RemoteTCPProtocol::setChannelGain:
            {
                int gain = (int)RemoteTCPProtocol::extractUInt32(&cmd[1]);
                qDebug() << "RemoteTCPSinkSink::processCommand: set channel gain " << gain;
                settings.m_gain = gain;
                if (m_messageQueueToGUI) {
                    m_messageQueueToGUI->push(RemoteTCPSink::MsgConfigureRemoteTCPSink::create(settings, {"gain"}, false, true));
                }
                if (m_messageQueueToChannel) {
                    m_messageQueueToChannel->push(RemoteTCPSink::MsgConfigureRemoteTCPSink::create(settings, {"gain"}, false, true));
                }
                break;
            }
            case RemoteTCPProtocol::setSampleBitDepth:
            {
                int bits = RemoteTCPProtocol::extractUInt32(&cmd[1]);
                qDebug() << "RemoteTCPSinkSink::processCommand: set sample bit depth " << bits;
                settings.m_sampleBits = bits;
                if (m_messageQueueToGUI) {
                    m_messageQueueToGUI->push(RemoteTCPSink::MsgConfigureRemoteTCPSink::create(settings, {"sampleBits"}, false, true));
                }
                if (m_messageQueueToChannel) {
                    m_messageQueueToChannel->push(RemoteTCPSink::MsgConfigureRemoteTCPSink::create(settings, {"sampleBits"}, false, true));
                }
                break;
            }
            default:
                qDebug() << "RemoteTCPSinkSink::processCommand: unknown command " << cmd[0];
                break;
            }
        }
        else
        {
            qDebug() << "RemoteTCPSinkSink::processCommand: read only " << len << " bytes - Expecting " << sizeof(cmd);
        }
    }
}
