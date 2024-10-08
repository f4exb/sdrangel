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
#include <QWebSocket>
#include <QHostAddress>
#include <QRecursiveMutex>
#include <QDateTime>

#include <FLAC/stream_decoder.h>
#include <zlib.h>

#include "util/messagequeue.h"
#include "util/movingaverage.h"
#include "util/socket.h"
#include "dsp/replaybuffer.h"
#include "remotetcpinputsettings.h"
#include "../../channelrx/remotetcpsink/remotetcpprotocol.h"
#include "spyserver.h"

class SampleSinkFifo;
class MessageQueue;
class DeviceAPI;

class FIFO {
public:

    explicit FIFO(qsizetype elements = 10);

    qsizetype write(quint8 *data, qsizetype elements);
    qsizetype read(quint8 *data, qsizetype elements);
    qsizetype readPtr(quint8 **data, qsizetype elements);
    void read(qsizetype elements);
    void resize(qsizetype elements); // Sets capacity
    void clear();
    qsizetype fill() const { return m_fill; }  // Number of elements in use
    bool empty() const { return m_fill == 0; }
    bool full() const { return m_fill == m_data.size(); }

private:

    qsizetype m_readPtr;
    qsizetype m_writePtr;
    qsizetype m_fill;

    QByteArray m_data;

};

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
        bool getIQOnly() const { return m_iqOnly; }
        bool getRemoteControl() const { return m_remoteControl; }
        int getMaxGain() const { return m_maxGain; }

        static MsgReportRemoteDevice* create(RemoteTCPProtocol::Device device, const QString& protocol, bool iqOnly, bool remoteControl, int maxGain = 0)
        {
            return new MsgReportRemoteDevice(device, protocol, iqOnly, remoteControl, maxGain);
        }

    protected:
        RemoteTCPProtocol::Device m_device;
        QString m_protocol;
        bool m_iqOnly;
        bool m_remoteControl;
        int m_maxGain;

        MsgReportRemoteDevice(RemoteTCPProtocol::Device device, const QString& protocol, bool iqOnly, bool remoteControl, int maxGain) :
            Message(),
            m_device(device),
            m_protocol(protocol),
            m_iqOnly(iqOnly),
            m_remoteControl(remoteControl),
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

    RemoteTCPInputTCPHandler(SampleSinkFifo* sampleFifo, DeviceAPI *deviceAPI, ReplayBuffer<FixReal> *replayBuffer);
    ~RemoteTCPInputTCPHandler();
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    void setMessageQueueToInput(MessageQueue *queue) { m_messageQueueToInput = queue; }
    void setMessageQueueToGUI(MessageQueue *queue) { m_messageQueueToGUI = queue; }
    void reset();
    void start();
    void stop();
    int getBufferGauge() const { return 0; }
    void processCommands();

    FLAC__StreamDecoderReadStatus flacRead(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes);
    FLAC__StreamDecoderWriteStatus flacWrite(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 *const buffer[]);
    void flacError(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status);

    void getMagSqLevels(double& avg, double& peak, int& nbSamples)
    {
        if (m_magsqCount > 0)
        {
            m_magsq = m_magsqSum / m_magsqCount;
            m_magSqLevelStore.m_magsq = m_magsq;
            m_magSqLevelStore.m_magsqPeak = m_magsqPeak;
        }

        avg = m_magSqLevelStore.m_magsq;
        peak = m_magSqLevelStore.m_magsqPeak;
        nbSamples = m_magsqCount == 0 ? 1 : m_magsqCount;

        m_magsqSum = 0.0f;
        m_magsqPeak = 0.0f;
        m_magsqCount = 0;
    }

public slots:
    void dataReadyRead();
    void connected();
    void disconnected();
    void errorOccurred(QAbstractSocket::SocketError socketError);
    void sslErrors(const QList<QSslError> &errors);

private:

    struct MagSqLevelsStore
    {
        MagSqLevelsStore() :
            m_magsq(1e-12),
            m_magsqPeak(1e-12)
        {}
        double m_magsq;
        double m_magsqPeak;
    };

    DeviceAPI *m_deviceAPI;
    bool m_running;
    Socket *m_dataSocket;
    QTcpSocket *m_tcpSocket;
    QWebSocket *m_webSocket;
    char *m_tcpBuf;
    SampleSinkFifo *m_sampleFifo;
    ReplayBuffer<FixReal> *m_replayBuffer;
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

    RemoteTCPProtocol::Command m_command;
    quint32 m_commandLength;

    int32_t *m_converterBuffer;
    uint32_t m_converterBufferNbSamples;

    QRecursiveMutex m_mutex;
    RemoteTCPInputSettings m_settings;

    bool m_remoteControl;
    bool m_iqOnly;
    QByteArray m_compressedData;

    // FLAC decompression
    qint64 m_compressedFrames;
    qint64 m_uncompressedFrames;
    FIFO m_uncompressedData;
    FLAC__StreamDecoder *m_decoder;
    int m_remainingSamples;

    // Zlib decompression
    z_stream m_zStream;
    QByteArray m_zOutBuf;
    static const int m_zBufSize = 32768+128; //

    bool m_blacklisted;

    double m_magsq;
	double m_magsqSum;
	double m_magsqPeak;
	int  m_magsqCount;
	MagSqLevelsStore m_magSqLevelStore;
    MovingAverageUtil<Real, double, 16> m_movingAverage;

    bool handleMessage(const Message& message);
    void connectToHost(const QString& address, quint16 port, const QString& protocol);
    //void disconnectFromHost();
    void cleanup();
    void clearBuffer();
    void sendCommand(RemoteTCPProtocol::Command cmd, quint32 value);
    void sendCommandFloat(RemoteTCPProtocol::Command cmd, float value);
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
    void setSquelchEnabled(bool enabled);
    void setSquelch(float squelch);
    void setSquelchGate(float squelchGate);
    void sendMessage(const QString& callsign, const QString& text, bool broadcast);
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
    void processDecompressedData(int requiredSamples);
    void processUncompressedData(const char *inBuf, int nbSamples);
    void processDecompressedZlibData(const char *inBuf, int nbSamples);
    void calcPower(const Sample *iq, int nbSamples);
    void sendSettings(const RemoteTCPInputSettings& settings, const QStringList& settingsKeys);

private slots:
    void started();
    void finished();
    void handleInputMessages();
    void processData();
    void reconnect();
};

#endif /* PLUGINS_SAMPLESOURCE_REMOTETCPINPUT_REMOTETCPINPUTUDPHANDLER_H_ */
