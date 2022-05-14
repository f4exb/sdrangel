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

#ifndef SDRBASE_PIPES_MESSAGEPIPESLEGACY_H_
#define SDRBASE_PIPES_MESSAGEPIPESLEGACY_H_

#include <QObject>
#include <QHash>
#include <QMap>
#include <QMutex>
#include <QThread>

#include "export.h"

#include "messagepipeslegacycommon.h"
#include "elementpipesregistrations.h"

class PipeEndPoint;
class MessagePipesLegacyGCWorker;
class MessageQueue;

class SDRBASE_API MessagePipesLegacy : public QObject
{
    Q_OBJECT
public:
    MessagePipesLegacy();
    MessagePipesLegacy(const MessagePipesLegacy&) = delete;
    MessagePipesLegacy& operator=(const MessagePipesLegacy&) = delete;
    ~MessagePipesLegacy();

    // FIXME: Names of these functions should probably change, as we now support channel or feature at either end
    MessageQueue *registerChannelToFeature(const PipeEndPoint *source, PipeEndPoint *dest, const QString& type);
    MessageQueue *unregisterChannelToFeature(const PipeEndPoint *source, PipeEndPoint *dest, const QString& type);
    QList<MessageQueue*>* getMessageQueues(const PipeEndPoint *source, const QString& type);

private:
    ElementPipesRegistrations<PipeEndPoint, PipeEndPoint, MessageQueue> m_registrations;
    QThread m_gcThread; //!< Garbage collector thread
    MessagePipesLegacyGCWorker *m_gcWorker; //!< Garbage collector

	void startGC(); //!< Start garbage collector
	void stopGC();  //!< Stop garbage collector
};

#endif // SDRBASE_PIPES_MESSAGEPIPESLEGACY_H_
