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

#include <QDebug>

#include "dsp/dspengine.h"
#include "util/droneid.h"

#include "droneiddemod.h"
#include "droneiddemodworker.h"

MESSAGE_CLASS_DEFINITION(DroneIDDemodWorker::MsgBurst, Message)

DroneIDDemodWorker::DroneIDDemodWorker() :
    m_messageQueueToChannel(nullptr),
    m_decoder(DSPEngine::instance()->getFFTFactory()),
    m_maxIterations(8),
    m_queuedBursts(0)
{
    connect(&m_inputMessageQueue, &MessageQueue::messageEnqueued, this, &DroneIDDemodWorker::handleInputMessages);
}

DroneIDDemodWorker::~DroneIDDemodWorker()
{
    m_inputMessageQueue.clear();
}

void DroneIDDemodWorker::queueBurst(const std::vector<std::complex<float>>& samples, QDateTime dateTime, qint64 frequency)
{
    m_queuedBursts++;
    m_inputMessageQueue.push(MsgBurst::create(samples, dateTime, frequency));
}

void DroneIDDemodWorker::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != nullptr)
    {
        if (MsgBurst::match(*message))
        {
            decodeBurst(*(MsgBurst *) message);
            m_queuedBursts--;
        }
        delete message;
    }
}

void DroneIDDemodWorker::decodeBurst(const MsgBurst& burst)
{
    DroneIDDecoder::Result result;
    const std::vector<std::complex<float>>& samples = burst.getSamples();

    m_decoder.setMaxIterations(m_maxIterations);
    bool detected = m_decoder.decode(samples.data(), (int) samples.size(), result);

    QByteArray message;
    if (detected && result.m_crcValid)
    {
        QByteArray data((const char *) result.m_payload.data(), (int) result.m_payload.size());
        int length = DroneIDMessage::messageLength(data);
        // Strip zero padding
        message = length > 0 ? data.left(length) : data;
    }

    if (m_messageQueueToChannel)
    {
        DroneIDDemod::MsgBurstResult *msg = DroneIDDemod::MsgBurstResult::create(
            detected,
            result.m_crcValid,
            message,
            burst.getDateTime(),
            result.m_snr,
            result.m_cfo,
            result.m_evm,
            result.m_zcMetric,
            result.m_iterations,
            result.m_legacy,
            burst.getFrequency()
        );
        m_messageQueueToChannel->push(msg);
    }
}
