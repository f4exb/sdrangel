///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#ifndef UTIL_MMSI_H
#define UTIL_MMSI_H

#include <QMap>
#include <QIcon>

#include "export.h"

// Maritime mobile service identities (basically ship identifiers)
// MMSIs defined by ITU-R M.585
// https://www.itu.int/dms_pubrec/itu-r/rec/m/R-REC-M.585-9-202205-I!!PDF-E.pdf

class SDRBASE_API MMSI {

public:

    static QString getMID(const QString &mmsi);
    static QString getCountry(const QString &mmsi);
    static QString getCategory(const QString &mmsi);
    static QIcon *getFlagIcon(const QString &mmsi);
    static QString getFlagIconURL(const QString &mmsi);

private:

    static QMap<int, QString> m_mid;

    static void checkFlags();
};

#endif /* UTIL_MMSI_H */
