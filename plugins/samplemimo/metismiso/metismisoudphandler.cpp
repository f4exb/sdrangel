///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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
#include "dsp/samplemofifo.h"

#include "metismisoudphandler.h"

MESSAGE_CLASS_DEFINITION(MetisMISOUDPHandler::MsgStartStop, Message)

MetisMISOUDPHandler::MetisMISOUDPHandler(SampleMIFifo *sampleMIFifo, SampleMOFifo *sampleMOFifo, DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_socket(nullptr),
	m_metisAddress(QHostAddress::LocalHost),
	m_metisPort(9090),
    m_running(false),
	m_dataConnected(false),
	m_sampleMIFifo(sampleMIFifo),
    m_sampleMOFifo(sampleMOFifo),
    m_sampleCount(0),
    m_sampleTxCount(0),
	m_messageQueueToGUI(nullptr),
    m_mutex(QMutex::Recursive),
    m_commandBase(0),
    m_receiveSequence(0),
    m_receiveSequenceError(0)
{
    setNbReceivers(m_settings.m_nbReceivers);

    for (unsigned int i = 0; i < MetisMISOSettings::m_maxReceivers; i++) {
        m_convertBuffer[i].resize(1024, Sample{0,0});
    }

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleMessages()));
}

MetisMISOUDPHandler::~MetisMISOUDPHandler()
{
    stop();
}

void MetisMISOUDPHandler::start()
{
	qDebug("MetisMISOUDPHandler::start");

    m_rxFrame = 0;
    m_txFrame = 0;
    m_sendSequence = -1;
    m_offset = 8;
    m_receiveSequence = 0;

	if (m_running) {
	    return;
	}

    if (!m_dataConnected)
	{
        if (m_socket.bind(QHostAddress::AnyIPv4, 10001, QUdpSocket::ShareAddress))
		{
			qDebug("MetisMISOUDPHandler::start: bind host socket OK");
            connect(&m_socket, SIGNAL(readyRead()), this, SLOT(dataReadyRead()));
			m_dataConnected = true;
		}
		else
		{
			qWarning("MetisMISOUDPHandler::start: cannot bind host socket");
			m_dataConnected = false;
            return;
		}
	}

    // Start Metis
    unsigned char buffer[64];

    buffer[0] = (unsigned char) 0xEF;
    buffer[1] = (unsigned char) 0XFE;
    buffer[2] = (unsigned char) 0x04;
    buffer[3] = (unsigned char) 0x01;
    std::fill(&buffer[4], &buffer[64], 0);

    if (m_socket.writeDatagram((const char*) buffer, sizeof(buffer), m_metisAddress, m_metisPort) < 0)
    {
        qDebug() << "MetisMISOUDPHandler::start: writeDatagram start command failed " << m_socket.errorString();
        return;
    }
    else
    {
        qDebug() << "MetisMISOUDPHandler::start: writeDatagram start command" << m_metisAddress.toString() << ":" << m_metisPort;
    }

    m_socket.flush();

    // send 2 frames with control data

    sendData();
    sendData(); // TODO: on the next send frequencies

    m_running = true;
}

void MetisMISOUDPHandler::stop()
{
	qDebug("MetisMISOUDPHandler::stop");

	if (!m_running) {
	    return;
	}

    // stop Metis
    unsigned char buffer[64];

    buffer[0] = (unsigned char) 0xEF;
    buffer[1] = (unsigned char) 0XFE;
    buffer[2] = (unsigned char) 0x04;
    buffer[3] = (unsigned char) 0x00;
    std::fill(&buffer[4], &buffer[64], 0);

    if (m_dataConnected)
    {
	    disconnect(&m_socket, SIGNAL(readyRead()), this, SLOT(dataReadyRead()));
		m_dataConnected = false;
	}


    if (m_socket.writeDatagram((const char*)buffer, sizeof(buffer), m_metisAddress,m_metisPort) < 0)
    {
        qDebug() << "MetisMISOUDPHandler::stop: writeDatagram failed " << m_socket.errorString();
        return;
    }
    else
    {
        qDebug()<<"MetisMISOUDPHandler::stop: writeDatagram stop command" << m_metisAddress.toString() << ":" << m_metisPort;
    }

    m_socket.flush();
    m_socket.close();
	m_running = false;
}

