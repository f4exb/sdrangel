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

#ifndef INCLUDE_DTVAPPSDIALOG_H
#define INCLUDE_DTVAPPSDIALOG_H

#include <set>

#include <QDialog>
#include <QTimer>

#include "export.h"

class DTVApps;
class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

// The interactive applications (red button) of the service played, from its AIT, and its object carousels: how much
// of each has been received, and its files, which can be saved
class SDRGUI_API DTVAppsDialog : public QDialog
{
    Q_OBJECT

public:
    DTVAppsDialog(DTVApps *apps, QWidget *parent = nullptr);
    //! The service played: 0 if none
    void setService(int serviceId, const QString& name);

private:
    void refresh();
    void refreshApps();
    void refreshCarousels();
    void fillFiles(QTreeWidgetItem *carouselItem);
    void appsMenu(const QPoint& position);
    void saveFiles();
    int selectedCarousel() const;   //!< Its PID, -1 if none's selected

    DTVApps *m_apps;
    int m_serviceId;
    QString m_serviceName;
    int m_revision;
    QLabel *m_service;
    QTreeWidget *m_appsTree;
    QTreeWidget *m_carouselsTree;
    QPushButton *m_save;
    std::set<QString> m_expanded;   //!< Of the carousels' items: by PID, and path
    QTimer m_timer;
};

#endif // INCLUDE_DTVAPPSDIALOG_H
