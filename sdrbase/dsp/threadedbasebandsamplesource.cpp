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

#include <QDebug>
#include <QThread>

#include "dsp/threadedbasebandsamplesource.h"

ThreadedBasebandSampleSource::ThreadedBasebandSampleSource(BasebandSampleSource* sampleSource, QObject *parent) :
        m_basebandSampleSource(sampleSource)
{
    QString name = "ThreadedBasebandSampleSource(" + m_basebandSampleSource->objectName() + ")";
    setObjectName(name);

    qDebug() << "ThreadedBasebandSampleSource::ThreadedBasebandSampleSource: " << name;

    m_thread = new QThread(parent);
    m_basebandSampleSource->moveToThread(m_thread);

    qDebug() << "ThreadedBasebandSampleSource::ThreadedBasebandSampleSource: thread: " << thread() << " m_thread: " << m_thread;
}

ThreadedBasebandSampleSource::~ThreadedBasebandSampleSource()
{
    if (m_thread->isRunning()) {
        stop();
    }

    delete m_thread;
}

void ThreadedBasebandSampleSource::start()
{
    qDebug() << "ThreadedBasebandSampleSource::start";
    m_thread->start();
    m_basebandSampleSource->start();
}

void ThreadedBasebandSampleSource::stop()
{
    qDebug() << "ThreadedBasebandSampleSource::stop";
    m_basebandSampleSource->stop();
    m_thread->exit();
    m_thread->wait();
}

void ThreadedBasebandSampleSource::pull(Sample& sample)
{
	m_basebandSampleSource->pull(sample);
}

void ThreadedBasebandSampleSource::feed(SampleSourceFifo* sampleFifo,
	int nbSamples)
{
	m_basebandSampleSource->feed(sampleFifo, nbSamples);
}

bool ThreadedBasebandSampleSource::handleSourceMessage(const Message& cmd)
{
    return m_basebandSampleSource->handleMessage(cmd);
}

QString ThreadedBasebandSampleSource::getSampleSourceObjectName() const
{
    return m_basebandSampleSource->objectName();
}
