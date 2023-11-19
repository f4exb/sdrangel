///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#include <QStandardPaths>
#include <QColorDialog>
#include <QFileDialog>

#include "util/db.h"
#include "util/csv.h"
#include "spectrummeasurementsdialog.h"
#include "spectrummeasurements.h"
#include "glspectrum.h"

#include "ui_spectrummeasurementsdialog.h"

SpectrumMeasurementsDialog::SpectrumMeasurementsDialog(GLSpectrum *glSpectrum, SpectrumSettings *settings, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SpectrumMeasurementsDialog),
    m_glSpectrum(glSpectrum),
    m_settings(settings)
{
    ui->setupUi(this);

    ui->measurement->setCurrentIndex((int)m_settings->m_measurement);
    ui->position->setCurrentIndex((int)m_settings->m_measurementsPosition);
    ui->precision->setValue(m_settings->m_measurementPrecision);
    ui->highlight->setChecked(m_settings->m_measurementHighlight);

    ui->centerFrequencyOffset->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->centerFrequencyOffset->setValueRange(false, 8, -99999999L, 99999999L);
    ui->centerFrequencyOffset->setValue(m_settings->m_measurementCenterFrequencyOffset);

    ui->bandwidth->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->bandwidth->setValueRange(true, 8, 0L, 99999999L);
    ui->bandwidth->setValue(m_settings->m_measurementBandwidth);

    ui->chSpacing->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->chSpacing->setValueRange(true, 8, 0L, 99999999L);
    ui->chSpacing->setValue(m_settings->m_measurementChSpacing);

    ui->adjChBandwidth->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->adjChBandwidth->setValueRange(true, 8, 0L, 99999999L);
    ui->adjChBandwidth->setValue(m_settings->m_measurementAdjChBandwidth);

    ui->harmonics->setValue(m_settings->m_measurementHarmonics);
    ui->peaks->setValue(m_settings->m_measurementPeaks);

    displaySettings();
}

SpectrumMeasurementsDialog::~SpectrumMeasurementsDialog()
{}

void SpectrumMeasurementsDialog::displaySettings()
{
    bool show = m_settings->m_measurement != SpectrumSettings::MeasurementNone;

    ui->positionLabel->setVisible(show);
    ui->position->setVisible(show);
    ui->precisionLabel->setVisible(show);
    ui->precision->setVisible(show);
    ui->highlightLabel->setVisible(show);
    ui->highlight->setVisible(show);

    bool reset = (m_settings->m_measurement >= SpectrumSettings::MeasurementChannelPower);
    ui->resetMeasurements->setVisible(reset && show);

    bool bw = (m_settings->m_measurement == SpectrumSettings::MeasurementChannelPower)
               || (m_settings->m_measurement == SpectrumSettings::MeasurementAdjacentChannelPower)
               || (m_settings->m_measurement == SpectrumSettings::MeasurementOccupiedBandwidth);
    ui->centerFrequencyOffsetLabel->setVisible(bw && show);
    ui->centerFrequencyOffset->setVisible(bw && show);
    ui->bandwidthLabel->setVisible(bw && show);
    ui->bandwidth->setVisible(bw && show);

    bool adj = m_settings->m_measurement == SpectrumSettings::MeasurementAdjacentChannelPower;
    ui->chSpacingLabel->setVisible(adj && show);
    ui->chSpacing->setVisible(adj && show);
    ui->adjChBandwidthLabel->setVisible(adj && show);
    ui->adjChBandwidth->setVisible(adj && show);

    bool harmonics = (m_settings->m_measurement == SpectrumSettings::MeasurementSNR);
    ui->harmonicsLabel->setVisible(harmonics && show);
    ui->harmonics->setVisible(harmonics && show);

    bool peaks = (m_settings->m_measurement == SpectrumSettings::MeasurementPeaks);
    ui->peaksLabel->setVisible(peaks && show);
    ui->peaks->setVisible(peaks && show);
}


void SpectrumMeasurementsDialog::on_measurement_currentIndexChanged(int index)
{
    m_settings->m_measurement = (SpectrumSettings::Measurement)index;
    displaySettings();
    emit updateMeasurements();
}

void SpectrumMeasurementsDialog::on_precision_valueChanged(int value)
{
    m_settings->m_measurementPrecision = value;
    emit updateMeasurements();
}

void SpectrumMeasurementsDialog::on_position_currentIndexChanged(int index)
{
    m_settings->m_measurementsPosition = (SpectrumSettings::MeasurementsPosition)index;
    emit updateMeasurements();
}

void SpectrumMeasurementsDialog::on_highlight_toggled(bool checked)
{
    m_settings->m_measurementHighlight = checked;
    emit updateMeasurements();
}

void SpectrumMeasurementsDialog::on_resetMeasurements_clicked(bool checked)
{
    (void) checked;

    if (m_glSpectrum) {
        m_glSpectrum->getMeasurements()->reset();
    }
}

void SpectrumMeasurementsDialog::on_centerFrequencyOffset_changed(qint64 value)
{
    m_settings->m_measurementCenterFrequencyOffset = value;
    emit updateMeasurements();
}

void SpectrumMeasurementsDialog::on_bandwidth_changed(qint64 value)
{
    m_settings->m_measurementBandwidth = value;
    emit updateMeasurements();
}

void SpectrumMeasurementsDialog::on_chSpacing_changed(qint64 value)
{
    m_settings->m_measurementChSpacing = value;
    emit updateMeasurements();
}

void SpectrumMeasurementsDialog::on_adjChBandwidth_changed(qint64 value)
{
    m_settings->m_measurementAdjChBandwidth = value;
    emit updateMeasurements();
}

void SpectrumMeasurementsDialog::on_harmonics_valueChanged(int value)
{
    m_settings->m_measurementHarmonics = value;
    emit updateMeasurements();
}

void SpectrumMeasurementsDialog::on_peaks_valueChanged(int value)
{
    m_settings->m_measurementPeaks = value;
    emit updateMeasurements();
}
