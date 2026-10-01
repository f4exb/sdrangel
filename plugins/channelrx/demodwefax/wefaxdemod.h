///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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

#ifndef INCLUDE_WEFAXDEMOD_H
#define INCLUDE_WEFAXDEMOD_H

#include <QImage>
#include <QNetworkRequest>
#include <QThread>

#include "channel/channelapi.h"
#include "dsp/basebandsamplesink.h"
#include "util/message.h"

#include "wefaxdemodbaseband.h"
#include "wefaxdemodsettings.h"

class DeviceAPI;
class QNetworkAccessManager;
class QNetworkReply;
class WefaxDemodImageWorker;

class WefaxDemod : public BasebandSampleSink, public ChannelAPI
{
public:
    class MsgConfigureWefaxDemod : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        const WefaxDemodSettings& getSettings() const { return m_settings; }
        const QStringList& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureWefaxDemod *create(
            const QStringList& settingsKeys,
            const WefaxDemodSettings& settings,
            bool force)
        {
            return new MsgConfigureWefaxDemod(settingsKeys, settings, force);
        }

    private:
        WefaxDemodSettings m_settings;
        QStringList m_settingsKeys;
        bool m_force;

        MsgConfigureWefaxDemod(
            const QStringList& settingsKeys,
            const WefaxDemodSettings& settings,
            bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        {}
    };

    class MsgDecoderAction : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        enum Action
        {
            StartPhasing,
            FinishPhasing,
            StartReceiving,
            Stop,
            Clear,
            Save
        };

        Action getAction() const { return m_action; }
        static MsgDecoderAction *create(Action action) { return new MsgDecoderAction(action); }