void MetisMISOUDPHandler::setNbReceivers(unsigned int nbReceivers)
{
    m_nbReceivers = nbReceivers;

    switch(m_nbReceivers)
    {
    case 1: m_bMax = 512-0; break;
    case 2: m_bMax = 512-0; break;
    case 3: m_bMax = 512-4; break;
    case 4: m_bMax = 512-10; break;
    case 5: m_bMax = 512-24; break;
    case 6: m_bMax = 512-10; break;
    case 7: m_bMax = 512-20; break;
    case 8: m_bMax = 512-4; break;
    }

    for (unsigned int i = 0; i < MetisMISOSettings::m_maxReceivers; i++) {
        m_convertBuffer[i].resize(1024, Sample{0,0});
    }
}

void MetisMISOUDPHandler::applySettings(const MetisMISOSettings& settings)
{
    if (m_settings.m_nbReceivers != settings.m_nbReceivers)
    {
        QMutexLocker mutexLocker(&m_mutex);
        int nbReceivers = settings.m_nbReceivers < 1 ?
            1 : settings.m_nbReceivers > MetisMISOSettings::m_maxReceivers ?
                MetisMISOSettings::m_maxReceivers : settings.m_nbReceivers;
        setNbReceivers(nbReceivers);
    }

    if (m_settings.m_log2Decim != settings.m_log2Decim)
    {
        QMutexLocker mutexLocker(&m_mutex);
        m_decimators.resetCounters();
    }

    m_settings = settings;
}

void MetisMISOUDPHandler::handleMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

bool MetisMISOUDPHandler::handleMessage(const Message& message)
{
    if (MsgStartStop::match(message))
    {
        const MsgStartStop& cmd = (const MsgStartStop&) message;

        if (cmd.getStartStop()) {
            start();
        } else {
            stop();
        }

        return true;
    }
    else
    {
        return false;
    }

}

void MetisMISOUDPHandler::sendMetisBuffer(int ep, unsigned char* buffer)
{
    if (m_offset == 8) // header and first HPSDR frame
    {
        m_sendSequence++;
        m_outputBuffer[0] = 0xEF;
        m_outputBuffer[1] = 0xFE;
        m_outputBuffer[2] = 0x01;
        m_outputBuffer[3] = ep;
        m_outputBuffer[4] = (m_sendSequence>>24) & 0xFF;
        m_outputBuffer[5] = (m_sendSequence>>16) & 0xFF;
        m_outputBuffer[6] = (m_sendSequence>>8) & 0xFF;
        m_outputBuffer[7] = (m_sendSequence) & 0xFF;
        std::copy(buffer, buffer+512, &m_outputBuffer[m_offset]); // copy the buffer over
        m_offset = 520;
    }
    else // second HPSDR frame and send
    {
        std::copy(buffer, buffer+512, &m_outputBuffer[m_offset]); // copy the buffer over
        m_offset = 8;

        // send the buffer
        if (m_socket.writeDatagram((const char*) m_outputBuffer, sizeof(m_outputBuffer), m_metisAddress, m_metisPort) < 0)
        {
            qDebug() << "MetisMISOUDPHandler::sendMetisBuffer: writeDatagram failed " << m_socket.errorString();
            return;
        }

        m_socket.flush();
    }
}

void MetisMISOUDPHandler::dataReadyRead()
{
    QHostAddress metisAddress;
    quint16 metisPort;
    unsigned char receiveBuffer[1032];
    qint64 length;
    unsigned long sequence;

    if ((length = m_socket.readDatagram((char*) &receiveBuffer, (qint64) sizeof(receiveBuffer), &metisAddress, &metisPort)) != 1032)
    {
        qDebug() << "MetisMISOUDPHandler::dataReadyRead: readDatagram failed " << m_socket.errorString();
        return;
    }

    if (receiveBuffer[0] == 0xEF && receiveBuffer[1] == 0xFE)
    {
        // valid frame
        switch(receiveBuffer[2])
        {
        case 1: // IQ data
            switch (receiveBuffer[3])
            {
            case 4: // EP4 data
                break;
            case 6: // EP6 data
                sequence = ((receiveBuffer[4] & 0xFF)<<24) + ((receiveBuffer[5] & 0xFF)<<16) + ((receiveBuffer[6] & 0xFF)<<8) +(receiveBuffer[7] & 0xFF);
                if (m_receiveSequence == 0)
                {
                    m_receiveSequence = sequence;
                }
                else
                {
                    m_receiveSequence++;

                    if (m_receiveSequence != sequence)
                    {
                        //qDebug()<<"Sequence error: expected "<<receive_sequence<<" got "<<sequence;
                        m_receiveSequence = sequence;
                        m_receiveSequenceError++;
                    }
                }
                processIQBuffer(&receiveBuffer[8]);
                processIQBuffer(&receiveBuffer[520]);
                break;
            default:
                qDebug() << "MetisMISOUDPHandler::dataReadyRead: invalid EP" << receiveBuffer[3];
                break;
            }
            break;
        default:
            qDebug() << "MetisMISOUDPHandler::dataReadyRead: expected data packet (1) got " << receiveBuffer[2];
            break;
        }
    }
    else
    {
        qDebug() << "MetisMISOUDPHandler::dataReadyRead: expected EFFE";
    }
}

