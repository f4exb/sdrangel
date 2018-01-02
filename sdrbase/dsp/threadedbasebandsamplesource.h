///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef SDRBASE_DSP_THREADEDBASEBANDSAMPLESOURCE_H_
#define SDRBASE_DSP_THREADEDBASEBANDSAMPLESOURCE_H_

#include <QMutex>

#include "dsp/basebandsamplesource.h"
#include "util/messagequeue.h"
#include "util/export.h"

class BasebandSampleSource;
class QThread;

/**
 * This class is a wrapper for BasebandSampleSource that runs the BasebandSampleSource object in its own thread
 */
class SDRANGEL_API ThreadedBasebandSampleSource : public QObject {
	Q_OBJECT

public:
	ThreadedBasebandSampleSource(BasebandSampleSource* sampleSource, QObject *parent = 0);
	~ThreadedBasebandSampleSource();

	const BasebandSampleSource *getSource() const { return m_basebandSampleSource; }

	void start(); //!< this thread start()
	void stop();  //!< this thread exit() and wait()

	bool handleSourceMessage(const Message& cmd);  //!< Send message to source synchronously
	void pull(Sample& sample);                     //!< Pull one sample from source
	void pullAudio(int nbSamples) { if (m_basebandSampleSource) m_basebandSampleSource->pullAudio(nbSamples); }

    /** direct feeding of sample source FIFO */
	void feed(SampleSourceFifo* sampleFifo,
		int nbSamples);

	SampleSourceFifo& getSampleSourceFifo() { return m_basebandSampleSource->getSampleSourceFifo(); }
	void setDeviceSampleSourceFifo(SampleSourceFifo *deviceSampleFifo) { m_basebandSampleSource->setDeviceSampleSourceFifo(deviceSampleFifo); }

	QString getSampleSourceObjectName() const;

protected:
	QThread *m_thread; //!< The thead object
	BasebandSampleSource* m_basebandSampleSource;
};

#endif /* SDRBASE_DSP_THREADEDBASEBANDSAMPLESOURCE_H_ */
