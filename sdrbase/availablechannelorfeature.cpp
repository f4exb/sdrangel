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
#include "feature/feature.h"
#include "channel/channelapi.h"
#include "maincore.h"

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
