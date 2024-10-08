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

#ifndef INCLUDE_REMOTETCPSINKSINK_H_
#define INCLUDE_REMOTETCPSINKSINK_H_

#include <QObject>
#include <QThread>
#include <QTcpServer>
#include <QTcpSocket>
#include <QWebSocketServer>
#include <QDateTime>
#include <QTimer>

#include <FLAC/stream_encoder.h>
#include <zlib.h>

#include "dsp/channelsamplesink.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "util/messagequeue.h"
#include "util/movingaverage.h"
#include "util/doublebufferfifo.h"

#include "remotetcpsinksettings.h"
#include "remotetcpprotocol.h"

#include "util/socket.h"

class DeviceSampleSource;

class RemoteTCPSinkSink : public QObject, public ChannelSampleSink {
    Q_OBJECT
public:
    RemoteTCPSinkSink();
    ~RemoteTCPSinkSink();

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    void start();
    void stop();
    void init();

    void applySettings(const RemoteTCPSinkSettings& settings, const QStringList& settingsKeys, bool force = false, bool restartRequired = false);
    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void setDeviceIndex(uint32_t deviceIndex) { m_deviceIndex = deviceIndex; }
    void setChannelIndex(uint32_t channelIndex) { m_channelIndex = channelIndex; }
    void setMessageQueueToGUI(MessageQueue *queue) { m_messageQueueToGUI = queue; }
    void setMessageQueueToChannel(MessageQueue *queue) { m_messageQueueToChannel = queue; }
    void acceptConnection(Socket *client);
    Socket *getSocket(QObject *object) const;
    void sendCommand(RemoteTCPProtocol::Command cmd, quint32 value);
    void sendCommandFloat(RemoteTCPProtocol::Command cmd, float value);
    void sendMessage(QHostAddress address, quint16 port, const QString& callsign, const QString& text, bool broadcast);

    FLAC__StreamEncoderWriteStatus flacWrite(const FLAC__StreamEncoder *encoder, const FLAC__byte buffer[], size_t bytes, uint32_t samples, uint32_t currentFrame);

    bool getSquelchOpen() const { return m_squelchOpen; }

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

private:
    void sendQueuePosition(Socket *client, int position);
    void sendBlacklisted(Socket *client);
    void sendTimeLimit(Socket *client);
    void sendPosition(float latitude, float longitude, float altitude);
    void sendDirection(bool isotropic, float azimuth, float elevation);
    void sendPosition();
    void sendRotatorDirection(bool force);

private slots:
    void acceptTCPConnection();
    void acceptWebConnection();
    void disconnected();
    void errorOccurred(QAbstractSocket::SocketError socketError);
    void processCommand();
    void started();
    void finished();
#ifndef QT_NO_OPENSSL
    void onSslErrors(const QList<QSslError> &errors);
#endif
    void preferenceChanged(int elementType);
    void checkDeviceSettings();

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

    RemoteTCPSinkSettings m_settings;
    bool m_running;
    MessageQueue *m_messageQueueToGUI;
    MessageQueue *m_messageQueueToChannel;

    int64_t m_channelFrequencyOffset;
    int32_t m_channelSampleRate;
    uint32_t m_deviceIndex;
    uint32_t m_channelIndex;
    float m_linearGain;

    QRecursiveMutex m_mutex;
    QTcpServer *m_server;
    QWebSocketServer *m_webSocketServer;
    QList<Socket *> m_clients;
    QList<QTimer *> m_timers;

    QDateTime m_bwDateTime;             //!< For calculating TX bandwidth
    qint64 m_bwBytes;

    NCO m_nco;
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;

    // FLAC compression
    FLAC__StreamEncoder *m_encoder;
    QByteArray m_flacHeader;
    static const int m_flacHeaderSize = 4 + 38 + 51;

    // Zlib compression
    z_stream m_zStream;
    bool m_zStreamInitialised;
    QByteArray m_zInBuf;
    QByteArray m_zOutBuf;
    int m_zInBufCount;
    static const int m_zBufSize = 32768+128; //

    qint64 m_bytesUncompressed;
    qint64 m_bytesCompressed;
    qint64 m_bytesTransmitted;

    Real m_squelchLevel;
	int m_squelchCount;
	bool m_squelchOpen;
	DoubleBufferFIFO<Complex> m_squelchDelayLine;
	double m_magsq;
	double m_magsqSum;
	double m_magsqPeak;
	int  m_magsqCount;
	MagSqLevelsStore m_magSqLevelStore;
    MovingAverageUtil<Real, double, 16> m_movingAverage;

    // Device settings
    double m_centerFrequency;
    qint32 m_ppmCorrection;
    int m_biasTeeEnabled;
    int m_directSampling;
    int m_agc;
    int m_dcOffsetRemoval;
    int m_iqCorrection;
    qint32 m_devSampleRate;
    qint32 m_log2Decim;
    qint32 m_rfBW;
    qint32 m_gain[4];
    QTimer m_timer;

    // Rotator setttings
    double m_azimuth;
    double m_elevation;

    void startServer();
    void stopServer();
    void processOneSample(Complex &ci);
    RemoteTCPProtocol::Device getDevice();

};

#endif // INCLUDE_REMOTETCPSINKSINK_H_
