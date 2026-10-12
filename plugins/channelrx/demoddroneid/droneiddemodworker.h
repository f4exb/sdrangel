///////////////////////////////////////////////////////////////////////////////////
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

#ifndef INCLUDE_DRONEIDDEMODWORKER_H
#define INCLUDE_DRONEIDDEMODWORKER_H

#include <atomic>
#include <complex>
#include <vector>

#include <QObject>
#include <QDateTime>

#include "util/message.h"
#include "util/messagequeue.h"

#include "droneiddecoder.h"

// Decodes bursts found by the detector in the sink. Runs in its own thread, as decoding a
// burst takes much longer than the time between samples.
class DroneIDDemodWorker : public QObject
{
    Q_OBJECT
public:
    class MsgBurst : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const std::vector<std::complex<float>>& getSamples() const { return m_samples; }
        QDateTime getDateTime() const { return m_dateTime; }
        qint64 getFrequency() const { return m_frequency; }

        static MsgBurst* create(const std::vector<std::complex<float>>& samples, QDateTime dateTime, qint64 frequency) {
            return new MsgBurst(samples, dateTime, frequency);
        }

    private:
        std::vector<std::complex<float>> m_samples;
        QDateTime m_dateTime;
        qint64 m_frequency;

        MsgBurst(const std::vector<std::complex<float>>& samples, QDateTime dateTime, qint64 frequency) :
            Message(),
            m_samples(samples),
            m_dateTime(dateTime),
            m_frequency(frequency)
        { }
    };

    DroneIDDemodWorker();
    ~DroneIDDemodWorker();

    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    void setMessageQueueToChannel(MessageQueue *messageQueue) { m_messageQueueToChannel = messageQueue; }
    void setMaxIterations(int iterations) { m_maxIterations = iterations; }

    // Whether the decoder is busy, so further bursts should be dropped
    bool isQueueFull() const { return m_queuedBursts >= m_maxQueuedBursts; }
    // Queue a burst for decoding. The caller (the single producer) should check isQueueFull() first.
    void queueBurst(const std::vector<std::complex<float>>& samples, QDateTime dateTime, qint64 frequency);

    static constexpr int m_maxQueuedBursts = 4;

private:
    MessageQueue m_inputMessageQueue;
    MessageQueue *m_messageQueueToChannel;
    DroneIDDecoder m_decoder;
    std::atomic<int> m_maxIterations;
    std::atomic<int> m_queuedBursts;

    void decodeBurst(const MsgBurst& burst);

private slots:
    void handleInputMessages();
};

#endif // INCLUDE_DRONEIDDEMODWORKER_H
