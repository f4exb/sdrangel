///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_CHANNELTX_MODATV_ATVMOD_H_
#define PLUGINS_CHANNELTX_MODATV_ATVMOD_H_

#include <vector>
#include <iostream>
#include <fstream>

#include <QMutex>
#include <QNetworkRequest>

#include "dsp/basebandsamplesource.h"
#include "channel/channelapi.h"
#include "util/message.h"

#include "atvmodsettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class QThread;
class ATVModBaseband;
class DeviceAPI;
class ObjectPipe;

class ATVMod : public BasebandSampleSource, public ChannelAPI {
public:
    class MsgConfigureATVMod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const ATVModSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureATVMod* create(const ATVModSettings& settings, bool force)
        {
            return new MsgConfigureATVMod(settings, force);
        }

    private:
        ATVModSettings m_settings;
        bool m_force;

        MsgConfigureATVMod(const ATVModSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    /**
    * |<------ Baseband from device (before device soft interpolation) -------------------------->|
    * |<- Channel SR ------->|<- Channel SR ------->|<- Channel SR ------->|<- Channel SR ------->|
    * |             ^-------------------------------|
    * |             |        Source CF
    * |      | Source SR   |
    */
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

    class MsgConfigureImageFileName : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString& getFileName() const { return m_fileName; }

        static MsgConfigureImageFileName* create(const QString& fileName)
        {
            return new MsgConfigureImageFileName(fileName);
        }

    private:
        QString m_fileName;

        MsgConfigureImageFileName(const QString& fileName) :
            Message(),
            m_fileName(fileName)
        { }
    };

    class MsgConfigureVideoFileName : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString& getFileName() const { return m_fileName; }

        static MsgConfigureVideoFileName* create(const QString& fileName)
        {
            return new MsgConfigureVideoFileName(fileName);
        }

    private:
        QString m_fileName;

        MsgConfigureVideoFileName(const QString& fileName) :
            Message(),
            m_fileName(fileName)
        { }
    };

    class MsgConfigureVideoFileSourceSeek : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        int getPercentage() const { return m_seekPercentage; }

        static MsgConfigureVideoFileSourceSeek* create(int seekPercentage)
        {
            return new MsgConfigureVideoFileSourceSeek(seekPercentage);
        }

    protected:
        int m_seekPercentage; //!< percentage of seek position from the beginning 0..100

        MsgConfigureVideoFileSourceSeek(int seekPercentage) :
            Message(),
            m_seekPercentage(seekPercentage)
        { }
    };

    class MsgConfigureVideoFileSourceStreamTiming : public Message {
        MESSAGE_CLASS_DECLARATION

    public:

        static MsgConfigureVideoFileSourceStreamTiming* create()
        {
            return new MsgConfigureVideoFileSourceStreamTiming();
        }

    private:

        MsgConfigureVideoFileSourceStreamTiming() :
            Message()
        { }
    };

    class MsgConfigureCameraIndex : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        int getIndex() const { return m_index; }

        static MsgConfigureCameraIndex* create(int index)
        {
            return new MsgConfigureCameraIndex(index);
        }

    private:
        int m_index;

        MsgConfigureCameraIndex(int index) :
            Message(),
			m_index(index)
        { }
    };

    class MsgConfigureCameraData : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        int getIndex() const { return m_index; }
        float getManualFPS() const { return m_manualFPS; }
        bool getManualFPSEnable() const { return m_manualFPSEnable; }

        static MsgConfigureCameraData* create(
        		int index,
				float manualFPS,
				bool manualFPSEnable)
        {
            return new MsgConfigureCameraData(index, manualFPS, manualFPSEnable);
        }

    private:
        int m_index;
        float m_manualFPS;
        bool m_manualFPSEnable;

        MsgConfigureCameraData(int index, float manualFPS, bool manualFPSEnable) :
            Message(),
			m_index(index),
			m_manualFPS(manualFPS),
			m_manualFPSEnable(manualFPSEnable)
        { }
    };

    //=================================================================

    ATVMod(DeviceAPI *deviceAPI);
    virtual ~ATVMod();
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
        const ATVModSettings& settings);

    static void webapiUpdateChannelSettings(
            ATVModSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);


    uint32_t getNumberOfDeviceStreams() const;
    double getMagSq() const;
    void setLevelMeter(QObject *levelMeter);
    int getEffectiveSampleRate() const;
    void getCameraNumbers(std::vector<int>& numbers);
    void setMessageQueueToGUI(MessageQueue* queue) override;

    static const char* const m_channelIdURI;
    static const char* const m_channelId;

private:
    DeviceAPI* m_deviceAPI;
    QThread *m_thread;
    ATVModBaseband* m_basebandSource;
    ATVModSettings m_settings;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    virtual bool handleMessage(const Message& cmd);
    void applySettings(const ATVModSettings& settings, bool force = false);
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);
    void webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const ATVModSettings& settings, bool force);
    void sendChannelSettings(
        const QList<ObjectPipe*>& pipes,
        QList<QString>& channelSettingsKeys,
        const ATVModSettings& settings,
        bool force
    );
    void webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const ATVModSettings& settings,
        bool force
    );

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

#endif /* PLUGINS_CHANNELTX_MODAM_AMMOD_H_ */
