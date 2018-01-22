///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef PLUGINS_SAMPLESOURCE_LIMESDROUTPUT_LIMESDROUTPUTTHREAD_H_
#define PLUGINS_SAMPLESOURCE_LIMESDROUTPUT_LIMESDROUTPUTTHREAD_H_

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include "lime/LimeSuite.h"

#include "dsp/samplesourcefifo.h"
#include "dsp/interpolators.h"
#include "limesdr/devicelimesdrshared.h"

#define LIMESDROUTPUT_BLOCKSIZE (1<<15) //complex samples per buffer ~10k (16k)

class LimeSDROutputThread : public QThread, public DeviceLimeSDRShared::ThreadInterface
{
    Q_OBJECT

public:
    LimeSDROutputThread(lms_stream_t* stream, SampleSourceFifo* sampleFifo, QObject* parent = 0);
    ~LimeSDROutputThread();

    virtual void startWork();
    virtual void stopWork();
    virtual void setDeviceSampleRate(int __attribute__((unused)) sampleRate) {}
    virtual bool isRunning() { return m_running; }
    void setLog2Interpolation(unsigned int log2_ioterp);
    void setFcPos(int fcPos);

private:
    QMutex m_startWaitMutex;
    QWaitCondition m_startWaiter;
    bool m_running;

    lms_stream_t* m_stream;
    qint16 m_buf[2*LIMESDROUTPUT_BLOCKSIZE]; //must hold I+Q values of each sample hence 2xcomplex size
    SampleSourceFifo* m_sampleFifo;

    unsigned int m_log2Interp; // soft decimation
    int m_fcPos;

    Interpolators<qint16, SDR_TX_SAMP_SZ, 12> m_interpolators;

    void run();
    void callback(qint16* buf, qint32 len);
};



#endif /* PLUGINS_SAMPLESOURCE_LIMESDROUTPUT_LIMESDROUTPUTTHREAD_H_ */
