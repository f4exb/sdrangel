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

#ifndef INCLUDE_NASAGLOBALIMAGERY_H
#define INCLUDE_NASAGLOBALIMAGERY_H

#include <QtCore>
#include <QTimer>

#include "export.h"

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkDiskCache;

// NASA GIBS (Global Imagery Browse Server) API (https://nasa-gibs.github.io/gibs-api-docs/)
// Gets details of available data sets for use on maps
// Also supports cached download of .svg legends
class SDRBASE_API NASAGlobalImagery : public QObject
{
    Q_OBJECT

public:

    struct Legend {
        QString m_url;  // Typically to .svg file
        int m_width;
        int m_height;
    };

    struct DataSet {
        QString m_identifier;
        QList<Legend> m_legends;
        QString m_tileMatrixSet;
        QString m_format;
        QString m_defaultDateTime;
        QStringList m_dates;
    };

    struct Layer {
        QString m_identifier;
        QString m_title;
        QString m_subtitle;
        QString m_descriptionURL;
        QDateTime m_startDate;
        QDateTime m_endDate;
        bool m_ongoing;
        QString m_layerPeriod;
        QString m_layerGroup;
    };

    struct MetaData {
        QHash<QString, Layer> m_layers;
    };

    NASAGlobalImagery();
    ~NASAGlobalImagery();

    void getData();
    void getMetaData();
    void downloadLegend(const Legend& legend);
    void downloadHTML(const QString& url);

public slots:
    void handleReply(QNetworkReply* reply);

signals:
    void dataUpdated(const QList<DataSet>& dataSets);  // Emitted when paths to new data are available.
    void metaDataUpdated(const MetaData& metaData);
    void legendAvailable(const QString& url, const QByteArray data);
    void htmlAvailable(const QString& url, const QByteArray data);

private:
    QNetworkAccessManager *m_networkManager;
    QNetworkDiskCache *m_cache;

    void handleXML(const QByteArray& bytes);
    void handleSVG(const QString& url, const QByteArray& bytes);
    void handleJSON(const QByteArray& bytes);
    void handleHTML(const QString& url, const QByteArray& bytes);

};

#endif /* INCLUDE_NASAGLOBALIMAGERY_H */
