///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2017, 2019-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com> //
// Copyright (C) 2015 John Greb <hexameron@spam.no>                              //
// Copyright (C) 2020 Kacper Michajłow <kasper93@gmail.com>                      //
// (C) 2015 John Greb                                                            //
// (C) 2020 Edouard Griffiths, F4EXB                                             //
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

#ifndef INCLUDE_MESHCOREDEMOD_H
#define INCLUDE_MESHCOREDEMOD_H

#include <vector>

#include <QNetworkRequest>
#include <QVector>

#include "dsp/basebandsamplesink.h"
#include "dsp/spectrumvis.h"
#include "channel/channelapi.h"
#include "util/message.h"
#include "util/udpsinkutil.h"

#include "meshcoredemodbaseband.h"

class QNetworkAccessManager;
class QNetworkReply;
class DeviceAPI;
class QThread;
class ObjectPipe;
class MeshcoreDemodDecoder;
namespace MeshcoreDemodMsg { class MsgReportDecodeBytes; }
namespace modemmeshcore { struct TxRadioSettings; struct DecodeResult; }

class MeshcoreDemod : public BasebandSampleSink, public ChannelAPI {
public:
    // Carries the full settings for each extra (non-primary) pipeline. Sent from GUI to demod
    // whenever the secondary pipeline list changes. Index 0 in the vector = pipeline id 1, etc.
    class MsgSetExtraPipelineSettings : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        const QVector<MeshcoreDemodSettings>& getSettingsList() const { return m_settingsList; }
        static MsgSetExtraPipelineSettings* create(const QVector<MeshcoreDemodSettings>& settingsList)
        {
            return new MsgSetExtraPipelineSettings(settingsList);
        }
    private:
        QVector<MeshcoreDemodSettings> m_settingsList;
        explicit MsgSetExtraPipelineSettings(const QVector<MeshcoreDemodSettings>& settingsList) :
            Message(), m_settingsList(settingsList)
        { }
    };

    class MsgConfigureMeshcoreDemod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const MeshcoreDemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureMeshcoreDemod* create(const MeshcoreDemodSettings& settings, bool force)
        {
            return new MsgConfigureMeshcoreDemod(settings, force);
        }

    private:
        MeshcoreDemodSettings m_settings;
        bool m_force;

        MsgConfigureMeshcoreDemod(const MeshcoreDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

	MeshcoreDemod(DeviceAPI* deviceAPI);
	virtual ~MeshcoreDemod();
	virtual void destroy() { delete this; }
    virtual void setDeviceAPI(DeviceAPI *deviceAPI);
    virtual DeviceAPI *getDeviceAPI() { return m_deviceAPI; }
    SpectrumVis *getSpectrumVis() { return &m_spectrumVis; }

    using BasebandSampleSink::feed;
    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool po);
	virtual void start();
	virtual void stop();
    virtual void pushMessage(Message *msg) { m_inputMessageQueue.push(msg); }
    virtual QString getSinkName() { return objectName(); }

    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual QString getIdentifier() const { return objectName(); }
    virtual void getTitle(QString& title) { title = m_settings.m_title; }
    virtual qint64 getCenterFrequency() const { return m_settings.m_inputFrequencyOffset; }
    int getChannelSampleRate() const override;
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
        const MeshcoreDemodSettings& settings);

    static void webapiUpdateChannelSettings(
            MeshcoreDemodSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    bool getDemodActive() const;
    double getCurrentNoiseLevel() const;
    double getTotalPower() const;

    uint32_t getNumberOfDeviceStreams() const;

    static const char* const m_channelIdURI;
    static const char* const m_channelId;

private:
    struct PipelineConfig
    {
        int id = -1;
        QString name;
        QString presetName;
        MeshcoreDemodSettings settings;
    };

    struct PipelineRuntime
    {
        int id = -1;
        QString name;
        QString presetName;
        MeshcoreDemodSettings settings;
        QThread *basebandThread = nullptr;
        QThread *decoderThread = nullptr;
        MeshcoreDemodBaseband *basebandSink = nullptr;
        MeshcoreDemodDecoder *decoder = nullptr;
    };

	DeviceAPI *m_deviceAPI;
    std::vector<PipelineConfig> m_pipelineConfigs;
    std::vector<PipelineRuntime> m_pipelines;
    int m_currentPipelineId;
    bool m_running;
    MeshcoreDemodSettings m_settings;
    SpectrumVis m_spectrumVis;
    int m_basebandSampleRate; //!< stored from device message used when starting baseband sink
    qint64 m_basebandCenterFrequency;
    bool m_haveBasebandCenterFrequency;
    float m_lastMsgSignalDb;
    float m_lastMsgNoiseDb;
    int m_lastMsgSyncWord;
    int m_lastMsgPacketLength;
    int m_lastMsgNbParityBits;
    bool m_lastMsgHasCRC;
    int m_lastMsgNbSymbols;
    int m_lastMsgNbCodewords;
    bool m_lastMsgEarlyEOM;
    bool m_lastMsgHeaderCRC;
    int m_lastMsgHeaderParityStatus;
    bool m_lastMsgPayloadCRC;
    int m_lastMsgPayloadParityStatus;
    QString m_lastMsgPipelineName;
    QString m_lastMsgTimestamp;
    QString m_lastMsgString;
    QString m_lastFrameType;
    QByteArray m_lastMsgBytes;
    UDPSinkUtil<uint8_t> m_udpSink;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

	virtual bool handleMessage(const Message& cmd);
    void applySettings(MeshcoreDemodSettings settings, bool force = false);
    QString buildMeshcoreJsonPacket(
        const MeshcoreDemodMsg::MsgReportDecodeBytes& msg,
        const modemmeshcore::DecodeResult& meshResult) const;
    void makePipelineConfigFromSettings(int configId, PipelineConfig& config, const MeshcoreDemodSettings& settings) const;
    MeshcoreDemodSettings makePipelineSettingsFromMeshRadio(
        const MeshcoreDemodSettings& baseSettings,
        const QString& presetName,
        const modemmeshcore::TxRadioSettings& meshRadio,
        qint64 selectedPresetFrequencyHz,
        bool haveSelectedPresetFrequency
    ) const;
    int findBandwidthIndexForHz(int bandwidthHz) const;
    void startPipelines(const std::vector<PipelineConfig>& configs);
    void stopPipelines();
    void applyPipelineRuntimeSettings(PipelineRuntime& runtime, const MeshcoreDemodSettings& settings, bool force);
    bool pipelineLayoutMatches(const std::vector<PipelineConfig>& configs) const;
    void syncPipelinesWithSettings(const MeshcoreDemodSettings& settings, bool force);
    void applyExtraPipelineSettings(const QVector<MeshcoreDemodSettings>& settingsList, bool force = false);
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);
    void webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const MeshcoreDemodSettings& settings, bool force);
    void sendChannelSettings(
        const QList<ObjectPipe*>& pipes,
        QList<QString>& channelSettingsKeys,
        const MeshcoreDemodSettings& settings,
        bool force
    );
    void webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const MeshcoreDemodSettings& settings,
        bool force
    );

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void handleIndexInDeviceSetChanged(int index);
};

#endif // INCLUDE_MESHCOREDEMOD_H
