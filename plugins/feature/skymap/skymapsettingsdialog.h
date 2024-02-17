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

#ifndef INCLUDE_FEATURE_SKYMAPSETTINGSDIALOG_H
#define INCLUDE_FEATURE_SKYMAPSETTINGSDIALOG_H

#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QMessageBox>

#include "ui_skymapsettingsdialog.h"
#include "skymapsettings.h"

class SkyMapSettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SkyMapSettingsDialog(SkyMapSettings *settings, QWidget* parent = 0);
    ~SkyMapSettingsDialog();

public:
    QStringList m_settingsKeysChanged; // List of setting keys that have been changed

private:
    SkyMapSettings *m_settings;

private slots:
    void accept();
    void on_constellationBoundaries_toggled(bool checked);
    void on_ecliptic_toggled(bool checked);
    void on_eclipticGrid_toggled(bool checked);
    void on_altAzGrid_toggled(bool checked);
    void on_equatorialGrid_toggled(bool checked);
    void on_galacticGrid_toggled(bool checked);
    void on_useMyPosition_toggled(bool checked);

private:
    Ui::SkyMapSettingsDialog* ui;

};

#endif // INCLUDE_FEATURE_SKYMAPSETTINGSDIALOG_H
