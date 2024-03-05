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

#ifndef SDRBASE_AVAILABLECHANNELORFEATURE_H_
#define SDRBASE_AVAILABLECHANNELORFEATURE_H_

#include <QString>
#include <QObject>

//#include "pipes/messagepipes.h"
#include "export.h"

struct SDRBASE_API AvailableChannelOrFeature
{
    QChar m_kind;           //!< 'R' or 'T' for channel, 'M' for MIMO channel, 'F' for feature as from MainCore::getDeviceSetTypeId
    int m_superIndex;       //!< Device Set index or Feature Set index
    int m_index;            //!< Channel or Feature index
    int m_streamIndex;      //!< For MIMO channels only
    QString m_type;         //!< Plugin type (E.g. NFMDemod)
    QObject *m_object;      //!< Pointer to the object (ChannelAPI or Feature object)

    AvailableChannelOrFeature() = default;
    AvailableChannelOrFeature(const AvailableChannelOrFeature&) = default;
    AvailableChannelOrFeature& operator=(const AvailableChannelOrFeature&) = default;

    bool operator==(const AvailableChannelOrFeature& a) const;
    QString getId() const; //!< Eg: "R3:4"
    QString getLongId() const; //!< Eg: "F:1 StarTracker"
};

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
inline uint qHash(const AvailableChannelOrFeature &c, uint seed = 0) noexcept
{
    return qHash(c.getLongId(), seed);
}
#else
inline size_t qHash(const AvailableChannelOrFeature &c, size_t seed = 0) noexcept
{
    return qHash(c.getLongId(), seed);
}
#endif

class SDRBASE_API AvailableChannelOrFeatureList : public QList<AvailableChannelOrFeature>
{
public:
    AvailableChannelOrFeatureList() {}
    inline explicit AvailableChannelOrFeatureList(const AvailableChannelOrFeature &i) { append(i); }

    int indexOfObject(const QObject *object, int from=0) const;     //!< // Find index of entry containing specified object. -1 if not found.
    int indexOfId(const QString& longId, int from=0) const;         //!< // Find index of entry with specified Id. -1 if not found.
    int indexOfLongId(const QString& longId, int from=0) const;     //!< // Find index of entry with specified long Id. -1 if not found.
};

#endif // SDRBASE_AVAILABLECHANNELORFEATURE_H_
