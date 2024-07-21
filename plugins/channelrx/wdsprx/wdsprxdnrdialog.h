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
#ifndef INCLUDE_WDSPRXDNRDIALOG_H
#define INCLUDE_WDSPRXDNRDIALOG_H

#include <QDialog>

#include "wdsprxsettings.h"

namespace Ui {
    class WDSPRxDNRDialog;
}

class WDSPRxDNRDialog : public QDialog {
    Q_OBJECT
public:
    enum ValueChanged {
        ChangedSNB,
        ChangedNR,
        ChangedNR2Gain,
        ChangedNR2NPE,
        ChangedNRPosition,
        ChangedNR2Artifacts,
    };

    explicit WDSPRxDNRDialog(QWidget* parent = nullptr);
    ~WDSPRxDNRDialog() override;

    void setSNB(bool snb);
    void setNRScheme(WDSPRxProfile::WDSPRxNRScheme scheme);
    void setNR2Gain(WDSPRxProfile::WDSPRxNR2Gain gain);
    void setNR2NPE(WDSPRxProfile::WDSPRxNR2NPE nr2NPE);
    void setNRPosition(WDSPRxProfile::WDSPRxNRPosition position);
    void setNR2ArtifactReduction(bool nr2ArtifactReducion);

    bool getSNB() const { return m_snb; }
    WDSPRxProfile::WDSPRxNRScheme getNRScheme() const { return m_nrScheme; }
    WDSPRxProfile::WDSPRxNR2Gain getNR2Gain() const { return m_nr2Gain; }
    WDSPRxProfile::WDSPRxNR2NPE getNR2NPE() const { return m_nr2NPE; }
    WDSPRxProfile::WDSPRxNRPosition getNRPosition() const { return m_nrPosition; }
    bool getNR2ArtifactReduction() const { return m_nr2ArtifactReduction; }

signals:
    void valueChanged(int valueChanged);

private:
    Ui::WDSPRxDNRDialog *ui;
    bool m_snb;
    WDSPRxProfile::WDSPRxNRScheme m_nrScheme;
    WDSPRxProfile::WDSPRxNR2Gain m_nr2Gain;
    WDSPRxProfile::WDSPRxNR2NPE m_nr2NPE;
    WDSPRxProfile::WDSPRxNRPosition m_nrPosition;
    bool m_nr2ArtifactReduction;

private slots:
    void on_snb_clicked(bool checked);
    void on_nr_currentIndexChanged(int index);
    void on_nr2Gain_currentIndexChanged(int index);
    void on_nr2NPE_currentIndexChanged(int index);
    void on_nrPosition_currentIndexChanged(int index);
    void on_nr2ArtifactReduction_clicked(bool checked);
};

#endif // INCLUDE_WDSPRXDNRDIALOG_H
