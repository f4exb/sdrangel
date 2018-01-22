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

#ifndef PLUGINS_SAMPLESOURCE_PLUTOSDROUTPUT_PLUTOSDROUTPUTTHREAD_H_
#define PLUGINS_SAMPLESOURCE_PLUTOSDROUTPUT_PLUTOSDROUTPUTTHREAD_H_

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include "dsp/samplesourcefifo.h"
#include "dsp/interpolators.h"
#include "plutosdr/deviceplutosdrshared.h"

class DevicePlutoSDRBox;

class PlutoSDROutputThread : public QThread, public DevicePlutoSDRShared::ThreadInterface
{
    Q_OBJECT

public:
    PlutoSDROutputThread(uint32_t blocksize, DevicePlutoSDRBox* plutoBox, SampleSourceFifo* sampleFifo, QObject* parent = 0);
    ~PlutoSDROutputThread();

    virtual void startWork();
    virtual void stopWork();
    virtual void setDeviceSampleRate(int sampleRate __attribute__((unused))) {}
    virtual bool isRunning() { return m_running; }
    void setLog2Interpolation(unsigned int log2_interp);

private:
    QMutex m_startWaitMutex;
    QWaitCondition m_startWaiter;
    bool m_running;

    DevicePlutoSDRBox *m_plutoBox;
    int16_t *m_buf;                 //!< holds I+Q values of each sample from devce
//    int16_t *m_bufConv;             //!< holds I+Q values of each sample converted to host format via iio_channel_convert
    uint32_t m_blockSizeSamples;    //!< buffer sizes in number of (I,Q) samples
    SampleSourceFifo* m_sampleFifo; //!< DSP sample FIFO (I,Q)

    unsigned int m_log2Interp; // soft interpolation

    Interpolators<qint16, SDR_TX_SAMP_SZ, 12> m_interpolators;

    void run();
    void convert(qint16* buf, qint32 len);

};


#endif /* PLUGINS_SAMPLESOURCE_PLUTOSDROUTPUT_PLUTOSDROUTPUTTHREAD_H_ */
