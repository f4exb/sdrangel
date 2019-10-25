///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_SAMPLEMIMO_TESTMOSYNC_TESTMOSYNCTHREAD_H_
#define PLUGINS_SAMPLEMIMO_TESTMOSYNC_TESTMOSYNCTHREAD_H_

// configure two Tx

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QElapsedTimer>

#include "dsp/interpolators.h"

#define TESTMOSYNC_THROTTLE_MS 50

class QTimer;
class SampleMOFifo;

class TestMOSyncThread : public QThread {
    Q_OBJECT

public:
    TestMOSyncThread(QObject* parent = nullptr);
    ~TestMOSyncThread();

    void startWork();
    void stopWork();
    bool isRunning() const { return m_running; }
	void setSamplerate(int samplerate);
    void setLog2Interpolation(unsigned int log2_interp);
    unsigned int getLog2Interpolation() const;
    void setFcPos(int fcPos);
    int getFcPos() const;
    void setFifo(SampleMOFifo *sampleFifo) { m_sampleFifo = sampleFifo; }
    SampleMOFifo *getFifo() { return m_sampleFifo; }
    void connectTimer(const QTimer& timer);

private:
    QMutex m_startWaitMutex;
    QWaitCondition m_startWaiter;
    bool m_running;

    qint16 *m_buf; //!< Full buffer for SISO or MIMO operation
    SampleMOFifo* m_sampleFifo;
    Interpolators<qint16, SDR_TX_SAMP_SZ, 12>  m_interpolators[2];
    unsigned int m_log2Interp;
    int m_fcPos;

    int m_throttlems;
    QElapsedTimer m_elapsedTimer;
    bool m_throttleToggle;
	unsigned int m_samplesChunkSize;
    unsigned int m_samplesRemainder;
	int m_samplerate;

    void run();
    unsigned int getNbFifos();
    void callbackPart(qint16* buf, qint32 samplesPerChannel, int iBegin, qint32 nSamples);
    void callback(qint16* buf, qint32 samplesPerChannel);

private slots:
	void tick();
};

#endif // PLUGINS_SAMPLEMIMO_TESTMOSYNC_TESTMOSYNCTHREAD_H_
