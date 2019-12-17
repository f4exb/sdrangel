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
#include "util/incrementalvector.h"

#define TESTMOSYNC_THROTTLE_MS 50

class QTimer;
class SampleMOFifo;
class BasebandSampleSink;

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
    void setSpectrumSink(BasebandSampleSink *spectrumSink) { m_spectrumSink = spectrumSink; }
    void setFeedSpectrumIndex(unsigned int  feedSpectrumIndex) { m_feedSpectrumIndex = feedSpectrumIndex > 1 ? 1 : feedSpectrumIndex; }

private:
#pragma pack(push, 1)
    struct Sample16
    {
        int16_t m_real;
        int16_t m_imag;
    };
#pragma pack(pop)
    QMutex m_startWaitMutex;
    QWaitCondition m_startWaiter;
    bool m_running;

    qint16 *m_buf; //!< Full buffer for SISO or MIMO operation
    SampleMOFifo* m_sampleFifo;
    Interpolators<qint16, SDR_TX_SAMP_SZ, 16>  m_interpolators[2];
    unsigned int m_log2Interp;
    int m_fcPos;

    int m_throttlems;
    QElapsedTimer m_elapsedTimer;
    bool m_throttleToggle;
	unsigned int m_samplesChunkSize;
    unsigned int m_blockSize;
    unsigned int m_samplesRemainder;
	int m_samplerate;

    unsigned int m_feedSpectrumIndex;
    BasebandSampleSink* m_spectrumSink;
    IncrementalVector<Sample> m_samplesVector;
    IncrementalVector<Sample> m_testVector;

    void run();
    unsigned int getNbFifos();
    void callbackPart(qint16* buf, qint32 nSamples, int iBegin);
    void callback(qint16* buf, qint32 samplesPerChannel);
    void feedSpectrum(int16_t *buf, unsigned int bufSize);

private slots:
	void tick();
};

#endif // PLUGINS_SAMPLEMIMO_TESTMOSYNC_TESTMOSYNCTHREAD_H_
