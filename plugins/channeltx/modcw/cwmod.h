///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 SDRangel Contributors                                      //
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

#ifndef PLUGINS_CHANNELTX_MODCW_CWMOD_H_
#define PLUGINS_CHANNELTX_MODCW_CWMOD_H_

#include <QRecursiveMutex>
#include <QNetworkRequest>

#include "dsp/basebandsamplesource.h"
#include "dsp/spectrumvis.h"
#include "channel/channelapi.h"
#include "util/message.h"

#include "cwmodsettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class QThread;
class DeviceAPI;
class CWModBaseband;
class ObjectPipe;

/**
 * CW (Morse code) modulator channel plugin.
 *
 * Generates a carrier that is keyed on and off according to International
 * Morse code encoding of the configured text, at the configured WPM speed.
 */
class CWMod : public BasebandSampleSource, public ChannelAPI
{
public:
    /** Message sent to request a settings change. */
    class MsgConfigureCWMod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const CWModSettings& getSettings() const { return m_settings; }
        const QStringList& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureCWMod* create(const QStringList& settingsKeys, const CWModSettings& settings, bool force)
        {
            return new MsgConfigureCWMod(settingsKeys, settings, force);
        }

    private:
        CWModSettings m_settings;
        QStringList m_settingsKeys;
        bool m_force;

        MsgConfigureCWMod(const QStringList& settingsKeys, const CWModSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    /** Trigger transmission of the currently configured text. */
    class MsgTx : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgTx* create() { return new MsgTx(); }

    private:
        MsgTx() : Message() { }
    };

    /** Trigger transmission of an arbitrary text string. */
    class MsgTXText : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgTXText* create(const QString& text)
        {
            return new MsgTXText(text);
        }

        QString m_text;

    private:
        explicit MsgTXText(const QString& text) : Message(), m_text(text) { }
    };

    //=================================================================

    explicit CWMod(DeviceAPI *deviceAPI);
    virtual ~CWMod();
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
    virtual int getStreamIndex() const { return m_settings.m_streamIndex; }

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

    SpectrumVis *getSpectrumVis() { return &m_spectrumVis; }
    double getMagSq() const;
    void setLevelMeter(QObject *levelMeter);
    uint32_t getNumberOfDeviceStreams() const;
    int getSourceChannelSampleRate() const;
    void setMessageQueueToGUI(MessageQueue* queue) final;

    static const char* const m_channelIdURI;
    static const char* const m_channelId;

private:
    DeviceAPI* m_deviceAPI;
    QThread *m_thread;
    CWModBaseband* m_basebandSource;
    CWModSettings m_settings;
    SpectrumVis m_spectrumVis;

    SampleVector m_sampleBuffer;
    QRecursiveMutex m_settingsMutex;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    virtual bool handleMessage(const Message& cmd);
    void applySettings(const QStringList& settingsKeys, const CWModSettings& settings, bool force = false);
    void sendSampleRateToDemodAnalyzer();
    void webapiReverseSendSettings(const QList<QString>& channelSettingsKeys, const CWModSettings& settings, bool force);
    void sendChannelSettings(
        const QList<ObjectPipe*>& pipes,
        const QList<QString>& channelSettingsKeys,
        const CWModSettings& settings,
        bool force
    );

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

#endif // PLUGINS_CHANNELTX_MODCW_CWMOD_H_
