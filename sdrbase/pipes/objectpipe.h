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

#ifndef SDRBASE_PIPES_OBJECTPIPE_H_
#define SDRBASE_PIPES_OBJECTPIPE_H_

#include <QObject>

#include "export.h"

class SDRBASE_API ObjectPipe : public QObject
{
    Q_OBJECT
public:
    ObjectPipe();

    void setToBeDeleted(int reason, QObject *object);
    void unsetToBeDeleted();
    int getGCCount() const;
    int decreaseGCCount();

    unsigned int m_pipeId;
    int m_typeId;
    const QObject *m_producer;
    const QObject *m_consumer;
    QObject *m_element;

signals:
    void toBeDeleted(int reason, QObject *object);

private:
    int m_gcCount;
};


#endif // SDRBASE_PIPES_OBJECTPIPE_H_
