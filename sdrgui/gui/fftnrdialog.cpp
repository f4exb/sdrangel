///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB                                   //
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

#include "util/db.h"
#include "fftnrdialog.h"
#include "ui_fftnrdialog.h"

FFTNRDialog::FFTNRDialog(QWidget* parent) :
    QDialog(parent),
    ui(new Ui::FFTNRDialog)
{
    ui->setupUi(this);
}

FFTNRDialog::~FFTNRDialog()
{
    delete ui;
}

void FFTNRDialog::setScheme(FFTNoiseReduction::Scheme scheme)
{
    ui->scheme->blockSignals(true);
    ui->genParam->blockSignals(true);

    switch (scheme)
    {
    case FFTNoiseReduction::Scheme::SchemeAverage:
        ui->scheme->setCurrentIndex(0);
        ui->genParam->setMinimum(200);
        ui->genParam->setMaximum(990);
        ui->genParam->setValue(m_aboveAvgFactor*10);
        ui->genParam->setToolTip("Above average multiplier");
        ui->genParamLabel->setText("Above avg mult");
        ui->genParamValue->setText(tr("%1").arg(m_aboveAvgFactor, 0, 'f', 1));
        break;
    case FFTNoiseReduction::Scheme::SchemeAvgStdDev:
        ui->scheme->setCurrentIndex(1);
        ui->genParam->setMinimum(20);
        ui->genParam->setMaximum(160);
        ui->genParam->setValue(m_sigmaFactor*10);
        ui->genParam->setToolTip("Standard deviation multiplier");
        ui->genParamLabel->setText("Sigma multiplier");
        ui->genParamValue->setText(tr("%1").arg(m_sigmaFactor, 0, 'f', 1));
        break;
    case FFTNoiseReduction::Scheme::SchemePeaks:
        ui->scheme->setCurrentIndex(2);
        ui->genParam->setMinimum(1);
        ui->genParam->setMaximum(40);
        ui->genParam->setValue(m_nbPeaks);
        ui->genParam->setToolTip("Number of max peaks selected");
        ui->genParamLabel->setText("Nb of peaks");
        ui->genParamValue->setText(tr("%1").arg(m_nbPeaks));
        break;
    default:
        break;
    }

    ui->scheme->blockSignals(false);
    ui->genParam->blockSignals(false);

    m_scheme = scheme;
}

void FFTNRDialog::setAboveAvgFactor(float aboveAvgFactor)
{
    if (aboveAvgFactor < 20.0f)
    {
        m_aboveAvgFactor = 20.0f;
        emit valueChanged(ChangedAboveAvgFactor);
    }
    else if (aboveAvgFactor > 99.0f)
    {
        m_aboveAvgFactor = 99.0f;
        emit valueChanged(ChangedAboveAvgFactor);
    }
    else{
        m_aboveAvgFactor = aboveAvgFactor;
    }

    if (m_scheme == FFTNoiseReduction::Scheme::SchemeAverage)
    {
        ui->genParam->blockSignals(true);
        ui->genParam->setValue(m_aboveAvgFactor*10);
        ui->genParamValue->setText(tr("%1").arg(m_aboveAvgFactor, 0, 'f', 1));
        ui->genParam->blockSignals(false);
    }
}

void FFTNRDialog::setSigmaFactor(float sigmaFactor)
{
    if (sigmaFactor < 2.0f)
    {
        m_sigmaFactor = 2.0f;
        emit valueChanged(ChangedSigmaFactor);
    }
    else if (sigmaFactor > 16.0f)
    {
        m_sigmaFactor = 16.0f;
        emit valueChanged(ChangedSigmaFactor);
    }
    else{
        m_sigmaFactor = sigmaFactor;
    }

    if (m_scheme == FFTNoiseReduction::Scheme::SchemeAvgStdDev)
    {
        ui->genParam->blockSignals(true);
        ui->genParam->setValue(m_sigmaFactor*10);
        ui->genParamValue->setText(tr("%1").arg(m_sigmaFactor, 0, 'f', 1));
        ui->genParam->blockSignals(false);
    }
}

void FFTNRDialog::setNbPeaks(int nbPeaks)
{
    if (nbPeaks < 1)
    {
        m_nbPeaks = 1;
        emit valueChanged(ChangedNbPeaks);
    }
    else if (nbPeaks > 40)
    {
        m_nbPeaks = 40;
        emit valueChanged(ChangedNbPeaks);
    }
    else{
        m_nbPeaks = nbPeaks;
    }

    if (m_scheme == FFTNoiseReduction::Scheme::SchemePeaks)
    {
        ui->genParam->blockSignals(true);
        ui->genParam->setValue(m_nbPeaks);
        ui->genParamValue->setText(tr("%1").arg(m_nbPeaks));
        ui->genParam->blockSignals(false);
    }
}

void FFTNRDialog::setAlpha(float alpha, int fftLength, int sampleRate)
{
    m_alpha = alpha < 0.0f ? 0.0f : alpha > 0.99999 ? 0.99999f : alpha;
    m_flen = fftLength;
    m_sampleRate = sampleRate;
    int alphaDisplay = -round(CalcDb::dbPower(1.0f - m_alpha));
    ui->alpha->blockSignals(true);
    ui->alpha->setValue(alphaDisplay);
    ui->alpha->blockSignals(false);
    ui->alphaValue->setText(tr("%1").arg(alphaDisplay));
    ui->alphaValue->setToolTip(tr("dB(1 - alpha) alpha=%1").arg(m_alpha, 0, 'f', 5));
    float t = m_flen;
    t /= m_sampleRate;
    float tau = -(t / log(m_alpha));
    ui->tauText->setText(tr("%1").arg(tau, 0, 'f', 3));
}

void FFTNRDialog::on_scheme_currentIndexChanged(int index)
{
    setScheme((FFTNoiseReduction::Scheme) index);
    emit valueChanged(ChangedScheme);
}

void FFTNRDialog::on_genParam_valueChanged(int value)
{
    switch (m_scheme)
    {
    case FFTNoiseReduction::Scheme::SchemeAverage:
        m_aboveAvgFactor = value / 10.0f;
        ui->genParamValue->setText(tr("%1").arg(m_aboveAvgFactor, 0, 'f', 1));
        emit valueChanged(ChangedAboveAvgFactor);
        break;
    case FFTNoiseReduction::Scheme::SchemeAvgStdDev:
        m_sigmaFactor = value / 10.0f;
        ui->genParamValue->setText(tr("%1").arg(m_sigmaFactor, 0, 'f', 1));
        emit valueChanged(ChangedSigmaFactor);
        break;
    case FFTNoiseReduction::Scheme::SchemePeaks:
        m_nbPeaks = value;
        ui->genParamValue->setText(tr("%1").arg(m_nbPeaks));
        emit valueChanged(ChangedNbPeaks);
        break;
    default:
        break;
    }
}

void FFTNRDialog::on_alpha_valueChanged(int value)
{
    m_alpha = 1.0 - CalcDb::powerFromdB(-value);
    ui->alphaValue->setText(tr("%1").arg(value));
    ui->alphaValue->setToolTip(tr("dB(1 - alpha) alpha=%1").arg(m_alpha, 0, 'f', 5));
    float t = m_flen;
    t /= m_sampleRate;
    float tau = -(t / log(m_alpha));
    ui->tauText->setText(tr("%1").arg(tau, 0, 'f', 3));
    emit valueChanged(ChangedAlpha);
}
