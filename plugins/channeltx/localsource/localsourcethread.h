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

#ifndef PLUGINS_CHANNELTX_LOCALOURCE_LOCALOURCETHREAD_H_
#define PLUGINS_CHANNELTX_LOCALOURCE_LOCALOURCETHREAD_H_

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include "dsp/dsptypes.h"
#include "util/message.h"
#include "util/messagequeue.h"

class SampleSourceFifo;

class LocalSourceThread : public QThread {
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

    LocalSourceThread(QObject* parent = nullptr);
    ~LocalSourceThread();

    void startStop(bool start);
    void setSampleFifo(SampleSourceFifo *sampleFifo);

public slots:
    void pullSamples(unsigned int count);

signals:
    void samplesAvailable(unsigned int iPart1Begin, unsigned int iPart1End, unsigned int iPart2Begin, unsigned int iPart2End);

private:
	QMutex m_startWaitMutex;
	QWaitCondition m_startWaiter;
	volatile bool m_running;
    SampleSourceFifo *m_sampleFifo;

    MessageQueue m_inputMessageQueue;

    void startWork();
    void stopWork();
    void run();

private slots:
    void handleInputMessages();
};

#endif // PLUGINS_CHANNELTX_LOCALSINK_LOCALSINKTHREAD_H_