void MetisMISOUDPHandler::sendData(bool nullPayload)
{
    unsigned char buffer[512];

    if ((m_settings.m_sampleRateIndex == 0) || (m_txFrame % (1<<m_settings.m_sampleRateIndex) == 0))
    {
        int commandIndex = 2*m_commandBase; // command rotation

        buffer[0] = (unsigned char) 0x7F;
        buffer[1] = (unsigned char) 0x7F;
        buffer[2] = (unsigned char) 0x7F;
        buffer[3] = commandIndex + (m_settings.m_txEnable ? 1 : 0); // C0

        int commandValue = getCommandValue(commandIndex);

        buffer[4]= commandValue>>24; // C1
        buffer[5]= (commandValue>>16) & 0xFF; // C2
        buffer[6]= (commandValue>>8) & 0xFF; // C3
        buffer[7]= commandValue & 0xFF; // C4

        if (m_commandBase < 18) { // base count 0 to 18
            m_commandBase++;
        } else {
            m_commandBase = 0;
        }

        if (nullPayload)
        {
            std::fill(&buffer[8], &buffer[512], 0);
        }
        else
        {
            unsigned int iPart1Begin, iPart1End, iPart2Begin, iPart2End;
            int bufferIndex = 8;

            m_sampleMOFifo->readSync(63, iPart1Begin, iPart1End, iPart2Begin, iPart2End);

            if (iPart1Begin != iPart1End) {
                fillBuffer(buffer, bufferIndex, iPart1Begin, iPart1End);
            }

            if (iPart2Begin != iPart2End) {
                fillBuffer(buffer, bufferIndex, iPart2Begin, iPart2End);
            }
        }

        sendMetisBuffer(2, buffer);
    }

    m_txFrame++;
}

void MetisMISOUDPHandler::fillBuffer(unsigned char *buffer, int& bufferIndex, int iBegin, int iEnd)
{
    SampleVector::iterator it = m_sampleMOFifo->getData(0).begin() + iBegin;
    const SampleVector::iterator itEnd = m_sampleMOFifo->getData(0).begin() + iEnd;

    for (; it != itEnd; ++it)
    {
        std::fill(&buffer[bufferIndex], &buffer[bufferIndex+4], 0); // Fill <L1><L0><R1><R0> with zeros
        bufferIndex += 4;
        buffer[bufferIndex++] = it->imag() >> 8;
        buffer[bufferIndex++] = it->imag() & 0xFF;
        buffer[bufferIndex++] = it->real() >> 8;
        buffer[bufferIndex++] = it->real() & 0xFF;
    }
}

int MetisMISOUDPHandler::getCommandValue(int commandIndex)
{
    int c1 = 0, c3 = 0, c4 = 0;

    if (commandIndex == 0)
    {
        c1 =  m_settings.m_sampleRateIndex & 0x03;
        c3 =  m_settings.m_preamp ? 0x04 : 0;
        c3 += m_settings.m_dither ? 0x08 : 0;
        c3 += m_settings.m_random ? 0x10 : 0;
        c4 =  m_settings.m_duplex ? 0x04 : 0;
        c4 += (((m_nbReceivers-1) & 0x07)<<3);
        return (c1<<24) + (c3<<8) + c4;
    }
    else if (commandIndex == 2)
    {
        return getTxCenterFrequency();
    }
    else if (commandIndex == 4)
    {
        return getRxCenterFrequency(0);
    }
    else if (commandIndex == 6)
    {
        return getRxCenterFrequency(1);
    }
    else if (commandIndex == 8)
    {
        return getRxCenterFrequency(2);
    }
    else if (commandIndex == 10)
    {
        return getRxCenterFrequency(3);
    }
    else if (commandIndex == 12)
    {
        return getRxCenterFrequency(4);
    }
    else if (commandIndex == 14)
    {
        return getRxCenterFrequency(5);
    }
    else if (commandIndex == 16)
    {
        return getRxCenterFrequency(6);
    }
    else if (commandIndex == 18)
    {
        c1 = (m_settings.m_txDrive & 0x0F) << 4;
        return (c1<<24) + (c3<<8) + c4;
    }
    else if (commandIndex == 36)
    {
        return getRxCenterFrequency(7);
    }
    else
    {
        return 0;
    }
}

