///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2025 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#include "dsp/samplemififo.h"
#include "spectran/devicespectran.h"
#include "spectranmisostreamworker.h"

SpectranMISOStreamWorker::SpectranMISOStreamWorker(SampleMIFifo* sampleMIFifo, SampleMOFifo* sampleMOFifo, QObject* parent) :
    QObject(parent),
    m_running(false),
    m_restart(false),
    m_sampleMIFifo(sampleMIFifo),
    m_sampleMOFifo(sampleMOFifo),
    m_device(nullptr)
{
    connect(&m_inputMessageQueue, &MessageQueue::messageEnqueued, this, &SpectranMISOStreamWorker::handleInputMessages, Qt::QueuedConnection);
    m_inputMessageQueueToGUI = nullptr;
    m_inputMessageQueueToSISO = nullptr;

    for (int i = 0; i < 2; i++) {
        m_convertBuffer[i].resize(262144, Sample{0, 0}); // should be enough for highest clock rate and no decimation
    }
}

SpectranMISOStreamWorker::~SpectranMISOStreamWorker()
{
    if (m_running) {
        stopWork();
    }
}

void SpectranMISOStreamWorker::startWork()
{
    if (m_running) {
        return;
    }

    qDebug("SpectranMISOStreamWorker::startWork");
    m_running = true;

    if (m_currentMode == SPECTRANMISO_MODE_2RX_RAW) {
        streamRaw2Rx();
    } else {
        streamIQ();
    }

    qDebug("SpectranMISOStreamWorker::startWork: exiting");
}

void SpectranMISOStreamWorker::stopWork()
{
    if (!m_running) {
        return;
    }

    qDebug("SpectranMISOStreamWorker::stopWork");
    m_running = false;
}

void SpectranMISOStreamWorker::restartWork()
{
    qDebug("SpectranMISOStreamWorker::restartWork");
    m_restart = true;
    stopWork();
}

void SpectranMISOStreamWorker::streamIQ()
{
    qDebug("SpectranMISOStreamWorker::streamIQ: starting");
    SampleVector::iterator convIt[2];
    AARTSAAPI_Result res;

    while (m_running)
    {
        // Prepare data packet
        AARTSAAPI_Packet packet = { sizeof(AARTSAAPI_Packet) };
        // Get the next data packet, sleep for some milliseconds, if none available yet.
        while (
            ((res = AARTSAAPI_GetPacket(m_device, 0, 0, &packet)) == AARTSAAPI_EMPTY)
            && m_running
        )
        {
            QThread::msleep(5);
        }

        if (res == AARTSAAPI_OK)
        {
            // qDebug() << "SpectranMISOStreamWorker::streamIQ: got packet with"
            //     << packet.num << "samples,"
            //     << packet.total << "total,"
            //     << packet.size << "size,"
            //     << packet.stride << "stride,"
            //     << packet.interleave << " interleave, "
            //     << (packet.endTime - packet.startTime)*1e3 << " ms, "
            //     << packet.startFrequency/1e6 << " MHz - "
            //     << (packet.startFrequency + packet.spanFrequency)/1e6 << " MHz, "
            //     << packet.rbwFrequency/1e3 << " kHz RBW, "
            //     << packet.stepFrequency/1e3 << " kHz Step, "
            //     << packet.spanFrequency/1e3 << " kHz Span";

            // Convert to Sample format
            convIt[0] = m_convertBuffer[0].begin();
            convIt[1] = m_convertBuffer[1].begin();

            if (SpectranMISOSettings::isDualRx(m_currentMode)) {
                m_decimatorsFloatIQ[0].decimate1_2(&convIt[0], &convIt[1], packet.fp32, 2 * packet.size * packet.num);
            } else {
                m_decimatorsFloatIQ[0].decimate1(&convIt[0], packet.fp32, packet.size * packet.num);
            }

            // Write samples to FIFO
            std::vector<SampleVector::const_iterator> vbegin;
            vbegin.push_back(m_convertBuffer[0].begin());
            vbegin.push_back(m_convertBuffer[1].begin());
            m_sampleMIFifo->writeSync(vbegin, packet.num);
            // Remove the first packet from the packet queue
            res = AARTSAAPI_ConsumePackets(m_device, 0, 1);

            if (res != AARTSAAPI_OK)
            {
                qWarning("SpectranMISOStreamWorker::streamIQ: AARTSAAPI_ConsumePackets() failed with error %d", res);
                m_running = false;
            }
        }
        else if (res != AARTSAAPI_EMPTY)
        {
            qWarning("SpectranMISOStreamWorker::streamIQ: AARTSAAPI_GetPacket() failed with error %d", res);
            m_running = false;
        }
    }

    if (m_restart)
    {
        qDebug("SpectranMISOStreamWorker::streamIQ: exit and restart");
        m_restart = false;
        emit restart();
    }
    else
    {
        qDebug("SpectranMISOStreamWorker::streamIQ: normal exit");
        emit stopped();
    }
}

