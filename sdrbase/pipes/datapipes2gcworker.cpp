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

#include "dsp/datafifo.h"
#include "datapipes2gcworker.h"

DataPipes2GCWorker::DataPipes2GCWorker(ObjectPipesRegistrations& objectPipesRegistrations) :
    m_running(false),
    m_objectPipesRegistrations(objectPipesRegistrations)
{}

DataPipes2GCWorker::~DataPipes2GCWorker()
{}

void DataPipes2GCWorker::startWork()
{
    connect(&m_gcTimer, SIGNAL(timeout()), this, SLOT(processGC()));
    m_gcTimer.start(10000); // collect garbage every 10s
    m_running = true;
}

void DataPipes2GCWorker::stopWork()
{
    m_running = false;
    m_gcTimer.stop();
    disconnect(&m_gcTimer, SIGNAL(timeout()), this, SLOT(processGC()));
}

void DataPipes2GCWorker::processGC()
{
    m_objectPipesRegistrations.processGC();
}
