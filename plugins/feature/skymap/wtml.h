///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef INCLUDE_WTML_H
#define INCLUDE_WTML_H

#include <QtCore>

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkDiskCache;

// World Wide Telescope WTML files containing imageset catalogs
class WTML : public QObject
{
    Q_OBJECT

public:

    struct ImageSet {
        QString m_name;
        QString m_dataSetType;
    };

   WTML();
   ~WTML();

    void getData();

public slots:
    void handleReply(QNetworkReply* reply);

signals:
    void dataUpdated(const QList<ImageSet>& dataSets);  // Emitted when new data are available.

private:
    QNetworkAccessManager *m_networkManager;
    QNetworkDiskCache *m_cache;

};

#endif /* INCLUDE_WTML_H */
