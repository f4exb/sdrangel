///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#ifndef PLUGINS_CHANNELTX_MODDSE_DSEMOD_H_
#define PLUGINS_CHANNELTX_MODDSE_DSEMOD_H_

#include <fstream>

#include <QDateTime>
#include <QHash>
#include <QMap>
#include <QSet>
#include <QRecursiveMutex>
#include <QTimer>
#include <QElapsedTimer>
#include <QNetworkRequest>

#include "availablechannelorfeature.h"
#include "dsp/basebandsamplesource.h"
#include "dsp/spectrumvis.h"
#include "channel/channelapi.h"
#include "util/message.h"
#include "dsp/dv/dvangel61ip.h"

#include "dsemodsettings.h"
#include "dsemodaudiofile.h"
#include "dsemodchannelaccess.h"

class QNetworkAccessManager;
class QNetworkReply;
class QThread;
class DeviceAPI;
class Feature;
class CWKeyer;
class DSEModBaseband;
class ObjectPipe;

class MessageQueue;

class IPTunnel;

namespace SWGSDRangel {
    class SWGDSEModTransmitStatus;
    class SWGDSEModCapabilities;
}

class DSEMod : public BasebandSampleSource, public ChannelAPI {
public:
    class MsgConfigureDSEMod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const DSEModSettings& getSettings() const { return m_settings; }
        const QStringList& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureDSEMod* create(const QStringList& settingsKeys, const DSEModSettings& settings, bool force)
        {
            return new MsgConfigureDSEMod(settingsKeys, settings, force);
        }

    private:
        DSEModSettings m_settings;
        QStringList m_settingsKeys;
        bool m_force;

        MsgConfigureDSEMod(const QStringList& settingsKeys, const DSEModSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    class MsgConfigureFileSourceSeek : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getPercentage() const { return m_seekPercentage; }

        static MsgConfigureFileSourceSeek* create(int seekPercentage)
        {
            return new MsgConfigureFileSourceSeek(seekPercentage);
        }

    protected:
        int m_seekPercentage; //!< percentage of seek position from the beginning 0..100

        MsgConfigureFileSourceSeek(int seekPercentage) :
            Message(),
            m_seekPercentage(seekPercentage)
        { }
    };

    class MsgConfigureFileSourceStreamTiming : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgConfigureFileSourceStreamTiming* create()
        {
            return new MsgConfigureFileSourceStreamTiming();
        }

