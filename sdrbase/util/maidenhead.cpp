///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
//                                                                               //
// Maidenhead locator utilities                                                  //
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

#include <cmath>

#include <QRegExp>

#include "maidenhead.h"

// See: https://en.wikipedia.org/wiki/Maidenhead_Locator_System

QString Maidenhead::toMaidenhead(float latitude, float longitude)
{
    longitude += 180.0;
    latitude += 90.0;
    int lon1 = floor(longitude/20);
    longitude -= lon1*20;
    int lat1 = floor(latitude/10);
    latitude -= lat1*10;
    int lon2 = floor(longitude/2);
    longitude -= lon2*2;
    int lat2 = floor(latitude/1);
    latitude -= lat2;
    int lon3 = round(longitude*12);
    int lat3 = round(latitude*24);
    return QString("%1%2%3%4%5%6").arg(QChar(lon1+'A')).arg(QChar(lat1+'A'))
                                  .arg(QChar(lon2+'0')).arg(QChar(lat2+'0'))
                                  .arg(QChar(lon3+'A')).arg(QChar(lat3+'A'));
}

bool Maidenhead::fromMaidenhead(const QString& maidenhead, float& latitude, float& longitude)
{
    if (Maidenhead::isMaidenhead(maidenhead))
    {
        int lon1 = maidenhead[0].toUpper().toLatin1() - 'A';
        int lat1 = maidenhead[1].toUpper().toLatin1() - 'A';
        int lon2 = maidenhead[2].toLatin1() - '0';
        int lat2 = maidenhead[3].toLatin1() - '0';
        int lon3 = 0;
        int lat3 = 0;
        int lon4 = 0;
        int lat4 = 0;
        if (maidenhead.length() >= 6)
        {
            lon3 = maidenhead[4].toUpper().toLatin1() - 'A';
            lat3 = maidenhead[5].toUpper().toLatin1() - 'A';
        }
        if (maidenhead.length() == 8)
        {
            lon4 = maidenhead[6].toLatin1() - '0';
            lat4 = maidenhead[7].toLatin1() - '0';
        }
        longitude = lon1 * 20 + lon2 * 2 + lon3 * 2.0/24.0 + lon4 * 2.0/24.0/10; // 20=360/18
        latitude = lat1 * 10 + lat2 * 1 + lat3 * 1.0/24.0 + lat4 * 1.0/24.0/10;  // 10=180/18

        longitude -= 180.0;
        latitude -= 90.0;

        return true;
    }
    else
        return false;
}

bool Maidenhead::isMaidenhead(const QString& maidenhead)
{
    int length = maidenhead.length();
    if ((length != 4) && (length != 6) && (length != 8))
        return false;
    QRegExp re("[A-Ra-r][A-Ra-r][0-9][0-9]([A-Xa-x][A-Xa-x]([0-9][0-9])?)?");
    return re.exactMatch(maidenhead);
}
