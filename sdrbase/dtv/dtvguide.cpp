///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#include "dtvguide.h"

QString DTVGuide::Service::channelText() const
{
    if (m_channel < 0) {
        return QString();
    }

    return m_minorChannel >= 0 ? QString("%1.%2").arg(m_channel).arg(m_minorChannel) : QString::number(m_channel);
}

bool DTVGuide::Service::channelBefore(const Service& other) const
{
    // Unsigned, so -1 (none) sorts last
    if (m_channel != other.m_channel) {
        return (unsigned) m_channel < (unsigned) other.m_channel;
    }

    return (unsigned) m_minorChannel < (unsigned) other.m_minorChannel;
}

QString DTVGuide::genreName(int genre, int networkId) const
{
    (void) networkId;
    return genreText(genre);
}

// Content level 1: EN 300 468 table 29
QString DTVGuide::genreText(int genre)
{
    static const char *const names[16] = {
        "", "Film/Drama", "News/Current affairs", "Entertainment", "Sport", "Children's", "Music", "Arts/Culture",
        "Social/Political", "Education/Factual", "Leisure/Hobbies", "", "", "", "", ""
    };

    if (genre < 0) {
        return QString();
    }

    return QString(names[(genre >> 4) & 0x0F]);
}
