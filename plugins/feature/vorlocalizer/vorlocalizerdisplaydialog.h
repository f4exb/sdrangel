///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef INCLUDE_VORLOCALIZERDISPLAYDIALOG_H
#define INCLUDE_VORLOCALIZERDISPLAYDIALOG_H

#include "ui_vorlocalizerdisplaydialog.h"
#include "vorlocalizersettings.h"

class VORLocalizerDisplayDialog : public QDialog {
    Q_OBJECT

public:
    explicit VORLocalizerDisplayDialog(VORLocalizerSettings *settings, QWidget* parent = nullptr);
    ~VORLocalizerDisplayDialog();

    const QStringList& getSettingsKeys() const { return m_settingsKeys; };

private slots:
    void accept();

private:
    Ui::VORLocalizerDisplayDialog* ui;
    VORLocalizerSettings *m_settings;
    QStringList m_settingsKeys;
};

#endif // INCLUDE_VORLOCALIZERDISPLAYDIALOG_H
