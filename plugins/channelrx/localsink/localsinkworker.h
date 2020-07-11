///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB.                                  //
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

#ifndef PLUGINS_CHANNELRX_LOCALSINK_LOCALSINKWORKER_H_
#define PLUGINS_CHANNELRX_LOCALSINK_LOCALSINKWORKER_H_

#include <QObject>
#include <QMutex>
#include <QWaitCondition>

#include "dsp/dsptypes.h"
#include "util/message.h"
#include "util/messagequeue.h"

class SampleSinkFifo;

class LocalSinkWorker : public QObject {
    Q_OBJECT

public:
    class MsgStartStop : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getStartStop() const { return m_startStop; }

        static MsgStartStop* create(bool startStop) {
            return new MsgStartStop(startStop);
        }

    protected:
        bool m_startStop;

        MsgStartStop(bool startStop) :
            Message(),
            m_startStop(startStop)
        { }
    };

    LocalSinkWorker(QObject* parent = 0);
    ~LocalSinkWorker();

    void startStop(bool start);
    void setSampleFifo(SampleSinkFifo *sampleFifo) { m_sampleFifo = sampleFifo; }
    void setDeviceSampleFifo(SampleSinkFifo *sampleFifo) { m_deviceSampleFifo  = sampleFifo; }

public slots:
    void handleData(); //!< Handle data when samples have to be processed

private:
	volatile bool m_running;
    SampleSinkFifo *m_sampleFifo;
    SampleSinkFifo *m_deviceSampleFifo;
    MessageQueue m_inputMessageQueue;

	void startWork();
	void stopWork();

private slots:
    void handleInputMessages();
};

#endif // PLUGINS_CHANNELRX_LOCALSINK_LOCALSINKWORKER_H_

