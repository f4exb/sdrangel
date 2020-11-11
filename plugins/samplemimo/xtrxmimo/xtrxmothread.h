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

#ifndef PLUGINS_SAMPLEMIMO_XTRXMIMO_XTRXMOTHREAD_H_
#define PLUGINS_SAMPLEMIMO_XTRXMIMO_XTRXMOTHREAD_H_

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include "xtrx_api.h"

#include "xtrx/devicextrx.h"
#include "dsp/interpolators.h"

class SampleMOFifo;

class XTRXMOThread : public QThread {
    Q_OBJECT

public:
    XTRXMOThread(struct xtrx_dev *dev, QObject* parent = nullptr);
    ~XTRXMOThread();

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
    struct xtrx_dev* m_dev;

    qint16 *m_buf; //!< Full buffer for SISO or MIMO operation
    SampleMOFifo* m_sampleFifo;
    Interpolators<qint16, SDR_TX_SAMP_SZ, 12>  m_interpolators[2];
    unsigned int m_log2Interp;

    void run();
    void callback(qint16* buf0, qint16* buf1, qint32 samplesPerChannel);
    void callbackPart(qint16* buf0, qint16* buf1, qint32 nSamples, int iBegin);
};

#endif // PLUGINS_SAMPLEMIMO_XTRXMIMO_XTRXMOTHREAD_H_