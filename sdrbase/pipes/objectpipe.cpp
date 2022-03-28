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

#include "objectpipe.h"

ObjectPipe::ObjectPipe() :
    m_pipeId(0),
    m_typeId(0),
    m_producer(nullptr),
    m_consumer(nullptr),
    m_element(nullptr),
    m_gcCount(0)
{}

void ObjectPipe::setToBeDeleted(int reason, QObject *object)
{
    qDebug("ObjectPipe::setToBeDeleted: %d (%p)", reason, object);
    m_gcCount = 2; // will defer actual deletion by one GC pass
    emit toBeDeleted(reason, object);
}

void ObjectPipe::unsetToBeDeleted()
{
    m_gcCount = 0;
}

int ObjectPipe::getGCCount() const {
    return m_gcCount;
}

int ObjectPipe::decreaseGCCount()
{
    if (m_gcCount > 0) {
        return m_gcCount--;
    } else {
        return m_gcCount;
    }
}

