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

#include "dsp/datafifo.h"

#include "datapipesgcworker.h"
#include "datapipes.h"

DataPipes::DataPipes()
{
	m_gcWorker = new DataPipesGCWorker();
	m_gcWorker->setC2FRegistrations(
		m_registrations.getMutex(),
		m_registrations.getElements(),
		m_registrations.getConsumers()
	);
	m_gcWorker->moveToThread(&m_gcThread);
	startGC();
}

DataPipes::~DataPipes()
{
	if (m_gcWorker->isRunning()) {
		stopGC();
	}
}

DataFifo *DataPipes::registerChannelToFeature(const ChannelAPI *source, Feature *feature, const QString& type)
{
	return m_registrations.registerProducerToConsumer(source, feature, type);
}

DataFifo *DataPipes::unregisterChannelToFeature(const ChannelAPI *source, Feature *feature, const QString& type)
{
	DataFifo *dataFifo = m_registrations.unregisterProducerToConsumer(source, feature, type);
	m_gcWorker->addDataFifoToDelete(dataFifo);
	return dataFifo;
}

QList<DataFifo*>* DataPipes::getFifos(const ChannelAPI *source, const QString& type)
{
	return m_registrations.getElements(source, type);
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
