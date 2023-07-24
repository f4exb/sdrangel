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
#include "datafifostore.h"

DataFifoStore::DataFifoStore()
{}

DataFifoStore::~DataFifoStore()
{
    deleteAllElements();
}

QObject *DataFifoStore::createElement()
{
    DataFifo *fifo = new DataFifo();
    m_dataFifos.push_back(fifo);
    qDebug("DataFifoStore::createElement: %d added", m_dataFifos.size() - 1);
    return fifo;
}

void DataFifoStore::deleteElement(QObject *element)
{
    int i = m_dataFifos.indexOf((DataFifo*) element);

    if (i >= 0)
    {
        qDebug("DataFifoStore::deleteElement: delete element at %d", i);
        delete m_dataFifos[i];
        m_dataFifos.removeAt(i);
    }
}

void DataFifoStore::deleteAllElements()
{
    for (auto& fifo : m_dataFifos) {
        delete fifo;
    }

    m_dataFifos.clear();
}
