///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2019 Edouard Griffiths, F4EXB                              //
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

#ifndef INCLUDE_FILEINPUTWORKER_H
#define INCLUDE_FILEINPUTWORKER_H

#include <QTimer>
#include <QElapsedTimer>
#include <iostream>
#include <fstream>
#include <cstdlib>

#include "dsp/inthalfbandfilter.h"
#include "util/message.h"

#define FILESOURCE_THROTTLE_MS 50

class SampleSinkFifo;
class MessageQueue;

class FileInputWorker : public QObject {
	Q_OBJECT

public:
    class MsgReportEOF : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgReportEOF* create() {
            return new MsgReportEOF();
        }

    private:
        MsgReportEOF() :
            Message()
        { }
    };

	FileInputWorker(std::ifstream *samplesStream,
	        SampleSinkFifo* sampleFifo,
	        const QTimer& timer,
	        MessageQueue *fileInputMessageQueue,
	        QObject* parent = NULL);
	~FileInputWorker();

	void startWork();
	void stopWork();
	void setSampleRateAndSize(int samplerate, quint32 samplesize);
    void setBuffers(std::size_t chunksize);
	bool isRunning() const { return m_running; }
    quint64 getSamplesCount() const { return m_samplesCount; }
    void setSamplesCount(quint64 samplesCount) { m_samplesCount = samplesCount; }

private:
	volatile bool m_running;

	std::ifstream* m_ifstream;
	quint8  *m_fileBuf;
	quint8  *m_convertBuf;
	std::size_t m_bufsize;
    qint64 m_chunksize;
	SampleSinkFifo* m_sampleFifo;
    quint64 m_samplesCount;
    const QTimer& m_timer;
    MessageQueue *m_fileInputMessageQueue;

	int m_samplerate;      //!< File I/Q stream original sample rate
    quint64 m_samplesize;  //!< File effective sample size in bits (I or Q). Ex: 16, 24.
    quint64 m_samplebytes; //!< Number of bytes used to store a I or Q sample. Ex: 2. 4.
    qint64 m_throttlems;
    QElapsedTimer m_elapsedTimer;
    bool m_throttleToggle;

	//void decimate1(SampleVector::iterator* it, const qint16* buf, qint32 len);
	void writeToSampleFifo(const quint8* buf, qint32 nbBytes);

private slots:
	void tick();
};

#endif // INCLUDE_FILEINPUTWORKER_H
