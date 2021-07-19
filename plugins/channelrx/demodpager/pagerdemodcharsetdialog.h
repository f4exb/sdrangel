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

#ifndef INCLUDE_PAGERDEMODCHARSETDIALOG_H
#define INCLUDE_PAGERDEMODCHARSETDIALOG_H

#include <QHash>
#include <QNetworkRequest>

#include "ui_pagerdemodcharsetdialog.h"
#include "pagerdemodsettings.h"

class PagerDemodCharsetDialog : public QDialog {
    Q_OBJECT

public:
    explicit PagerDemodCharsetDialog(PagerDemodSettings* settings, QWidget* parent = 0);
    ~PagerDemodCharsetDialog();

    PagerDemodSettings *m_settings;

private slots:
    void accept();
    void on_add_clicked();
    void on_remove_clicked();
    void on_preset_currentIndexChanged(int index);
    void on_table_cellChanged(int row, int column);

private:
    Ui::PagerDemodCharsetDialog* ui;

    enum Columns {
        SEVENBIT_COL,
        UNICODE_COL,
        GLYPH_COL
    };

    void addRow(int sevenBit, int unicode);
};

#endif // INCLUDE_PAGERDEMODCHARSETDIALOG_H
