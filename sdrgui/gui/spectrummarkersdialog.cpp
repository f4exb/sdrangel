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

#include <QStandardPaths>
#include <QColorDialog>
#include <QFileDialog>

#include "util/db.h"
#include "util/csv.h"
#include "spectrummarkersdialog.h"

#include "ui_spectrummarkersdialog.h"


SpectrumMarkersDialog::SpectrumMarkersDialog(
    QList<SpectrumHistogramMarker>& histogramMarkers,
    QList<SpectrumWaterfallMarker>& waterfallMarkers,
    QList<SpectrumAnnotationMarker>& annotationMarkers,
    SpectrumSettings::MarkersDisplay& markersDisplay,
    bool& findPeaks,
    float calibrationShiftdB,
    QWidget* parent) :
    QDialog(parent),
    ui(new Ui::SpectrumMarkersDialog),
    m_histogramMarkers(histogramMarkers),
    m_waterfallMarkers(waterfallMarkers),
    m_annotationMarkers(annotationMarkers),
    m_markersDisplay(markersDisplay),
    m_findPeaks(findPeaks),
    m_calibrationShiftdB(calibrationShiftdB),
    m_histogramMarkerIndex(0),
    m_waterfallMarkerIndex(0),
    m_annotationMarkerIndex(0),
    m_centerFrequency(0),
    m_power(0.5f),
    m_annoFreqStartElseCenter(true)
{
    ui->setupUi(this);
    ui->markerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->markerFrequency->setValueRange(false, 12, -999999999999L, 999999999999L);
    ui->marker->setMaximum(m_histogramMarkers.size() - 1);
    ui->wMarkerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->wMarkerFrequency->setValueRange(false, 12, -999999999999L, 999999999999L);
    ui->wMarker->setMaximum(m_waterfallMarkers.size() - 1);
    ui->aMarkerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->aMarkerFrequency->setValueRange(false, 12, -999999999999L, 999999999999L);
    ui->aMarker->setMaximum(m_annotationMarkers.size() - 1);
    ui->aMarkerBandwidth->setColorMapper(ColorMapper::GrayGreenYellow);
    ui->aMarkerBandwidth->setValueRange(true, 9, 0, 999999999L);
    ui->fixedPower->setColorMapper(ColorMapper::GrayYellow);
    ui->fixedPower->setValueRange(false, 4, -2000, 400, 1);
    ui->showSelect->setCurrentIndex((int) m_markersDisplay);
    ui->findPeaks->setChecked(m_findPeaks);
    displayHistogramMarker();
    displayWaterfallMarker();
    displayAnnotationMarker();
}

SpectrumMarkersDialog::~SpectrumMarkersDialog()
{}

void SpectrumMarkersDialog::displayHistogramMarker()
{
    ui->markerFrequency->blockSignals(true);
    ui->centerFrequency->blockSignals(true);
    ui->markerColor->blockSignals(true);
    ui->showMarker->blockSignals(true);
    ui->marker->blockSignals(true);
    ui->powerMode->blockSignals(true);
    ui->fixedPower->blockSignals(true);

    if (m_histogramMarkers.size() == 0)
    {
        ui->marker->setEnabled(false);
        ui->markerFrequency->setEnabled(false);
        ui->powerMode->setEnabled(false);
        ui->fixedPower->setEnabled(false);
        ui->showMarker->setEnabled(false);
        ui->marker->setValue(0);
        ui->markerText->setText("-");
        ui->fixedPower->setValue(0);
    }
    else
    {
        bool disableFreq = m_findPeaks && (
            (m_histogramMarkers[m_histogramMarkerIndex].m_markerType == SpectrumHistogramMarker::SpectrumMarkerTypePower) ||
            (m_histogramMarkers[m_histogramMarkerIndex].m_markerType == SpectrumHistogramMarker::SpectrumMarkerTypePowerMax)
        );
        ui->marker->setEnabled(true);
        ui->markerFrequency->setEnabled(!disableFreq);
        ui->powerMode->setEnabled(true);
        ui->fixedPower->setEnabled(true);
        ui->showMarker->setEnabled(true);
        ui->marker->setValue(m_histogramMarkerIndex);
        ui->markerText->setText(tr("%1").arg(m_histogramMarkerIndex));
        ui->markerFrequency->setValue(m_histogramMarkers[m_histogramMarkerIndex].m_frequency);
        ui->powerMode->setCurrentIndex((int) m_histogramMarkers[m_histogramMarkerIndex].m_markerType);
        float powerDB = CalcDb::dbPower(m_histogramMarkers[m_histogramMarkerIndex].m_power) + m_calibrationShiftdB;
        ui->fixedPower->setValue(powerDB*10);
        int r,g,b,a;
        m_histogramMarkers[m_histogramMarkerIndex].m_markerColor.getRgb(&r, &g, &b, &a);
        ui->markerColor->setStyleSheet(tr("QLabel { background-color : rgb(%1,%2,%3); }").arg(r).arg(g).arg(b));
        ui->showMarker->setChecked(m_histogramMarkers[m_histogramMarkerIndex].m_show);
        ui->fixedPower->setVisible(m_histogramMarkers[m_histogramMarkerIndex].m_markerType == SpectrumHistogramMarker::SpectrumMarkerTypeManual);
        ui->fixedPowerUnits->setVisible(m_histogramMarkers[m_histogramMarkerIndex].m_markerType == SpectrumHistogramMarker::SpectrumMarkerTypeManual);
    }

    ui->markerFrequency->blockSignals(false);
    ui->centerFrequency->blockSignals(false);
    ui->markerColor->blockSignals(false);
    ui->showMarker->blockSignals(false);
    ui->marker->blockSignals(false);
    ui->powerMode->blockSignals(false);
    ui->fixedPower->blockSignals(false);
}

