///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_FEATURE_MAPLOCATIONDIALOG_H
#define INCLUDE_FEATURE_MAPLOCATIONDIALOG_H

#include "ui_maplocationdialog.h"

#include <QList>
#include <QGeoLocation>

class MapLocationDialog : public QDialog {
    Q_OBJECT

public:
    explicit MapLocationDialog(const QList<QGeoLocation>& locations, QWidget* parent = 0);
    ~MapLocationDialog();

    const QList<QGeoLocation> *m_locations;
    QGeoLocation m_selectedLocation;

private slots:
    void accept();

private:
    Ui::MapLocationDialog* ui;
};

#endif // INCLUDE_FEATURE_MAPLOCATIONDIALOG_H