quint64 MetisMISOUDPHandler::getRxCenterFrequency(int index)
{
    qint64 deviceCenterFrequency;
    qint64 loHalfFrequency = 61440000LL - ((m_settings.m_LOppmTenths * 122880000LL) / 20000000LL);
    qint64 requiredRxFrequency =  m_settings.m_rxCenterFrequencies[index]
        - (m_settings.m_rxTransverterMode ? m_settings.m_rxTransverterDeltaFrequency : 0);
    requiredRxFrequency = requiredRxFrequency < 0 ? 0 : requiredRxFrequency;

    if (m_settings.m_rxSubsamplingIndexes[index] % 2 == 0) {
        deviceCenterFrequency = requiredRxFrequency - m_settings.m_rxSubsamplingIndexes[index]*loHalfFrequency;
    } else {
        deviceCenterFrequency = (m_settings.m_rxSubsamplingIndexes[index] + 1)*loHalfFrequency - requiredRxFrequency;
    }

	qint64 df = ((qint64)deviceCenterFrequency * m_settings.m_LOppmTenths) / 10000000LL;
	deviceCenterFrequency += df;

    return deviceCenterFrequency < 0 ? 0 : deviceCenterFrequency > 61440000 ? 61440000 : deviceCenterFrequency;
}

quint64 MetisMISOUDPHandler::getTxCenterFrequency()
{
    qint64 requiredTxFrequency =  m_settings.m_txCenterFrequency
        - (m_settings.m_txTransverterMode ? m_settings.m_txTransverterDeltaFrequency : 0);
    requiredTxFrequency = requiredTxFrequency < 0 ? 0 : requiredTxFrequency;
    return requiredTxFrequency;
}

bool MetisMISOUDPHandler::getRxIQInversion(int index)
{
    return ((m_settings.m_rxSubsamplingIndexes[index] % 2) == 1) ^ !m_settings.m_iqOrder;
}

