///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB                                   //
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

#include "datapipes.h"
#include "datapipesgcworker.h"

DataPipes::DataPipes() :
    m_registrations(&m_dataFifoStore)
{
  	m_gcWorker = new DataPipesGCWorker(m_registrations);
	m_gcWorker->moveToThread(&m_gcThread);
	startGC();
}

DataPipes::~DataPipes()
{
	if (m_gcWorker->isRunning()) {
		stopGC();
	}

    m_gcWorker->deleteLater();
}

ObjectPipe *DataPipes::registerProducerToConsumer(const QObject *producer, const QObject *consumer, const QString& type)
{
    return m_registrations.registerProducerToConsumer(producer, consumer, type);
}

ObjectPipe *DataPipes::unregisterProducerToConsumer(const QObject *producer, const QObject *consumer, const QString& type)
{
    return m_registrations.unregisterProducerToConsumer(producer, consumer, type);
}

void DataPipes::getDataPipes(const QObject *producer, const QString& type, QList<ObjectPipe*>& pipes)
{
    return m_registrations.getPipes(producer, type, pipes);
}

void DataPipes::startGC()
{
	qDebug("DataPipes::startGC");

    m_gcWorker->startWork();
    m_gcThread.start();
}

void DataPipes::stopGC()
{
    qDebug("DataPipes::stopGC");
	m_gcWorker->stopWork();
	m_gcThread.quit();
	m_gcThread.wait();
}
