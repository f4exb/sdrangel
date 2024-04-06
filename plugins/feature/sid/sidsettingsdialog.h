///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023-2024 Jon Beniston, M7RCE                                   //
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

#ifndef INCLUDE_SIDSETTINGSDIALOG_H
#define INCLUDE_SIDSETTINGSDIALOG_H

#include <QFileDialog>

#include "ui_sidsettingsdialog.h"
#include "sidsettings.h"

class TableColorChooser;

class SIDSettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SIDSettingsDialog(SIDSettings *settings, QWidget* parent = 0);
    ~SIDSettingsDialog();

private:
    void addColor(const QString& name, QRgb rgb);

private slots:
    void accept();
    void on_browse_clicked();
    void on_remove_clicked();

private:
    Ui::SIDSettingsDialog* ui;
    SIDSettings *m_settings;
    QList<TableColorChooser *> m_channelColorGUIs;
    QList<TableColorChooser *> m_colorGUIs;
    QFileDialog m_fileDialog;
    QStringList m_removeIds;

    enum ChannelsRows {
        CHANNELS_COL_ID,
        CHANNELS_COL_ENABLED,
        CHANNELS_COL_LABEL,
        CHANNELS_COL_COLOR
    };

    enum ColorsRows {
        COLORS_COL_NAME,
        COLORS_COL_COLOR
    };

signals:
    void removeChannels(const QStringList& ids);

};

#endif // INCLUDE_SIDSETTINGSDIALOG_H
