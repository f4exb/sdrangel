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

#ifndef PLUGINS_SAMPLEMIMO_LIMESDRMIMO_LIMESDRMOTHREAD_H_
#define PLUGINS_SAMPLEMIMO_LIMESDRMIMO_LIMESDRMOTHREAD_H_

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include "lime/LimeSuite.h"

#include "limesdr/devicelimesdr.h"
#include "dsp/interpolators.h"

class SampleMOFifo;

class LimeSDRMOThread : public QThread {
    Q_OBJECT

public:
    LimeSDRMOThread(lms_stream_t* stream0, lms_stream_t* stream1, QObject* parent = nullptr);
    ~LimeSDRMOThread();

    void startWork();
    void stopWork();
    bool isRunning() const { return m_running; }
    void setLog2Interpolation(unsigned int log2Interp);
    unsigned int getLog2Interpolation() const;
    void setFifo(SampleMOFifo *sampleFifo) { m_sampleFifo = sampleFifo; }
    SampleMOFifo *getFifo() { return m_sampleFifo; }

private:
    QMutex m_startWaitMutex;
    QWaitCondition m_startWaiter;
    bool m_running;
    lms_stream_t* m_stream0;
    lms_stream_t* m_stream1;

    qint16 *m_buf; //!< Full buffer for SISO or MIMO operation
    SampleMOFifo* m_sampleFifo;
    Interpolators<qint16, SDR_TX_SAMP_SZ, 12>  m_interpolators[2];
    unsigned int m_log2Interp;

    void run();
    unsigned int getNbFifos();
    void callbackPart(qint16* buf, qint32 nSamples, int iBegin);
    void callback(qint16* buf, qint32 samplesPerChannel);
};



#endif /* PLUGINS_SAMPLEMIMO_LIMESDRMIMO_LIMESDRMOTHREAD_H_ */
