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

#include <thread>
#include <chrono>

#include <QMutexLocker>

#include "dsp/samplemififo.h"
#include "dsp/samplemofifo.h"
#include "spectran/devicespectran.h"
#include "spectranmisostreamworker.h"

SpectranMISOStreamWorker::SpectranMISOStreamWorker(SampleMIFifo* sampleMIFifo, SampleMOFifo* sampleMOFifo, QObject* parent) :
    QObject(parent),
    m_running(false),
    m_localRestart(false),
    m_restart(false),
    m_sampleRateHz(100.0e3),
    m_centerFrequencyHz(2350.0e6),
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

    connect(this, &SpectranMISOStreamWorker::localRestart, this, &SpectranMISOStreamWorker::startWork, Qt::QueuedConnection);
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

    qDebug("SpectranMISOStreamWorker::startWork: %s", SpectranMISOSettings::m_modeDisplayNames.value(m_currentMode, "Unknown").toStdString().c_str());
    m_running = true;

    if (m_currentMode == SPECTRANMISO_MODE_TX_IQ) {
        streamTx();
    }
    else if (m_currentMode == SPECTRANMISO_MODE_RXTX_IQ)
    {
        streamRxTx();
        // qDebug("SpectranMISOStreamWorker::startWork: RX TX IQ mode not implemented, stopping");
        // m_running = false;
        // emit stopped();
        // return;
    }
    else if (m_currentMode == SPECTRANMISO_MODE_2RX_RAW)
    {
        streamRaw2Rx();
    }
    else
    {
        streamRxIQ();
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
    m_localRestart = false;
    stopWork();
}

