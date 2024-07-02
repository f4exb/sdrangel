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
#include "wdsprxdnbdialog.h"
#include "ui_wdsprxdnbdialog.h"

WDSPRxDNBDialog::WDSPRxDNBDialog(QWidget* parent) :
    QDialog(parent),
    ui(new Ui::WDSPRxDNBDialog)
{
    ui->setupUi(this);
}

WDSPRxDNBDialog::~WDSPRxDNBDialog()
{
    delete ui;
}

void WDSPRxDNBDialog::setNBScheme(WDSPRxProfile::WDSPRxNBScheme scheme)
{
    ui->nb->blockSignals(true);
    ui->nb->setCurrentIndex((int) scheme);
    ui->nb->blockSignals(false);
    m_nbScheme = scheme;
}

void WDSPRxDNBDialog::setNB2Mode(WDSPRxProfile::WDSPRxNB2Mode mode)
{
    ui->nb2Mode->blockSignals(true);
    ui->nb2Mode->setCurrentIndex((int) mode);
    ui->nb2Mode->blockSignals(false);
    m_nb2Mode = mode;
}

void WDSPRxDNBDialog::setNBSlewTime(double time)
{
    ui->nbSlewTime->blockSignals(true);
    ui->nbSlewTime->setValue(time);
    ui->nbSlewTime->blockSignals(false);
    m_nbSlewTime = time;
}

void WDSPRxDNBDialog::setNBLeadTime(double time)
{
    ui->nbLeadTime->blockSignals(true);
    ui->nbLeadTime->setValue(time);
    ui->nbLeadTime->blockSignals(false);
    m_nbLeadTime = time;
}

void WDSPRxDNBDialog::setNBLagTime(double time)
{
    ui->nbLagTime->blockSignals(true);
    ui->nbLagTime->setValue(time);
    ui->nbLagTime->blockSignals(false);
    m_nbLagTime = time;
}

void WDSPRxDNBDialog::setNBThreshold(int threshold)
{
    ui->nbThreshold->blockSignals(true);
    ui->nbThreshold->setValue(threshold);
    ui->nbThreshold->blockSignals(false);
    m_nbThreshold = threshold;
}

void WDSPRxDNBDialog::setNBAvgTime(double time)
{
    ui->nbAvgTime->blockSignals(true);
    ui->nbAvgTime->setValue(time);
    ui->nbAvgTime->blockSignals(false);
    m_nbAvgTime = time;
}

void WDSPRxDNBDialog::on_nb_currentIndexChanged(int index)
{
    m_nbScheme = (WDSPRxProfile::WDSPRxNBScheme) index;
    emit valueChanged(ChangedNB);
}

void WDSPRxDNBDialog::on_nb2Mode_currentIndexChanged(int index)
{
    m_nb2Mode = (WDSPRxProfile::WDSPRxNB2Mode) index;
    emit valueChanged(ChangedNB2Mode);
}

void WDSPRxDNBDialog::on_nbSlewTime_valueChanged(double value)
{
    m_nbSlewTime = value;
    emit valueChanged(ChangedNBSlewTime);
}

void WDSPRxDNBDialog::on_nbLeadTime_valueChanged(double value)
{
    m_nbLeadTime = value;
    emit valueChanged(ChangedNBLeadTime);
}

void WDSPRxDNBDialog::on_nbLagTime_valueChanged(double value)
{
    m_nbLagTime = value;
    emit valueChanged(ChangedNBLagTime);
}

void WDSPRxDNBDialog::on_nbThreshold_valueChanged(int value)
{
    m_nbThreshold = value;
    emit valueChanged(ChangedNBThreshold);
}

void WDSPRxDNBDialog::on_nbAvgTime_valueChanged(double value)
{
    m_nbAvgTime = value;
    emit valueChanged(ChangedNBAvgTime);
}
