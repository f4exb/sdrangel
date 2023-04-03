///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#include "util/units.h"

#include "dfmstatusdialog.h"

DFMStatusDialog::DFMStatusDialog(QWidget* parent) :
    QDialog(parent),
    ui(new Ui::DFMStatusDialog)
{
    ui->setupUi(this);
    // Make checkboxes read-only
    ui->trackOn->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui->driveOn->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui->brakesOn->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui->pumpsOn->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui->controller->setAttribute(Qt::WA_TransparentForMouseEvents);
}

void DFMStatusDialog::displayStatus(const DFMProtocol::DFMStatus& dfmStatus)
{
    ui->currentHA->setText(QString::number(dfmStatus.m_currentHA, 'f'));
    ui->currentRA->setText(QString::number(dfmStatus.m_currentRA, 'f'));
    ui->currentDec->setText(QString::number(dfmStatus.m_currentDec, 'f'));

    ui->st->setText(Units::decimalHoursToHoursMinutesAndSeconds(dfmStatus.m_siderealTime));
    ui->ut->setText(Units::decimalHoursToHoursMinutesAndSeconds(dfmStatus.m_universalTime));

    ui->currentX->setText(QString::number(dfmStatus.m_currentX, 'f'));
    ui->currentY->setText(QString::number(dfmStatus.m_currentY, 'f'));
    ui->siderealRateX->setText(QString::number(dfmStatus.m_siderealRateX, 'f'));
    ui->siderealRateY->setText(QString::number(dfmStatus.m_siderealRateY, 'f'));
    ui->rateX->setText(QString::number(dfmStatus.m_rateX, 'f'));
    ui->rateY->setText(QString::number(dfmStatus.m_rateY, 'f'));
    ui->torqueX->setText(QString::number(dfmStatus.m_torqueX, 'f'));
    ui->torqueY->setText(QString::number(dfmStatus.m_torqueY, 'f'));

    ui->trackOn->setChecked(dfmStatus.m_trackOn);
    ui->driveOn->setChecked(dfmStatus.m_drivesOn);
    ui->brakesOn->setChecked(dfmStatus.m_brakesOn);
    ui->pumpsOn->setChecked(dfmStatus.m_pumpsReady); // ?
    ui->controller->setCurrentIndex((int)dfmStatus.m_controller);
}

DFMStatusDialog::~DFMStatusDialog()
{
    delete ui;
}

void DFMStatusDialog::accept()
{
    QDialog::accept();
}

