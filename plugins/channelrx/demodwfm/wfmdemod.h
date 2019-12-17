///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
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

#ifndef INCLUDE_WFMDEMOD_H
#define INCLUDE_WFMDEMOD_H

#include <vector>

#include <QMutex>
#include <QNetworkRequest>

#include "dsp/basebandsamplesink.h"
#include "channel/channelapi.h"
#include "util/message.h"

#include "wfmdemodsettings.h"
#include "wfmdemodbaseband.h"

class QNetworkAccessManager;
class QNetworkReply;
class DeviceAPI;

class WFMDemod : public BasebandSampleSink, public ChannelAPI {
    Q_OBJECT
public:
    class MsgConfigureWFMDemod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const WFMDemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureWFMDemod* create(const WFMDemodSettings& settings, bool force)
        {
            return new MsgConfigureWFMDemod(settings, force);
        }

    private:
        WFMDemodSettings m_settings;
        bool m_force;

        MsgConfigureWFMDemod(const WFMDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

	WFMDemod(DeviceAPI *deviceAPI);
	virtual ~WFMDemod();
	virtual void destroy() { delete this; }

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool po);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& cmd);

    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual void getTitle(QString& title) { title = m_settings.m_title; }
    virtual qint64 getCenterFrequency() const { return m_settings.m_inputFrequencyOffset; }

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual int getNbSinkStreams() const { return 1; }
    virtual int getNbSourceStreams() const { return 0; }

    virtual qint64 getStreamCenterFrequency(int streamIndex, bool sinkElseSource) const
    {
        (void) streamIndex;
        (void) sinkElseSource;
        return m_settings.m_inputFrequencyOffset;
    }

	double getMagSq() const { return m_basebandSink->getMagSq(); }
    bool getSquelchOpen() const { return m_basebandSink->getSquelchOpen(); }

    void getMagSqLevels(double& avg, double& peak, int& nbSamples) { m_basebandSink->getMagSqLevels(avg, peak, nbSamples); }

    virtual int webapiSettingsGet(
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

    virtual int webapiSettingsPutPatch(
            bool force,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

    virtual int webapiReportGet(
            SWGSDRangel::SWGChannelReport& response,
            QString& errorMessage);

    static void webapiFormatChannelSettings(
        SWGSDRangel::SWGChannelSettings& response,
        const WFMDemodSettings& settings);

    static void webapiUpdateChannelSettings(
            WFMDemodSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    uint32_t getNumberOfDeviceStreams() const;

    static const QString m_channelIdURI;
    static const QString m_channelId;

private:
    DeviceAPI* m_deviceAPI;
    QThread *m_thread;
    WFMDemodBaseband* m_basebandSink;
    WFMDemodSettings m_settings;
    int m_basebandSampleRate; //!< stored from device message used when starting baseband sink

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    static const int m_udpBlockSize;

    void applySettings(const WFMDemodSettings& settings, bool force = false);

    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);
    void webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const WFMDemodSettings& settings, bool force);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

#endif // INCLUDE_WFMDEMOD_H
