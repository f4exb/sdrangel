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

#ifndef SDRBASE_PIPES_ELEMNTPIPESGCWORKER_H_
#define SDRBASE_PIPES_ELEMNTPIPESGCWORKER_H_

#include <QMap>
#include <QList>
#include <QMutex>

#include "elementpipescommon.h"

template<typename Producer, typename Consumer, typename Element>
class ElementPipesGC
{
public:
    ElementPipesGC() :
        m_mutex(nullptr),
        m_elements(nullptr),
        m_consumers(nullptr)
    {}

    ~ElementPipesGC()
    {}

    void setRegistrations(
        QMutex *mutex,
        QMap<ElementPipesCommon::RegistrationKey<Producer>, QList<Element*>> *elements,
        QMap<ElementPipesCommon::RegistrationKey<Producer>, QList<Consumer*>> *consumers
    )
    {
        m_mutex = mutex;
        m_elements = elements;
        m_consumers = consumers;
    }

    void addElementToDelete(Element *element)
    {
        m_elementsToDelete.append(element);
    }

    void processGC()
    {
        if (m_mutex)
        {
            QMutexLocker mlock(m_mutex);
            typename QMap<ElementPipesCommon::RegistrationKey<Producer>, QList<Consumer*>>::iterator cIt = m_consumers->begin();

            // remove fifos to be deleted from last run
            for (typename QList<Element*>::iterator elIt = m_elementsToDelete.begin(); elIt != m_elementsToDelete.end(); ++elIt) {
                delete *elIt;
            }

            m_elementsToDelete.clear();

            // remove keys with empty features
            while (cIt != m_consumers->end())
            {
                if (cIt.value().size() == 0) {
                    cIt = m_consumers->erase(cIt);
                } else {
                    ++cIt;
                }
            }

            // remove keys with empty fifos
            typename QMap<ElementPipesCommon::RegistrationKey<Producer>, QList<Element*>>::iterator elIt = m_elements->begin();

            while (elIt != m_elements->end())
            {
                if (elIt.value().size() == 0) {
                    elIt = m_elements->erase(elIt);
                } else {
                    ++elIt;
                }
            }

            // check deleted channels and features
            cIt = m_consumers->begin();

            for (;cIt != m_consumers->end(); ++cIt)
            {
                ElementPipesCommon::RegistrationKey<Producer> producerKey = cIt.key();
                const Producer *producer = producerKey.m_key;

                if (existsProducer(producer)) // look for deleted features
                {
                    QList<Consumer*>& consumers = cIt.value();
                    int i = 0;

                    while (i < consumers.size())
                    {
                        if (existsConsumer(consumers[i])) {
                            i++;
                        }
                        else
                        {
                            consumers.removeAt(i);
                            Element *element = m_elements->operator[](producerKey)[i];
                            m_elementsToDelete.append(element);
                            m_elements->operator[](producerKey).removeAt(i);
                        }
                    }
                }
                else // channel was destroyed
                {
                    QList<Consumer*>& consumers = cIt.value();

                    for (int i = 0; i < consumers.size(); i++) {
                        if (existsConsumer(consumers[i]))
                            sendMessageToConsumer(m_elements->operator[](producerKey)[i], producerKey, consumers[i]);
                    }
                }
            }
        }
    }

protected:
    virtual bool existsProducer(const Producer *producer) = 0;
    virtual bool existsConsumer(const Consumer *consumer) = 0;
    virtual void sendMessageToConsumer(const Element *element, ElementPipesCommon::RegistrationKey<Producer> producerKey, Consumer *consumer) = 0;

private:
    QMutex *m_mutex;
    QMap<ElementPipesCommon::RegistrationKey<Producer>, QList<Element*>> *m_elements;
    QMap<ElementPipesCommon::RegistrationKey<Producer>, QList<Consumer*>> *m_consumers;
    QList<Element*> m_elementsToDelete;
};


#endif // SDRBASE_PIPES_ELEMNTPIPESGCWORKER_H_
