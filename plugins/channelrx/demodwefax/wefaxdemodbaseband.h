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

#ifndef INCLUDE_WEFAXDEMODBASEBAND_H
#define INCLUDE_WEFAXDEMODBASEBAND_H

#include <cstdint>
#include <vector>

#include <QObject>
#include <QRecursiveMutex>

#include "dsp/samplesinkfifo.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "wefaxdemodsink.h"

class DownChannelizer;

class WefaxDemodBaseband : public QObject
{
    Q_OBJECT

public:
    class MsgConfigureWefaxDemodBaseband : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        const WefaxDemodSettings& getSettings() const { return m_settings; }
        const QStringList& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureWefaxDemodBaseband *create(
            const QStringList& settingsKeys,
            const WefaxDemodSettings& settings,
            bool force)
        {
            return new MsgConfigureWefaxDemodBaseband(settingsKeys, settings, force);
        }

    private:
        WefaxDemodSettings m_settings;
        QStringList m_settingsKeys;
        bool m_force;

        MsgConfigureWefaxDemodBaseband(
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
            SourceChanged,
            Shutdown
        };

        Action getAction() const { return m_action; }
        static MsgDecoderAction *create(Action action) { return new MsgDecoderAction(action); }

    private:
        Action m_action;
        explicit MsgDecoderAction(Action action) : Message(), m_action(action) {}
    };

    class MsgDecoderReport : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        using Lines = std::vector<std::vector<std::uint8_t>>;

        const Lines& getLines() const { return m_lines; }
        WefaxDecoder::State getState() const { return m_state; }
        int getPhasingLineCount() const { return m_phasingLineCount; }
        double getSamplesPerLine() const { return m_samplesPerLine; }
        double getClockCorrectionPpm() const { return m_clockCorrectionPpm; }
        double getAppliedClockCorrectionPpm() const { return m_appliedClockCorrectionPpm; }
        double getConfidence() const { return m_confidence; }
        int getIOC() const { return m_ioc; }
        int getLinesPerMinute() const { return m_linesPerMinute; }
        bool getActionSucceeded() const { return m_actionSucceeded; }
        WefaxDemodSink::CompletionReason getCompletionReason() const { return m_completionReason; }
        double getTuningErrorHz() const { return m_tuningErrorHz; }
        double getEffectiveLinesPerMinute() const { return m_effectiveLinesPerMinute; }
        QString getTimingSource() const { return m_timingSource; }
        QString getTimingStatus() const { return m_timingStatus; }

        static MsgDecoderReport *create(
            Lines&& lines,
            WefaxDecoder::State state,
            int phasingLineCount,
            double samplesPerLine,
            double clockCorrectionPpm,
            double appliedClockCorrectionPpm,
            double confidence,
            int ioc,
            int linesPerMinute,
            WefaxDemodSink::CompletionReason completionReason,
            double tuningErrorHz,
            double effectiveLinesPerMinute,
            const QString& timingSource,
            const QString& timingStatus,
            bool actionSucceeded = true)
        {
            return new MsgDecoderReport(
                std::move(lines),
                state,
                phasingLineCount,
                samplesPerLine,
                clockCorrectionPpm,
                appliedClockCorrectionPpm,
                confidence,
                ioc,
                linesPerMinute,
                completionReason,
                tuningErrorHz,
                effectiveLinesPerMinute,
                timingSource,
                timingStatus,
                actionSucceeded);
        }

    private:
        Lines m_lines;
        WefaxDecoder::State m_state;
        int m_phasingLineCount;
        double m_samplesPerLine;
        double m_clockCorrectionPpm;
        double m_appliedClockCorrectionPpm;
        double m_confidence;
        int m_ioc;
        int m_linesPerMinute;
        WefaxDemodSink::CompletionReason m_completionReason;
        double m_tuningErrorHz;
        double m_effectiveLinesPerMinute;
        QString m_timingSource;
        QString m_timingStatus;
        bool m_actionSucceeded;

        MsgDecoderReport(
            Lines&& lines,
            WefaxDecoder::State state,
            int phasingLineCount,
            double samplesPerLine,
            double clockCorrectionPpm,
            double appliedClockCorrectionPpm,
            double confidence,
            int ioc,
            int linesPerMinute,
            WefaxDemodSink::CompletionReason completionReason,
            double tuningErrorHz,
            double effectiveLinesPerMinute,
            const QString& timingSource,
            const QString& timingStatus,
            bool actionSucceeded) :
            Message(),
            m_lines(std::move(lines)),
            m_state(state),
            m_phasingLineCount(phasingLineCount),
            m_samplesPerLine(samplesPerLine),
            m_clockCorrectionPpm(clockCorrectionPpm),
            m_appliedClockCorrectionPpm(appliedClockCorrectionPpm),
            m_confidence(confidence),
            m_ioc(ioc),
            m_linesPerMinute(linesPerMinute),
            m_completionReason(completionReason),
            m_tuningErrorHz(tuningErrorHz),
            m_effectiveLinesPerMinute(effectiveLinesPerMinute),
            m_timingSource(timingSource),
            m_timingStatus(timingStatus),
            m_actionSucceeded(actionSucceeded)
        {}
    };

    WefaxDemodBaseband();
    ~WefaxDemodBaseband();

    void reset();
    void startWork();
    void stopWork();
    // Ends an active capture and queues its final report, including rows the
    // sink was still holding. Call after stopWork() when the channel stops.
    void finishCapture(WefaxDemodSink::CompletionReason reason);
    void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);
    void setBasebandSampleRate(int sampleRate);
    void setMessageQueueToChannel(MessageQueue *messageQueue) { m_messageQueueToChannel = messageQueue; }
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    bool isRunning() const { return m_running; }
    double getMagSq() const { return m_sink.magnitudeSquared(); }
    void getMagSqLevels(double& average, double& peak, int& sampleCount)
    {
        m_sink.getMagSqLevels(average, peak, sampleCount);
    }
    void setFifoLabel(const QString& label) { m_sampleFifo.setLabel(label); }

private:
    SampleSinkFifo m_sampleFifo;
    DownChannelizer *m_channelizer;
    WefaxDemodSink m_sink;
    MessageQueue m_inputMessageQueue;
    MessageQueue *m_messageQueueToChannel;
    WefaxDemodSettings m_settings;
    bool m_running;
    QRecursiveMutex m_mutex;

    bool handleMessage(const Message& message);
    void applySettings(const QStringList& settingsKeys, const WefaxDemodSettings& settings, bool force);
    void sendDecoderReport(bool actionSucceeded = true);
    void sendDecoderReport(MsgDecoderReport::Lines&& lines, bool actionSucceeded = true);

private slots:
    void handleInputMessages();
    void handleData();
};

#endif // INCLUDE_WEFAXDEMODBASEBAND_H