void SpectrumMarkersDialog::displayWaterfallMarker()
{
    ui->wMarkerFrequency->blockSignals(true);
    ui->wCenterFrequency->blockSignals(true);
    ui->wMarkerColor->blockSignals(true);
    ui->wShowMarker->blockSignals(true);
    ui->wMarker->blockSignals(true);
    ui->timeFine->blockSignals(true);
    ui->timeCoarse->blockSignals(true);
    ui->timeExp->blockSignals(true);

    if (m_waterfallMarkers.size() == 0)
    {
        ui->wMarker->setEnabled(false);
        ui->wMarkerFrequency->setEnabled(false);
        ui->timeCoarse->setEnabled(false);
        ui->timeFine->setEnabled(false);
        ui->timeExp->setEnabled(false);
        ui->wShowMarker->setEnabled(false);
        ui->wMarker->setValue(0);
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
        ui->wMarker->setValue(m_waterfallMarkerIndex);
        ui->wMarkerText->setText(tr("%1").arg(m_waterfallMarkerIndex));
        ui->wMarkerFrequency->setValue(m_waterfallMarkers[m_waterfallMarkerIndex].m_frequency);
        int r,g,b,a;
        m_waterfallMarkers[m_waterfallMarkerIndex].m_markerColor.getRgb(&r, &g, &b, &a);
        ui->wMarkerColor->setStyleSheet(tr("QLabel { background-color : rgb(%1,%2,%3); }").arg(r).arg(g).arg(b));
        displayTime(m_waterfallMarkers[m_waterfallMarkerIndex].m_time);
    }

    ui->wMarkerFrequency->blockSignals(false);
    ui->wCenterFrequency->blockSignals(false);
    ui->wMarkerColor->blockSignals(false);
    ui->wShowMarker->blockSignals(false);
    ui->wMarker->blockSignals(false);
    ui->timeFine->blockSignals(false);
    ui->timeCoarse->blockSignals(false);
    ui->timeExp->blockSignals(false);
}

