///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_SAMPLESOURCE_SDRPLAY_SDRPLAYTHREAD_H_
#define PLUGINS_SAMPLESOURCE_SDRPLAY_SDRPLAYTHREAD_H_

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <mirisdr.h>
#include "dsp/samplesinkfifo.h"
#include "dsp/decimators.h"

#define SDRPLAY_INIT_NBSAMPLES (1<<14)

class SDRPlayThread : public QThread {
    Q_OBJECT

public:
    SDRPlayThread(mirisdr_dev_t* dev, SampleSinkFifo* sampleFifo, QObject* parent = NULL);
    ~SDRPlayThread();

    void startWork();
    void stopWork();
    void setSamplerate(int samplerate);
    void setLog2Decimation(unsigned int log2_decim);
    void setFcPos(int fcPos);

private:
    QMutex m_startWaitMutex;
    QWaitCondition m_startWaiter;
    bool m_running;

    mirisdr_dev_t *m_dev;
    SampleVector m_convertBuffer;
    SampleSinkFifo* m_sampleFifo;

    int m_samplerate;
    unsigned int m_log2Decim;
    int m_fcPos;

#ifdef SDR_RX_SAMPLE_24BIT
    Decimators<qint64, qint16, SDR_RX_SAMP_SZ, 12> m_decimators;
#else
    Decimators<qint32, qint16, SDR_RX_SAMP_SZ, 12> m_decimators;
#endif

    void run();
    void callback(const qint16* buf, qint32 len);

    static void callbackHelper(unsigned char* buf, uint32_t len, void* ctx);
};

#endif /* PLUGINS_SAMPLESOURCE_SDRPLAY_SDRPLAYTHREAD_H_ */
