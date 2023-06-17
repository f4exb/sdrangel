///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_AUDIOCATINPUTWORKER_H
#define INCLUDE_AUDIOCATINPUTWORKER_H

#include <QObject>

#include "dsp/decimators.h"
#include "audiocatsisosettings.h"

class AudioFifo;
class SampleMIFifo;

class AudioCATInputWorker : public QObject {
    Q_OBJECT

public:
    AudioCATInputWorker(SampleMIFifo* sampleFifo, AudioFifo *fifo, QObject* parent = nullptr);
    ~AudioCATInputWorker();

    void startWork();
    void stopWork();
    void setLog2Decimation(unsigned int log2_decim) {m_log2Decim = log2_decim;}
    void setFcPos(int fcPos) { m_fcPos = fcPos; }
    void setIQMapping(AudioCATSISOSettings::IQMapping iqMapping) {m_iqMapping = iqMapping;}

    static const int m_convBufSamples = 4096;
private:
    AudioFifo* m_fifo;

    bool m_running;
    unsigned int m_log2Decim;
    int m_fcPos;
    AudioCATSISOSettings::IQMapping m_iqMapping;

    qint16 m_buf[m_convBufSamples*2]; // stereo (I, Q)
    SampleVector m_convertBuffer;
    SampleMIFifo* m_sampleFifo;
    Decimators<qint32, qint16, SDR_RX_SAMP_SZ, 16, true> m_decimatorsIQ;

    void workIQ(unsigned int nbRead);
    void decimate(qint16 *buf, unsigned int nbRead);

private slots:
    void handleAudio();
};

#endif // INCLUDE_AUDIOCATINPUTWORKER_H
