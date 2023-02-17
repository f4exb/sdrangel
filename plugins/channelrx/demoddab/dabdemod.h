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

#ifndef INCLUDE_DABDEMOD_H
#define INCLUDE_DABDEMOD_H

#include <vector>

#include <QNetworkRequest>
#include <QThread>

#include "dsp/basebandsamplesink.h"
#include "channel/channelapi.h"
#include "util/message.h"

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

        static MsgConfigureDABDemod* create(const DABDemodSettings& settings, bool force)
        {
            return new MsgConfigureDABDemod(settings, force);
        }

    private:
        DABDemodSettings m_settings;
        bool m_force;

        MsgConfigureDABDemod(const DABDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
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
        int16_t getSNR() const { return m_snr; }
        int32_t getFrequencyOffset() const { return m_frequencyOffset; }

        static MsgDABSystemData* create(bool sync, int16_t snr, int32_t frequencyOffset)
        {
            return new MsgDABSystemData(sync, snr, frequencyOffset);
        }

    private:
        bool m_sync;
        int16_t m_snr;
        int32_t m_frequencyOffset;

        MsgDABSystemData(bool sync, int16_t snr, int32_t frequencyOffset) :
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
    virtual void setCenterFrequency(qint64 frequency);

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual int getNbSinkStreams() const { return 1; }
    virtual int getNbSourceStreams() const { return 0; }

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
/*    void setMessageQueueToGUI(MessageQueue* queue) override {
        ChannelAPI::setMessageQueueToGUI(queue);
        m_basebandSink->setMessageQueueToGUI(queue);
    }*/

    uint32_t getNumberOfDeviceStreams() const;
    int getAudioSampleRate() const { return m_basebandSink->getAudioSampleRate(); }

    static const char * const m_channelIdURI;
    static const char * const m_channelId;

private:
    DeviceAPI *m_deviceAPI;
    QThread m_thread;
    DABDemodBaseband* m_basebandSink;
    DABDemodSettings m_settings;
    int m_basebandSampleRate; //!< stored from device message used when starting baseband sink
    qint64 m_centerFrequency;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    virtual bool handleMessage(const Message& cmd);
    void applySettings(const DABDemodSettings& settings, bool force = false);
    void sendSampleRateToDemodAnalyzer();
    void webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const DABDemodSettings& settings, bool force);
    void webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const DABDemodSettings& settings,
        bool force
    );

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void handleIndexInDeviceSetChanged(int index);
};

#endif // INCLUDE_DABDEMOD_H
