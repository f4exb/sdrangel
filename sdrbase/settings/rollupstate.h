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

#ifndef INCLUDE_ROLLUPSTATE_H
#define INCLUDE_ROLLUPSTATE_H

#include <QByteArray>
#include <QString>
#include <QList>

#include "export.h"
#include "serializable.h"

class SDRBASE_API RollupState : public Serializable
{
public:
    enum {
        VersionMarker = 0xff
    };
    struct RollupChildState
    {
        QString m_objectName;
        bool m_isHidden;
    };

    explicit RollupState();
    virtual ~RollupState() {}

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);
    virtual void formatTo(SWGSDRangel::SWGObject *swgObject) const;
    virtual void updateFrom(const QStringList& keys, const SWGSDRangel::SWGObject *swgObject);

    void setVersion(int version) { m_version = version; }
    QList<RollupChildState>& getChildren() { return m_childrenStates; }
    const QList<RollupChildState>& getChildren() const { return m_childrenStates; }

private:
    QList<RollupChildState> m_childrenStates;
    int m_version;
};

#endif // INCLUDE_ROLLUPSTATE_H
