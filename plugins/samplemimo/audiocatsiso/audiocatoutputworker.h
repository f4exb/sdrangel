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

#ifndef INCLUDE_AUDIOCATOUTPUTWORKER_H
#define INCLUDE_AUDIOCATOUTPUTWORKER_H

#include <QObject>
#include <QElapsedTimer>

#include "dsp/interpolators.h"
#include "audiocatsisosettings.h"

class QTimer;
class SampleMOFifo;
class AudioFifo;

class AudioCATOutputWorker : public QObject {
    Q_OBJECT

public:
    AudioCATOutputWorker(SampleMOFifo* sampleFifo, AudioFifo *fifo, QObject* parent = nullptr);
    ~AudioCATOutputWorker();

    void startWork();
    void stopWork();
    void setSamplerate(int samplerate);
    void setVolume(int volume);
    void setIQMapping(AudioCATSISOSettings::IQMapping iqMapping) {m_iqMapping = iqMapping;}
    void connectTimer(const QTimer& timer);

private:
    bool m_running;
    int m_samplerate;
    float m_volume;
    int m_throttlems;
    int m_maxThrottlems;
    QElapsedTimer m_elapsedTimer;
    bool m_throttleToggle;
    AudioCATSISOSettings::IQMapping m_iqMapping;
    AudioVector m_audioBuffer;
    uint32_t m_audioBufferFill;
    qint16 *m_buf; // stereo (I, Q)
    unsigned int m_samplesChunkSize;
    SampleMOFifo* m_sampleFifo;
    AudioFifo* m_audioFifo;
    Interpolators<qint16, SDR_TX_SAMP_SZ, 16> m_interpolators;

    void callbackPart(SampleVector& data, unsigned int iBegin, unsigned int iEnd);

private slots:
	void tick();
};

#endif // INCLUDE_AUDIOCATOUTPUTWORKER_H
