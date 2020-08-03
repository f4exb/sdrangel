///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_FILEOUTPUTWORKER_H
#define INCLUDE_FILEOUTPUTWORKER_H

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <stdint.h>

#include "dsp/inthalfbandfilter.h"
#include "dsp/interpolators.h"

#define FILEOUTPUT_THROTTLE_MS 50

class SampleSourceFifo;

class FileOutputWorker : public QObject {
	Q_OBJECT

public:
	FileOutputWorker(std::ofstream *samplesStream, SampleSourceFifo* sampleFifo, QObject* parent = 0);
	~FileOutputWorker();

	void startWork();
	void stopWork();
	void setSamplerate(int samplerate);
	void setLog2Interpolation(int log2Interpolation);
    void setBuffer(std::size_t chunksize);
	bool isRunning() const { return m_running; }
    std::size_t getSamplesCount() const { return m_samplesCount; }
    void setSamplesCount(int samplesCount) { m_samplesCount = samplesCount; }

	void connectTimer(const QTimer& timer);

private:
	volatile bool m_running;

	std::ofstream* m_ofstream;
	std::size_t m_bufsize;
	unsigned int m_samplesChunkSize;
	SampleSourceFifo* m_sampleFifo;
    std::size_t m_samplesCount;

	int m_samplerate;
	int m_log2Interpolation;
    int m_throttlems;
    int m_maxThrottlems;
    QElapsedTimer m_elapsedTimer;
    bool m_throttleToggle;

    Interpolators<qint16, SDR_TX_SAMP_SZ, 16> m_interpolators;
    int16_t *m_buf;

	void callbackPart(SampleVector& data, unsigned int iBegin, unsigned int iEnd);

private slots:
	void tick();
};

#endif // INCLUDE_FILEOUTPUTWORKER_H
