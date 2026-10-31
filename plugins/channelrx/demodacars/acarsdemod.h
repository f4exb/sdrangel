///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2018 Edouard Griffiths, F4EXB.                             //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_ACARSDEMOD_H
#define INCLUDE_ACARSDEMOD_H

#include <vector>

#include <QNetworkRequest>
#include <QUdpSocket>
#include <QThread>
#include <QDateTime>
#include <QFile>
#include <QTextStream>

#include "dsp/basebandsamplesink.h"
#include "channel/channelapi.h"
#include "util/message.h"
#include "maincore.h"       // MsgAcarsPacket subclasses MainCore::MsgPacket

#include "acarsdemodbaseband.h"
#include "acarsdemodsettings.h"
#include "acarsvdl2.h"
#include "acarshfdl.h"
#include "acarsaero.h"

class QNetworkAccessManager;
class QNetworkReply;
class QThread;
class DeviceAPI;
class AcarsDemodWorker;

class AcarsDemod : public BasebandSampleSink, public ChannelAPI {
public:
    class MsgConfigureAcarsDemod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const AcarsDemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureAcarsDemod* create(const AcarsDemodSettings& settings, bool force)
        {
            return new MsgConfigureAcarsDemod(settings, force);
        }

    private:
        AcarsDemodSettings m_settings;
        bool m_force;

        MsgConfigureAcarsDemod(const AcarsDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    // A demodulated HFDL LPDU or SPDU that does not carry an ACARS message (logons,
    // squitters). As with VDL-2, ACARS frames go out as MainCore::MsgPacket instead.
    class MsgHfdlFrame : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const AcarsHfdlReceiver::Frame& getFrame() const { return m_frame; }
        QDateTime getDateTime() const { return m_dateTime; }

        static MsgHfdlFrame* create(const AcarsHfdlReceiver::Frame& frame)
        {
            return new MsgHfdlFrame(frame);
        }

    private:
        AcarsHfdlReceiver::Frame m_frame;
        QDateTime m_dateTime;

        MsgHfdlFrame(const AcarsHfdlReceiver::Frame& frame) :
            Message(),
            m_frame(frame),
            m_dateTime(QDateTime::currentDateTime())
        { }
    };

    // A demodulated Aero signal unit that does not carry an ACARS message (log-on and
    // log-off, channel control and assignment, acknowledgements, system tables). The
    // reassembled ACARS blocks go out as MainCore::MsgPacket instead, as on every other
    // protocol, so consumers of that message always see pure ACARS bytes.
    // Which protocol the packets that FOLLOW came off, pushed by the sink into the same
    // queue the packets themselves go to.
    //
    // The worker cannot take this from its own settings. The sink and the worker are
    // configured through independent queues, so they change mode at different instants,
    // and a packet demodulated under the old mode can reach the worker after its
    // settings have already moved to the new one - which parses it under the wrong
    // protocol, and so under the wrong reassembly rules. Stamping it where the packets are
    // produced makes the association exact rather than merely likely.
    class MsgProtocolChange : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getMode() const { return m_mode; }

        static MsgProtocolChange* create(int mode) {
            return new MsgProtocolChange(mode);
        }

    private:
        int m_mode;

