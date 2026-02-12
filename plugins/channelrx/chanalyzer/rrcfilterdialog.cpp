///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#include "rrcfilterdialog.h"
#include "ui_rrcfilterdialog.h"

RRCFilterDialog::RRCFilterDialog(QWidget* parent) :
    QDialog(parent),
    ui(new Ui::RRCFilterDialog),
    m_rrcType(ChannelAnalyzerSettings::RRCFIR),
    m_rrcSymbolSpan(8),
    m_rrcNormalization(ChannelAnalyzerSettings::RRCNormGain),
    m_rrcFFTLog2Size(9)
{
    ui->setupUi(this);
    makeUIConnections();
}

RRCFilterDialog::~RRCFilterDialog()
{
    delete ui;
}

void RRCFilterDialog::setRRCType(ChannelAnalyzerSettings::RRCType rrcType)
{
    ui->firGroup->blockSignals(true);
    ui->fftGroup->blockSignals(true);

    ui->firGroup->setChecked(false);
    ui->fftGroup->setChecked(false);

    switch(rrcType)
    {
    case ChannelAnalyzerSettings::RRCFIR:
        ui->firGroup->setChecked(true);
        break;
    case ChannelAnalyzerSettings::RRCFFT:
        ui->fftGroup->setChecked(true);
        break;
    default:
        break;
    }

    ui->firGroup->blockSignals(false);
    ui->fftGroup->blockSignals(false);

    m_rrcType = rrcType;
}

void RRCFilterDialog::setRRCSymbolSpan(int symbolSpan)
{
    m_rrcSymbolSpan = symbolSpan;
    ui->symbolSpan->setValue(symbolSpan);
}

void RRCFilterDialog::setRRCNormalization(ChannelAnalyzerSettings::RRCNormalization normalization)
{
    m_rrcNormalization = normalization;
    ui->normalization->setCurrentIndex((int) normalization);
}

void RRCFilterDialog::setRRCFFTLog2Size(int log2Size)
{
    m_rrcFFTLog2Size = log2Size;
    ui->fftSize->setCurrentIndex(log2Size - 7);
}

void RRCFilterDialog::on_symbolSpan_valueChanged(int value)
{
    m_rrcSymbolSpan = value;
    emit valueChanged(ChangedRRCSymbolSpan);
}

void RRCFilterDialog::on_normalization_currentIndexChanged(int index)
{
    m_rrcNormalization = (ChannelAnalyzerSettings::RRCNormalization) index;
    emit valueChanged(ChangedRRCNormalization);
}

void RRCFilterDialog::on_fftLog2Size_currentIndexChanged(int index)
{
    m_rrcFFTLog2Size = index + 7;
    emit valueChanged(ChangedRRCFFTLog2Size);
}

void RRCFilterDialog::on_firGroup_clicked(bool checked)
{
    qDebug("RRCFilterDialog::on_firGroup_clicked: checked: %d", checked);

    if (checked)
    {
        ui->fftGroup->blockSignals(true);
        ui->fftGroup->setChecked(false);
        ui->fftGroup->blockSignals(false);

        m_rrcType = ChannelAnalyzerSettings::RRCFIR;
        emit valueChanged(ChangedRRCType);
    }
    else
    {
        ui->firGroup->blockSignals(true);
        ui->firGroup->setChecked(true);
        ui->firGroup->blockSignals(false);
    }
}

void RRCFilterDialog::on_fftGroup_clicked(bool checked)
{
    qDebug("RRCFilterDialog::on_fftGroup_clicked: checked: %d", checked);

    if (checked)
    {
        ui->firGroup->blockSignals(true);
        ui->firGroup->setChecked(false);
        ui->firGroup->blockSignals(false);

        m_rrcType = ChannelAnalyzerSettings::RRCFFT;
        emit valueChanged(ChangedRRCType);
    }
    else
    {
        ui->fftGroup->blockSignals(true);
        ui->fftGroup->setChecked(true);
        ui->fftGroup->blockSignals(false);
    }
}

void RRCFilterDialog::makeUIConnections()
{
    connect(ui->firGroup, &QGroupBox::clicked, this, &RRCFilterDialog::on_firGroup_clicked);
    connect(ui->fftGroup, &QGroupBox::clicked, this, &RRCFilterDialog::on_fftGroup_clicked);
    connect(ui->symbolSpan, QOverload<int>::of(&QSpinBox::valueChanged), this, &RRCFilterDialog::on_symbolSpan_valueChanged);
    connect(ui->normalization, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RRCFilterDialog::on_normalization_currentIndexChanged);
    connect(ui->fftSize, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RRCFilterDialog::on_fftLog2Size_currentIndexChanged);
}
