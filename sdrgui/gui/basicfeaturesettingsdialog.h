///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_SDRGUI_BASICFEATURESETTINGSDIALOG_H
#define INCLUDE_SDRGUI_BASICFEATURESETTINGSDIALOG_H

#include <QDialog>

#include "../../exports/export.h"

namespace Ui {
    class BasicFeatureSettingsDialog;
}

class SDRGUI_API BasicFeatureSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BasicFeatureSettingsDialog(QWidget *parent = nullptr);
    ~BasicFeatureSettingsDialog();
    void setTitle(const QString& title);
    void setColor(const QColor& color);
    const QString& getTitle() const { return m_title; }
    const QColor& getColor() const { return m_color; }
    bool hasChanged() const { return m_hasChanged; }

private slots:
    void on_colorBtn_clicked();
    void on_title_editingFinished();
    void accept();

private:
    Ui::BasicFeatureSettingsDialog *ui;
    QColor m_color;
    QString m_title;
    bool m_hasChanged;

    void paintColor();
};

#endif // INCLUDE_SDRGUI_BASICFEATURESETTINGSDIALOG_H