void SpectranMISOStreamWorker::streamRaw2Rx()
{
    qDebug("SpectranMISOStreamWorker::streamRaw2Rx: starting");
    AARTSAAPI_Result res;
    SampleVector::iterator convIt[2];
	// Prepare data packet
	AARTSAAPI_Packet	packets[2] = { { sizeof(AARTSAAPI_Packet) }, { sizeof(AARTSAAPI_Packet) } };
	int					pi[2] = { 0, 0 };

	// Get the first data packets for both streams, sleep for some milliseconds, if none
	// available yet.

	for (int j = 0; j < 2; j++)
	{
        while (((res = AARTSAAPI_GetPacket(m_device, j, 0, packets + j)) == AARTSAAPI_EMPTY) && m_running)
            QThread::msleep(5);

		if (res != AARTSAAPI_OK)
        {
            qWarning("SpectranMISOStreamWorker::streamRaw2Rx: AARTSAAPI_GetPacket() stream %d [1] failed with error %d", j, res);
            m_running = false;
        }
	}

    while (m_running)
    {
		// Synchronize streams by time stamp

		if (packets[0].endTime < packets[1].startTime)
			pi[0] = packets[0].num;
		else if (packets[1].endTime < packets[0].startTime)
			pi[1] = packets[1].num;
		else if (packets[0].startTime < packets[1].startTime)
		{
			pi[1] = 0;
			pi[0] = (packets[1].startTime - packets[0].startTime) * packets[0].stepFrequency;
		}
		else
		{
			pi[0] = 0;
			pi[1] = (packets[0].startTime - packets[1].startTime) * packets[0].stepFrequency;
		}

        // Convert to Sample format
        convIt[0] = m_convertBuffer[0].begin();
        convIt[1] = m_convertBuffer[1].begin();

        m_decimatorsFloatIQ[0].decimate1(&convIt[0], packets[0].fp32, packets[0].size * packets[0].num);
        m_decimatorsFloatIQ[1].decimate1(&convIt[1], packets[1].fp32, packets[1].size * packets[1].num);

        // Write samples to FIFO
        std::vector<SampleVector::const_iterator> vbegin;
        vbegin.push_back(m_convertBuffer[0].begin());
        vbegin.push_back(m_convertBuffer[1].begin());
        m_sampleMIFifo->writeSync(vbegin, std::min(packets[0].num, packets[1].num));

		// Get fresh packets
        pi[0] += std::min(packets[0].num, packets[1].num);
        pi[1] += std::min(packets[0].num, packets[1].num);

		if (pi[0] >= packets[0].num)
		{
			// Remove the first packet from the packet queue
			AARTSAAPI_ConsumePackets(m_device, 0, 1);

			// Get next packet
            while (((res = AARTSAAPI_GetPacket(m_device, 0, 0, packets + 0)) == AARTSAAPI_EMPTY) && m_running)
                QThread::msleep(5);

			pi[0] = 0;

			if (res != AARTSAAPI_OK)
            {
                qWarning("SpectranMISOStreamWorker::streamRaw2Rx: AARTSAAPI_GetPacket() stream %d [2] failed with error %d", 0, res);
                m_running = false;
            }
		}

		if (pi[1] >= packets[1].num)
		{
			// Remove the first packet from the packet queue
			AARTSAAPI_ConsumePackets(m_device, 1, 1);

			// Get next packet
            while (((res = AARTSAAPI_GetPacket(m_device, 1, 0, packets + 1)) == AARTSAAPI_EMPTY) && m_running)
                QThread::msleep(5);

			pi[1] = 0;

			if (res != AARTSAAPI_OK)
            {
                qWarning("SpectranMISOStreamWorker::streamRaw2Rx: AARTSAAPI_GetPacket() stream %d [2] failed with error %d", 1, res);
                m_running = false;
            }
		}
    }

    if (m_restart)
    {
        qDebug("SpectranMISOStreamWorker::streamRaw2Rx: exit and restart");
        m_restart = false;
        emit restart();
    }
    else
    {
        qDebug("SpectranMISOStreamWorker::streamRaw2Rx: normal exit");
        emit stopped();
    }
}

bool SpectranMISOStreamWorker::handleMessage(const Message& message)
{
    return false;
}

void SpectranMISOStreamWorker::handleInputMessages()
{
    while (m_inputMessageQueue.size() > 0)
    {
        std::unique_ptr<Message> message(m_inputMessageQueue.pop());

        if (message)
        {
            if (!handleMessage(*message)) {
                qWarning("SpectranMISOStreamWorker::handleInputMessages: could not handle message of type %s", message->getIdentifier());
            }

            delete message.release();
        }
    }
}
