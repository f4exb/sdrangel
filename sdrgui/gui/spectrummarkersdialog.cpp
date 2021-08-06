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

#include <QColorDialog>

#include "util/db.h"
#include "spectrummarkersdialog.h"

#include "ui_spectrummarkersdialog.h"


SpectrumMarkersDialog::SpectrumMarkersDialog(
    QList<SpectrumHistogramMarker>& histogramMarkers,
    QList<SpectrumWaterfallMarker>& waterfallMarkers,
    SpectrumSettings::MarkersDisplay& markersDisplay,
    QWidget* parent) :
    QDialog(parent),
    ui(new Ui::SpectrumMarkersDialog),
    m_histogramMarkers(histogramMarkers),
    m_waterfallMarkers(waterfallMarkers),
    m_markersDisplay(markersDisplay),
    m_histogramMarkerIndex(0),
    m_waterfallMarkerIndex(0),
    m_centerFrequency(0),
    m_power(0.5f)
{
    ui->setupUi(this);
    ui->markerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->markerFrequency->setValueRange(false, 10, -9999999999L, 9999999999L);
    ui->marker->setMaximum(m_histogramMarkers.size() - 1);
    ui->wMarkerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->wMarkerFrequency->setValueRange(false, 10, -9999999999L, 9999999999L);
    ui->wMarker->setMaximum(m_waterfallMarkers.size() - 1);
    ui->showSelect->setCurrentIndex((int) m_markersDisplay);
    displayHistogramMarker();
    displayWaterfallMarker();
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
        ui->showMarker->setEnabled(false);
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
        ui->showMarker->setEnabled(true);
        ui->markerText->setText(tr("%1").arg(m_histogramMarkerIndex));
        ui->markerFrequency->setValue(m_histogramMarkers[m_histogramMarkerIndex].m_frequency);
        ui->powerMode->setCurrentIndex((int) m_histogramMarkers[m_histogramMarkerIndex].m_markerType);
        float powerDB = CalcDb::dbPower(m_histogramMarkers[m_histogramMarkerIndex].m_power);
        ui->fixedPower->setValue(powerDB*10);
        ui->fixedPowerText->setText(QString::number(powerDB, 'f', 1));
        int r,g,b,a;
        m_histogramMarkers[m_histogramMarkerIndex].m_markerColor.getRgb(&r, &g, &b, &a);
        ui->markerColor->setStyleSheet(tr("QLabel { background-color : rgb(%1,%2,%3); }").arg(r).arg(g).arg(b));
        ui->showMarker->setChecked(m_histogramMarkers[m_histogramMarkerIndex].m_show);
    }
}

void SpectrumMarkersDialog::displayWaterfallMarker()
{
    if (m_waterfallMarkers.size() == 0)
    {
        ui->wMarker->setEnabled(false);
        ui->wMarkerFrequency->setEnabled(false);
        ui->timeCoarse->setEnabled(false);
        ui->timeFine->setEnabled(false);
        ui->timeExp->setEnabled(false);
        ui->wShowMarker->setEnabled(false);
        ui->wMarkerText->setText("-");
        ui->timeCoarse->setValue(0);
        ui->timeFine->setValue(0);
        ui->timeText->setText("0.000");
        ui->timeExp->setValue(0);
        ui->timeExpText->setText("e+0");
    }
    else
    {
        ui->wMarker->setEnabled(true);
        ui->wMarkerFrequency->setEnabled(true);
        ui->timeCoarse->setEnabled(true);
        ui->timeFine->setEnabled(true);
        ui->timeExp->setEnabled(true);
        ui->wShowMarker->setEnabled(true);
        ui->wMarkerText->setText(tr("%1").arg(m_waterfallMarkerIndex));
        ui->wMarkerFrequency->setValue(m_waterfallMarkers[m_waterfallMarkerIndex].m_frequency);
        int r,g,b,a;
        m_waterfallMarkers[m_waterfallMarkerIndex].m_markerColor.getRgb(&r, &g, &b, &a);
        ui->wMarkerColor->setStyleSheet(tr("QLabel { background-color : rgb(%1,%2,%3); }").arg(r).arg(g).arg(b));
        displayTime(m_waterfallMarkers[m_waterfallMarkerIndex].m_time);
    }
}

