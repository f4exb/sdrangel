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
    const QString& getTitle() const { return m_title; }
    bool useReverseAPI() const { return m_useReverseAPI; }
    const QString& getReverseAPIAddress() const { return m_reverseAPIAddress; }
    uint16_t getReverseAPIPort() const { return m_reverseAPIPort; }
    uint16_t getReverseAPIFeatureSetIndex() const { return m_reverseAPIFeatureSetIndex; }
    uint16_t getReverseAPIFeatureIndex() const { return m_reverseAPIFeatureIndex; }
    void setUseReverseAPI(bool useReverseAPI);
    void setReverseAPIAddress(const QString& address);
    void setReverseAPIPort(uint16_t port);
    void setReverseAPIFeatureSetIndex(uint16_t featureSetIndex);
    void setReverseAPIFeatureIndex(uint16_t featureIndex);
    void setDefaultTitle(const QString& title) { m_defaultTitle = title; }
    bool hasChanged() const { return m_hasChanged; }

private slots:
    void on_title_editingFinished();
    void on_titleReset_clicked();
    void on_reverseAPI_toggled(bool checked);
    void on_reverseAPIAddress_editingFinished();
    void on_reverseAPIPort_editingFinished();
    void on_reverseAPIFeatureSetIndex_editingFinished();
    void on_reverseAPIFeatureIndex_editingFinished();
    void on_presets_clicked();
    void accept();

private:
    Ui::BasicFeatureSettingsDialog *ui;
    QString m_title;
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIFeatureSetIndex;
    uint16_t m_reverseAPIFeatureIndex;
    QString m_defaultTitle;
    bool m_hasChanged;
};

#endif // INCLUDE_SDRGUI_BASICFEATURESETTINGSDIALOG_H
