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

#ifndef _SPECTRANMISO_SPECTRANMISOWORKER_H_
#define _SPECTRANMISO_SPECTRANMISOWORKER_H_

#include <QObject>
#include <QThread>
#include <QMap>
#include <atomic>

#include <aaroniartsaapi.h>

#include "util/message.h"
#include "util/messagequeue.h"
#include "dsp/decimatorsfi.h"
#include "dsp/interpolatorsifnorm.h"

#include "spectranmisosettings.h"

class AARTSAAPI_Device;
class SampleMIFifo;
class SampleMOFifo;

class SpectranMISOStreamWorker : public QObject {
    Q_OBJECT
public:
    SpectranMISOStreamWorker(SampleMIFifo* sampleInFifo, SampleMOFifo* sampleOutFifo, QObject* parent = nullptr);
    ~SpectranMISOStreamWorker();

    bool isRunning() const { return m_running; }
    void setDevice(AARTSAAPI_Device* device) { m_device = device; }
    void setMode(SpectranMISOMode mode) { m_currentMode = mode; }
    void setCenterFrequency(double centerFrequencyHz) { m_centerFrequencyHz = centerFrequencyHz; };
    void setSampleRate(double sampleRateHz);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    void setMessageQueueToGUI(MessageQueue *queue) { m_inputMessageQueueToGUI = queue; }
    void setMessageQueueToSISO(MessageQueue *queue) { m_inputMessageQueueToSISO = queue; }
    void stopWork();
    void restartWork(); // restart in new mode

signals:
    void stopped();
    void restart(); // restart in new mode
    void localRestart(); // restart in same mode

public slots:
    void startWork();

private:
    void streamRxIQ();
    void streamRaw2Rx();
    void streamTx();
    void streamRxTx();
    void streamTransponderIQ();
    std::atomic<bool> m_running;
    std::atomic<bool> m_localRestart;
    bool m_restart;
    SpectranMISOMode m_currentMode;
    double m_sampleRateHz;
    double m_centerFrequencyHz;
    SampleMIFifo *m_sampleMIFifo;
    SampleMOFifo *m_sampleMOFifo;
    MessageQueue m_inputMessageQueue;
    MessageQueue *m_inputMessageQueueToGUI;
    MessageQueue *m_inputMessageQueueToSISO;
    AARTSAAPI_Device *m_device;
    DecimatorsFI<true> m_decimatorsFloatIQ[2]; // one per channel in non interleaved mode
    SampleVector m_convertBuffer[2]; // one per channel
    InterpolatorsIFNormalized m_interpolatorsFloatIQ;
    bool handleMessage(const Message& message);
    int m_nbSamplesPerPacket;
    static const int m_packetIntervalMs = 50; // 50 ms
    static const int m_maxSamplesPerPacket = static_cast<int>(m_packetIntervalMs * (20000000 / 1000)); // max clock rate 20 MHz

private slots:
    void handleInputMessages();
};


#endif // _SPECTRANMISO_SPECTRANMISOWORKER_H_
