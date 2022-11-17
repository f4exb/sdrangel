///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include <QDataStream>
#include <QIODevice>

#include "SWGRollupState.h"

#include "rollupstate.h"

RollupState::RollupState() :
    m_version(0)
{ }

QByteArray RollupState::serialize() const
{
    QByteArray state;
    QDataStream stream(&state, QIODevice::WriteOnly);
    stream << VersionMarker;
    stream << m_version;
    stream << m_childrenStates.size();

    for (const auto &child : m_childrenStates)
    {
        stream << child.m_objectName;
        stream << (child.m_isHidden ? (int) 0 : (int) 1);
    }

    return state;
}

bool RollupState::deserialize(const QByteArray& data)
{
    if (data.isEmpty()) {
        return false;
    }

    QByteArray sd = data;
    QDataStream stream(&sd, QIODevice::ReadOnly);
    int marker, v;
    stream >> marker;
    stream >> v;

    if ((stream.status() != QDataStream::Ok) || (marker != VersionMarker) || (v != m_version)) {
        return false;
    }

    int count;
    stream >> count;

    if (stream.status() != QDataStream::Ok) {
        return false;
    }

    m_childrenStates.clear();

    for (int i = 0; i < count; ++i)
    {
        QString name;
        int visible;

        stream >> name;
        stream >> visible;

        m_childrenStates.push_back({name, visible == 0});
    }

    return true;
}

void RollupState::formatTo(SWGSDRangel::SWGObject *swgObject) const
{
    SWGSDRangel::SWGRollupState *swgRollupState = static_cast<SWGSDRangel::SWGRollupState *>(swgObject);
    swgRollupState->setVersion(m_version);
    swgRollupState->setChildrenStates(new QList<SWGSDRangel::SWGRollupChildState *>);

    for (const auto &child : m_childrenStates)
    {
        swgRollupState->getChildrenStates()->append(new SWGSDRangel::SWGRollupChildState);
        swgRollupState->getChildrenStates()->back()->init();
        swgRollupState->getChildrenStates()->back()->setObjectName(new QString(child.m_objectName));
        swgRollupState->getChildrenStates()->back()->setIsHidden(child.m_isHidden ? 1 : 0);
    }
}

void RollupState::updateFrom(const QStringList& keys, const SWGSDRangel::SWGObject *swgObject)
{
    SWGSDRangel::SWGRollupState *swgRollupState =
        static_cast<SWGSDRangel::SWGRollupState *>(const_cast<SWGSDRangel::SWGObject *>(swgObject));

    if (keys.contains("rollupState.version")) {
        m_version = swgRollupState->getVersion();
    }

    if (keys.contains("rollupState.childrenStates"))
    {
        QList<SWGSDRangel::SWGRollupChildState *> *swgChildrenStates = swgRollupState->getChildrenStates();
        m_childrenStates.clear();

        for (const auto swgChildState : *swgChildrenStates) {
            m_childrenStates.append({*swgChildState->getObjectName(), swgChildState->getIsHidden() != 0});
        }
    }
}