void SpectranMISOStreamWorker::streamRxIQ()
{
    qDebug("SpectranMISOStreamWorker::streamRxIQ: starting");
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
                qWarning("SpectranMISOStreamWorker::streamRxIQ: AARTSAAPI_ConsumePackets() failed with error %d", res);
                m_running = false;
            }
        }
        else if (res != AARTSAAPI_EMPTY)
        {
            qWarning("SpectranMISOStreamWorker::streamRxIQ: AARTSAAPI_GetPacket() failed with error %d", res);
            m_running = false;
        }
    }

    if (m_restart)
    {
        qDebug("SpectranMISOStreamWorker::streamRxIQ: exit and restart");
        m_restart = false;
        emit restart();
    }
    else
    {
        qDebug("SpectranMISOStreamWorker::streamRxIQ: normal exit");
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

void SpectranMISOStreamWorker::streamTx()
{
    qDebug("SpectranMISOStreamWorker::streamTx: starting - waiting for device to be ready...");
    while (AARTSAAPI_GetDeviceState(m_device) != AARTSAAPI_RUNNING) {
        std::this_thread::sleep_for( std::chrono::milliseconds(100));
    }
    qDebug("SpectranMISOStreamWorker::streamTx: device is ready");

    unsigned int iPart1Begin, iPart1End, iPart2Begin, iPart2End;

	static const double	zeroDBm = sqrt(1.0 / 20.0);
	float *iqbuffer = new float[3*m_maxSamplesPerPacket]; // provision of 50% more than max size

	// Prepare output packet
	AARTSAAPI_Packet	packet = { sizeof(AARTSAAPI_Packet) };


	// Get the current stream time
	double	startTime;
	AARTSAAPI_GetMasterStreamTime(m_device, startTime);

	// Prepare the first packet to be played in 200ms

	packet.startTime = startTime + 0.2;
	packet.size = 2;
	packet.stride = 2;
	packet.fp32 = iqbuffer;

	double packetTime; // time for a packet to complete
	double maxQueueTime; // Max seconds of queued data

    int i = 0;
    // Initialize with current sample rate
    m_nbSamplesPerPacket = static_cast<int>(0.2 * m_sampleRateHz); // 200 ms of samples basically capped to max size
    m_nbSamplesPerPacket = m_nbSamplesPerPacket > m_maxSamplesPerPacket ? m_maxSamplesPerPacket : m_nbSamplesPerPacket;

	// Stream packets
	while (m_running)
	{
        // now = std::chrono::high_resolution_clock::now().time_since_epoch().count() / 1e9;
        // qDebug("SpectranMISOStreamWorker::streamTx: nbSamplesFIFO: %d stepFrequency: %f", nbSamplesFIFO, stepFrequency);

        // Read samples from FIFO
        m_sampleMOFifo->readSync(m_nbSamplesPerPacket, iPart1Begin, iPart1End, iPart2Begin, iPart2End);
        int iqbufPos = 0;

        if (iPart1Begin != iPart1End)
        {
            SampleVector::iterator begin = m_sampleMOFifo->getData(0).begin() + iPart1Begin;
            m_interpolatorsFloatIQ.interpolate1(&begin, iqbuffer, (iPart1End - iPart1Begin)*2, zeroDBm, false);
            iqbufPos += (iPart1End - iPart1Begin)*2;
        }
        if (iPart2Begin != iPart2End)
        {
            SampleVector::iterator begin = m_sampleMOFifo->getData(0).begin() + iPart2Begin;
            m_interpolatorsFloatIQ.interpolate1(&begin, iqbuffer + iqbufPos, (iPart2End - iPart2Begin)*2, zeroDBm, false);
            iqbufPos += (iPart2End - iPart2Begin)*2;
        }

        // Parameters based on frequency and sample rate
        packet.startFrequency = m_centerFrequencyHz; // Center frequency
        packet.stepFrequency = m_sampleRateHz; // Sample rate
        packet.startFrequency -= 0.5 * packet.stepFrequency;
        packet.num = m_nbSamplesPerPacket; // nb samples / step frequency s per packet
        packetTime = packet.num / packet.stepFrequency; // time for a packet to complete (actaual)
	    maxQueueTime = packetTime; // Max seconds of queued data

		// Calculate end time of packet base on number of samples
		// and sample rate
		packet.endTime = packet.startTime + packetTime;
        packet.fp32 = iqbuffer;

		// Set start flags
		if (i == 0)
			packet.flags = AARTSAAPI_PACKET_SEGMENT_START | AARTSAAPI_PACKET_STREAM_START;
		else
			packet.flags = 0;

		// Wait for a max queue fill level of maxQueueTime seconds
		AARTSAAPI_GetMasterStreamTime(m_device, startTime);
		while ((startTime + maxQueueTime < packet.startTime) && m_running)
		{
            std::this_thread::sleep_for( std::chrono::milliseconds(int(100 * maxQueueTime)));
			// std::this_thread::sleep_for( std::chrono::milliseconds(int(1000 * (packet.startTime - startTime - maxQueueTime - (maxQueueTime/10.0)))));
			AARTSAAPI_GetMasterStreamTime(m_device, startTime);
		}

		// Send the packet
		AARTSAAPI_SendPacket(m_device, 0, &packet); // asynchronous send

        // Advance packet time and loop counter
        // qDebug("SpectranMISOStreamWorker::streamTx: loop: %d time: %f %f", i, packetTime, packet.endTime - packet.startTime);
		packet.startTime = packet.endTime;
        i++;
	}

    // Repeat last packet to terminate the stream with end flags
    packet.flags = AARTSAAPI_PACKET_SEGMENT_END | AARTSAAPI_PACKET_STREAM_END;
    AARTSAAPI_SendPacket(m_device, 0, &packet);
    // Advance packet time
    packet.startTime = packet.endTime;

	// Wait for the last packet to finish
	AARTSAAPI_GetMasterStreamTime(m_device, startTime);
	while (startTime < packet.startTime)
	{
		std::this_thread::sleep_for( std::chrono::milliseconds(int(1000 * (packet.startTime - startTime))));
		AARTSAAPI_GetMasterStreamTime(m_device, startTime);
	}

    delete[] iqbuffer;

    if (m_localRestart)
    {
        m_localRestart = false;
        qDebug("SpectranMISOStreamWorker::streamTx: local restart requested");
        emit localRestart();
        return;
    }

    if (m_restart)
    {
        qDebug("SpectranMISOStreamWorker::streamTx: exit and restart");
        m_restart = false;
        emit restart();
    }
    else
    {
        qDebug("SpectranMISOStreamWorker::streamTx: normal exit");
        emit stopped();
    }
}

void SpectranMISOStreamWorker::streamRxTx()
{
    qDebug("SpectranMISOStreamWorker::streamTx: starting - waiting for device to be ready...");
    while (AARTSAAPI_GetDeviceState(m_device) != AARTSAAPI_RUNNING) {
        std::this_thread::sleep_for( std::chrono::milliseconds(100));
    }
    qDebug("SpectranMISOStreamWorker::streamTx: device is ready %s", m_running ? "true" : "false");

    SampleVector::iterator convIt[2];
    AARTSAAPI_Result res;

    unsigned int iPart1Begin, iPart1End, iPart2Begin, iPart2End;

	static const double	zeroDBm = sqrt(1.0 / 20.0);
	float *iqbuffer = new float[3*m_convertBuffer[0].size()]; // provision of 50% more than max size

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
            // Read samples from FIFO
            m_sampleMOFifo->readSync(packet.num, iPart1Begin, iPart1End, iPart2Begin, iPart2End);
            int iqbufPos = 0;

            if (iPart1Begin != iPart1End)
            {
                SampleVector::iterator begin = m_sampleMOFifo->getData(0).begin() + iPart1Begin;
                m_interpolatorsFloatIQ.interpolate1(&begin, iqbuffer, (iPart1End - iPart1Begin)*2, zeroDBm, false);
                iqbufPos += (iPart1End - iPart1Begin)*2;
            }
            if (iPart2Begin != iPart2End)
            {
                SampleVector::iterator begin = m_sampleMOFifo->getData(0).begin() + iPart2Begin;
                m_interpolatorsFloatIQ.interpolate1(&begin, iqbuffer + iqbufPos, (iPart2End - iPart2Begin)*2, zeroDBm, false);
                iqbufPos += (iPart2End - iPart2Begin)*2;
            }

            // Convert to Sample format
            convIt[0] = m_convertBuffer[0].begin();
            convIt[1] = m_convertBuffer[1].begin();

            m_decimatorsFloatIQ[0].decimate1(&convIt[0], packet.fp32, packet.size * packet.num);

            // Write samples to FIFO
            std::vector<SampleVector::const_iterator> vbegin;
            vbegin.push_back(m_convertBuffer[0].begin());
            vbegin.push_back(m_convertBuffer[1].begin());
            m_sampleMIFifo->writeSync(vbegin, packet.num);
            // Remove the first packet from the packet queue
            res = AARTSAAPI_ConsumePackets(m_device, 0, 1);

            if (res != AARTSAAPI_OK)
            {
                qWarning("SpectranMISOStreamWorker::streamRxIQ: AARTSAAPI_ConsumePackets() failed with error %d", res);
                m_running = false;
            }

            // Get the current system time
			double	streamTime;
			AARTSAAPI_GetMasterStreamTime(m_device, streamTime);

			// Check if we are still close to the capture stream
			if (packet.startTime > streamTime - 0.1)
			{
				// Push packet into future for buffering

				packet.startTime += 0.2;
				packet.endTime += 0.2;
				packet.startFrequency = m_centerFrequencyHz - 0.5 * packet.stepFrequency;
                packet.fp32 = iqbuffer;

				// Send packet to transmitter queue
				AARTSAAPI_SendPacket(m_device, 0, &packet);
			}
        }
        else if (res != AARTSAAPI_EMPTY)
        {
            qWarning("SpectranMISOStreamWorker::streamRxIQ: AARTSAAPI_GetPacket() failed with error %d", res);
            m_running = false;
        }
    }

    delete[] iqbuffer;

    if (m_restart)
    {
        qDebug("SpectranMISOStreamWorker::streamRxTx: exit and restart");
        m_restart = false;
        emit restart();
    }
    else
    {
        qDebug("SpectranMISOStreamWorker::streamRxTx: normal exit");
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

void SpectranMISOStreamWorker::setSampleRate(double sampleRateHz)
{
    m_sampleRateHz = sampleRateHz;
    m_nbSamplesPerPacket = static_cast<int>(0.2 * m_sampleRateHz); // 200 ms of samples basically capped to max size
    m_nbSamplesPerPacket = m_nbSamplesPerPacket > m_maxSamplesPerPacket ? m_maxSamplesPerPacket : m_nbSamplesPerPacket;
    qDebug("SpectranMISOStreamWorker::setSampleRate: sample rate set to %f Hz, nbSamplesPerPacket=%d", m_sampleRateHz, m_nbSamplesPerPacket);
    m_running = false;
    m_localRestart = true;
}
