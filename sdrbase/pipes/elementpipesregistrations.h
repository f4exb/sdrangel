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

#ifndef SDRBASE_PIPES_ELEMNTPIPESREGISTRATION_H_
#define SDRBASE_PIPES_ELEMNTPIPESREGISTRATION_H_

#include <QMap>
#include <QHash>
#include <QList>
#include <QMutex>

#include "elementpipescommon.h"

template<typename Producer, typename Consumer, typename Element>
class ElementPipesRegistrations
{
public:
    ElementPipesRegistrations() :
        m_typeCount(0),
        m_mutex(QMutex::Recursive)
    {}

    ~ElementPipesRegistrations()
    {
        typename QMap<ElementPipesCommon::RegistrationKey<Producer>, QList<Element*>>::iterator mit = m_elements.begin();

        for (; mit != m_elements.end(); ++mit)
        {
            typename QList<Element*>::iterator elIt = mit->begin();

            for (; elIt != mit->end(); ++elIt) {
                delete *elIt;
            }
        }
    }

    Element *registerProducerToConsumer(const Producer *producer, Consumer *consumer, const QString& type)
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
        }

        const typename ElementPipesCommon::RegistrationKey<Producer> regKey
            = ElementPipesCommon::RegistrationKey<Producer>{producer, typeId};
        Element *element;

        if (m_consumers[regKey].contains(consumer))
        {
            int i = m_consumers[regKey].indexOf(consumer);
            element = m_elements[regKey][i];
        }
        else
        {
            element = new Element();
            m_elements[regKey].append(element);
            m_consumers[regKey].append(consumer);
        }

        return element;
    }

    Element *unregisterProducerToConsumer(const Producer *producer, Consumer *consumer, const QString& type)
    {
        Element *element = nullptr;

        if (m_typeIds.contains(type))
        {
            int typeId = m_typeIds.value(type);
            const typename ElementPipesCommon::RegistrationKey<Producer> regKey
                = ElementPipesCommon::RegistrationKey<Producer>{producer, typeId};

            if (m_consumers.contains(regKey) && m_consumers[regKey].contains(consumer))
            {
                QMutexLocker mlock(&m_mutex);
                int i = m_consumers[regKey].indexOf(consumer);
                m_consumers[regKey].removeAt(i);
                element = m_elements[regKey][i];
                // delete element; // will be deferred to GC
                m_elements[regKey].removeAt(i);
            }
        }

        return element;
    }

    QList<Element*>* getElements(const Producer *producer, const QString& type)
    {
        if (!m_typeIds.contains(type)) {
            return nullptr;
        }

        QMutexLocker mlock(&m_mutex);
        const typename ElementPipesCommon::RegistrationKey<Producer> regKey
            = ElementPipesCommon::RegistrationKey<Producer>{producer, m_typeIds.value(type)};

        if (m_elements.contains(regKey)) {
            return &m_elements[regKey];
        } else {
            return nullptr;
        }
    }

    QMap<ElementPipesCommon::RegistrationKey<Producer>, QList<Element*>> *getElements() { return &m_elements; }
    QMap<ElementPipesCommon::RegistrationKey<Producer>, QList<Consumer*>>  *getConsumers() { return &m_consumers; }
    QMutex *getMutex() { return &m_mutex; }


private:
    QHash<QString, int> m_typeIds;
    int m_typeCount;
    QMap<ElementPipesCommon::RegistrationKey<Producer>, QList<Element*>> m_elements;
    QMap<ElementPipesCommon::RegistrationKey<Producer>, QList<Consumer*>> m_consumers;
    QMutex m_mutex;
};

#endif // SDRBASE_PIPES_ELEMNTPIPESREGISTRATION_H_
