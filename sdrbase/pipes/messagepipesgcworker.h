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

#ifndef SDRBASE_PIPES_MESSAGEPIPESGCWORKER_H_
#define SDRBASE_PIPES_MESSAGEPIPESGCWORKER_H_

#include <QObject>
#include <QTimer>

#include "export.h"
#include "objectpipesregistrations.h"

class SDRBASE_API MessagePipesGCWorker : public QObject
{
    Q_OBJECT
public:
    MessagePipesGCWorker(ObjectPipesRegistrations& objectPipesRegistrations);
    ~MessagePipesGCWorker();

    void startWork();
    void stopWork();
    bool isRunning() const { return m_running; }

private:
    bool m_running;
    QTimer m_gcTimer;
    ObjectPipesRegistrations& m_objectPipesRegistrations;

private slots:
    void processGC(); //!< Collect garbage
};

#endif // SDRBASE_PIPES_MESSAGEPIPESGCWORKER_H_
