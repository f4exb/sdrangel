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

#ifndef PLUGINS_SAMPLESOURCE_REMOTETCPINPUT_REMOTETCPINPUTUDPHANDLER_H_
#define PLUGINS_SAMPLESOURCE_REMOTETCPINPUT_REMOTETCPINPUTUDPHANDLER_H_

#include <QObject>
#include <QTcpSocket>
#include <QHostAddress>
#include <QRecursiveMutex>
#include <QDateTime>

#include "util/messagequeue.h"
#include "remotetcpinputsettings.h"
#include "../../channelrx/remotetcpsink/remotetcpprotocol.h"
#include "spyserver.h"

class SampleSinkFifo;
class MessageQueue;
class DeviceAPI;

class RemoteTCPInputTCPHandler : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureTcpHandler : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const RemoteTCPInputSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureTcpHandler* create(const RemoteTCPInputSettings& settings, const QList<QString>& settingsKeys, bool force)
        {
            return new MsgConfigureTcpHandler(settings, settingsKeys, force);
        }

    private:
        RemoteTCPInputSettings m_settings;
        QList<QString> m_settingsKeys;
        bool m_force;

        MsgConfigureTcpHandler(const RemoteTCPInputSettings& settings, const QList<QString>& settingsKeys, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    class MsgReportRemoteDevice : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        RemoteTCPProtocol::Device getDevice() const { return m_device; }
        QString getProtocol() const { return m_protocol; }
        int getMaxGain() const { return m_maxGain; }

        static MsgReportRemoteDevice* create(RemoteTCPProtocol::Device device, const QString& protocol, int maxGain = 0)
        {
            return new MsgReportRemoteDevice(device, protocol, maxGain);
        }

    protected:
        RemoteTCPProtocol::Device m_device;
        QString m_protocol;
        int m_maxGain;

        MsgReportRemoteDevice(RemoteTCPProtocol::Device device, const QString& protocol, int maxGain) :
            Message(),
            m_device(device),
            m_protocol(protocol),
            m_maxGain(maxGain)
        { }
    };

    class MsgReportConnection : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getConnected() const { return m_connected; }

        static MsgReportConnection* create(bool connected)
        {
            return new MsgReportConnection(connected);
        }

    protected:
        bool m_connected;

        MsgReportConnection(bool connected) :
            Message(),
            m_connected(connected)
        { }
    };

    RemoteTCPInputTCPHandler(SampleSinkFifo* sampleFifo, DeviceAPI *deviceAPI);
    ~RemoteTCPInputTCPHandler();
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    void setMessageQueueToInput(MessageQueue *queue) { m_messageQueueToInput = queue; }
    void setMessageQueueToGUI(MessageQueue *queue) { m_messageQueueToGUI = queue; }
    void reset();
    void start();
    void stop();
    int getBufferGauge() const { return 0; }

public slots:
    void dataReadyRead();
    void connected();
    void disconnected();
    void errorOccurred(QAbstractSocket::SocketError socketError);

private:

    DeviceAPI *m_deviceAPI;
    bool m_running;
    QTcpSocket *m_dataSocket;
    char *m_tcpBuf;
    SampleSinkFifo *m_sampleFifo;
    MessageQueue m_inputMessageQueue;  //!< Queue for asynchronous inbound communication
    MessageQueue *m_messageQueueToInput;
    MessageQueue *m_messageQueueToGUI;
    bool m_readMetaData;
    bool m_fillBuffer;
    QTimer m_timer;
    QTimer m_reconnectTimer;
    QDateTime m_prevDateTime;
    bool m_sdra;
    bool m_spyServer;
    RemoteTCPProtocol::Device m_device;
    SpyServerProtocol::Header m_spyServerHeader;
    enum {HEADER, DATA} m_state;    //!< FSM for reading Spy Server packets


    int32_t *m_converterBuffer;
    uint32_t m_converterBufferNbSamples;

    QRecursiveMutex m_mutex;
    RemoteTCPInputSettings m_settings;

    void applyTCPLink(const QString& address, quint16 port);
    bool handleMessage(const Message& message);
    void convert(int nbSamples);
    void connectToHost(const QString& address, quint16 port);
    void disconnectFromHost();
    void cleanup();
    void clearBuffer();
    void setSampleRate(int sampleRate);
    void setCenterFrequency(quint64 frequency);
    void setTunerAGC(bool agc);
    void setTunerGain(int gain);
    void setGainByIndex(int gain);
    void setFreqCorrection(int correction);
    void setIFGain(quint16 stage, quint16 gain);
    void setAGC(bool agc);
    void setDirectSampling(bool enabled);
    void setDCOffsetRemoval(bool enabled);
    void setIQCorrection(bool enabled);
    void setBiasTee(bool enabled);
    void setBandwidth(int bandwidth);
    void setDecimation(int dec);
    void setChannelSampleRate(int dec);
    void setChannelFreqOffset(int offset);
    void setChannelGain(int gain);
    void setSampleBitDepth(int sampleBits);
    void applySettings(const RemoteTCPInputSettings& settings, const QList<QString>& settingsKeys, bool force = false);
    void processMetaData();
    void spyServerConnect();
    void spyServerSet(int setting, int value);
    void spyServerSetIQFormat(int sampleBits);
    void spyServerSetStreamIQ();
    void processSpyServerMetaData();
    void processSpyServerDevice(const SpyServerProtocol::Device* ssDevice);
    void processSpyServerState(const SpyServerProtocol::State* ssState, bool initial);
    void processSpyServerData(int requiredBytes, bool clear);

private slots:
    void started();
    void finished();
    void handleInputMessages();
    void processData();
    void reconnect();
};

#endif /* PLUGINS_SAMPLESOURCE_REMOTETCPINPUT_REMOTETCPINPUTUDPHANDLER_H_ */
