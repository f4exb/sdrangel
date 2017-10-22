///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_FCDPROPLUSREADER_H
#define INCLUDE_FCDPROPLUSREADER_H

#include <QObject>
#include <QMutex>
#include <QWaitCondition>
#include <QByteArray>

#include "dsp/samplesinkfifo.h"
#include "dsp/inthalfbandfilter.h"

class QAudioInput;
class QIODevice;
class QAudioDeviceInfo;

class FCDProPlusReader : public QObject {
	Q_OBJECT

public:
	FCDProPlusReader(SampleSinkFifo* sampleFifo, QObject* parent = NULL);
	~FCDProPlusReader();

	void startWork();
	void stopWork();

private:
	QAudioInput *m_fcdAudioInput;
	QIODevice *m_fcdInput;

	QMutex m_startWaitMutex;
	QWaitCondition m_startWaiter;
	bool m_running;

	SampleVector m_convertBuffer;
	QByteArray m_fcdBuffer;
	SampleSinkFifo* m_sampleFifo;

	void openFcdAudio(const QAudioDeviceInfo& fcdAudioDeviceInfo);

private slots:
	void readFcdAudio();
};
#endif // INCLUDE_FCDPROPLUSREADER_H
