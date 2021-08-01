///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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
#include "spectrummarkersdialog.h"

#include "ui_spectrummarkersdialog.h"


SpectrumMarkersDialog::SpectrumMarkersDialog(
    QList<SpectrumHistogramMarker>& histogramMarkers,
    QList<SpectrumWaterfallMarker>& waterfallMarkers,
    QWidget* parent) :
    QDialog(parent),
    ui(new Ui::SpectrumMarkersDialog),
    m_histogramMarkers(histogramMarkers),
    m_waterfallMarkers(waterfallMarkers),
    m_histogramMarkerIndex(0),
    m_centerFrequency(0),
    m_power(0.5f)
{
    ui->setupUi(this);
    ui->markerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->markerFrequency->setValueRange(false, 10, -9999999999L, 9999999999L);
    ui->marker->setMaximum(m_histogramMarkers.size() - 1);
    displayHistogramMarker();
}

SpectrumMarkersDialog::~SpectrumMarkersDialog()
{}

void SpectrumMarkersDialog::displayHistogramMarker()
{
    if (m_histogramMarkers.size() == 0)
    {
        ui->marker->setEnabled(false);
        ui->markerFrequency->setEnabled(false);
        ui->powerMode->setEnabled(false);
        ui->fixedPower->setEnabled(false);
        ui->markerText->setText("-");
        ui->fixedPower->setValue(0);
        ui->fixedPowerText->setText(tr("0.0"));
    }
    else
    {
        ui->marker->setEnabled(true);
        ui->markerFrequency->setEnabled(true);
        ui->powerMode->setEnabled(true);
        ui->fixedPower->setEnabled(true);
        ui->markerText->setText(tr("%1").arg(m_histogramMarkerIndex));
        ui->markerFrequency->setValue(m_histogramMarkers[m_histogramMarkerIndex].m_frequency);
        ui->powerMode->setCurrentIndex((int) m_histogramMarkers[m_histogramMarkerIndex].m_markerType);
        float powerDB = CalcDb::dbPower(m_histogramMarkers[m_histogramMarkerIndex].m_power);
        ui->fixedPower->setValue(powerDB*10);
        ui->fixedPowerText->setText(QString::number(powerDB, 'f', 1));
    }
}

void SpectrumMarkersDialog::on_markerFrequency_changed(qint64 value)
{
    if (m_histogramMarkers.size() == 0) {
        return;
    }

    m_histogramMarkers[m_histogramMarkerIndex].m_frequency = value;
    emit updateHistogram();
}

void SpectrumMarkersDialog::on_fixedPower_valueChanged(int value)
{
    if (m_histogramMarkers.size() == 0) {
        return;
    }

    float powerDB = value / 10.0f;
    ui->fixedPowerText->setText(QString::number(powerDB, 'f', 1));
    m_histogramMarkers[m_histogramMarkerIndex].m_power = CalcDb::powerFromdB(powerDB);
    emit updateHistogram();
}

void SpectrumMarkersDialog::on_marker_valueChanged(int value)
{
    if (m_histogramMarkers.size() == 0) {
        return;
    }

    m_histogramMarkerIndex = value;
    displayHistogramMarker();
}

void SpectrumMarkersDialog::on_setReference_clicked(bool checked)
{
    (void) checked;

    if ((m_histogramMarkerIndex == 0) || (m_histogramMarkers.size() < 2)) {
        return;
    }

    SpectrumHistogramMarker marker0 = m_histogramMarkers.at(0);
    m_histogramMarkers[0] = m_histogramMarkers[m_histogramMarkerIndex];
    m_histogramMarkers[m_histogramMarkerIndex] = marker0;
    displayHistogramMarker();
    emit updateHistogram();
}

void SpectrumMarkersDialog::on_markerAdd_clicked(bool checked)
{
    (void) checked;

    if (m_histogramMarkers.size() == SpectrumHistogramMarker::m_maxNbOfMarkers) {
        return;
    }

    m_histogramMarkers.append(SpectrumHistogramMarker());
    m_histogramMarkers.back().m_frequency = m_centerFrequency;
    m_histogramMarkers.back().m_power = m_power;
    m_histogramMarkerIndex = m_histogramMarkers.size() - 1;
    ui->marker->setMaximum(m_histogramMarkers.size() - 1);
    displayHistogramMarker();
}

void SpectrumMarkersDialog::on_markerDel_clicked(bool checked)
{
    (void) checked;

    if (m_histogramMarkers.size() == 0) {
        return;
    }

    m_histogramMarkers.removeAt(m_histogramMarkerIndex);
    m_histogramMarkerIndex = m_histogramMarkerIndex < m_histogramMarkers.size() ?
        m_histogramMarkerIndex : m_histogramMarkerIndex - 1;
    ui->marker->setMaximum(m_histogramMarkers.size() - 1);
    displayHistogramMarker();
}

void SpectrumMarkersDialog::on_powerMode_currentIndexChanged(int index)
{
    if (m_histogramMarkers.size() == 0) {
        return;
    }

    m_histogramMarkers[m_histogramMarkerIndex].m_markerType = (SpectrumHistogramMarkerType) index;
}