void SpectrumMarkersDialog::displayTime(float time)
{
    int timeExp;
    double timeMant = CalcDb::frexp10(time, &timeExp) * 10.0;
    int timeCoarse = (int) timeMant;
    int timeFine = round((timeMant - timeCoarse) * 1000.0);
    timeExp -= timeMant == 0 ? 0 : 1;
    qDebug("SpectrumMarkersDialog::displayTime: time: %e fine: %d coarse: %d exp: %d",
        time, timeFine, timeCoarse, timeExp);
    ui->timeFine->setValue(timeFine);
    ui->timeCoarse->setValue(timeCoarse);
    ui->timeExp->setValue(timeExp);
    ui->timeText->setText(tr("%1").arg(timeMant, 0, 'f', 3));
    ui->timeExpText->setText(tr("e%1%2").arg(timeExp < 0 ? "" : "+").arg(timeExp));
}

float SpectrumMarkersDialog::getTime() const
{
    return ((ui->timeFine->value() / 1000.0) + ui->timeCoarse->value()) * pow(10.0, ui->timeExp->value());
}

void SpectrumMarkersDialog::on_markerFrequency_changed(qint64 value)
{
    if (m_histogramMarkers.size() == 0) {
        return;
    }

    m_histogramMarkers[m_histogramMarkerIndex].m_frequency = value;
    emit updateHistogram();
}

void SpectrumMarkersDialog::on_centerFrequency_clicked()
{
    if (m_histogramMarkers.size() == 0) {
        return;
    }

    m_histogramMarkers[m_histogramMarkerIndex].m_frequency = m_centerFrequency;
    displayHistogramMarker();
    emit updateHistogram();
}

void SpectrumMarkersDialog::on_markerColor_clicked()
{
    if (m_histogramMarkers.size() == 0) {
        return;
    }

    QColor newColor = QColorDialog::getColor(
        m_histogramMarkers[m_histogramMarkerIndex].m_markerColor,
        this,
        tr("Select Color for marker"),
        QColorDialog::DontUseNativeDialog
    );

    if (newColor.isValid()) // user clicked OK and selected a color
    {
        m_histogramMarkers[m_histogramMarkerIndex].m_markerColor = newColor;
        displayHistogramMarker();
    }
}

