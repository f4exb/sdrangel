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

#include "objectpipesregistrations.h"

ObjectPipesRegistrations::ObjectPipesRegistrations(ObjectPipeElementsStore *objectPipeElementsStore) :
    m_typeCount(0),
    m_pipeId(0),
    m_objectPipeElementsStore(objectPipeElementsStore),
    m_mutex(QMutex::Recursive)
{}

ObjectPipesRegistrations::~ObjectPipesRegistrations()
{}

ObjectPipe *ObjectPipesRegistrations::registerProducerToConsumer(const QObject *producer, const QObject *consumer, const QString& type)
{
    int typeId;
    QMutexLocker mlock(&m_mutex);

    if (m_typeIds.contains(type))
    {
        typeId = m_typeIds.value(type);
    }
    else
    {
        typeId = m_typeCount++;
        m_typeIds.insert(type, typeId);
        m_types.insert(typeId, type);
    }

    for (auto& pipe : m_pipes) // check if pipe exists already - there is a unique pipe per producer, consumer and type
    {
        if ((producer == pipe->m_producer) && (consumer == pipe->m_consumer) && (typeId == pipe->m_typeId))
        {
            qDebug("ObjectPipesRegistrations::registerProducerToConsumer: return existing pipe %p %p %s %s",
                producer, consumer, qPrintable(pipe->m_element->objectName()), qPrintable(type));
            pipe->unsetToBeDeleted();
            if (!m_producerPipes[producer].contains(pipe)) {
                m_producerPipes[producer].push_back(pipe);
            }
            if (!m_consumerPipes[consumer].contains(pipe)) {
                m_consumerPipes[consumer].push_back(pipe);
            }
            if (!m_typeIdPipes[typeId].contains(pipe)) {
                m_typeIdPipes[typeId].push_back(pipe);
            }
            if (!m_producerAndTypeIdPipes[std::make_tuple(producer, typeId)].contains(pipe)) {
                m_producerAndTypeIdPipes[std::make_tuple(producer, typeId)].push_back(pipe);
            }
            if (!m_pipeMap.contains(std::make_tuple(producer, consumer, typeId))) {
                m_pipeMap[std::make_tuple(producer, consumer, typeId)] = pipe;
            }
            return pipe;
        }
    }

    QObject *element = m_objectPipeElementsStore->createElement();
    qDebug("ObjectPipesRegistrations::registerProducerToConsumer: new pipe %p %p %s %s",
        producer, consumer, qPrintable(element->objectName()), qPrintable(type));
    m_pipes.push_back(new ObjectPipe());
    m_pipes.back()->m_pipeId = ++m_pipeId;
    m_pipes.back()->m_typeId = typeId;
    m_pipes.back()->m_producer = producer;
    m_pipes.back()->m_consumer = consumer;
    m_pipes.back()->m_element = element;

    m_producerPipes[producer].push_back(m_pipes.back());
    m_consumerPipes[consumer].push_back(m_pipes.back());
    m_typeIdPipes[typeId].push_back(m_pipes.back());
    m_producerAndTypeIdPipes[std::make_tuple(producer, typeId)].push_back(m_pipes.back());
    m_pipeMap[std::make_tuple(producer, consumer, typeId)] = m_pipes.back();

    connect(producer, SIGNAL(destroyed(QObject*)), this, SLOT(removeProducer(QObject*)));
    connect(consumer, SIGNAL(destroyed(QObject*)), this, SLOT(removeConsumer(QObject*)));

    return m_pipes.back();
}

ObjectPipe *ObjectPipesRegistrations::unregisterProducerToConsumer(const QObject *producer, const QObject *consumer, const QString& type)
{
    ObjectPipe *pipe = nullptr;

    if (m_typeIds.contains(type))
    {
        int typeId = m_typeIds.value(type);

        if (m_pipeMap.contains(std::make_tuple(producer, consumer, typeId)))
        {
            pipe = m_pipeMap[std::make_tuple(producer, consumer, typeId)];
            m_producerPipes[producer].removeAll(pipe);
            m_consumerPipes[consumer].removeAll(pipe);
            m_typeIdPipes[typeId].removeAll(pipe);
            m_producerAndTypeIdPipes[std::make_tuple(producer, typeId)].removeAll(pipe);

            if (m_producerPipes[producer].size() == 0) {
                m_producerPipes.remove(producer);
            }

            if (m_consumerPipes[consumer].size() == 0) {
                m_consumerPipes.remove(consumer);
            }

            if (m_typeIdPipes[typeId].size() == 0) {
                m_typeIdPipes.remove(typeId);
            }

            if (m_producerAndTypeIdPipes[std::make_tuple(producer, typeId)].size() == 0) {
                m_producerAndTypeIdPipes.remove(std::make_tuple(producer, typeId));
            }

            pipe->setToBeDeleted(PipeDeletionReason::PipeDeleted, pipe);
        }
    }

    if (pipe)
    {
        qDebug("ObjectPipesRegistrations::unregisterProducerToConsumer: %p %p %s %s",
            producer, consumer, qPrintable(pipe->m_element->objectName()), qPrintable(type));
    }
    else
    {
        qDebug("ObjectPipesRegistrations::unregisterProducerToConsumer: %p %p %s nullptr",
            producer, consumer, qPrintable(type));
    }

    return pipe;
}

