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

#ifndef INCLUDE_DRMDEMODBASEBAND_H
#define INCLUDE_DRMDEMODBASEBAND_H

#include <QObject>
#include <QRecursiveMutex>
#include <atomic>
#include <deque>
#include <QMutex>

#include "dsp/samplesinkfifo.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "drmdemodsink.h"

class DownChannelizer;
class ChannelAPI;

class DRMDemodBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgResetDRMDemodBaseband : public Message
    {
        MESSAGE_CLASS_DECLARATION
    public:
        static MsgResetDRMDemodBaseband* create() {
            return new MsgResetDRMDemodBaseband();
        }
    private:
        MsgResetDRMDemodBaseband() = default;
    };

    class MsgConfigureDRMDemodBaseband : public Message
    {
        MESSAGE_CLASS_DECLARATION
    public:
        const DRMDemodSettings& getSettings() const { return m_settings; }
        const QStringList& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }
        static MsgConfigureDRMDemodBaseband* create(
            const DRMDemodSettings& settings,
            const QStringList& settingsKeys,
            bool force
        ) {
            return new MsgConfigureDRMDemodBaseband(settings, settingsKeys, force);
        }
    private:
        DRMDemodSettings m_settings;
        QStringList m_settingsKeys;
        bool m_force;
        MsgConfigureDRMDemodBaseband(
            const DRMDemodSettings& settings,
            const QStringList& settingsKeys,
            bool force
        ) :
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        {}
    };

    DRMDemodBaseband();
    ~DRMDemodBaseband();

    void reset();
    void startWork();
    void stopWork();
    void feed(
        const SampleVector::const_iterator& begin,
        const SampleVector::const_iterator& end
    );
    void setBasebandSampleRate(int sampleRate);
    void markStreamBoundary(bool endOfStream);
    int getChannelSampleRate() const;
    int getAudioSampleRate() const { return m_sink.getAudioSampleRate(); }
    void getMagSqLevels(double& average, double& peak, int& samples) {
        m_sink.getMagSqLevels(average, peak, samples);
    }

    bool isRunning() const { return m_running; }
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    DRMDemodStatus getStatus() const {
        auto status = m_sink.getStatus();
        status.inputDroppedSamples = m_inputDroppedSamples.load();
        return status;
    }

    void setFifoLabel(const QString& label) { m_sampleFifo.setLabel(label); }
    void setChannel(ChannelAPI *channel) { m_sink.setChannel(channel); }
    bool getChannelPowerDB(double& db) const { return m_sink.getChannelPowerDB(db); }
    void setAudioFifoLabel(const QString& label) { m_sink.setAudioFifoLabel(label); }

private:
    friend class DRMDemodBasebandTest;
    SampleSinkFifo m_sampleFifo;
    DownChannelizer *m_channelizer;
    DRMDemodSink m_sink;
    MessageQueue m_inputMessageQueue;
    DRMDemodSettings m_settings;
    std::atomic_bool m_running;
    // Set by stopWork before it waits for the worker, so a long drain stops.
    std::atomic_bool m_stopping{false};
    bool m_drainedAtEOF = false; // the last thing handled was an EOF drain
    // The device's rate and centre frequency last notified
    int m_notifiedSampleRate = -1;
    qint64 m_notifiedCenterFrequency = 0;
    QRecursiveMutex m_mutex;
    std::atomic<quint64> m_inputDroppedSamples{0};
    std::atomic<int> m_channelSampleRate{0}; // the channelizer's, for other threads
    QMutex m_streamMutex;
    quint64 m_writtenSamples = 0;
    quint64 m_readSamples = 0;
    // Boundaries and gaps carry the order they were recorded in, which
    // decides between the two at the same stream position.
    quint64 m_streamSequence = 0;
    struct StreamBoundary { quint64 sample; bool endOfStream; quint64 sequence; };
    std::deque<StreamBoundary> m_streamBoundaries;
    // Samples dropped by the FIFO, at the stream position where they belong.
    struct InputGap { quint64 sample; quint64 length; quint64 sequence; };
    std::deque<InputGap> m_inputGaps;

    static unsigned int fifoSize(int sampleRate);
    void scheduleHandleData();
    void invalidateInput(bool retune, bool keepQueuedAudio = false);
    void resetDecoder(bool retune, bool keepQueuedAudio = false);
    void bridgeGap(quint64 basebandSamples);

    bool handleMessage(const Message& message);
    void applySettings(
        const DRMDemodSettings& settings,
        const QStringList& settingsKeys,
        bool force
    );

private slots:
    void handleInputMessages();
    void handleData();
};

#endif // INCLUDE_DRMDEMODBASEBAND_H
