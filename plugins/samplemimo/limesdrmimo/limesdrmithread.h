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

#ifndef PLUGINS_SAMPLEMIMO_LIMESDRMIMO_LIMESDRMITHREAD_H_
#define PLUGINS_SAMPLEMIMO_LIMESDRMIMO_LIMESDRMITHREAD_H_

// LimeSDR is a SISO/MIMO device. It can support one or two Rx. Here ww will
// configure two Rx

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include "lime/LimeSuite.h"

#include "limesdr/devicelimesdr.h"
#include "dsp/decimators.h"

class SampleMIFifo;

class LimeSDRMIThread : public QThread {
    Q_OBJECT

public:
    LimeSDRMIThread(lms_stream_t* stream0, lms_stream_t* stream1, QObject* parent = nullptr);
    ~LimeSDRMIThread();

    void startWork();
    void stopWork();
    bool isRunning() const { return m_running; }
    void setLog2Decimation(unsigned int log2_decim);
    unsigned int getLog2Decimation() const;
    void setFifo(SampleMIFifo *sampleFifo) { m_sampleFifo = sampleFifo; }
    SampleMIFifo *getFifo() { return m_sampleFifo; }
    void setIQOrder(bool iqOrder) { m_iqOrder = iqOrder; }

private:
    QMutex m_startWaitMutex;
    QWaitCondition m_startWaiter;
    bool m_running;

    lms_stream_t* m_stream0;
    lms_stream_t* m_stream1;
    qint16 m_buf0[2*DeviceLimeSDR::blockSize];
    qint16 m_buf1[2*DeviceLimeSDR::blockSize];
	SampleVector m_convertBuffer[2];
    std::vector<SampleVector::const_iterator> m_vBegin;
    SampleMIFifo* m_sampleFifo;
    Decimators<qint32, qint16, SDR_RX_SAMP_SZ, 12, true> m_decimatorsIQ[2];
    Decimators<qint32, qint16, SDR_RX_SAMP_SZ, 12, false> m_decimatorsQI[2];
    unsigned int m_log2Decim;
    bool m_iqOrder;

    void run();
    int channelCallbackIQ(const qint16* buf, qint32 len, int channel);
    int channelCallbackQI(const qint16* buf, qint32 len, int channel);
};

#endif // PLUGINS_SAMPLEMIMO_LIMESDRMIMO_LIMESDRMITHREAD_H_
