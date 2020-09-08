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

#ifndef _METISMISO_METISMISOUDPHANDLER_H_
#define _METISMISO_METISMISOUDPHANDLER_H_

#include <QObject>
#include <QUdpSocket>
#include <QHostAddress>
#include <QMutex>

#include "dsp/decimators.h"
#include "util/messagequeue.h"
#include "util/message.h"

#include "metismisosettings.h"
#include "metismisodecimators.h"

class SampleMIFifo;
class SampleMOFifo;
class DeviceAPI;

class MetisMISOUDPHandler : public QObject
{
	Q_OBJECT
public:
    class MsgStartStop : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getStartStop() const { return m_startStop; }

        static MsgStartStop* create(bool startStop) {
            return new MsgStartStop(startStop);
        }

    protected:
        bool m_startStop;

        MsgStartStop(bool startStop) :
            Message(),
            m_startStop(startStop)
        { }
    };

	MetisMISOUDPHandler(SampleMIFifo* sampleMIFifo, SampleMOFifo *sampleMOFifo, DeviceAPI *deviceAPI);
	~MetisMISOUDPHandler();
	void setMessageQueueToGUI(MessageQueue *queue) { m_messageQueueToGUI = queue; }
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    void start();
	void stop();
	void setMetisAddress(const QHostAddress& address, quint16 port) {
        m_metisAddress = address;
        m_metisPort = port;
    }
    void setNbReceivers(unsigned int nbReceivers);
    void applySettings(const MetisMISOSettings& settings);

public slots:
	void dataReadyRead();

private:
	DeviceAPI *m_deviceAPI;
	QUdpSocket m_socket;
	QHostAddress m_metisAddress;
    quint16 m_metisPort;
    bool m_running;
    bool m_dataConnected;
    SampleMIFifo *m_sampleMIFifo;
    SampleMOFifo *m_sampleMOFifo;
    SampleVector m_convertBuffer[MetisMISOSettings::m_maxReceivers];
    int m_sampleCount;
    int m_sampleTxCount;
	MessageQueue *m_messageQueueToGUI;
    MessageQueue m_inputMessageQueue;
    MetisMISOSettings m_settings;
    QMutex m_mutex;
    MetisMISODecimators m_decimators;

    unsigned long m_sendSequence;
    int m_offset;
    int m_commandBase;
    unsigned long m_rxFrame;
    unsigned long m_txFrame;
    unsigned char m_outputBuffer[1032]; //!< buffer to send
    unsigned char m_metisBuffer[512];   //!< current HPSDR frame
    int metisBufferIndex;
    unsigned long m_receiveSequence;
    int m_receiveSequenceError;
    unsigned char m_controlIn[5];
    unsigned int m_nbReceivers;
    int m_bMax;
    bool m_ptt;
    bool m_dash;
    bool m_dot;
    bool m_lt2208ADCOverflow;
    int m_IO1;
    int m_IO2;
    int m_IO3;
    int m_mercurySoftwareVersion;
    int m_penelopeSoftwareVersion;
    int m_ozySoftwareVersion;
    int m_forwardPower;
    int m_alexForwardPower;
    int m_AIN3;
    int m_AIN4;
    int m_AIN6;

    void sendMetisBuffer(int ep, unsigned char* buffer);
	bool handleMessage(const Message& message);
    void sendData(bool nullPayload = true);
    void fillBuffer(unsigned char *buffer, int& bufferIndex, int iBegin, int iEnd);
    int getCommandValue(int commandIndex);
    void processIQBuffer(unsigned char* buffer);
    quint64 getRxCenterFrequency(int index);
    quint64 getTxCenterFrequency();
    bool getRxIQInversion(int index);

private slots:
    void handleMessages();
};

#endif // _METISMISO_METISMISOUDPHANDLER_H_