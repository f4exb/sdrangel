///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_SAMPLESOURCE_LIMESDRINPUT_LIMESDRINPUTTHREAD_H_
#define PLUGINS_SAMPLESOURCE_LIMESDRINPUT_LIMESDRINPUTTHREAD_H_

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include "lime/LimeSuite.h"

#include "dsp/samplesinkfifo.h"
#include "dsp/decimators.h"
#include "limesdr/devicelimesdrshared.h"
#include "limesdr/devicelimesdr.h"

class LimeSDRInputThread : public QThread, public DeviceLimeSDRShared::ThreadInterface
{
    Q_OBJECT

public:
    LimeSDRInputThread(lms_stream_t* stream, SampleSinkFifo* sampleFifo, QObject* parent = 0);
    ~LimeSDRInputThread();

    virtual void startWork();
    virtual void stopWork();
    virtual void setDeviceSampleRate(int sampleRate) { (void) sampleRate; }
    virtual bool isRunning() { return m_running; }
    void setLog2Decimation(unsigned int log2_decim);
    void setIQOrder(bool iqOrder) { m_iqOrder = iqOrder; }

private:
    QMutex m_startWaitMutex;
    QWaitCondition m_startWaiter;
    bool m_running;

    lms_stream_t* m_stream;
    qint16 m_buf[2*DeviceLimeSDR::blockSize]; //must hold I+Q values of each sample hence 2xcomplex size
    SampleVector m_convertBuffer;
    SampleSinkFifo* m_sampleFifo;

    unsigned int m_log2Decim; // soft decimation
    bool m_iqOrder;

    Decimators<qint32, qint16, SDR_RX_SAMP_SZ, 12, true> m_decimatorsIQ;
    Decimators<qint32, qint16, SDR_RX_SAMP_SZ, 12, false> m_decimatorsQI;

    void run();
    void callbackIQ(const qint16* buf, qint32 len);
    void callbackQI(const qint16* buf, qint32 len);
};



#endif /* PLUGINS_SAMPLESOURCE_LIMESDRINPUT_LIMESDRINPUTTHREAD_H_ */