    private:
        MsgConfigureFileSourceStreamTiming() :
            Message()
        { }
    };

    class MsgReportFileSourceStreamTiming : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        std::size_t getSamplesCount() const { return m_samplesCount; }

        static MsgReportFileSourceStreamTiming* create(std::size_t samplesCount)
        {
            return new MsgReportFileSourceStreamTiming(samplesCount);
        }

    protected:
        std::size_t m_samplesCount;

        MsgReportFileSourceStreamTiming(std::size_t samplesCount) :
            Message(),
            m_samplesCount(samplesCount)
        { }
    };

    class MsgReportFileSourceStreamData : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getSampleRate() const { return m_sampleRate; }
        quint32 getRecordLength() const { return m_recordLength; }

        static MsgReportFileSourceStreamData* create(int sampleRate, quint32 recordLength)
        {
            return new MsgReportFileSourceStreamData(sampleRate, recordLength);
        }

    protected:
        int m_sampleRate;
        quint32 m_recordLength;

        MsgReportFileSourceStreamData(int sampleRate, quint32 recordLength) :
            Message(),
            m_sampleRate(sampleRate),
            m_recordLength(recordLength)
        { }
    };

    class MsgQueryAvailableAMBEFeatures : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgQueryAvailableAMBEFeatures* create()
        {
            return new MsgQueryAvailableAMBEFeatures();
        }

    private:
        MsgQueryAvailableAMBEFeatures() :
            Message()
        { }
    };

    //! What was heard of the Angel61 destination (channel -> GUI): its rate request and the one this station sends
    class MsgReportLinkState : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool isHeard() const { return m_heard; }
        int getRequestedTier() const { return m_requestedTier; }
        int getSendTier() const { return m_sendTier; }

        static MsgReportLinkState* create(bool heard, int requestedTier, int sendTier)
        {
            return new MsgReportLinkState(heard, requestedTier, sendTier);
        }

    private:
        bool m_heard;
        int m_requestedTier;    //!< The tier the destination asked of this station (2.10)
        int m_sendTier;         //!< The tier this station asks of the destination

        MsgReportLinkState(bool heard, int requestedTier, int sendTier) :
            Message(),
            m_heard(heard),
            m_requestedTier(requestedTier),
            m_sendTier(sendTier)
        { }
    };

    //! The GUI asks for the DSD Demodulators the Angel61 rate request can be taken from
    class MsgQueryAvailableDemodulators : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgQueryAvailableDemodulators* create()
        {
            return new MsgQueryAvailableDemodulators();
        }

    private:
        MsgQueryAvailableDemodulators() :
            Message()
        { }
    };

    //! The DSD Demodulators in the instance (channel -> GUI), for the Angel61 receiver combo
    class MsgReportAvailableDemodulators : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        AvailableChannelOrFeatureList& getDemodulators() { return m_availableDemodulators; }

        static MsgReportAvailableDemodulators* create()
        {
            return new MsgReportAvailableDemodulators();
        }

    private:
        AvailableChannelOrFeatureList m_availableDemodulators;

        MsgReportAvailableDemodulators() :
            Message()
        { }
    };

    class MsgReportAvailableAMBEFeatures : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        QList<DSEModSettings::AvailableAMBEFeature>& getFeatures() { return m_availableFeatures; }

        static MsgReportAvailableAMBEFeatures* create()
        {
            return new MsgReportAvailableAMBEFeatures();
        }

    private:
        QList<DSEModSettings::AvailableAMBEFeature> m_availableFeatures;

        MsgReportAvailableAMBEFeatures() :
            Message()
        { }
    };

    //! The actual host tunnel state (channel -> GUI), including validation and native errors
    class MsgReportTunnelStatus : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString& getStatus() const { return m_status; }

        static MsgReportTunnelStatus* create(const QString& status)
        {
            return new MsgReportTunnelStatus(status);
        }

    private:
        QString m_status;

        MsgReportTunnelStatus(const QString& status) :
            Message(),
            m_status(status)
        { }
    };

    //! IP payload totals for the current enabled tunnel session (channel -> GUI)
    class MsgReportTunnelTraffic : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        quint64 getTxBytes() const { return m_txBytes; }
        quint64 getRxBytes() const { return m_rxBytes; }
        const QString& getRefused() const { return m_refused; }   //!< what the filter kept back, by reason

        static MsgReportTunnelTraffic* create(quint64 txBytes, quint64 rxBytes, const QString& refused = QString())
        {
            return new MsgReportTunnelTraffic(txBytes, rxBytes, refused);
        }

    private:
        quint64 m_txBytes;
        quint64 m_rxBytes;
        QString m_refused;

        MsgReportTunnelTraffic(quint64 txBytes, quint64 rxBytes, const QString& refused) :
            Message(),
            m_txBytes(txBytes),
            m_rxBytes(rxBytes),
            m_refused(refused)
        { }
    };

    //=================================================================

    DSEMod(DeviceAPI *deviceAPI);
    virtual ~DSEMod();
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

    virtual int webapiReportGet(
            SWGSDRangel::SWGChannelReport& response,
            QString& errorMessage);

    virtual int webapiActionsPost(
            const QStringList& channelActionsKeys,
            SWGSDRangel::SWGChannelActions& query,
            QString& errorMessage);

    static void webapiFormatChannelSettings(
        SWGSDRangel::SWGChannelSettings& response,
        const DSEModSettings& settings);

    static void webapiUpdateChannelSettings(
            DSEModSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    //! The processor's transmit status (getAPIReport) into the report's generated class, through its setters
    static void webapiFormatTransmitStatus(SWGSDRangel::SWGDSEModTransmitStatus *swg, const QJsonObject& status);
    //! DSEModTxSession::capabilities and the transcode fields into the report's generated class
    static void webapiFormatCapabilities(SWGSDRangel::SWGDSEModCapabilities *swg, const QJsonObject& capabilities);

    SpectrumVis *getSpectrumVis() { return &m_spectrumVis; }
    double getMagSq() const;
    CWKeyer *getCWKeyer();
    void setLevelMeter(QObject *levelMeter);
    uint32_t getNumberOfDeviceStreams() const;
    int getAudioSampleRate() const;
    int getPcmSampleRate() const; //!< Rate of the encoder audio (the audio spectrum's rate)
    bool isTransmitting() const;
    bool isVocoderReady() const;
    uint32_t getEncodedFrameCount() const;
    uint32_t getUnderrunCount() const;
    uint32_t getOverrunCount() const;
    uint32_t getTrimCount() const;
    uint32_t getTxFrameCount() const;
    uint32_t getAudioUnderrunCount() const;
    uint32_t getAudioOverrunCount() const;

    static const char* const m_channelIdURI;
    static const char* const m_channelId;

private:
    DeviceAPI* m_deviceAPI;
    QThread *m_thread;
    DSEModBaseband* m_basebandSource;
    DSEModSettings m_settings;
    SpectrumVis m_spectrumVis;
    QRecursiveMutex m_fileMutex; //!< Guards m_ifstream between this thread (open, seek) and the DSP thread (read)

    DSEModAudioFile m_audioFile; //!< The speech file (.wav or .raw) of the audioFileName setting, read by the DSP thread under m_fileMutex
    quint32 m_recordLength; //!< record length in seconds
    int m_fileSampleRate;

    QHash<Feature*, DSEModSettings::AvailableAMBEFeature> m_availableAMBEFeatures;
    Feature *m_connectedAMBEFeature = nullptr;   //!< The feature whose port the processor was given (identity only)
    bool m_dtmfAutoTransmit;               //!< Transmission was started for a DTMF sequence and stops when it is sent
    bool m_messageAutoTransmit;            //!< Transmission keyed up for an Angel61 message, keyed down when it has been sent
    bool m_fileAutoTransmit;               //!< Transmission keyed up for a file transfer round (3.4)
    int m_fileState;                       //!< DVAngel61FileSender::State, as last reported
    QTimer m_fileAnswerTimer;              //!< A round's transmission ended: the receiver's answer is due within it
    IPTunnel *m_ipTunnel;                  //!< the host interface, owned here: this is the side that keys up
    bool m_tunnelAutoTransmit;
    DVAngel61IP::Control m_tunnelPeer;     //!< what the far station last advertised
    bool m_tunnelPeerValid;
    int m_tunnelSent;                      //!< datagrams put on the air
    int m_tunnelReceived;
    int m_tunnelRefused;                   //!< what the filter of 3.5 would not carry
    int m_tunnelRxRefused;                 //!< datagrams from the radio that did not reach the host
    QString m_tunnelStatus;                //!< the last status sent to the GUI
    int m_tunnelRefusedBy[DVAngel61IP::NbRefusals]; //!< ... by the rule that refused it
    int m_tunnelUnroutable;                //!< ... outside the prefixes the peer routes
    DVAngel61IP::Filter m_tunnelFilter;    //!< the operator's allow list and multicast groups
    QString m_tunnelFilterError;           //!< a term of theirs that could not be read: nothing is sent
    DSEModChannelAccess m_channelAccess;   //!< when the tunnel may key up (3.5)
    QTimer m_tunnelAccessTimer;            //!< ... the key up it waits for
    QTimer m_tunnelAdvertiseTimer;         //!< the advertise every 30 s while the tunnel is up
    QElapsedTimer m_clock;                 //!< channel access's monotonic clock
    int m_tunnelTxInTransmission;          //!< datagrams put in the transmission going on: it expects an answer
    bool m_tunnelOwed;                     //!< tunnel control for the peer is queued: sent in the reply slot
    qint64 m_lastTransmissionEndMs;        //!< m_clock's, -1 for none
    quint64 m_tunnelTxBytes;               //!< IP payload accepted from the host for radio transmission
    quint64 m_tunnelRxBytes;               //!< IP payload accepted from the radio for host injection

    /** What a DSD Demodulator heard from an Angel61 station (ANGEL61.md 2.10) */
    struct Angel61Station
    {
        QString m_destination;          //!< Of its last transmission
        int m_requestedTier = 0;        //!< The tier it asked of its destination
        int m_sendTier = 0;             //!< The tier our receiver would ask of it
        QString m_profile;              //!< NB or WB, the profile it was heard on
        QDateTime m_heard;
    };
    QMap<QString, Angel61Station> m_angel61Stations; //!< By callsign
    static const int MaxAngel61Stations = 256;
    QSet<ChannelAPI*> m_angel61Demodulators;         //!< DSD Demodulators listened to
    AvailableChannelOrFeatureList m_availableDemodulators; //!< The same, for the GUI's combo
    const QObject *m_angel61DemodSelected;           //!< The one the angel61Demod setting names, null for any of them
    int m_linkRequestedTier;
    int m_linkSendTier;
    bool m_linkHeard;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    virtual bool handleMessage(const Message& cmd);
    void applySettings(const QStringList& settingsKeys, const DSEModSettings& settings, bool force = false);
    void openFileStream();
    void seekFileStream(int seekPercentage);
    void scanAvailableAMBEFeatures();
    void notifyUpdateAMBEFeatures();
    void applyAMBEFeature(const DSEModSettings& settings);
    void setTransmit(bool on);
    void scanDSDDemodulators();
    void listenToDemodulator(ChannelAPI *channel);
    void notifyUpdateDemodulators();
    void applyDemodulatorSelection(const DSEModSettings& settings);
    void updateLinkState();
    //! Read the file of the angel61FileName setting and offer it (3.4); false with the reason logged
    bool sendFile(const QString& fileName);
    //! The largest file the Angel61 transfer can carry at the configured rate
    qint64 angel61FileCapacity() const;
    //! Whether a file control from the demodulator pipe is addressed to this station
    bool angel61FileControlIsOurs(const QString& destination) const;
    void sendAngel61FileControl(const QString& destination, const QByteArray& control);
    int fileAnswerTimeoutMs() const;
    void fileAnswerTimeout();

    // ---- IP tunnelling (ANGEL61.md 3.5) ------------------------------------------------------
    //! Open, reconfigure or close the host interface to match the settings, and tell the protocol
    void updateTunnel(const QStringList& settingsKeys, bool force = false);
    //! Tell the modulator that the host interface is ready (or no longer ready)
    void setTunnelProtocolEnabled(bool enabled);
    //! Send the real asynchronous host-interface state to the GUI
    void reportTunnelStatus(const QString& status);
    //! Send the current IP payload byte totals to the GUI
    void reportTunnelTraffic();
    //! Tell the instance's DSD Demodulators what the tunnel makes of what they hear
    void reportTunnelToDemodulators();
    void updateTunnelFilter(const DSEModSettings& settings);
    void updateChannelAccessTiming();
    void tunnelAdvertiseTimeout();
    bool tunnelKeyUpAllowed(bool datagrams);
    QString tunnelRefusedText() const;
    //! Close and dispose of the host interface without leaving callbacks from the old instance
    void destroyTunnel();
    //! Everything the host has for us, filtered and queued for the air
    void takeTunnelPackets();
    //! The advertise this station sends: what it is, and what it will route
    DVAngel61IP::Control tunnelAdvertise() const;
    //! The prefixes of a comma separated setting ("192.168.20.0/24, 2001:db8::/64")
    static std::vector<DVAngel61IP::Prefix> parsePrefixes(const QString& text);
    //! One datagram heard on the air, from the demodulator's pipe
    void injectTunnelDatagram(const QString& source, const QByteArray& datagram);
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);
    void webapiReverseSendSettings(const QStringList& channelSettingsKeys, const DSEModSettings& settings, bool force);
    void webapiReverseSendCWSettings(const CWKeyerSettings& cwKeyerSettings);
    void sendChannelSettings(
        const QList<ObjectPipe*>& pipes,
        const QStringList& channelSettingsKeys,
        const DSEModSettings& settings,
        bool force
    );
    void webapiFormatChannelSettings(
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const DSEModSettings& settings,
        bool force
    );

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void handleFeatureAdded(int featureSetIndex, Feature *feature);
    void handleFeatureRemoved(int featureSetIndex, Feature *feature);
    void handleChannelAdded(int deviceSetIndex, ChannelAPI *channel);
    void handleChannelRemoved(int deviceSetIndex, ChannelAPI *channel);
    void handleDemodulatorMessageQueue(MessageQueue *messageQueue);
};

#endif /* PLUGINS_CHANNELTX_MODDSE_DSEMOD_H_ */
