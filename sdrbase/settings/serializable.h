///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB.                                  //
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

#ifndef SDRBASE_SETTINGS_SERIALIZABLE_H_
#define SDRBASE_SETTINGS_SERIALIZABLE_H_

namespace SWGSDRangel {
    class SWGObject;
}

#include <QStringList>

class Serializable
{
public:
    virtual ~Serializable() {}
    virtual QByteArray serialize() const = 0; //!< Serialize to inary
    virtual bool deserialize(const QByteArray& data) = 0; //!< Deserialize from binary
    virtual void formatTo(SWGSDRangel::SWGObject *swgObject) const = 0; //!< Serialize to API
    virtual void updateFrom(const QStringList& keys, const SWGSDRangel::SWGObject *swgObject) = 0; //!< Deserialize from API
};

#endif /* SDRBASE_SETTINGS_SERIALIZABLE_H_ */
