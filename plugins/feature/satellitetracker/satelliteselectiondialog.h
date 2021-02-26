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

#ifndef INCLUDE_SATELLITESELECTIONDIALOG_H
#define INCLUDE_SATELLITESELECTIONDIALOG_H

#include <QHash>
#include <QNetworkRequest>

#include "ui_satelliteselectiondialog.h"
#include "satellitetrackersettings.h"
#include "satnogs.h"

class QNetworkAccessManager;
class QNetworkReply;

class SatelliteSelectionDialog : public QDialog {
    Q_OBJECT

public:
    explicit SatelliteSelectionDialog(SatelliteTrackerSettings* settings, const QHash<QString, SatNogsSatellite *>& satellites, QWidget* parent = 0);
    ~SatelliteSelectionDialog();

    SatelliteTrackerSettings *m_settings;

private:
    void displaySatInfo(const QString& name);

private slots:
    void accept();
    void on_find_textChanged(const QString &text);
    void on_addSat_clicked();
    void on_removeSat_clicked();
    void on_moveUp_clicked();
    void on_moveDown_clicked();
    void on_availableSats_itemDoubleClicked(QListWidgetItem *item);
    void on_selectedSats_itemDoubleClicked(QListWidgetItem *item);
    void on_availableSats_itemSelectionChanged();
    void on_selectedSats_itemSelectionChanged();
    void on_openSatelliteWebsite_clicked();
    void on_openSatNogsObservations_clicked();
    void networkManagerFinished(QNetworkReply *reply);

private:
    QNetworkAccessManager *m_networkManager;
    const QHash<QString, SatNogsSatellite *>& m_satellites;
    SatNogsSatellite *m_satInfo;
    Ui::SatelliteSelectionDialog* ui;
};

#endif // INCLUDE_SATELLITESELECTIONDIALOG_H
