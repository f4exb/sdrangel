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

#ifndef _PLUTOSDR_PLUTOSDRMOTHREAD_H_
#define _PLUTOSDR_PLUTOSDRMOTHREAD_H_

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include "dsp/interpolators.h"

class SampleMOFifo;
class DevicePlutoSDRBox;

class PlutoSDRMOThread : public QThread {
    Q_OBJECT
public:
    PlutoSDRMOThread(DevicePlutoSDRBox* plutoBox, QObject* parent = nullptr);
    ~PlutoSDRMOThread();

    void startWork();
    void stopWork();
    bool isRunning() const { return m_running; }
    void setLog2Interpolation(unsigned int log2Interp);
    unsigned int getLog2Interpolation() const;
    void setFcPos(int fcPos);
    int getFcPos() const;
    void setFifo(SampleMOFifo *sampleFifo) { m_sampleFifo = sampleFifo; }
    SampleMOFifo *getFifo() { return m_sampleFifo; }

private:
    QMutex m_startWaitMutex;
    QWaitCondition m_startWaiter;
    bool m_running;

    DevicePlutoSDRBox *m_plutoBox;
    qint16 *m_buf[2]; //!< one buffer per I/Q channel
    SampleMOFifo *m_sampleFifo;
    Interpolators<qint16, SDR_TX_SAMP_SZ, 12>  m_interpolators[2];
    unsigned int m_log2Interp;
    int m_fcPos;

    void run();
    unsigned int getNbFifos();
    void callbackPart(qint16* buf[2], qint32 nSamples, int iBegin);
    void callback(qint16* buf[2], qint32 samplesPerChannel);
};

#endif // _PLUTOSDR_PLUTOSDRMOTHREAD_H_