void MetisMISOUDPHandler::processIQBuffer(unsigned char* buffer)
{
    int b = 0;
    unsigned int r;
    int sampleI, sampleQ, sampleMic;


    if (buffer[b++]==0x7F && buffer[b++]==0x7F && buffer[b++]==0x7F)
    {
        QMutexLocker mutexLocker(&m_mutex);

        // extract control bytes
        m_controlIn[0] = buffer[b++];
        m_controlIn[1] = buffer[b++];
        m_controlIn[2] = buffer[b++];
        m_controlIn[3] = buffer[b++];
        m_controlIn[4] = buffer[b++];

        // extract PTT, DOT and DASH
        m_ptt = (m_controlIn[0] & 0x01) == 0x01;
        m_dash = (m_controlIn[0] & 0x02) == 0x02;
        m_dot = (m_controlIn[0] & 0x04) == 0x04;

        switch((m_controlIn[0]>>3) & 0x1F)
        {
        case 0:
            m_lt2208ADCOverflow = m_controlIn[1] & 0x01;
            m_IO1 = (m_controlIn[1] & 0x02) ? 0 : 1;
            m_IO2 = (m_controlIn[1] & 0x04) ? 0 : 1;
            m_IO3 = (m_controlIn[1] & 0x08) ? 0 : 1;
            // {
            //     int nbRx = (m_controlIn[4]>>3) & 0x07;
            //     nbRx++;
            //     qDebug("MetisMISOUDPHandler::processIQBuffer: nbRx: %d", nbRx);
            // }
            if (m_mercurySoftwareVersion != m_controlIn[2])
            {
                m_mercurySoftwareVersion = m_controlIn[2];
                qDebug("MetisMISOUDPHandler::processIQBuffer: Mercury Software version: %d (0x%0X)",
                    m_mercurySoftwareVersion, m_mercurySoftwareVersion);
            }

            if (m_penelopeSoftwareVersion != m_controlIn[3])
            {
                m_penelopeSoftwareVersion = m_controlIn[3];
                qDebug("MetisMISOUDPHandler::processIQBuffer: Penelope Software version: %d (0x%0X)",
                    m_penelopeSoftwareVersion, m_penelopeSoftwareVersion);
            }

            if (m_ozySoftwareVersion != m_controlIn[4])
            {
                m_ozySoftwareVersion = m_controlIn[4];
                qDebug("MetisMISOUDPHandler::processIQBuffer: Ozy Software version: %d (0x%0X)",
                    m_ozySoftwareVersion, m_ozySoftwareVersion);
            }
            break;

        case 1:
            m_forwardPower = (m_controlIn[1]<<8) + m_controlIn[2]; // from Penelope or Hermes
            m_alexForwardPower = (m_controlIn[3]<<8) + m_controlIn[4]; // from Alex or Apollo
            break;

        case 2:
            m_alexForwardPower = (m_controlIn[1]<<8) + m_controlIn[2]; // from Alex or Apollo
            m_AIN3 = (m_controlIn[3]<<8) + m_controlIn[4]; // from Pennelope or Hermes
            break;

        case 3:
            m_AIN4 = (m_controlIn[1]<<8) + m_controlIn[2]; // from Pennelope or Hermes
            m_AIN6 = (m_controlIn[3]<<8) + m_controlIn[4]; // from Pennelope or Hermes
            break;
        }

        // extract the samples
        while (b < m_bMax)
        {
            int samplesAdded = 0;

            // extract each of the receivers
            for (r = 0; r < m_nbReceivers; r++)
            {
                if (SDR_RX_SAMP_SZ == 16)
                {
                    sampleQ  = (int)((signed char)buffer[b++]) << 8;
                    sampleQ += (int)((unsigned char)buffer[b++]);
                    b++;
                    sampleI  = (int)((signed char)buffer[b++]) << 8;
                    sampleI += (int)((unsigned char)buffer[b++]);
                    b++;
                }
                else
                {
                    sampleQ  = (int)((signed char)buffer[b++]) << 16;
                    sampleQ += (int)((unsigned char)buffer[b++]) << 8;
                    sampleQ += (int)((unsigned char)buffer[b++]);
                    sampleI  = (int)((signed char)buffer[b++]) << 16;
                    sampleI += (int)((unsigned char)buffer[b++]) << 8;
                    sampleI += (int)((unsigned char)buffer[b++]);
                }

                if (r < MetisMISOSettings::m_maxReceivers)
                {
                    if (m_settings.m_log2Decim == 0) // no decimation - direct conversion
                    {
                        m_convertBuffer[r][m_sampleCount] = getRxIQInversion(r)
                            ? Sample{(FixReal) sampleQ, (FixReal) sampleI}
                            : Sample{(FixReal) sampleI, (FixReal) sampleQ};
                        samplesAdded = 1;
                    }
                    else
                    {
                        SampleVector v(2, Sample{0, 0});

                        switch (m_settings.m_log2Decim)
                        {
                            case 1:
                                samplesAdded = getRxIQInversion(r) ?
                                    m_decimators.decimate2(sampleQ, sampleI, v, r) : m_decimators.decimate2(sampleI, sampleQ, v, r);
                                break;
                            case 2:
                                samplesAdded = getRxIQInversion(r) ?
                                    m_decimators.decimate4(sampleQ, sampleI, v, r) : m_decimators.decimate4(sampleI, sampleQ, v, r);
                                break;
                            case 3:
                                samplesAdded = getRxIQInversion(r) ?
                                    m_decimators.decimate8(sampleQ, sampleI, v, r) : m_decimators.decimate8(sampleI, sampleQ, v, r);
                                break;
                            default:
                                break;
                        }

                        if (samplesAdded != 0)
                        {
                            std::copy(
                                v.begin(),
                                v.begin() + samplesAdded,
                                &m_convertBuffer[r][m_sampleCount]
                            );
                        }
                    }
                }
            }

            sampleMic  = (int)((signed char) buffer[b++]) << 8;
            sampleMic += (int)((unsigned char)buffer[b++]);
            m_sampleTxCount++;

            if (m_sampleTxCount >= 63) // 63 samples per 512 byte Tx block
            {
                sendData(!m_settings.m_txEnable); // synchronous sending of a Tx frame
                m_sampleTxCount = 0;
            }

            if (samplesAdded != 0)
            {
                m_sampleCount += samplesAdded;

                if (m_sampleCount >= 1024)
                {
                    std::vector<SampleVector::const_iterator> vbegin;

                    for (unsigned int channel = 0; channel < MetisMISOSettings::m_maxReceivers; channel++) {
                        vbegin.push_back(m_convertBuffer[channel].begin());
                    }

                    m_sampleMIFifo->writeSync(vbegin, 1024);
                    m_sampleCount = 0;
                }
            }
        }
    }
    else
    {
        qDebug()<<"MetisMISOUDPHandler::processIQBuffer: SYNC Error";
    }

    m_rxFrame++;
}
