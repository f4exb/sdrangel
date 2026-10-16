///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021, 2023 Jon Beniston, M7RCE <jon@beniston.com>               //
// Copyright (C) 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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

#ifndef INCLUDE_DABDEMOD_H
#define INCLUDE_DABDEMOD_H

#include <map>
#include <memory>
#include <vector>

#include <QNetworkRequest>
#include <QHash>
#include <QList>
#include <QMap>
#include <QPair>
#include <QSet>
#include <QMutex>
#include <QThread>

#include "dsp/basebandsamplesink.h"
#include "channel/channelapi.h"
#include "util/message.h"

#include "modemdab/dabensemble.h"
#include "modemdr/journaline.h"
#include "modemdr/mot.h"
#include "modemdr/spi.h"

#include "dabdemodbaseband.h"
#include "dabdemodsettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class QThread;
class DeviceAPI;

class DABDemod : public BasebandSampleSink, public ChannelAPI {
public:
    class MsgConfigureDABDemod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const DABDemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }
        const QStringList& getSettingsKeys() const { return m_settingsKeys; }

        static MsgConfigureDABDemod* create(const QStringList& settingsKeys, const DABDemodSettings& settings, bool force)
        {
            return new MsgConfigureDABDemod(settingsKeys, settings, force);
        }

    private:
        DABDemodSettings m_settings;
        QStringList m_settingsKeys;
        bool m_force;

        MsgConfigureDABDemod(const QStringList& settingsKeys, const DABDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    class MsgDABEnsembleName : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString getName() const { return m_name; }
        int getId() const { return m_id; }

        static MsgDABEnsembleName* create(const QString& name, int id)
        {
            return new MsgDABEnsembleName(name, id);
        }

    private:
        QString m_name;
        int m_id;

        MsgDABEnsembleName(const QString& name, int id) :
            Message(),
            m_name(name),
            m_id(id)
        { }
    };

    class MsgDABProgramName : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString getName() const { return m_name; }
        int getId() const { return m_id; }

        static MsgDABProgramName* create(const QString& name, int id)
        {
            return new MsgDABProgramName(name, id);
        }

    private:
        QString m_name;
        int m_id;

        MsgDABProgramName(const QString& name, int id) :
            Message(),
            m_name(name),
            m_id(id)
        { }
    };

    class MsgDABProgramData: public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getBitrate() const { return m_bitrate; }
        const QString getAudio() const { return m_audio; }
        const QString getLanguage() const { return m_language; }
        const QString getProgramType() const { return m_programType; }


        static MsgDABProgramData* create(int bitrate, const QString& audio, const QString& language, const QString& programType)
        {
            return new MsgDABProgramData(bitrate, audio, language, programType);
        }

    private:
        int m_bitrate;
        QString m_audio;
        QString m_language;
        QString m_programType;

        MsgDABProgramData(int bitrate, const QString& audio, const QString& language, const QString& programType) :
            Message(),
            m_bitrate(bitrate),
            m_audio(audio),
            m_language(language),
            m_programType(programType)
        { }
    };

    class MsgDABSystemData : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getSync() const { return m_sync; }
        float getSNR() const { return m_snr; }
        int32_t getFrequencyOffset() const { return m_frequencyOffset; }

        static MsgDABSystemData* create(bool sync, float snr, int32_t frequencyOffset)
        {
            return new MsgDABSystemData(sync, snr, frequencyOffset);
        }

    private:
        bool m_sync;
        float m_snr;
        int32_t m_frequencyOffset;

        MsgDABSystemData(bool sync, float snr, int32_t frequencyOffset) :
            Message(),
            m_sync(sync),
            m_snr(snr),
            m_frequencyOffset(frequencyOffset)
        { }
    };

    class MsgDABProgramQuality : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getFrames() const { return m_frames; }
        int getRS() const { return m_rs; }
        int getAAC() const { return m_aac; }

        static MsgDABProgramQuality* create(int frames, int rs, int aac)
        {
            return new MsgDABProgramQuality(frames, rs, aac);
        }

    private:
        int m_frames;
        int m_rs;
        int m_aac;

        MsgDABProgramQuality(int frames, int rs, int aac) :
            Message(),
            m_frames(frames),
            m_rs(rs),
            m_aac(aac)
        { }
    };

    class MsgDABFIBQuality : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getPercent() const { return m_percent; }

        static MsgDABFIBQuality* create(int percent)
        {
            return new MsgDABFIBQuality(percent);
        }

    private:
        int m_percent;

        MsgDABFIBQuality(int percent) :
            Message(),
            m_percent(percent)
        { }
    };

    class MsgDABSampleRate : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getSampleRate() const { return m_sampleRate; }

        static MsgDABSampleRate* create(int sampleRate)
        {
            return new MsgDABSampleRate(sampleRate);
        }

    private:
        int m_sampleRate;

        MsgDABSampleRate(int sampleRate) :
            Message(),
            m_sampleRate(sampleRate)
        { }
    };

    class MsgDABData : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString getData() const { return m_data; }

        static MsgDABData* create(const QString& data)
        {
            return new MsgDABData(data);
        }

    private:
        QString m_data;

        MsgDABData(const QString& data) :
            Message(),
            m_data(data)
        { }
    };

    class MsgDABMOTData : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        QByteArray getData() const { return m_data; }
        const QString getFilename() const { return m_filename; }
        int getContentSubType() const { return m_contentSubType; }

        static MsgDABMOTData* create(QByteArray data, const QString& filename, int contentSubType)
        {
            return new MsgDABMOTData(data, filename, contentSubType);
        }

    private:
        QByteArray m_data;
        QString m_filename;
        int m_contentSubType;

        MsgDABMOTData(QByteArray data, const QString& filename, int contentSubType) :
            Message(),
            m_data(data),
            m_filename(filename),
            m_contentSubType(contentSubType)
        { }
    };

    // DL Plus tags of the selected service's dynamic label: content type name and text
    class MsgDABDLPlus : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QList<QPair<QString, QString>>& getTags() const { return m_tags; }
        bool getItemRunning() const { return m_itemRunning; }

        static MsgDABDLPlus* create(const QList<QPair<QString, QString>>& tags, bool itemRunning)
        {
            return new MsgDABDLPlus(tags, itemRunning);
        }

    private:
        QList<QPair<QString, QString>> m_tags;
        bool m_itemRunning;

        MsgDABDLPlus(const QList<QPair<QString, QString>>& tags, bool itemRunning) :
            Message(),
            m_tags(tags),
            m_itemRunning(itemRunning)
        { }
    };

    using JournalineObjects = std::map<int, modemdr::JournalineObject>;

    // The Journaline objects of the selected service
    class MsgDABJournaline : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        std::shared_ptr<const JournalineObjects> getObjects() const { return m_objects; }

        static MsgDABJournaline* create(std::shared_ptr<const JournalineObjects> objects)
        {
            return new MsgDABJournaline(objects);
        }

    private:
        std::shared_ptr<const JournalineObjects> m_objects;

        MsgDABJournaline(std::shared_ptr<const JournalineObjects> objects) :
            Message(),
            m_objects(objects)
        { }
    };

    // The selected service's announcement, programme on now and where else it can be received
    struct ServiceInfo
    {
        quint32 sid = 0;
        QString announcement;               //!< Type of the announcement in progress, empty for none
        QString announcementService;        //!< The service it comes from
        bool announcementSwitched = false;  //!< The audio is the announcement's
        QString programmeNow;               //!< From the SPI
        QString programmeNowDescription;
        std::vector<modemdab::DABServiceLink> links;
    };

    class MsgDABServiceInfo : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const ServiceInfo& getInfo() const { return m_info; }

        static MsgDABServiceInfo* create(const ServiceInfo& info)
        {
            return new MsgDABServiceInfo(info);
        }

    private:
        ServiceInfo m_info;

        MsgDABServiceInfo(const ServiceInfo& info) :
            Message(),
            m_info(info)
        { }
    };

    // The ensemble's Service and Programme Information
    class MsgDABSPI : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        std::shared_ptr<const modemdr::SPIData> getData() const { return m_data; }
        int getEnsembleId() const { return m_ensembleId; }

        static MsgDABSPI* create(std::shared_ptr<const modemdr::SPIData> data, int ensembleId)
        {
            return new MsgDABSPI(data, ensembleId);
        }

    private:
        std::shared_ptr<const modemdr::SPIData> m_data;
        int m_ensembleId;       //!< -1 if not known

        MsgDABSPI(std::shared_ptr<const modemdr::SPIData> data, int ensembleId) :
            Message(),
            m_data(data),
            m_ensembleId(ensembleId)
        { }
    };

    // The program's MOT Broadcast Website: the files received so far
    class MsgDABWebsite : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        std::shared_ptr<const modemdr::MOTWebsite> getWebsite() const { return m_website; }
        bool getPresent() const { return m_present; }

        static MsgDABWebsite* create(std::shared_ptr<const modemdr::MOTWebsite> website, bool present)
        {
            return new MsgDABWebsite(website, present);
        }

    private:
        std::shared_ptr<const modemdr::MOTWebsite> m_website;
        bool m_present;         //!< The program has a Broadcast Website

        MsgDABWebsite(std::shared_ptr<const modemdr::MOTWebsite> website, bool present) :
            Message(),
            m_website(website),
            m_present(present)
        { }
    };

    // SPI files of the ensemble kept from before, for the receiver
    class MsgDABSPICache : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const std::vector<modemdr::MOTFile>& getFiles() const { return m_files; }

        static MsgDABSPICache* create(const std::vector<modemdr::MOTFile>& files)
        {
            return new MsgDABSPICache(files);
        }

    private:
        std::vector<modemdr::MOTFile> m_files;

        MsgDABSPICache(const std::vector<modemdr::MOTFile>& files) :
            Message(),
            m_files(files)
        { }
    };

    static const int TIINone = -1;      //!< TII value when the ensemble sends none (or none is found yet)

    class MsgDABTII : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getTII() const { return m_tii; }

        static MsgDABTII* create(int tii)
        {
            return new MsgDABTII(tii);
        }

    private:
        int m_tii;

        MsgDABTII(int tii) :
            Message(),
            m_tii(tii)
        { }
    };

    // Measurements of the signal, for its quality and the gain setting
    struct SignalQuality
    {
        bool measured = false;          // The receiver's statistics are set (not in a default SignalQuality)
        float cn = 0.0f;                // dB, from the null symbol
        float nullDepth = 0.0f;         // dB
        float mscBER = 0.0f;            // Before error correction
        float ficBER = 0.0f;
        float rsCorrectedRate = 0.0f;   // Bytes per second
        int framesLost = 0;
        int reacquisitions = 0;
        float inputLevel = -100.0f;     // dBFS, mean power
        float clipping = 0.0f;          // % of samples
        int gainStatus = -1;            // GainStatus
        QString gainAdvice;
    };

    enum GainStatus {
        GainUnknown = -1,
        GainOK = 0,
        GainIncrease = 1,
        GainReduce = 2,
        GainCheck = 3       //!< MER well below C/N: overload, or multipath or interference
    };

    class MsgDABSignalQuality : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const SignalQuality& getQuality() const { return m_quality; }

        static MsgDABSignalQuality* create(const SignalQuality& quality)
        {
            return new MsgDABSignalQuality(quality);
        }

    private:
        SignalQuality m_quality;

        MsgDABSignalQuality(const SignalQuality& quality) :
            Message(),
            m_quality(quality)
        { }
    };

    class MsgDABCarrierCN : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const std::vector<float>& getCarrierCN() const { return m_carrierCN; }
        float getMedian() const { return m_median; }

        static MsgDABCarrierCN* create(const std::vector<float>& carrierCN, float median)
        {
            return new MsgDABCarrierCN(carrierCN, median);
        }

    private:
        std::vector<float> m_carrierCN;
        float m_median;

        MsgDABCarrierCN(const std::vector<float>& carrierCN, float median) :
            Message(),
            m_carrierCN(carrierCN),
            m_median(median)
        { }
    };

    class MsgDABReset : public Message {
        MESSAGE_CLASS_DECLARATION

    public:

        static MsgDABReset* create()
        {
            return new MsgDABReset();
        }

    private:

        MsgDABReset() :
            Message()
        { }
    };

    class MsgDABResetService : public Message {
        MESSAGE_CLASS_DECLARATION

    public:

        static MsgDABResetService* create()
        {
            return new MsgDABResetService();
        }

    private:

        MsgDABResetService() :
            Message()
        { }
    };


    DABDemod(DeviceAPI *deviceAPI);
    virtual ~DABDemod();
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
            const DABDemodSettings& settings);

    static void webapiUpdateChannelSettings(
            DABDemodSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    double getMagSq() const { return m_basebandSink->getMagSq(); }

    void getMagSqLevels(double& avg, double& peak, int& nbSamples) {
        m_basebandSink->getMagSqLevels(avg, peak, nbSamples);
    }
    bool isAudioActive() const { return m_basebandSink->isAudioActive(); }

    uint32_t getNumberOfDeviceStreams() const;
    int getAudioSampleRate() const override { return m_basebandSink->getAudioSampleRate(); }

    // Advice on the receiver gain from the input level, clipping, and the MER (snr) against
    // the C/N. Returns a GainStatus, and sets the text.
    static int gainAdvice(const SignalQuality& quality, bool sync, float snr, QString& advice);

    static const char * const m_channelIdURI;
    static const char * const m_channelId;

private:
    DeviceAPI *m_deviceAPI;
    QThread m_thread;
    DABDemodBaseband* m_basebandSink;
    DABDemodSettings m_settings;
    int m_basebandSampleRate; //!< stored from device message used when starting baseband sink
    qint64 m_centerFrequency;

    mutable QMutex m_reportMutex;
    bool m_reportSync;
    float m_reportSNR;
    int m_reportFrequencyOffset;
    QString m_reportEnsembleName;
    int m_reportEnsembleId;
    QMap<int, QString> m_reportPrograms;
    bool m_reportAudioActive;
    int m_reportBitrate;
    QString m_reportAudio;
    QString m_reportLanguage;
    QString m_reportProgramType;
    int m_reportFrameQuality;
    int m_reportReedSolomonQuality;
    int m_reportAACQuality;
    int m_reportFIBQuality;
    int m_reportTII;
    QString m_reportData;
    SignalQuality m_reportQuality;
    QList<QPair<QString, QString>> m_reportDLPlus;
    QString m_reportAnnouncement;
    QString m_reportProgrammeNow;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    virtual bool handleMessage(const Message& cmd);
    void applySettings(const QStringList& settingsKeys, const DABDemodSettings& settings, bool force = false);
    void sendSampleRateToDemodAnalyzer();
    void clearReport();
    void resetReceiver();
    void clearProgramReport();

    // The SPI (logos, station information and schedules) of each ensemble, kept between runs
    static constexpr int SPICacheDays = 14;     //!< Files not received again in this time are removed
    int m_spiCacheEnsemble;                     //!< Whose cache the receiver has, -1 for none
    QHash<QString, const void*> m_spiSaved;     //!< The data of each file saved, by name
    QSet<const void*> m_spiLoaded;              //!< The data loaded from the cache
    static QString spiCacheDirectory(int ensembleId);
    void loadSPICache(int ensembleId);
    void saveSPICache(const modemdr::SPIData& data, int ensembleId);
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);
    void webapiReverseSendSettings(const QList<QString>& channelSettingsKeys, const DABDemodSettings& settings, bool force);
    void webapiFormatChannelSettings(
        const QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const DABDemodSettings& settings,
        bool force
    );

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void handleIndexInDeviceSetChanged(int index);
};

#endif // INCLUDE_DABDEMOD_H
