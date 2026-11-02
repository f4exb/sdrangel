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

#ifndef INCLUDE_AIRCRAFTSETTINGSDIALOG_H
#define INCLUDE_AIRCRAFTSETTINGSDIALOG_H

#include <QDialog>
#include <QStringList>

#include "ui_aircraftsettingsdialog.h"
#include "aircraftsettings.h"

// Settings that are set once and then left alone: how long aircraft are kept, where
// the session is stored, which demodulators to listen to and how aircraft are labelled
class AircraftSettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit AircraftSettingsDialog(AircraftSettings *settings, QWidget *parent = nullptr);
    ~AircraftSettingsDialog();

    // Which settings the user actually changed, for applySettings
    const QStringList& changedSettings() const { return m_changed; }

private slots:
    void accept();
    void on_browse_clicked();

private:
    Ui::AircraftSettingsDialog *ui;
    AircraftSettings *m_settings;
    QStringList m_changed;
};

#endif // INCLUDE_AIRCRAFTSETTINGSDIALOG_H
