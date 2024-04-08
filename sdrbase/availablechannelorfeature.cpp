///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Jon Beniston <jon@beniston.com>                            //
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

#include "availablechannelorfeature.h"

bool AvailableChannelOrFeature::operator==(const AvailableChannelOrFeature& a) const
{
    return (m_kind == a.m_kind) && (m_superIndex == a.m_superIndex) && (m_index == a.m_index) && (m_type == a.m_type) && ((m_kind == 'M') && (m_streamIndex == a.m_streamIndex));
}

QString AvailableChannelOrFeature::getId() const
{
    QString id;

    if (m_kind == 'F') {
        id = QString("%1:%2").arg(m_kind).arg(m_index); // superIndex always 0 for features
    } else {
        id = QString("%1%2:%3").arg(m_kind).arg(m_superIndex).arg(m_index);
    }
    if (m_kind == 'M') {
        id.append(QString(".%1").arg(m_streamIndex));
    }
    return id;
}

QString AvailableChannelOrFeature::getLongId() const
{
    return QString("%1 %2").arg(getId()).arg(m_type);
}

int AvailableChannelOrFeatureList::indexOfObject(const QObject *object, int from) const
{
    for (int index = from; index < size(); index++)
    {
        if (at(index).m_object == object) {
            return index;
        }
    }
    return -1;
}

int AvailableChannelOrFeatureList::indexOfId(const QString& id, int from) const
{
    for (int index = from; index < size(); index++)
    {
        if (at(index).getId() == id) {
            return index;
        }
    }
    return -1;
}

int AvailableChannelOrFeatureList::indexOfLongId(const QString& longId, int from) const
{
    for (int index = from; index < size(); index++)
    {
        if (at(index).getLongId() == longId) {
            return index;
        }
    }
    return -1;
}