void SpectrumMarkersDialog::on_showMarker_clicked(bool clicked)
{
    if (m_histogramMarkers.size() == 0) {
        return;
    }

    m_histogramMarkers[m_histogramMarkerIndex].m_show = clicked;
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

void SpectrumMarkersDialog::on_setReference_clicked()
{
    if ((m_histogramMarkerIndex == 0) || (m_histogramMarkers.size() < 2)) {
        return;
    }

    SpectrumHistogramMarker marker0 = m_histogramMarkers.at(0);
    QColor color0 = marker0.m_markerColor; // do not exchange colors
    QColor colorI = m_histogramMarkers[m_histogramMarkerIndex].m_markerColor;
    m_histogramMarkers[0] = m_histogramMarkers[m_histogramMarkerIndex];
    m_histogramMarkers[0].m_markerColor = color0;
    m_histogramMarkers[m_histogramMarkerIndex] = marker0;
    m_histogramMarkers[m_histogramMarkerIndex].m_markerColor = colorI;
    displayHistogramMarker();
    emit updateHistogram();
}

void SpectrumMarkersDialog::on_markerAdd_clicked()
{
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

void SpectrumMarkersDialog::on_markerDel_clicked()
{
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

    SpectrumHistogramMarker::SpectrumMarkerType newType = (SpectrumHistogramMarker::SpectrumMarkerType) index;

    if ((m_histogramMarkers[m_histogramMarkerIndex].m_markerType != newType)
       && (newType == SpectrumHistogramMarker::SpectrumMarkerTypePowerMax))
    {
        m_histogramMarkers[m_histogramMarkerIndex].m_holdReset = true;
    }

    m_histogramMarkers[m_histogramMarkerIndex].m_markerType = newType;
}

void SpectrumMarkersDialog::on_powerHoldReset_clicked()
{
    if (m_histogramMarkers.size() == 0) {
        return;
    }

    m_histogramMarkers[m_histogramMarkerIndex].m_holdReset = true;
}

void SpectrumMarkersDialog::on_wMarkerFrequency_changed(qint64 value)
{
    if (m_waterfallMarkers.size() == 0) {
        return;
    }

    m_waterfallMarkers[m_waterfallMarkerIndex].m_frequency = value;
    emit updateWaterfall();
}

void SpectrumMarkersDialog::on_timeCoarse_valueChanged(int value)
{
    double timeMant = value + (ui->timeFine->value() / 1000.0);
    ui->timeText->setText(tr("%1").arg(timeMant, 0, 'f', 3));

    if (m_waterfallMarkers.size() == 0) {
        return;
    }

    m_waterfallMarkers[m_waterfallMarkerIndex].m_time = getTime();
    emit updateWaterfall();
}

void SpectrumMarkersDialog::on_timeFine_valueChanged(int value)
{
    double timeMant = ui->timeCoarse->value() + (value / 1000.0);
    ui->timeText->setText(tr("%1").arg(timeMant, 0, 'f', 3));

    if (m_waterfallMarkers.size() == 0) {
        return;
    }

    m_waterfallMarkers[m_waterfallMarkerIndex].m_time = getTime();
    emit updateWaterfall();
}

void SpectrumMarkersDialog::on_timeExp_valueChanged(int value)
{
    ui->timeExpText->setText(tr("e%1%2").arg(value < 0 ? "" : "+").arg(value));

    if (m_waterfallMarkers.size() == 0) {
        return;
    }

    m_waterfallMarkers[m_waterfallMarkerIndex].m_time = getTime();
    emit updateWaterfall();
}

void SpectrumMarkersDialog::on_wCenterFrequency_clicked()
{
    if (m_waterfallMarkers.size() == 0) {
        return;
    }

    m_waterfallMarkers[m_waterfallMarkerIndex].m_frequency = m_centerFrequency;
    displayWaterfallMarker();
    emit updateWaterfall();
}

void SpectrumMarkersDialog::on_wMarkerColor_clicked()
{
    if (m_waterfallMarkers.size() == 0) {
        return;
    }

    QColor newColor = QColorDialog::getColor(
        m_waterfallMarkers[m_waterfallMarkerIndex].m_markerColor,
        this,
        tr("Select Color for marker"),
        QColorDialog::DontUseNativeDialog
    );

    if (newColor.isValid()) // user clicked OK and selected a color
    {
        m_waterfallMarkers[m_waterfallMarkerIndex].m_markerColor = newColor;
        displayWaterfallMarker();
    }
}

void SpectrumMarkersDialog::on_wShowMarker_clicked(bool clicked)
{
    if (m_waterfallMarkers.size() == 0) {
        return;
    }

    m_waterfallMarkers[m_waterfallMarkerIndex].m_show = clicked;
}

void SpectrumMarkersDialog::on_wMarker_valueChanged(int value)
{
    if (m_waterfallMarkers.size() == 0) {
        return;
    }

    m_waterfallMarkerIndex = value;
    displayWaterfallMarker();
}

void SpectrumMarkersDialog::on_wSetReference_clicked()
{
    if ((m_waterfallMarkerIndex == 0) || (m_waterfallMarkers.size() < 2)) {
        return;
    }

    SpectrumWaterfallMarker marker0 = m_waterfallMarkers.at(0);
    QColor color0 = marker0.m_markerColor; // do not exchange colors
    QColor colorI = m_waterfallMarkers[m_waterfallMarkerIndex].m_markerColor;
    m_waterfallMarkers[0] = m_waterfallMarkers[m_waterfallMarkerIndex];
    m_waterfallMarkers[0].m_markerColor = color0;
    m_waterfallMarkers[m_waterfallMarkerIndex] = marker0;
    m_waterfallMarkers[m_waterfallMarkerIndex].m_markerColor = colorI;
    displayWaterfallMarker();
    emit updateWaterfall();
}

void SpectrumMarkersDialog::on_wMarkerAdd_clicked()
{
    if (m_waterfallMarkers.size() == SpectrumWaterfallMarker::m_maxNbOfMarkers) {
        return;
    }

    m_waterfallMarkers.append(SpectrumWaterfallMarker());
    m_waterfallMarkers.back().m_frequency = m_centerFrequency;
    m_waterfallMarkers.back().m_time = m_time;
    m_waterfallMarkerIndex = m_waterfallMarkers.size() - 1;
    ui->wMarker->setMaximum(m_waterfallMarkers.size() - 1);
    displayWaterfallMarker();
}

void SpectrumMarkersDialog::on_wMarkerDel_clicked()
{
    if (m_waterfallMarkers.size() == 0) {
        return;
    }

    m_waterfallMarkers.removeAt(m_waterfallMarkerIndex);
    m_waterfallMarkerIndex = m_waterfallMarkerIndex < m_waterfallMarkers.size() ?
        m_waterfallMarkerIndex : m_waterfallMarkerIndex - 1;
    ui->wMarker->setMaximum(m_waterfallMarkers.size() - 1);
    displayWaterfallMarker();
}

void SpectrumMarkersDialog::on_showSelect_currentIndexChanged(int index)
{
    m_markersDisplay = (SpectrumSettings::MarkersDisplay) index;
}