    private:
        Action m_action;
        explicit MsgDecoderAction(Action action) : Message(), m_action(action) {}
    };

    class MsgImage : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        const QImage& getImage() const { return m_image; }
        WefaxDecoder::State getState() const { return m_state; }
        int getPhasingLineCount() const { return m_phasingLineCount; }
        double getSamplesPerLine() const { return m_samplesPerLine; }
        double getClockCorrectionPpm() const { return m_clockCorrectionPpm; }
        double getAppliedClockCorrectionPpm() const { return m_appliedClockCorrectionPpm; }
        double getConfidence() const { return m_confidence; }
        int getIOC() const { return m_ioc; }
        int getLinesPerMinute() const { return m_linesPerMinute; }
        bool getActionSucceeded() const { return m_actionSucceeded; }
        quint64 getImageId() const { return m_imageId; }
        QString getCompletionReason() const { return m_completionReason; }
        QString getLastSaveError() const { return m_lastSaveError; }
        double getTuningErrorHz() const { return m_tuningErrorHz; }
        double getEffectiveLinesPerMinute() const { return m_effectiveLinesPerMinute; }
        QString getTimingSource() const { return m_timingSource; }
        QString getTimingStatus() const { return m_timingStatus; }
        double getChannelPowerDb() const { return m_channelPowerDb; }
        double getRequiredBandwidthHz() const { return m_requiredBandwidthHz; }
        bool getBandwidthSufficient() const { return m_bandwidthSufficient; }
        double getSlantCorrectionPpm() const { return m_slantCorrectionPpm; }
        void setSlantCorrectionPpm(double ppm) { m_slantCorrectionPpm = ppm; }
        int getAlignmentPx() const { return m_alignmentPx; }
        void setAlignmentPx(int pixels) { m_alignmentPx = pixels; }

        static MsgImage *create(
            const QImage& image,
            WefaxDecoder::State state,
            int phasingLineCount,
            double samplesPerLine,
            double clockCorrectionPpm,
            double appliedClockCorrectionPpm,
            double confidence,
            int ioc,
            int linesPerMinute,
            quint64 imageId,
            const QString& completionReason,
            const QString& lastSaveError,
            double tuningErrorHz,
            double effectiveLinesPerMinute,
            const QString& timingSource,
            const QString& timingStatus,
            double channelPowerDb,
            double requiredBandwidthHz,
            bool bandwidthSufficient,
            bool actionSucceeded)
        {
            return new MsgImage(
                image,
                state,
                phasingLineCount,
                samplesPerLine,
                clockCorrectionPpm,
                appliedClockCorrectionPpm,
                confidence,
                ioc,
                linesPerMinute,
                imageId,
                completionReason,
                lastSaveError,
                tuningErrorHz,
                effectiveLinesPerMinute,
                timingSource,
                timingStatus,
                channelPowerDb,
                requiredBandwidthHz,
                bandwidthSufficient,
                actionSucceeded);
        }

    private:
        QImage m_image;
        WefaxDecoder::State m_state;
        int m_phasingLineCount;
        double m_samplesPerLine;
        double m_clockCorrectionPpm;
        double m_appliedClockCorrectionPpm;
        double m_confidence;
        int m_ioc;
        int m_linesPerMinute;
        quint64 m_imageId;
        QString m_completionReason;
        QString m_lastSaveError;
        double m_tuningErrorHz;
        double m_effectiveLinesPerMinute;
        QString m_timingSource;
        QString m_timingStatus;
        double m_channelPowerDb;
        double m_requiredBandwidthHz;
        bool m_bandwidthSufficient;
        bool m_actionSucceeded;
        double m_slantCorrectionPpm = 0.0;
        int m_alignmentPx = 0;

        MsgImage(
            const QImage& image,
            WefaxDecoder::State state,
            int phasingLineCount,
            double samplesPerLine,
            double clockCorrectionPpm,
            double appliedClockCorrectionPpm,
            double confidence,
            int ioc,
            int linesPerMinute,
            quint64 imageId,
            const QString& completionReason,
            const QString& lastSaveError,
            double tuningErrorHz,
            double effectiveLinesPerMinute,
            const QString& timingSource,
            const QString& timingStatus,
            double channelPowerDb,
            double requiredBandwidthHz,
            bool bandwidthSufficient,
            bool actionSucceeded) :
            Message(),
            m_image(image),
            m_state(state),
            m_phasingLineCount(phasingLineCount),
            m_samplesPerLine(samplesPerLine),
            m_clockCorrectionPpm(clockCorrectionPpm),
            m_appliedClockCorrectionPpm(appliedClockCorrectionPpm),
            m_confidence(confidence),
            m_ioc(ioc),
            m_linesPerMinute(linesPerMinute),
            m_imageId(imageId),
            m_completionReason(completionReason),
            m_lastSaveError(lastSaveError),
            m_tuningErrorHz(tuningErrorHz),
            m_effectiveLinesPerMinute(effectiveLinesPerMinute),
            m_timingSource(timingSource),
            m_timingStatus(timingStatus),
            m_channelPowerDb(channelPowerDb),
            m_requiredBandwidthHz(requiredBandwidthHz),
            m_bandwidthSufficient(bandwidthSufficient),
            m_actionSucceeded(actionSucceeded)
        {}
    };

    explicit WefaxDemod(DeviceAPI *deviceAPI);
    ~WefaxDemod() override;

    void destroy() override { delete this; }
    void setDeviceAPI(DeviceAPI *deviceAPI) override;
    DeviceAPI *getDeviceAPI() override { return m_deviceAPI; }

    using BasebandSampleSink::feed;
    void feed(
        const SampleVector::const_iterator& begin,
        const SampleVector::const_iterator& end,
        bool firstOfBurst) override;
    void start() override;
    void stop() override;
    void pushMessage(Message *message) override { m_inputMessageQueue.push(message); }
    QString getSinkName() override { return objectName(); }

    void getIdentifier(QString& id) override { id = objectName(); }
    QString getIdentifier() const override { return objectName(); }
    void getTitle(QString& title) override { title = m_settings.m_title; }
    qint64 getCenterFrequency() const override { return m_settings.m_inputFrequencyOffset; }
    void setCenterFrequency(qint64 frequency) override;
    QByteArray serialize() const override { return m_settings.serialize(); }
    bool deserialize(const QByteArray& data) override;

    int getNbSinkStreams() const override { return 1; }
    int getNbSourceStreams() const override { return 0; }
    int getStreamIndex() const override { return m_settings.m_streamIndex; }
    qint64 getStreamCenterFrequency(int streamIndex, bool sinkElseSource) const override;

    int webapiSettingsGet(SWGSDRangel::SWGChannelSettings& response, QString& errorMessage) override;
    int webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage) override;
    int webapiReportGet(SWGSDRangel::SWGChannelReport& response, QString& errorMessage) override;
    int webapiActionsPost(
        const QStringList& channelActionsKeys,
        SWGSDRangel::SWGChannelActions& query,
        QString& errorMessage) override;
    int webapiWorkspaceGet(SWGSDRangel::SWGWorkspaceInfo& response, QString& errorMessage) override;

    static void webapiFormatChannelSettings(
        SWGSDRangel::SWGChannelSettings& response,
        const WefaxDemodSettings& settings);
    static void webapiUpdateChannelSettings(
        WefaxDemodSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response);
    static void webapiFormatReverseSettings(
        SWGSDRangel::SWGChannelSettings& response,
        const WefaxDemodSettings& settings,
        const QStringList& channelSettingsKeys,
        bool force);

    double getMagSq() const { return m_basebandSink->getMagSq(); }
    void getMagSqLevels(double& average, double& peak, int& sampleCount)
    {
        m_basebandSink->getMagSqLevels(average, peak, sampleCount);
    }
    QImage image() const;
    bool saveImage(const QString& fileName = QString());
    uint32_t getNumberOfDeviceStreams() const;

    static const char * const m_channelIdURI;
    static const char * const m_channelId;

