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

#include "feature/feature.h"
#include "dsp/datafifo.h"
#include "maincore.h"
#include "datapipescommon.h"
#include "datapipesgcworker.h"

bool DataPipesGCWorker::DataPipesGC::existsProducer(const ChannelAPI *channel)
{
    return MainCore::instance()->existsChannel(channel);
}

bool DataPipesGCWorker::DataPipesGC::existsConsumer(const Feature *feature)
{
    return MainCore::instance()->existsFeature(feature);
}

void DataPipesGCWorker::DataPipesGC::sendMessageToConsumer(const DataFifo *fifo,  DataPipesCommon::ChannelRegistrationKey channelKey, Feature *feature)
{
    DataPipesCommon::MsgReportChannelDeleted *msg = DataPipesCommon::MsgReportChannelDeleted::create(
        fifo, channelKey);
    feature->getInputMessageQueue()->push(msg);
}

DataPipesGCWorker::DataPipesGCWorker() :
    m_running(false)
{}

DataPipesGCWorker::~DataPipesGCWorker()
{}

void DataPipesGCWorker::startWork()
{
    connect(&m_gcTimer, SIGNAL(timeout()), this, SLOT(processGC()));
    m_gcTimer.start(10000); // collect garbage every 10s
    m_running = true;
}

void DataPipesGCWorker::stopWork()
{
    m_running = false;
    m_gcTimer.stop();
    disconnect(&m_gcTimer, SIGNAL(timeout()), this, SLOT(processGC()));
}

void DataPipesGCWorker::addDataFifoToDelete(DataFifo *dataFifo)
{
    if (dataFifo)
    {
        m_gcTimer.start(10000); // restart GC to make sure deletion is postponed
        m_dataPipesGC.addElementToDelete(dataFifo);
    }
}

void DataPipesGCWorker::processGC()
{
    // qDebug("MessagePipesGCWorker::processGC");
    m_dataPipesGC.processGC();
}
