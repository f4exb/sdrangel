///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2017 Sergey Kostanbaev, Fairwaves Inc.                          //
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

#ifndef PLUGINS_SAMPLESOURCE_XTRXINPUT_XTRXINPUTTHREAD_H_
#define PLUGINS_SAMPLESOURCE_XTRXINPUT_XTRXINPUTTHREAD_H_

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include "xtrx_api.h"

#include "dsp/samplesinkfifo.h"
#include "dsp/decimators.h"
#include "xtrx/devicextrxshared.h"

#define XTRX_BLOCKSIZE (1<<15) //complex samples per buffer (was 1<<13)

class XTRXInputThread : public QThread, public DeviceXTRXShared::ThreadInterface
{
    Q_OBJECT

public:
    XTRXInputThread(DeviceXTRXShared* shared, SampleSinkFifo* sampleFifo, QObject* parent = 0);
    ~XTRXInputThread();

    virtual void startWork();
    virtual void stopWork();
    virtual void setDeviceSampleRate(int sampleRate __attribute__((unused))) {}
    virtual bool isRunning() { return m_running; }
    void setLog2Decimation(unsigned int log2_decim);
    void setFcPos(int fcPos);

private:
    QMutex m_startWaitMutex;
    QWaitCondition m_startWaiter;
    bool m_running;

    qint16 m_buf[2*XTRX_BLOCKSIZE]; //must hold I+Q values of each sample hence 2xcomplex size
    SampleVector m_convertBuffer;
    SampleSinkFifo* m_sampleFifo;

    unsigned int m_log2Decim; // soft decimation
    int m_fcPos;

    Decimators<qint32, qint16, SDR_RX_SAMP_SZ, 12> m_decimators;

    DeviceXTRXShared* m_shared;

    void run();
    void callback(const qint16* buf, qint32 len);
};



#endif /* PLUGINS_SAMPLESOURCE_XTRXINPUT_XTRXINPUTTHREAD_H_ */
