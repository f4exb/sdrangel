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
    int16_t *m_buf;               //!< holds I+Q values of each sample from devce
    int16_t *m_bufConv;           //!< holds I+Q values of each sample converted to host format via iio_channel_convert
    uint32_t m_blockSizeSamples;  //!< buffer sizes in number of (I,Q) samples
    SampleVector m_convertBuffer; //!< vector of (I,Q) samples used for decimation and scaling conversion
    SampleVector::iterator m_convertIt;
    SampleSinkFifo* m_sampleFifo; //!< DSP sample FIFO (I,Q)

    unsigned int m_log2Decim; // soft decimation
    int m_fcPos;
    float m_phasor;

#ifdef SDR_RX_SAMPLE_24BIT
    Decimators<qint64, qint16, SDR_RX_SAMP_SZ, 12> m_decimators;
#else
    Decimators<qint32, qint16, SDR_RX_SAMP_SZ, 12> m_decimators;
#endif

    void run();
    void convert(const qint16* buf, qint32 len);

};


#endif /* PLUGINS_SAMPLESOURCE_PLUTOSDRINPUT_PLUTOSDRINPUTTHREAD_H_ */
