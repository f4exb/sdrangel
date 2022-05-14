///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_CHANNELTX_MODDATV_DATVMOD_H_
#define PLUGINS_CHANNELTX_MODDATV_DATVMOD_H_

#include <vector>
#include <iostream>
#include <fstream>

#include <QMutex>
#include <QNetworkRequest>

#include "dsp/basebandsamplesource.h"
#include "channel/channelapi.h"
#include "util/message.h"

#include "datvmodsettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class QThread;
class DATVModBaseband;
class DeviceAPI;
class ObjectPipe;

class DATVMod : public BasebandSampleSource, public ChannelAPI {
public:
    class MsgConfigureDATVMod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const DATVModSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureDATVMod* create(const DATVModSettings& settings, bool force)
        {
            return new MsgConfigureDATVMod(settings, force);
        }

    private:
        DATVModSettings m_settings;
        bool m_force;

        MsgConfigureDATVMod(const DATVModSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    class MsgConfigureChannelizer : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getSourceSampleRate() const { return m_sourceSampleRate; }
        int getSourceCenterFrequency() const { return m_sourceCenterFrequency; }

        static MsgConfigureChannelizer* create(int sourceSampleRate, int sourceCenterFrequency) {
            return new MsgConfigureChannelizer(sourceSampleRate, sourceCenterFrequency);
        }

    private:
        int m_sourceSampleRate;
        int m_sourceCenterFrequency;

        MsgConfigureChannelizer(int sourceSampleRate, int sourceCenterFrequency) :
            Message(),
            m_sourceSampleRate(sourceSampleRate),
            m_sourceCenterFrequency(sourceCenterFrequency)
        { }
    };

    class MsgConfigureSourceCenterFrequency : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        int getSourceCenterFrequency() const { return m_sourceCenterFrequency; }

        static MsgConfigureSourceCenterFrequency *create(int sourceCenterFrequency) {
            return new MsgConfigureSourceCenterFrequency(sourceCenterFrequency);
        }

    private:
        int m_sourceCenterFrequency;

        MsgConfigureSourceCenterFrequency(int sourceCenterFrequency) :
            Message(),
            m_sourceCenterFrequency(sourceCenterFrequency)
        { }
    };

    class MsgConfigureTsFileName : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString& getFileName() const { return m_fileName; }

        static MsgConfigureTsFileName* create(const QString& fileName)
        {
            return new MsgConfigureTsFileName(fileName);
        }

    private:
        QString m_fileName;

        MsgConfigureTsFileName(const QString& fileName) :
            Message(),
            m_fileName(fileName)
        { }
    };

    class MsgConfigureTsFileSourceSeek : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        int getPercentage() const { return m_seekPercentage; }

        static MsgConfigureTsFileSourceSeek* create(int seekPercentage)
        {
            return new MsgConfigureTsFileSourceSeek(seekPercentage);
        }

    protected:
        int m_seekPercentage; //!< percentage of seek position from the beginning 0..100

        MsgConfigureTsFileSourceSeek(int seekPercentage) :
            Message(),
            m_seekPercentage(seekPercentage)
        { }
    };

    class MsgConfigureTsFileSourceStreamTiming : public Message {
        MESSAGE_CLASS_DECLARATION

    public:

        static MsgConfigureTsFileSourceStreamTiming* create()
        {
            return new MsgConfigureTsFileSourceStreamTiming();
        }

    private:

        MsgConfigureTsFileSourceStreamTiming() :
            Message()
        { }
    };

    class MsgGetUDPBitrate : public Message {
        MESSAGE_CLASS_DECLARATION

    public:

        static MsgGetUDPBitrate* create()
        {
            return new MsgGetUDPBitrate();
        }

    private:

        MsgGetUDPBitrate() :
            Message()
        { }
    };

    class MsgGetUDPBufferUtilization : public Message {
        MESSAGE_CLASS_DECLARATION

    public:

        static MsgGetUDPBufferUtilization* create()
        {
            return new MsgGetUDPBufferUtilization();
        }

    private:

        MsgGetUDPBufferUtilization() :
            Message()
        { }
    };

    //=================================================================

    DATVMod(DeviceAPI *deviceAPI);
    virtual ~DATVMod();
    virtual void destroy() { delete this; }
    virtual void setDeviceAPI(DeviceAPI *deviceAPI);
    virtual DeviceAPI *getDeviceAPI() { return m_deviceAPI; }

    virtual void start();
    virtual void stop();
    virtual void pull(SampleVector::iterator& begin, unsigned int nbSamples);
    virtual void pushMessage(Message *msg) { m_inputMessageQueue.push(msg); }
    virtual QString getSourceName() { return objectName(); }

    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual QString getIdentifier() const { return objectName(); }
    virtual void getTitle(QString& title) { title = m_settings.m_title; }
    virtual qint64 getCenterFrequency() const { return m_settings.m_inputFrequencyOffset; }
    virtual void setCenterFrequency(qint64 frequency);

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

    virtual int webapiWorkspaceGet(
            SWGSDRangel::SWGWorkspaceInfo& response,
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
        const DATVModSettings& settings);

    static void webapiUpdateChannelSettings(
            DATVModSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);


    uint32_t getNumberOfDeviceStreams() const;
    double getMagSq() const;
    int getEffectiveSampleRate() const;
    void setMessageQueueToGUI(MessageQueue* queue) override;

    static const char* const m_channelIdURI;
    static const char* const m_channelId;

private:
    DeviceAPI* m_deviceAPI;
    QThread *m_thread;
    DATVModBaseband* m_basebandSource;
    DATVModSettings m_settings;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    virtual bool handleMessage(const Message& cmd);
    void applySettings(const DATVModSettings& settings, bool force = false);
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);
    void webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const DATVModSettings& settings, bool force);
    void sendChannelSettings(
        const QList<ObjectPipe*>& pipes,
        QList<QString>& channelSettingsKeys,
        const DATVModSettings& settings,
        bool force
    );
    void webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const DATVModSettings& settings,
        bool force
    );

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

#endif /* PLUGINS_CHANNELTX_MODDATV_DATVMOD_H_ */
