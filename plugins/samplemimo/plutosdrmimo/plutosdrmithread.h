///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Edouard Griffiths, F4EXB                                   //
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

#ifndef _PLUTOSDR_PLUTOSDRMITHREAD_H_
#define _PLUTOSDR_PLUTOSDRMITHREAD_H_

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include "dsp/decimators.h"

class SampleMIFifo;
class DevicePlutoSDRBox;

class PlutoSDRMIThread : public QThread {
    Q_OBJECT
public:
    PlutoSDRMIThread(DevicePlutoSDRBox* plutoBox, QObject* parent = nullptr);
    ~PlutoSDRMIThread();

    void startWork();
    void stopWork();
    bool isRunning() const { return m_running; }
    void setLog2Decimation(unsigned int log2Decim);
    unsigned int getLog2Decimation() const;
    void setFcPos(int fcPos);
    int getFcPos() const;
    void setFifo(SampleMIFifo *sampleFifo) { m_sampleFifo = sampleFifo; }
    SampleMIFifo *getFifo() { return m_sampleFifo; }
    void setIQOrder(bool iqOrder) { m_iqOrder = iqOrder; }

private:
    QMutex m_startWaitMutex;
    QWaitCondition m_startWaiter;
    bool m_running;

    DevicePlutoSDRBox *m_plutoBox;
    qint16 *m_buf[2]; //!< one buffer per I/Q channel
	SampleVector m_convertBuffer[2];
    SampleMIFifo *m_sampleFifo;
    Decimators<qint32, qint16, SDR_RX_SAMP_SZ, 12, true> m_decimatorsIQ[2];
    Decimators<qint32, qint16, SDR_RX_SAMP_SZ, 12, false> m_decimatorsQI[2];
    unsigned int m_log2Decim;
    int m_fcPos;
    bool m_iqOrder;

    static const int m_plutoSDRBlockSizeSamples = 64*256; //complex samples per buffer (must be multiple of 64)

    void run();
    int channelCallbackIQ(const qint16* buf, qint32 len, int channel);
    int channelCallbackQI(const qint16* buf, qint32 len, int channel);
};

#endif // _PLUTOSDR_PLUTOSDRMITHREAD_H_