protected:
    bool handleMessage(const Message& message) override;

private:
    DeviceAPI *m_deviceAPI;
    QThread m_thread;
    QThread m_imageThread;
    WefaxDemodBaseband *m_basebandSink;
    WefaxDemodImageWorker *m_imageWorker;
    WefaxDemodSettings m_settings;
    int m_basebandSampleRate;
    qint64 m_centerFrequency;
    QImage m_image;
    int m_imageRows;
    WefaxDecoder::State m_decoderState;
    int m_phasingLineCount;
    double m_samplesPerLine;
    double m_clockCorrectionPpm;
    double m_appliedClockCorrectionPpm;
    double m_confidence;
    quint64 m_imageQueueOverflows;
    quint64 m_imageId;
    QString m_completionReason;
    QString m_lastSaveError;
    double m_tuningErrorHz;
    double m_slantCorrectionPpm;
    int m_alignmentPx;
    double m_effectiveLinesPerMinute;
    QString m_timingSource;
    QString m_timingStatus;
    QString m_captureStartTime;
    qint64 m_captureFrequencyHz;
    int m_captureIOC;
    int m_captureLinesPerMinute;
    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    void applySettings(const QStringList& settingsKeys, const WefaxDemodSettings& settings, bool force);
    void startBasebandSink();
    void stopBasebandSink();
    void clearImage(bool newGeneration = true);
    bool appendLines(const WefaxDemodBaseband::MsgDecoderReport::Lines& lines);
    void forwardImageReport(const WefaxDemodBaseband::MsgDecoderReport& report);
    void handleImageUpdated(quint64 imageId, const QImage& image, int rowCount, double slantPpm, int alignmentPx);
    void sendCurrentImageReport(bool actionSucceeded = true);
    double requiredBandwidthHz() const;
    bool bandwidthSufficient() const;
    void webapiReverseSendSettings(
        const QStringList& channelSettingsKeys,
        const WefaxDemodSettings& settings,
        bool force);
    void webapiFormatReverseChannelSettings(
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        const WefaxDemodSettings& settings,
        bool force) const;

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

#endif // INCLUDE_WEFAXDEMOD_H
