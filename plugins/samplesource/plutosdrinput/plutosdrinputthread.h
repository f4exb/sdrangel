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

#ifndef PLUGINS_SAMPLESOURCE_PLUTOSDRINPUT_PLUTOSDRINPUTTHREAD_H_
#define PLUGINS_SAMPLESOURCE_PLUTOSDRINPUT_PLUTOSDRINPUTTHREAD_H_

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include "dsp/samplesinkfifo.h"
#include "dsp/decimators.h"
#include "plutosdr/deviceplutosdrshared.h"

class DevicePlutoSDRBox;

class PlutoSDRInputThread : public QThread, public DevicePlutoSDRShared::ThreadInterface
{
    Q_OBJECT

public:
    PlutoSDRInputThread(uint32_t blocksize, DevicePlutoSDRBox* plutoBox, SampleSinkFifo* sampleFifo, QObject* parent = 0);
    ~PlutoSDRInputThread();

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

    DevicePlutoSDRBox *m_plutoBox;
    qint16 *m_buf; // holds I+Q values of each sample
    uint32_t m_blockSize;
    SampleVector m_convertBuffer;
    SampleSinkFifo* m_sampleFifo;

    unsigned int m_log2Decim; // soft decimation
    int m_fcPos;

    Decimators<qint16, SDR_SAMP_SZ, 12> m_decimators;

    void run();
    void convert(const qint16* buf, qint32 len);

};


#endif /* PLUGINS_SAMPLESOURCE_PLUTOSDRINPUT_PLUTOSDRINPUTTHREAD_H_ */
