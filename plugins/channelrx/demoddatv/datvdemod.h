///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4HKW                                                      //
// for F4EXB / SDRAngel                                                          //
// using LeanSDR Framework (C) 2016 F4DAV                                        //
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

#ifndef INCLUDE_DATVDEMOD_H
#define INCLUDE_DATVDEMOD_H

class DeviceAPI;

#include <QNetworkRequest>
#include <QThread>

#include "channel/channelapi.h"
#include "dsp/basebandsamplesink.h"
#include "dsp/devicesamplesource.h"
#include "dsp/dspcommands.h"
#include "util/message.h"

#include "datvdemodbaseband.h"

class QNetworkAccessManager;
class QNetworkReply;

class DATVDemod : public BasebandSampleSink, public ChannelAPI
{
	Q_OBJECT

public:

    DATVDemod(DeviceAPI *);
    virtual ~DATVDemod();

    virtual void destroy() { delete this; }
    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual void getTitle(QString& title) { title = objectName(); }
    virtual qint64 getCenterFrequency() const { return m_settings.m_centerFrequency; }

    virtual QByteArray serialize() const { return QByteArray(); }
    virtual bool deserialize(const QByteArray& data) { (void) data; return false; }

    using BasebandSampleSink::feed;
    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool po);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& cmd);

    virtual int getNbSinkStreams() const { return 1; }
    virtual int getNbSourceStreams() const { return 0; }
    uint32_t getNumberOfDeviceStreams() const;

    virtual qint64 getStreamCenterFrequency(int streamIndex, bool sinkElseSource) const
    {
        (void) streamIndex;
        (void) sinkElseSource;
        return m_settings.m_centerFrequency;
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
            const DATVDemodSettings& settings);

    static void webapiUpdateChannelSettings(
            DATVDemodSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    void setMessageQueueToGUI(MessageQueue* queue) override
    {
        ChannelAPI::setMessageQueueToGUI(queue);
        m_basebandSink->setMessageQueueToGUI(queue);
    }

    void SetTVScreen(TVScreen *objScreen) { m_basebandSink->setTVScreen(objScreen); }
    void setMERLabel(QLabel *merLabel) { m_basebandSink->setMERLabel(merLabel); }
    void setCNRLabel(QLabel *cnrLabel) { m_basebandSink->setCNRLabel(cnrLabel); }
    void setMERMeter(LevelMeterSignalDB *merMeter) { m_basebandSink->setMERMeter(merMeter); }
    void setCNRMeter(LevelMeterSignalDB *cnrMeter) { m_basebandSink->setCNRMeter(cnrMeter); }
    void SetVideoRender(DATVideoRender *objScreen) { m_basebandSink->SetVideoRender(objScreen); }
    DATVideostream *getVideoStream() { return m_basebandSink->getVideoStream(); }
    DATVUDPStream *getUDPStream() { return m_basebandSink->getUDPStream(); }
    bool audioActive() { return m_basebandSink->audioActive(); }
    bool audioDecodeOK() { return m_basebandSink->audioDecodeOK(); }
    bool videoActive() { return m_basebandSink->videoActive(); }
    bool videoDecodeOK() { return m_basebandSink->videoDecodeOK(); }
    bool udpRunning() { return m_basebandSink->udpRunning(); }

    bool playVideo() { return m_basebandSink->playVideo(); }

    double getMagSq() const { return m_basebandSink->getMagSq(); } //!< Beware this is scaled to 2^30
    int getModcodModulation() const { return m_basebandSink->getModcodModulation(); }
    int getModcodCodeRate() const { return m_basebandSink->getModcodCodeRate(); }
    bool isCstlnSetByModcod() const { return m_basebandSink->isCstlnSetByModcod(); }

    static const char* const m_channelIdURI;
    static const char* const m_channelId;

    class MsgConfigureDATVDemod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const DATVDemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureDATVDemod* create(const DATVDemodSettings& settings, bool force)
        {
            return new MsgConfigureDATVDemod(settings, force);
        }

    private:
        DATVDemodSettings m_settings;
        bool m_force;

        MsgConfigureDATVDemod(const DATVDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

private:
    DeviceAPI* m_deviceAPI;
    QThread m_thread;
    DATVDemodBaseband* m_basebandSink;
    DATVDemodSettings m_settings;
    int m_basebandSampleRate; //!< stored from device message used when starting baseband sink

    void applySettings(const DATVDemodSettings& settings, bool force = false);
    void webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const DATVDemodSettings& settings, bool force);
    void sendChannelSettings(
        QList<MessageQueue*> *messageQueues,
        QList<QString>& channelSettingsKeys,
        const DATVDemodSettings& settings,
        bool force
    );
    void webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const DATVDemodSettings& settings,
        bool force
    );

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

#endif // INCLUDE_DATVDEMOD_H