        MsgProtocolChange(int mode) :
            Message(),
            m_mode(mode)
        { }
    };


    class MsgAeroFrame : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const AcarsAeroReceiver::Frame& getFrame() const { return m_frame; }
        QDateTime getDateTime() const { return m_dateTime; }

        static MsgAeroFrame* create(const AcarsAeroReceiver::Frame& frame)
        {
            return new MsgAeroFrame(frame);
        }

    private:
        AcarsAeroReceiver::Frame m_frame;
        QDateTime m_dateTime;

        MsgAeroFrame(const AcarsAeroReceiver::Frame& frame) :
            Message(),
            m_frame(frame),
            m_dateTime(QDateTime::currentDateTime())
        { }
    };

    // An ACARS block, plus what the bearer knew about it that the block itself does not
    // say. VDL-2 puts the aircraft's 24 bit ICAO address in the AVLC header of the frame
    // the block came out of, where the block names only a registration.
    //
    // A subclass rather than a separate message so that the address travels WITH its
    // packet: there is then no ordering to get right and no way for an address to be
    // applied to the wrong block. MainCore::MsgPacket::match() matches this too, because
    // Message::matchIdentifier defers to the base class, so the existing packet handling
    // - the APRS pipe, the UDP feed, the log - needs no change and sees a plain packet.
    class MsgAcarsPacket : public MainCore::MsgPacket {
        MESSAGE_CLASS_DECLARATION

    public:
        //!< 0 when the bearer gave no address, which is every bearer but VDL-2
        quint32 getIcao() const { return m_icao; }

        static MsgAcarsPacket* create(const QObject *pipeSource, QByteArray packet, quint32 icao)
        {
            return new MsgAcarsPacket(pipeSource, packet, QDateTime::currentDateTime(), icao);
        }

    private:
        quint32 m_icao;

        MsgAcarsPacket(const QObject *pipeSource, QByteArray packet, QDateTime dateTime,
                       quint32 icao) :
            MainCore::MsgPacket(pipeSource, packet, dateTime),
            m_icao(icao)
        { }
    };

    // A demodulated VDL-2 AVLC frame that does not carry an ACARS message (XID/GSIF,
    // X.25, supervisory frames). ACARS frames go out as MainCore::MsgPacket instead, so
    // downstream consumers of that message always see pure ACARS bytes.
    class MsgVdl2Frame : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const AcarsVdl2Receiver::Frame& getFrame() const { return m_frame; }
        QDateTime getDateTime() const { return m_dateTime; }

        static MsgVdl2Frame* create(const AcarsVdl2Receiver::Frame& frame)
        {
            return new MsgVdl2Frame(frame);
        }

    private:
        AcarsVdl2Receiver::Frame m_frame;
        QDateTime m_dateTime;

        MsgVdl2Frame(const AcarsVdl2Receiver::Frame& frame) :
            Message(),
            m_frame(frame),
            m_dateTime(QDateTime::currentDateTime())
        { }
    };

    AcarsDemod(DeviceAPI *deviceAPI);
    virtual ~AcarsDemod();
    virtual void destroy() { delete this; }
    virtual void setDeviceAPI(DeviceAPI *deviceAPI);
    virtual DeviceAPI *getDeviceAPI() { return m_deviceAPI; }

    using BasebandSampleSink::feed;
    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool po);
    virtual void start();
    virtual void stop();
    virtual void pushMessage(Message *msg) { m_inputMessageQueue.push(msg); }
    virtual QString getSinkName() { return objectName(); }

    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual QString getIdentifier() const { return objectName(); }
    virtual const QString& getURI() const { return getName(); }
    virtual void getTitle(QString& title) { title = m_settings.m_title; }
    virtual qint64 getCenterFrequency() const { return m_settings.m_inputFrequencyOffset;; }
    virtual void setCenterFrequency(qint64 frequency);

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual int getNbSinkStreams() const { return 1; }
    virtual int getNbSourceStreams() const { return 0; }
    virtual int getStreamIndex() const { return m_settings.m_streamIndex; }

    virtual qint64 getStreamCenterFrequency(int streamIndex, bool sinkElseSource) const
    {
        (void) streamIndex;
        (void) sinkElseSource;
        return 0;
    }

    virtual int webapiSettingsGet(
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

    virtual int webapiSettingsPutPatch(
            bool force,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

    static void webapiFormatChannelSettings(
            SWGSDRangel::SWGChannelSettings& response,
            const AcarsDemodSettings& settings);

    virtual int webapiReportGet(
            SWGSDRangel::SWGChannelReport& response,
            QString& errorMessage);

    static void webapiUpdateChannelSettings(
            AcarsDemodSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);

    ScopeVis *getScopeSink() { return m_basebandSink->getScopeSink(); }
    double getMagSq() const { return m_basebandSink->getMagSq(); }

    void getAeroQuality(double& evm, double& suRate) const {
        m_basebandSink->getAeroQuality(evm, suRate);
    }

    void getMagSqLevels(double& avg, double& peak, int& nbSamples) {
        m_basebandSink->getMagSqLevels(avg, peak, nbSamples);
    }
/*    void setMessageQueueToGUI(MessageQueue* queue) override {
        ChannelAPI::setMessageQueueToGUI(queue);
        m_basebandSink->setMessageQueueToGUI(queue);
    }*/

    uint32_t getNumberOfDeviceStreams() const;

    // The worker processes decoded frames on its own thread, in both GUI and
    // server builds; the GUI connects to it for display events
    AcarsDemodWorker *getWorker() { return m_worker; }

    static const char * const m_channelIdURI;
    static const char * const m_channelId;

private:
    DeviceAPI *m_deviceAPI;
    QThread m_thread;
    AcarsDemodBaseband* m_basebandSink;
    QThread *m_workerThread;
    AcarsDemodWorker *m_worker;
    AcarsDemodSettings m_settings;
    int m_basebandSampleRate; //!< stored from device message used when starting baseband sink
    qint64 m_centerFrequency;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    virtual bool handleMessage(const Message& cmd);
    void applySettings(const AcarsDemodSettings& settings, bool force = false);
    void webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const AcarsDemodSettings& settings, bool force);
    void webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const AcarsDemodSettings& settings,
        bool force
    );

private slots:
    void networkManagerFinished(QNetworkReply *reply);

};

#endif // INCLUDE_ACARSDEMOD_H
