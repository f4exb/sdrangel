///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Edouard Griffiths, F4EXB <f4exb06@gmail.com>                   //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////
#ifndef INCLUDE_WDSPRXSQUELCHDIALOG_H
#define INCLUDE_WDSPRXSQUELCHDIALOG_H

#include <QDialog>

#include "wdsprxsettings.h"

namespace Ui {
    class WDSPRxSquelchDialog;
}

class WDSPRxSquelchDialog : public QDialog {
    Q_OBJECT
public:
    enum ValueChanged {
        ChangedMode,
        ChangedSSQLTauMute,
        ChangedSSQLTauUnmute,
        ChangedAMSQMaxTail,
    };

    explicit WDSPRxSquelchDialog(QWidget* parent = nullptr);
    ~WDSPRxSquelchDialog() override;

    void setMode(WDSPRxProfile::WDSPRxSquelchMode mode);
    void setSSQLTauMute(double value);
    void setSSQLTauUnmute(double value);
    void setAMSQMaxTail(double value);

    WDSPRxProfile::WDSPRxSquelchMode getMode() const { return m_mode; }
    double getSSQLTauMute() const { return m_ssqlTauMute; }
    double getSSQLTauUnmute() const { return m_ssqlTauUnmute; }
    double getAMSQMaxTail() const { return m_amsqMaxTail; }

signals:
    void valueChanged(int valueChanged);

private:
    Ui::WDSPRxSquelchDialog *ui;
    WDSPRxProfile::WDSPRxSquelchMode m_mode;
    double m_ssqlTauMute;   //!< Voice squelch tau mute
    double m_ssqlTauUnmute; //!< Voice squelch tau unmute
    double m_amsqMaxTail;

private slots:
    void on_voiceSquelchGroup_clicked(bool checked);
    void on_amSquelchGroup_clicked(bool checked);
    void on_fmSquelchGroup_clicked(bool checked);
    void on_ssqlTauMute_valueChanged(double value);
    void on_ssqlTauUnmute_valueChanged(double value);
    void on_amsqMaxTail_valueChanged(double value);
};

#endif
