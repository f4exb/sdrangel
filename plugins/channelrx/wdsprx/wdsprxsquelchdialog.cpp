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

#include "wdsprxsquelchdialog.h"
#include "ui_wdsprxsquelchdialog.h"

WDSPRxSquelchDialog::WDSPRxSquelchDialog(QWidget* parent) :
    QDialog(parent),
    ui(new Ui::WDSPRxSquelchDialog)
{
    ui->setupUi(this);
}

WDSPRxSquelchDialog::~WDSPRxSquelchDialog()
{
    delete ui;
}

void WDSPRxSquelchDialog::setMode(WDSPRxProfile::WDSPRxSquelchMode mode)
{
    ui->voiceSquelchGroup->blockSignals(true);
    ui->amSquelchGroup->blockSignals(true);
    ui->fmSquelchGroup->blockSignals(true);

    ui->voiceSquelchGroup->setChecked(false);
    ui->amSquelchGroup->setChecked(false);
    ui->fmSquelchGroup->setChecked(false);

    switch(mode)
    {
    case WDSPRxProfile::SquelchModeVoice:
        ui->voiceSquelchGroup->setChecked(true);
        break;
    case WDSPRxProfile::SquelchModeAM:
        ui->amSquelchGroup->setChecked(true);
        break;
    case WDSPRxProfile::SquelchModeFM:
        ui->fmSquelchGroup->setChecked(true);
        break;
    default:
        break;
    }

    ui->voiceSquelchGroup->blockSignals(false);
    ui->amSquelchGroup->blockSignals(false);
    ui->fmSquelchGroup->blockSignals(false);
}

void WDSPRxSquelchDialog::setSSQLTauMute(double value)
{
    ui->ssqlTauMute->blockSignals(true);
    ui->ssqlTauMute->setValue(value);
    ui->ssqlTauMute->blockSignals(false);
}

void WDSPRxSquelchDialog::setSSQLTauUnmute(double value)
{
    ui->ssqlTauUnmute->blockSignals(true);
    ui->ssqlTauUnmute->setValue(value);
    ui->ssqlTauUnmute->blockSignals(false);
}

void WDSPRxSquelchDialog::setAMSQMaxTail(double value)
{
    ui->amsqMaxTail->blockSignals(true);
    ui->amsqMaxTail->setValue(value);
    ui->amsqMaxTail->blockSignals(false);
}

void WDSPRxSquelchDialog::on_voiceSquelchGroup_clicked(bool checked)
{
    if (checked)
    {
        ui->amSquelchGroup->blockSignals(true);
        ui->fmSquelchGroup->blockSignals(true);

        ui->amSquelchGroup->setChecked(false);
        ui->fmSquelchGroup->setChecked(false);

        m_mode = WDSPRxProfile::SquelchModeVoice;

        ui->amSquelchGroup->blockSignals(false);
        ui->fmSquelchGroup->blockSignals(false);
        emit valueChanged(ChangedMode);
    }
    else
    {
        ui->voiceSquelchGroup->blockSignals(true);
        ui->voiceSquelchGroup->setChecked(true);
        ui->voiceSquelchGroup->blockSignals(false);
    }
}

void WDSPRxSquelchDialog::on_amSquelchGroup_clicked(bool checked)
{
    if (checked)
    {
        ui->voiceSquelchGroup->blockSignals(true);
        ui->fmSquelchGroup->blockSignals(true);

        ui->voiceSquelchGroup->setChecked(false);
        ui->fmSquelchGroup->setChecked(false);

        m_mode = WDSPRxProfile::SquelchModeAM;

        ui->voiceSquelchGroup->blockSignals(false);
        ui->fmSquelchGroup->blockSignals(false);
        emit valueChanged(ChangedMode);
    }
    else
    {
        ui->amSquelchGroup->blockSignals(true);
        ui->amSquelchGroup->setChecked(true);
        ui->amSquelchGroup->blockSignals(false);
    }
}

void WDSPRxSquelchDialog::on_fmSquelchGroup_clicked(bool checked)
{
    if (checked)
    {
        ui->voiceSquelchGroup->blockSignals(true);
        ui->amSquelchGroup->blockSignals(true);

        ui->voiceSquelchGroup->setChecked(false);
        ui->amSquelchGroup->setChecked(false);

        m_mode = WDSPRxProfile::SquelchModeFM;

        ui->voiceSquelchGroup->blockSignals(false);
        ui->amSquelchGroup->blockSignals(false);
        emit valueChanged(ChangedMode);
    }
    else
    {
        ui->fmSquelchGroup->blockSignals(true);
        ui->fmSquelchGroup->setChecked(true);
        ui->fmSquelchGroup->blockSignals(false);
    }
}

void WDSPRxSquelchDialog::on_ssqlTauMute_valueChanged(double value)
{
    m_ssqlTauMute = value;
    emit valueChanged(ChangedSSQLTauMute);
}

void WDSPRxSquelchDialog::on_ssqlTauUnmute_valueChanged(double value)
{
    m_ssqlTauUnmute = value;
    emit valueChanged(ChangedSSQLTauUnmute);
}

void WDSPRxSquelchDialog::on_amsqMaxTail_valueChanged(double value)
{
    m_amsqMaxTail = value;
    emit valueChanged(ChangedAMSQMaxTail);
}
