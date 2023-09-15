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

#ifndef INCLUDE_REMOTETCPSINKSINK_H_
#define INCLUDE_REMOTETCPSINKSINK_H_

#include <QObject>
#include <QThread>
#include <QTcpServer>
#include <QTcpSocket>
#include <QDateTime>

#include "dsp/channelsamplesink.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "channel/remotedatablock.h"
#include "util/messagequeue.h"

#include "remotetcpsinksettings.h"
#include "remotetcpprotocol.h"

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

    void applySettings(const RemoteTCPSinkSettings& settings, const QStringList& settingsKeys, bool force = false, bool remoteChange = false);
    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void setDeviceIndex(uint32_t deviceIndex) { m_deviceIndex = deviceIndex; }
    void setChannelIndex(uint32_t channelIndex) { m_channelIndex = channelIndex; }
    void setMessageQueueToGUI(MessageQueue *queue) { m_messageQueueToGUI = queue; }
    void setMessageQueueToChannel(MessageQueue *queue) { m_messageQueueToChannel = queue; }

private slots:
    void acceptConnection();
    void disconnected();
    void errorOccurred(QAbstractSocket::SocketError socketError);
    void processCommand();
    void started();
    void finished();

private:
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
    QList<QTcpSocket *> m_clients;

    QDateTime m_bwDateTime;             //!< For calculating TX bandwidth
    qint64 m_bwBytes;

    NCO m_nco;
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;

    void startServer();
    void stopServer();
    void processOneSample(Complex &ci);
    RemoteTCPProtocol::Device getDevice();

};

#endif // INCLUDE_REMOTETCPSINKSINK_H_
