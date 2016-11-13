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
#include <mirsdrapi-rsp.h>
#include "dsp/samplesinkfifo.h"
#include "dsp/decimators.h"

#define SDRPLAY_INIT_NBSAMPLES (1<<14)

class SDRPlayThread : public QThread {
    Q_OBJECT

public:
    SDRPlayThread(SampleSinkFifo* sampleFifo, QObject* parent = NULL);
    ~SDRPlayThread();

    void startWork();
    void stopWork();
    void setLog2Decimation(unsigned int log2_decim);
    void setFcPos(int fcPos);

    static void streamCallback (
            short *xi,
            short *xq,
            unsigned int firstSampleNum,
            int grChanged,
            int rfChanged,
            int fsChanged,
            unsigned int numSamples,
            unsigned int reset,
            void *cbContext);

private:
    QMutex m_startWaitMutex;
    QWaitCondition m_startWaiter;
    bool m_running;

    SampleVector m_convertBuffer;
    SampleSinkFifo* m_sampleFifo;

    unsigned int m_log2Decim;
    int m_fcPos;
    static SDRPlayThread *m_this;

    Decimators<qint16, SDR_SAMP_SZ, 12> m_decimators;

    void run();
    void callback(short *xi, short *xq, unsigned int numSamples);
};

#endif /* PLUGINS_SAMPLESOURCE_SDRPLAY_SDRPLAYTHREAD_H_ */