void SpectrumMarkersDialog::displayAnnotationMarker()
{
    ui->aMarkerFrequency->blockSignals(true);
    ui->aCenterFrequency->blockSignals(true);
    ui->aMarkerColor->blockSignals(true);
    ui->aMarkerShowState->blockSignals(true);
    ui->aMarkerText->blockSignals(true);
    ui->aMarker->blockSignals(true);
    ui->aMarkerAdd->blockSignals(true);
    ui->aMarkerDel->blockSignals(true);
    ui->aMarkerBandwidth->blockSignals(true);
    ui->aMarkerToggleFrequency->blockSignals(true);

    if (m_annotationMarkers.size() == 0)
    {
        ui->aMarker->setEnabled(false);
        ui->aMarkerFrequency->setEnabled(false);
        ui->aMarkerBandwidth->setEnabled(false);
        ui->aMarkerShowState->setEnabled(false);
        ui->aMarkerIndexText->setText("-");
        ui->aMarkerText->setText("");
    }
    else
    {
        ui->aMarker->setEnabled(true);
        ui->aMarkerFrequency->setEnabled(true);
        ui->aMarkerBandwidth->setEnabled(true);
        ui->aMarkerShowState->setEnabled(true);
        ui->aMarker->setValue(m_annotationMarkerIndex);
        ui->aMarkerIndexText->setText(tr("%1").arg(m_annotationMarkerIndex));
        qint64 frequency = m_annotationMarkers[m_annotationMarkerIndex].m_startFrequency +
            (m_annoFreqStartElseCenter ? 0 : m_annotationMarkers[m_annotationMarkerIndex].m_bandwidth / 2);
        ui->aMarkerFrequency->setValue(frequency);
        ui->aMarkerBandwidth->setValue(m_annotationMarkers[m_annotationMarkerIndex].m_bandwidth);
        ui->aMarkerFreqLabel->setText(m_annoFreqStartElseCenter ? "Cent" : "Start");
        frequency = m_annotationMarkers[m_annotationMarkerIndex].m_startFrequency +
            (m_annoFreqStartElseCenter ? m_annotationMarkers[m_annotationMarkerIndex].m_bandwidth / 2 : 0);
        ui->aMarkerFreqText->setText(tr("%L1").arg(frequency));
        ui->aMarkerStopText->setText(tr("%L1").arg(
            m_annotationMarkers[m_annotationMarkerIndex].m_startFrequency + m_annotationMarkers[m_annotationMarkerIndex].m_bandwidth));
        int r,g,b,a;
        m_annotationMarkers[m_annotationMarkerIndex].m_markerColor.getRgb(&r, &g, &b, &a);
        ui->aMarkerColor->setStyleSheet(tr("QLabel { background-color : rgb(%1,%2,%3); }").arg(r).arg(g).arg(b));
        ui->aMarkerText->setText(tr("%1").arg(m_annotationMarkers[m_annotationMarkerIndex].m_text));
        ui->aMarkerShowState->setCurrentIndex((int) m_annotationMarkers[m_annotationMarkerIndex].m_show);
    }

    ui->aMarkerToggleFrequency->setChecked(m_annoFreqStartElseCenter);

    ui->aMarkerFrequency->blockSignals(false);
    ui->aCenterFrequency->blockSignals(false);
    ui->aMarkerColor->blockSignals(false);
    ui->aMarkerShowState->blockSignals(false);
    ui->aMarkerText->blockSignals(false);
    ui->aMarker->blockSignals(false);
    ui->aMarkerAdd->blockSignals(false);
    ui->aMarkerDel->blockSignals(false);
    ui->aMarkerBandwidth->blockSignals(false);
    ui->aMarkerToggleFrequency->blockSignals(false);
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

void SpectrumMarkersDialog::on_fixedPower_changed(qint64 value)
{
    if (m_histogramMarkers.size() == 0) {
        return;
    }

    float powerDB = (value / 10.0f) - m_calibrationShiftdB;
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
    ui->marker->setMinimum(0);
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

    ui->fixedPower->setVisible(newType == SpectrumHistogramMarker::SpectrumMarkerTypeManual);
    ui->fixedPowerUnits->setVisible(newType == SpectrumHistogramMarker::SpectrumMarkerTypeManual);

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

void SpectrumMarkersDialog::on_findPeaks_toggled(bool checked)
{
    m_findPeaks = checked;
    displayHistogramMarker();
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
    ui->wMarker->setMinimum(0);
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

void SpectrumMarkersDialog::on_aMarkerToggleFrequency_toggled(bool checked)
{
    m_annoFreqStartElseCenter = checked;
    ui->aMarkerToggleFrequency->setText(checked ? "Start" : "Cent");
    displayAnnotationMarker();
}

void SpectrumMarkersDialog::on_aMarkerFrequency_changed(qint64 value)
{
    if (m_annotationMarkers.size() == 0) {
        return;
    }

    if (m_annoFreqStartElseCenter) {
        m_annotationMarkers[m_annotationMarkerIndex].m_startFrequency = value;
    } else {
        m_annotationMarkers[m_annotationMarkerIndex].m_startFrequency = value -
            (m_annotationMarkers[m_annotationMarkerIndex].m_bandwidth / 2);
    }

    displayAnnotationMarker();
    emit updateAnnotations();
}

void SpectrumMarkersDialog::on_aCenterFrequency_clicked()
{
    if (m_annotationMarkers.size() == 0) {
        return;
    }

    qDebug("SpectrumMarkersDialog::on_aCenterFrequency_clicked: %lld", m_centerFrequency);
    m_annotationMarkers[m_annotationMarkerIndex].m_startFrequency = m_centerFrequency -
        (m_annoFreqStartElseCenter ? 0 : m_annotationMarkers[m_annotationMarkerIndex].m_bandwidth/2);
    displayAnnotationMarker();
    emit updateAnnotations();
}

void SpectrumMarkersDialog::on_aMakerDuplicate_clicked()
{
    if (m_annotationMarkers.size() == 0) {
        return;
    }

    m_annotationMarkers.push_back(m_annotationMarkers[m_annotationMarkerIndex]);
    ui->aMarker->setMaximum(m_annotationMarkers.size() - 1);
    m_annotationMarkerIndex = m_annotationMarkers.size() - 1;
    displayAnnotationMarker();
    emit updateAnnotations();
}

void SpectrumMarkersDialog::on_aMakersSort_clicked()
{
    std::sort(m_annotationMarkers.begin(), m_annotationMarkers.end(), annotationMarkerLessThan);
    displayAnnotationMarker();
    emit updateAnnotations();
}

void SpectrumMarkersDialog::on_aMarkerColor_clicked()
{
    if (m_annotationMarkers.size() == 0) {
        return;
    }

    QColor newColor = QColorDialog::getColor(
        m_annotationMarkers[m_annotationMarkerIndex].m_markerColor,
        this,
        tr("Select Color for marker"),
        QColorDialog::DontUseNativeDialog
    );

    if (newColor.isValid()) // user clicked OK and selected a color
    {
        m_annotationMarkers[m_annotationMarkerIndex].m_markerColor = newColor;
        displayAnnotationMarker();
    }
}

void SpectrumMarkersDialog::on_aMarkerShowState_currentIndexChanged(int state)
{
    if (m_annotationMarkers.size() == 0) {
        return;
    }

    m_annotationMarkers[m_annotationMarkerIndex].m_show = (SpectrumAnnotationMarker::ShowState) state;
}

void SpectrumMarkersDialog::on_aMarkerShowStateAll_clicked()
{
    for (auto &marker : m_annotationMarkers) {
        marker.m_show = (SpectrumAnnotationMarker::ShowState) ui->aMarkerShowState->currentIndex();
    }
}

// We don't get aMarkerText_editingFinished signal until after aMarkerToggleFrequency_toggled
// when that button is pressed, so save the text immediately when edited, so it isn't lost
// after call to displayAnnotationMarker
void SpectrumMarkersDialog::on_aMarkerText_textEdited()
{
    if (m_annotationMarkers.size() == 0) {
        return;
    }

    m_annotationMarkers[m_annotationMarkerIndex].m_text = ui->aMarkerText->text();
}

void SpectrumMarkersDialog::on_aMarkerText_editingFinished()
{
    if (m_annotationMarkers.size() == 0) {
        return;
    }

    m_annotationMarkers[m_annotationMarkerIndex].m_text = ui->aMarkerText->text();
    emit updateAnnotations();
}

void SpectrumMarkersDialog::on_aMarker_valueChanged(int value)
{
    if (m_annotationMarkers.size() == 0) {
        return;
    }

    m_annotationMarkerIndex = value;
    displayAnnotationMarker();
}

void SpectrumMarkersDialog::on_aMarkerAdd_clicked()
{
    m_annotationMarkers.append(SpectrumAnnotationMarker());
    m_annotationMarkers.back().m_startFrequency = m_centerFrequency;
    m_annotationMarkerIndex = m_annotationMarkers.size() - 1;
    ui->aMarker->setMaximum(m_annotationMarkers.size() - 1);
    ui->aMarker->setMinimum(0);
    displayAnnotationMarker();
    emit updateAnnotations();
}

void SpectrumMarkersDialog::on_aMarkerDel_clicked()
{
    if (m_annotationMarkers.size() == 0) {
        return;
    }

    m_annotationMarkers.removeAt(m_annotationMarkerIndex);
    m_annotationMarkerIndex = m_annotationMarkerIndex < m_annotationMarkers.size() ?
        m_annotationMarkerIndex : m_annotationMarkerIndex - 1;
    ui->aMarker->setMaximum(m_annotationMarkers.size() - 1);
    displayAnnotationMarker();
    emit updateAnnotations();
}

void SpectrumMarkersDialog::on_aMarkerBandwidth_changed(qint64 value)
{
    if (m_annotationMarkers.size() == 0) {
        return;
    }

    m_annotationMarkers[m_annotationMarkerIndex].m_bandwidth = value < 0 ? 0 : value;

    if (!m_annoFreqStartElseCenter)
    {
        m_annotationMarkers[m_annotationMarkerIndex].m_startFrequency = ui->aMarkerFrequency->getValue() -
            (m_annotationMarkers[m_annotationMarkerIndex].m_bandwidth / 2);
    }

    displayAnnotationMarker();
    emit updateAnnotations();
}

void SpectrumMarkersDialog::on_aMarkersImport_clicked()
{
    QFileDialog fileDialog(
        nullptr,
        "Select .csv annotation markers file to read",
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation),
        "*.csv"
    );

    if (fileDialog.exec())
    {
        QStringList fileNames = fileDialog.selectedFiles();

        if (fileNames.size() > 0)
        {
            QFile file(fileNames[0]);

            if (file.open(QIODevice::ReadOnly | QIODevice::Text))
            {

                QTextStream in(&file);
                QString error;
                QHash<QString, int> colIndexes = CSV::readHeader(
                    in,
                    {"Start", "Width", "Text", "Show", "Red", "Green", "Blue"},
                    error
                );

                if (error.isEmpty())
                {
                    QStringList cols;
                    int startCol = colIndexes.value("Start");
                    int widthCol = colIndexes.value("Width");
                    int textCol = colIndexes.value("Text");
                    int showCol = colIndexes.value("Show");
                    int redCol = colIndexes.value("Red");
                    int greenCol = colIndexes.value("Green");
                    int blueCol = colIndexes.value("Blue");

                    m_annotationMarkers.clear();

                    while (CSV::readRow(in, &cols))
                    {
                        m_annotationMarkers.push_back(SpectrumAnnotationMarker());
                        m_annotationMarkers.back().m_startFrequency = cols[startCol].toLongLong();
                        m_annotationMarkers.back().m_bandwidth = cols[widthCol].toUInt();
                        m_annotationMarkers.back().m_text = cols[textCol];
                        m_annotationMarkers.back().m_show = (SpectrumAnnotationMarker::ShowState) cols[showCol].toInt();
                        int r = cols[redCol].toInt();
                        int g = cols[greenCol].toInt();
                        int b = cols[blueCol].toInt();
                        m_annotationMarkers.back().m_markerColor = QColor(r, g, b);
                    }

                    m_annotationMarkerIndex = 0;
                    ui->aMarker->setMaximum(m_annotationMarkers.size() - 1);
                    displayAnnotationMarker();
                    emit updateAnnotations();
                }
            }
        }
    }
}

void SpectrumMarkersDialog::on_aMarkersExport_clicked()
{
    QFileDialog fileDialog(
        nullptr,
        "Select file to write annotation markers to",
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation),
        "*.csv"
    );
    fileDialog.setAcceptMode(QFileDialog::AcceptSave);

    if (fileDialog.exec())
    {
        QStringList fileNames = fileDialog.selectedFiles();

        if (fileNames.size() > 0)
        {
            QFile file(fileNames[0]);

            if (file.open(QIODevice::WriteOnly | QIODevice::Text))
            {
                QTextStream stream;
                stream.setDevice(&file);
                stream << "Start,Width,Text,Show,Red,Green,Blue\n";

                for (const auto &marker : m_annotationMarkers)
                {
                    stream << marker.m_startFrequency << ","
                        << marker.m_bandwidth << ","
                        << marker.m_text << ","
                        << ((int)marker.m_show) << ","
                        << marker.m_markerColor.red() << ","
                        << marker.m_markerColor.green() << ","
                        << marker.m_markerColor.blue() << "\n";
                }

                stream.flush();
                file.close();
            }
        }
    }
}

void SpectrumMarkersDialog::on_showSelect_currentIndexChanged(int index)
{
    m_markersDisplay = (SpectrumSettings::MarkersDisplay) index;
    emit updateMarkersDisplay();
}

void SpectrumMarkersDialog::updateHistogramMarkersDisplay()
{
    m_histogramMarkerIndex = std::max(m_histogramMarkerIndex, (int)m_histogramMarkers.size() - 1);
    ui->marker->setMaximum(m_histogramMarkers.size() - 1);
    displayHistogramMarker();
}

void SpectrumMarkersDialog::updateWaterfallMarkersDisplay()
{
    m_waterfallMarkerIndex = std::max(m_waterfallMarkerIndex, (int)m_waterfallMarkers.size() - 1);
    ui->wMarker->setMaximum(m_waterfallMarkers.size() - 1);
    displayWaterfallMarker();
}