void ObjectPipesRegistrations::getPipes(const QObject *producer, const QString& type, QList<ObjectPipe*>& pipes)
{
    QMutexLocker mlock(&m_mutex);

    if (m_typeIds.contains(type))
    {
        if (m_producerAndTypeIdPipes.contains(std::make_tuple(producer, m_typeIds[type]))) {
            pipes = m_producerAndTypeIdPipes[std::make_tuple(producer, m_typeIds[type])];
        }
    }
}

void ObjectPipesRegistrations::processGC()
{
    QMutexLocker mlock(&m_mutex);

    typename QList<ObjectPipe*>::iterator itPipe = m_pipes.begin();

    while (itPipe != m_pipes.end())
    {
        if ((*itPipe)->getGCCount() > 0) // scheduled for deletion
        {
            if ((*itPipe)->decreaseGCCount() == 0) // delete on this pass
            {
                m_objectPipeElementsStore->deleteElement((*itPipe)->m_element);
                delete *itPipe;
                itPipe = m_pipes.erase(itPipe);
            }
            else
            {
                ++itPipe;
            }
        }
        else
        {
            ++itPipe;
        }
    }
}

void ObjectPipesRegistrations::removeProducer(QObject *producer)
{
    qDebug("ObjectPipesRegistrations::removeProducer: %p %s", producer, qPrintable(producer->objectName()));
    QMutexLocker mlock(&m_mutex);

    if (m_producerPipes.contains(producer) && (m_producerPipes[producer].size() != 0))
    {
        const QList<ObjectPipe*>& pipeList = m_producerPipes[producer];

        for (auto& pipe : pipeList)
        {
            for (const auto& consumer : m_consumerPipes.keys()) {
                m_consumerPipes[consumer].removeAll(pipe);
            }

            for (const auto& typeId : m_typeIdPipes.keys()) {
                m_typeIdPipes[typeId].removeAll(pipe);
            }

            pipe->setToBeDeleted(PipeDeletionReason::PipeProducerDeleted, producer);
        }

        m_producerPipes.remove(producer);
    }

    typename QMap<std::tuple<const QObject*, const QObject*, int>, ObjectPipe*>::iterator itP = m_pipeMap.begin();

    while (itP != m_pipeMap.end())
    {
        if (std::get<0>(itP.key())) {
            itP = m_pipeMap.erase(itP);
        } else {
            ++itP;
        }
    }

    typename QMap<std::tuple<const QObject*, int>, QList<ObjectPipe*>>::iterator itPT = m_producerAndTypeIdPipes.begin();

    while (itPT != m_producerAndTypeIdPipes.end())
    {
        if (std::get<0>(itPT.key()) == producer) {
            itPT = m_producerAndTypeIdPipes.erase(itPT);
        } else {
            ++itPT;
        }
    }
}

void ObjectPipesRegistrations::removeConsumer(QObject *consumer)
{
    qDebug("ObjectPipesRegistrations::removeConsumer: %p %s", consumer, qPrintable(consumer->objectName()));
    QMutexLocker mlock(&m_mutex);

    if (m_consumerPipes.contains(consumer) && (m_consumerPipes[consumer].size() != 0))
    {
        QList<ObjectPipe*>& pipeList = m_consumerPipes[consumer];

        for (auto& pipe : pipeList)
        {
            for (const auto& producer : m_producerPipes.keys()) {
                m_producerPipes[producer].removeAll(pipe);
            }

            for (const auto& typeId : m_typeIdPipes.keys()) {
                m_typeIdPipes[typeId].removeAll(pipe);
            }

            for (const auto& producerAndTypeId : m_producerAndTypeIdPipes.keys()) {
                m_producerAndTypeIdPipes[producerAndTypeId].removeAll(pipe);
            }

            pipe->setToBeDeleted(PipeDeletionReason::PipeConsumerDeleted, consumer);
        }

        m_consumerPipes.remove(consumer);
    }

    typename QMap<std::tuple<const QObject*, const QObject*, int>, ObjectPipe*>::iterator it = m_pipeMap.begin();

    while (it != m_pipeMap.end())
    {
        if (std::get<1>(it.key()) == consumer) {
            it = m_pipeMap.erase(it);
        } else {
            ++it;
        }
    }
}
