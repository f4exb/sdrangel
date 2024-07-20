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
#include "wdsprxdnrdialog.h"
#include "ui_wdsprxdnrdialog.h"

WDSPRxDNRDialog::WDSPRxDNRDialog(QWidget* parent) :
    QDialog(parent),
    ui(new Ui::WDSPRxDNRDialog)
{
    ui->setupUi(this);
    ui->snb->hide();
}

WDSPRxDNRDialog::~WDSPRxDNRDialog()
{
    delete ui;
}

void WDSPRxDNRDialog::setSNB(bool snb)
{
    ui->snb->blockSignals(true);
    ui->snb->setChecked(snb);
    ui->snb->blockSignals(false);
    m_snb = snb;
}

void WDSPRxDNRDialog::setNRScheme(WDSPRxProfile::WDSPRxNRScheme scheme)
{
    ui->nr->blockSignals(true);
    ui->nr->setCurrentIndex((int) scheme);
    ui->nr->blockSignals(false);
    m_nrScheme = scheme;
}

void WDSPRxDNRDialog::setNR2Gain(WDSPRxProfile::WDSPRxNR2Gain gain)
{
    ui->nr2Gain->blockSignals(true);
    ui->nr2Gain->setCurrentIndex((int) gain);
    ui->nr2Gain->blockSignals(false);
    m_nr2Gain = gain;
}

void WDSPRxDNRDialog::setNR2NPE(WDSPRxProfile::WDSPRxNR2NPE nr2NPE)
{
    ui->nr2NPE->blockSignals(true);
    ui->nr2NPE->setCurrentIndex((int) nr2NPE);
    ui->nr2NPE->blockSignals(false);
    m_nr2NPE = nr2NPE;
}

void WDSPRxDNRDialog::setNRPosition(WDSPRxProfile::WDSPRxNRPosition position)
{
    ui->nrPosition->blockSignals(true);
    ui->nrPosition->setCurrentIndex((int) position);
    ui->nrPosition->blockSignals(false);
    m_nrPosition = position;
}

void WDSPRxDNRDialog::setNR2ArtifactReduction(bool nr2ArtifactReducion)
{
    ui->nr2ArtifactReduction->blockSignals(true);
    ui->nr2ArtifactReduction->setChecked(nr2ArtifactReducion);
    ui->nr2ArtifactReduction->blockSignals(false);
    m_nr2ArtifactReduction = nr2ArtifactReducion;
}

void WDSPRxDNRDialog::on_snb_clicked(bool checked)
{
    m_snb = checked;
    emit valueChanged(ChangedSNB);
}

void WDSPRxDNRDialog::on_nr_currentIndexChanged(int index)
{
    m_nrScheme = (WDSPRxProfile::WDSPRxNRScheme) index;
    emit valueChanged(ChangedNR);
}

void WDSPRxDNRDialog::on_nr2Gain_currentIndexChanged(int index)
{
    m_nr2Gain = (WDSPRxProfile::WDSPRxNR2Gain) index;
    emit valueChanged(ChangedNR2Gain);
}

void WDSPRxDNRDialog::on_nr2NPE_currentIndexChanged(int index)
{
    m_nr2NPE = (WDSPRxProfile::WDSPRxNR2NPE) index;
    emit valueChanged(ChangedNR2NPE);
}

void WDSPRxDNRDialog::on_nrPosition_currentIndexChanged(int index)
{
    m_nrPosition = (WDSPRxProfile::WDSPRxNRPosition) index;
    emit valueChanged(ChangedNRPosition);
}

void WDSPRxDNRDialog::on_nr2ArtifactReduction_clicked(bool checked)
{
    m_nr2ArtifactReduction = checked;
    emit valueChanged(ChangedNR2Artifacts);
}
