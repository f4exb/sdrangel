///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef INCLUDE_UDPSINK_H
#define INCLUDE_UDPSINK_H

#include <QMutex>
#include <QNetworkRequest>

#include "dsp/basebandsamplesink.h"
#include "dsp/spectrumvis.h"
#include "channel/channelapi.h"
#include "util/message.h"

#include "udpsinkbaseband.h"

class QNetworkAccessManager;
class QNetworkReply;
class DeviceAPI;

class UDPSink : public BasebandSampleSink, public ChannelAPI {
	Q_OBJECT

public:
    class MsgConfigureUDPSink : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const UDPSinkSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureUDPSink* create(const UDPSinkSettings& settings, bool force)
        {
            return new MsgConfigureUDPSink(settings, force);
        }

    private:
        UDPSinkSettings m_settings;
        bool m_force;

        MsgConfigureUDPSink(const UDPSinkSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        {
        }
    };

	UDPSink(DeviceAPI *deviceAPI);
	virtual ~UDPSink();
	virtual void destroy() { delete this; }

    SpectrumVis *getSpectrumVis() { return &m_spectrumVis; }
    void enableSpectrum(bool enable);
    void setSpectrumPositiveOnly(bool positiveOnly) { m_basebandSink->setSpectrumPositiveOnly(positiveOnly); }
	double getMagSq() const { return m_basebandSink->getMagSq(); }
	double getInMagSq() const { return m_basebandSink->getInMagSq(); }
	bool getSquelchOpen() const { return m_basebandSink->getSquelchOpen(); }

    using BasebandSampleSink::feed;
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
        const UDPSinkSettings& settings);

    static void webapiUpdateChannelSettings(
            UDPSinkSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    uint32_t getNumberOfDeviceStreams() const;

    static const char* const m_channelIdURI;
    static const char* const m_channelId;
	static const int udpBlockSize = 512; // UDP block size in number of bytes

private slots:
    void networkManagerFinished(QNetworkReply *reply);

protected:
    DeviceAPI* m_deviceAPI;
    QThread *m_thread;
    UDPSinkBaseband* m_basebandSink;
    UDPSinkSettings m_settings;
    SpectrumVis m_spectrumVis;
    int m_basebandSampleRate; //!< stored from device message used when starting baseband sink

    int m_channelSampleRate;
    int m_channelFrequencyOffset;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    void applySettings(const UDPSinkSettings& settings, bool force = false);

    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);
    void webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const UDPSinkSettings& settings, bool force);
    void sendChannelSettings(
        QList<MessageQueue*> *messageQueues,
        QList<QString>& channelSettingsKeys,
        const UDPSinkSettings& settings,
        bool force
    );
    void webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const UDPSinkSettings& settings,
        bool force
    );
};

#endif // INCLUDE_UDPSINK_H
