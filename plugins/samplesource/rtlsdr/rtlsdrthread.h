///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2014 John Greb <hexameron@spam.no>                              //
// Copyright (C) 2015-2016, 2018-2020 Edouard Griffiths, F4EXB <f4exb06@gmail.com> //
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

#ifndef INCLUDE_RTLSDRTHREAD_H
#define INCLUDE_RTLSDRTHREAD_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <rtl-sdr.h>

#include "rtlsdrsettings.h"

#include "dsp/replaybuffer.h"
#include "dsp/samplesinkfifo.h"
#include "dsp/decimatorsu.h"

class RTLSDRThread : public QThread {
	Q_OBJECT

public:
	RTLSDRThread(rtlsdr_dev_t* dev, SampleSinkFifo* sampleFifo, ReplayBuffer<quint8> *replayBuffer, const RTLSDRSettings& settings, QObject* parent = nullptr);
	~RTLSDRThread();

	void startWork();
	void stopWork();
	MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

private:
	QMutex m_startWaitMutex;
	QWaitCondition m_startWaiter;
	bool m_running;

	rtlsdr_dev_t* m_dev;
	SampleVector m_convertBuffer;
	SampleSinkFifo* m_sampleFifo;
	ReplayBuffer<quint8> *m_replayBuffer;

	RTLSDRSettings m_settings;
	MessageQueue m_inputMessageQueue;  //!< Queue for asynchronous inbound communication

	DecimatorsU<qint32, quint8, SDR_RX_SAMP_SZ, 8, 127, true> m_decimatorsIQ;
	DecimatorsU<qint32, quint8, SDR_RX_SAMP_SZ, 8, 127, false> m_decimatorsQI;

	void run();
	void callbackIQ(const quint8* buf, qint32 len);
	void callbackQI(const quint8* buf, qint32 len);

	bool handleMessage(const Message& cmd);
	bool applySettings(const RTLSDRSettings& settings, const QStringList& settingsKeys, bool force);

#ifndef __EMSCRIPTEN__
	static void callbackHelper(unsigned char* buf, uint32_t len, void* ctx);
#endif

private slots:
	void handleInputMessages();
};

#endif // INCLUDE_RTLSDRTHREAD_H
