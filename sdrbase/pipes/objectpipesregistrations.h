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

#ifndef SDRBASE_PIPES_OBJECTPIPESREGISTRATION_H_
#define SDRBASE_PIPES_OBJECTPIPESREGISTRATION_H_

#include <QObject>
#include <QHash>
#include <QMap>
#include <QPair>
#include <QSet>
#include <QString>
#include <QMutex>

#include <tuple>

#include "export.h"
#include "objectpipe.h"
#include "objectpipeelementsstore.h"

class SDRBASE_API ObjectPipesRegistrations : public QObject
{
    Q_OBJECT
public:
    enum PipeDeletionReason
    {
        PipeProducerDeleted,
        PipeConsumerDeleted,
        PipeDeleted
    };

    ObjectPipesRegistrations(ObjectPipeElementsStore *objectPipeElementsStore);
    ~ObjectPipesRegistrations();

    ObjectPipe *registerProducerToConsumer(const QObject *producer, const QObject *consumer, const QString& type);
    ObjectPipe *unregisterProducerToConsumer(const QObject *producer, const QObject *consumer, const QString& type);
    void getPipes(const QObject *producer, const QString& type, QList<ObjectPipe*>& pipes);
    void processGC();

private slots:
    void removeProducer(QObject *producer);
    void removeConsumer(QObject *consumer);

private:
    QHash<QString, int> m_typeIds;
    QMap<int, QString> m_types;
    int m_typeCount;
    unsigned int m_pipeId;
    ObjectPipeElementsStore *m_objectPipeElementsStore;
    QList<ObjectPipe*> m_pipes;

    QMap<const QObject*, QList<ObjectPipe*>> m_producerPipes;
    QMap<const QObject*, QList<ObjectPipe*>> m_consumerPipes;
    QMap<int, QList<ObjectPipe*>> m_typeIdPipes;
    QMap<std::tuple<const QObject*, int>, QList<ObjectPipe*>> m_producerAndTypeIdPipes;
    QMap<std::tuple<const QObject*, const QObject*, int>, ObjectPipe*> m_pipeMap;

    QMutex m_mutex;
};

#endif // SDRBASE_PIPES_OBJECTPIPESREGISTRATION_H_
