///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#include <limits>
#include <numeric>
#include <algorithm>
#include <functional>
#include <ctype.h>
#include <QDockWidget>
#include <QMainWindow>
#include <QDebug>
#include <QMessageBox>
#include <QAction>
#include <QRegExp>
#include <QClipboard>
#include <QFileDialog>
#include <QImage>
#include <QTimer>

#include "radioastronomygui.h"

#include "device/deviceuiset.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "ui_radioastronomygui.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "util/astronomy.h"
#include "util/interpolation.h"
#include "util/png.h"
#include "util/units.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "dsp/dspengine.h"
#include "gui/crightclickenabler.h"
#include "gui/timedelegate.h"
#include "gui/decimaldelegate.h"
#include "gui/dialogpositioner.h"
#include "channel/channelwebapiutils.h"
#include "maincore.h"
#include "feature/featurewebapiutils.h"
#include "feature/feature.h"
#include "feature/featureset.h"

#include "radioastronomy.h"
#include "radioastronomysink.h"
#include "radioastronomysensordialog.h"
#include "radioastronomycalibrationdialog.h"

#include "SWGMapItem.h"
#include "SWGStarTrackerTarget.h"
#include "SWGStarTrackerDisplaySettings.h"
#include "SWGStarTrackerDisplayLoSSettings.h"

// Time value is in milliseconds - Displays hh:mm:ss or d hh:mm:ss
class TimeDeltaDelegate : public QStyledItemDelegate {

public:
    virtual QString displayText(const QVariant &value, const QLocale &locale) const override
    {
        (void) locale;
        qint64 v = value.toLongLong(); // In milliseconds
        bool neg = v < 0;
        v = abs(v);
        qint64 days = v / (1000*60*60*24);
        v = v % (1000*60*60*24);
        qint64 hours = v / (1000*60*60);
        v = v % (1000*60*60);
        qint64 minutes = v / (1000*60);
        v = v % (1000*60);
        qint64 seconds = v / (1000);
        //qint64 msec = v % 1000;

        if (days > 0) {
            return QString("%1%2 %3:%4:%5").arg(neg ? "-" : "").arg(days).arg(hours, 2, 10, QChar('0')).arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
        } else {
            return QString("%1%2:%3:%4").arg(neg ? "-" : "").arg(hours, 2, 10, QChar('0')).arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
        }
    }

private:
    QString m_format;

};

// Delegate for table to display hours, minutes and seconds
class HMSDelegate : public QStyledItemDelegate {

public:
    virtual QString displayText(const QVariant &value, const QLocale &locale) const override
    {
        (void) locale;
        return Units::decimalHoursToHoursMinutesAndSeconds(value.toDouble());
    }

};

// Delegate for table to display degrees, minutes and seconds
class DMSDelegate : public QStyledItemDelegate {

public:
    virtual QString displayText(const QVariant &value, const QLocale &locale) const override
    {
        (void) locale;
        return Units::decimalDegreesToDegreeMinutesAndSeconds(value.toDouble());
    }

};

void RadioAstronomyGUI::LABData::read(QFile* file, float l, float b)
{
    m_l = l;
    m_b = b;
    m_vlsr.clear();
    m_temp.clear();
    QTextStream in(file);

    while (!in.atEnd())
    {
        QString line = in.readLine().trimmed();
        if (!line.startsWith("%") && (line.size() > 0))  // Lines starting with % are comments
        {
            // 4 cols: v_lsr [km/s], T_B [K], freq. [Mhz], wavel. [cm]
            line = line.simplified();
            QStringList cols = line.split(" ");
            if (cols.size() == 4)
            {
                m_vlsr.append(cols[0].toFloat());
                m_temp.append(cols[1].toFloat());
            }
            else
            {
                qDebug() << "RadioAstronomyGUI::parseLAB: Unexpected number of columns";
            }
        }
    }
}

void RadioAstronomyGUI::LABData::toSeries(QLineSeries *series)
{
    series->clear();
    series->setName(QString("LAB l=%1 b=%2").arg(m_l).arg(m_b));
    for (int i = 0; i < m_vlsr.size(); i++) {
        series->append(m_vlsr[i], m_temp[i]);
    }
}

void RadioAstronomyGUI::SensorMeasurements::init(const QString& name, bool visible)
{
    m_series = new QLineSeries();
    m_series->setName(name);
    m_series->setVisible(visible);
    m_yAxis = new QValueAxis();
    m_yAxis->setTitleText(name);
    m_yAxis->setVisible(visible);
    m_min = std::numeric_limits<double>::max();
    m_max = -std::numeric_limits<double>::max();
}

void RadioAstronomyGUI::SensorMeasurements::setName(const QString& name)
{
    if (m_series) {
        m_series->setName(name);
    }
    if (m_yAxis) {
       m_yAxis->setTitleText(name);
    }
}

void RadioAstronomyGUI::SensorMeasurements::clicked(bool checked)
{
    if (m_series) {
        m_series->setVisible(checked);
    }
    if (m_yAxis) {
        m_yAxis->setVisible(checked);
    }
}

void RadioAstronomyGUI::SensorMeasurements::append(SensorMeasurement *measurement)
{
    m_measurements.append(measurement);
    addToSeries(measurement);
}

void RadioAstronomyGUI::SensorMeasurements::addToSeries(SensorMeasurement *measurement)
{
    m_series->append(measurement->m_dateTime.toMSecsSinceEpoch(), measurement->m_value);

    m_max = std::max(m_max, measurement->m_value);
    m_min = std::min(m_min, measurement->m_value);
    if (m_min == m_max) {
        // Axis isn't drawn properly if min and max are the same
        m_yAxis->setRange(m_min*0.9, m_max*1.1);
    } else {
        m_yAxis->setRange(m_min, m_max);
    }
}

void RadioAstronomyGUI::SensorMeasurements::addAllToSeries()
{
    for (int i = 0; i < m_measurements.size(); i++) {
        addToSeries(m_measurements[i]);
    }
}

void RadioAstronomyGUI::SensorMeasurements::clear()
{
    m_series->clear();
    qDeleteAll(m_measurements);
    m_measurements.clear();
}

void RadioAstronomyGUI::SensorMeasurements::addToChart(QChart* chart, QDateTimeAxis* xAxis)
{
    chart->addSeries(m_series);
    m_series->attachAxis(xAxis);
    m_series->attachAxis(m_yAxis);
}

void RadioAstronomyGUI::SensorMeasurements::setPen(const QPen& pen)
{
    m_series->setPen(pen);
}

QValueAxis* RadioAstronomyGUI::SensorMeasurements::yAxis() const
{
    return m_yAxis;
}

qreal RadioAstronomyGUI::SensorMeasurements::lastValue()
{
    if (m_measurements.size() > 0) {
        return m_measurements.last()->m_value;
    } else {
        return 0.0;
    }
}

void RadioAstronomyGUI::resizePowerTable()
{
    // Fill table with a row of dummy data that will size the columns nicely
    // Trailing spaces are for sort arrow
    int row = ui->powerTable->rowCount();
    ui->powerTable->setRowCount(row + 1);
    ui->powerTable->setItem(row, POWER_COL_DATE, new QTableWidgetItem("15/04/2016"));
    ui->powerTable->setItem(row, POWER_COL_TIME, new QTableWidgetItem("10:17:00"));
    ui->powerTable->setItem(row, POWER_COL_POWER, new QTableWidgetItem("1.235-e5"));
    ui->powerTable->setItem(row, POWER_COL_POWER_DB, new QTableWidgetItem("-100.0"));
    ui->powerTable->setItem(row, POWER_COL_POWER_DBM, new QTableWidgetItem("-100.0"));
    ui->powerTable->setItem(row, POWER_COL_TSYS, new QTableWidgetItem("3000"));
    ui->powerTable->setItem(row, POWER_COL_TSYS0, new QTableWidgetItem("100"));
    ui->powerTable->setItem(row, POWER_COL_TSOURCE, new QTableWidgetItem("300"));
    ui->powerTable->setItem(row, POWER_COL_TB, new QTableWidgetItem("100000"));
    ui->powerTable->setItem(row, POWER_COL_TSKY, new QTableWidgetItem("300"));
    ui->powerTable->setItem(row, POWER_COL_FLUX, new QTableWidgetItem("100000.00"));
    ui->powerTable->setItem(row, POWER_COL_SIGMA_T, new QTableWidgetItem("0.01"));
    ui->powerTable->setItem(row, POWER_COL_SIGMA_S, new QTableWidgetItem("1000.0"));
    ui->powerTable->setItem(row, POWER_COL_OMEGA_A, new QTableWidgetItem("0.000001"));
    ui->powerTable->setItem(row, POWER_COL_OMEGA_S, new QTableWidgetItem("0.000001"));
    ui->powerTable->setItem(row, POWER_COL_RA, new QTableWidgetItem("12h59m59.10s"));
    ui->powerTable->setItem(row, POWER_COL_DEC, new QTableWidgetItem("-90d59\'59.00\""));
    ui->powerTable->setItem(row, POWER_COL_GAL_LAT, new QTableWidgetItem("-90.0"));
    ui->powerTable->setItem(row, POWER_COL_GAL_LON, new QTableWidgetItem("359.0"));
    ui->powerTable->setItem(row, POWER_COL_AZ, new QTableWidgetItem("359.0"));
    ui->powerTable->setItem(row, POWER_COL_EL, new QTableWidgetItem("90.0"));
    ui->powerTable->setItem(row, POWER_COL_VBCRS, new QTableWidgetItem("10.0"));
    ui->powerTable->setItem(row, POWER_COL_VLSR, new QTableWidgetItem("10.0"));
    ui->powerTable->setItem(row, POWER_COL_SOLAR_FLUX, new QTableWidgetItem("60.0"));
    ui->powerTable->setItem(row, POWER_COL_AIR_TEMP, new QTableWidgetItem("20.0"));
    ui->powerTable->setItem(row, POWER_COL_SENSOR_1, new QTableWidgetItem("1.0000000"));
    ui->powerTable->setItem(row, POWER_COL_SENSOR_2, new QTableWidgetItem("1.0000000"));
    ui->powerTable->setItem(row, POWER_COL_UTC, new QTableWidgetItem("15/04/2016 10:17:00"));
    ui->powerTable->resizeColumnsToContents();
    ui->powerTable->removeRow(row);
}

void RadioAstronomyGUI::resizePowerMarkerTable()
{
    // Fill table with a row of dummy data that will size the columns nicely
    int row = ui->powerMarkerTable->rowCount();
    ui->powerMarkerTable->setRowCount(row + 1);
    ui->powerMarkerTable->setItem(row, POWER_MARKER_COL_NAME, new QTableWidgetItem("Max"));
    ui->powerMarkerTable->setItem(row, POWER_MARKER_COL_DATE, new QTableWidgetItem("15/04/2016"));
    ui->powerMarkerTable->setItem(row, POWER_MARKER_COL_TIME, new QTableWidgetItem("10:17:00"));
    ui->powerMarkerTable->setItem(row, POWER_MARKER_COL_VALUE, new QTableWidgetItem("1000.0"));
    ui->powerMarkerTable->setItem(row, POWER_MARKER_COL_DELTA_X, new QTableWidgetItem("1 23:59:59"));
    ui->powerMarkerTable->setItem(row, POWER_MARKER_COL_DELTA_Y, new QTableWidgetItem("1000.0"));
    ui->powerMarkerTable->setItem(row, POWER_MARKER_COL_DELTA_TO, new QTableWidgetItem("Max"));
    ui->powerMarkerTable->resizeColumnsToContents();
    ui->powerMarkerTable->removeRow(row);
}

void RadioAstronomyGUI::resizeSpectrumMarkerTable()
{
    // Fill table with a row of dummy data that will size the columns nicely
    int row = ui->spectrumMarkerTable->rowCount();
    ui->spectrumMarkerTable->setRowCount(row + 1);
    ui->spectrumMarkerTable->setItem(row, SPECTRUM_MARKER_COL_NAME, new QTableWidgetItem("Max"));
    ui->spectrumMarkerTable->setItem(row, SPECTRUM_MARKER_COL_FREQ, new QTableWidgetItem("1420.405000"));
    ui->spectrumMarkerTable->setItem(row, SPECTRUM_MARKER_COL_VALUE, new QTableWidgetItem("1000.0"));
    ui->spectrumMarkerTable->setItem(row, SPECTRUM_MARKER_COL_DELTA_X, new QTableWidgetItem("1420.405000"));
    ui->spectrumMarkerTable->setItem(row, SPECTRUM_MARKER_COL_DELTA_Y, new QTableWidgetItem("1000.0"));
    ui->spectrumMarkerTable->setItem(row, SPECTRUM_MARKER_COL_DELTA_TO, new QTableWidgetItem("M1"));
    ui->spectrumMarkerTable->setItem(row, SPECTRUM_MARKER_COL_VR, new QTableWidgetItem("-100.0"));
    ui->spectrumMarkerTable->setItem(row, SPECTRUM_MARKER_COL_R, new QTableWidgetItem("10.0"));
    ui->spectrumMarkerTable->setItem(row, SPECTRUM_MARKER_COL_D, new QTableWidgetItem("10.0/10.0"));
    QTableWidgetItem* check = new QTableWidgetItem();
    check->setFlags(Qt::ItemIsUserCheckable);
    ui->spectrumMarkerTable->setItem(row, SPECTRUM_MARKER_COL_PLOT_MAX, check);
    ui->spectrumMarkerTable->setItem(row, SPECTRUM_MARKER_COL_R_MIN, new QTableWidgetItem("10.0"));
    ui->spectrumMarkerTable->setItem(row, SPECTRUM_MARKER_COL_V, new QTableWidgetItem("250.0"));
    ui->spectrumMarkerTable->resizeColumnsToContents();
    ui->spectrumMarkerTable->removeRow(row);
}

void RadioAstronomyGUI::calcSpectrumMarkerDelta()
{
    if (m_spectrumM1Valid && m_spectrumM2Valid)
    {
        qreal dx = m_spectrumM2X - m_spectrumM1X;
        qreal dy = m_spectrumM2Y - m_spectrumM1Y;

        ui->spectrumMarkerTable->item(SPECTRUM_MARKER_ROW_M2, SPECTRUM_MARKER_COL_DELTA_X)->setData(Qt::DisplayRole, dx);
        ui->spectrumMarkerTable->item(SPECTRUM_MARKER_ROW_M2, SPECTRUM_MARKER_COL_DELTA_Y)->setData(Qt::DisplayRole, dy);
        ui->spectrumMarkerTable->item(SPECTRUM_MARKER_ROW_M2, SPECTRUM_MARKER_COL_DELTA_TO)->setData(Qt::DisplayRole, "M1");
    }
}

void RadioAstronomyGUI::calcPowerMarkerDelta()
{
    if (m_powerM1Valid && m_powerM2Valid)
    {
        qreal dx = m_powerM2X - m_powerM1X;
        qreal dy = m_powerM2Y - m_powerM1Y;

        ui->powerMarkerTable->item(POWER_MARKER_ROW_M2, POWER_MARKER_COL_DELTA_X)->setData(Qt::DisplayRole, dx);
        ui->powerMarkerTable->item(POWER_MARKER_ROW_M2, POWER_MARKER_COL_DELTA_Y)->setData(Qt::DisplayRole, dy);
        ui->powerMarkerTable->item(POWER_MARKER_ROW_M2, POWER_MARKER_COL_DELTA_TO)->setData(Qt::DisplayRole, "M1");
    }
}

void RadioAstronomyGUI::calcPowerPeakDelta()
{
    qreal dx = m_powerMaxX - m_powerMinX;
    qreal dy = m_powerMaxY - m_powerMinY;
    ui->powerMarkerTable->item(POWER_MARKER_ROW_PEAK_MIN, POWER_MARKER_COL_DELTA_X)->setData(Qt::DisplayRole, dx);
    ui->powerMarkerTable->item(POWER_MARKER_ROW_PEAK_MIN, POWER_MARKER_COL_DELTA_Y)->setData(Qt::DisplayRole, dy);
    ui->powerMarkerTable->item(POWER_MARKER_ROW_PEAK_MIN, POWER_MARKER_COL_DELTA_TO)->setData(Qt::DisplayRole, "Max");
}

void RadioAstronomyGUI::addToPowerSeries(FFTMeasurement *fft, bool skipCalcs)
{
    if (   ((m_settings.m_powerYUnits == RadioAstronomySettings::PY_DBFS) || fft->m_temp)                 // Only dBFS valid if no temp was calculated
        && !((m_settings.m_powerYUnits == RadioAstronomySettings::PY_DBM) && (fft->m_tSys == 0.0f))  // dBm value not valid if temp is 0
       )
    {
        qreal power;
        switch (m_settings.m_powerYData)
        {
        case RadioAstronomySettings::PY_POWER:
            switch (m_settings.m_powerYUnits)
            {
            case RadioAstronomySettings::PY_DBFS:
                power = fft->m_totalPowerdBFS;
                break;
            case RadioAstronomySettings::PY_DBM:
                power = fft->m_totalPowerdBm;
                break;
            case RadioAstronomySettings::PY_WATTS:
                power = fft->m_totalPowerWatts;
                break;
            default:
                break;
            }
            break;
        case RadioAstronomySettings::PY_TSYS:
            power = fft->m_tSys;
            break;
        case RadioAstronomySettings::PY_TSOURCE:
            power = fft->m_tSource;
            break;
        case RadioAstronomySettings::PY_FLUX:
            switch (m_settings.m_powerYUnits)
            {
            case RadioAstronomySettings::PY_SFU:
                power = Units::wattsPerMetrePerHertzToSolarFluxUnits(fft->m_flux);
                break;
            case RadioAstronomySettings::PY_JANSKY:
                power = Units::wattsPerMetrePerHertzToJansky(fft->m_flux);
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }
        QDateTime dateTime = fft->m_dateTime;

        if (m_powerSeries->count() == 0)
        {
            m_powerMin = power;
            m_powerMax = power;
        }
        else
        {
            m_powerMin = std::min(power, m_powerMin);
            m_powerMax = std::max(power, m_powerMax);
        }
        m_powerSeries->append(dateTime.toMSecsSinceEpoch(), power);
        addToPowerFilter(dateTime.toMSecsSinceEpoch(), power);
        if (!skipCalcs)
        {
            if (m_settings.m_powerAutoscale)
            {
                blockApplySettings(true);
                powerAutoscaleY(false);
                blockApplySettings(false);
            }
        }

        if (m_settings.m_powerYUnits == RadioAstronomySettings::PY_KELVIN) {
            m_powerTsys0Series->append(dateTime.toMSecsSinceEpoch(), fft->m_tSys0);
        } else if (m_settings.m_powerYUnits == RadioAstronomySettings::PY_DBM) {
            m_powerTsys0Series->append(dateTime.toMSecsSinceEpoch(), Astronomy::noisePowerdBm(fft->m_tSys0, fft->m_sampleRate));
        } else if (m_settings.m_powerYUnits == RadioAstronomySettings::PY_WATTS) {
            m_powerTsys0Series->append(dateTime.toMSecsSinceEpoch(), Astronomy::m_boltzmann * fft->m_tSys0 * fft->m_sampleRate);
        }

        if (!m_powerPeakValid)
        {
            m_powerPeakValid = true;
            m_powerMinY = power;
            m_powerMinX = dateTime.toMSecsSinceEpoch();
            m_powerMaxY = power;
            m_powerMaxX = dateTime.toMSecsSinceEpoch();
            m_powerPeakSeries->clear();
            m_powerPeakSeries->append(m_powerMaxX, m_powerMaxY);
            m_powerPeakSeries->append(m_powerMaxX, m_powerMaxY);
            QDateTime dt = QDateTime::fromMSecsSinceEpoch(m_powerMaxX);
            if (!skipCalcs)
            {
                ui->powerMarkerTable->item(POWER_MARKER_ROW_PEAK_MAX, POWER_MARKER_COL_DATE)->setData(Qt::DisplayRole, dt.date());
                ui->powerMarkerTable->item(POWER_MARKER_ROW_PEAK_MAX, POWER_MARKER_COL_TIME)->setData(Qt::DisplayRole, dt.time());
                ui->powerMarkerTable->item(POWER_MARKER_ROW_PEAK_MAX, POWER_MARKER_COL_VALUE)->setData(Qt::DisplayRole, m_powerMaxY);
                ui->powerMarkerTable->item(POWER_MARKER_ROW_PEAK_MIN, POWER_MARKER_COL_DATE)->setData(Qt::DisplayRole, dt.date());
                ui->powerMarkerTable->item(POWER_MARKER_ROW_PEAK_MIN, POWER_MARKER_COL_TIME)->setData(Qt::DisplayRole, dt.time());
                ui->powerMarkerTable->item(POWER_MARKER_ROW_PEAK_MIN, POWER_MARKER_COL_VALUE)->setData(Qt::DisplayRole, m_powerMinY);
                ui->powerMarkerTable->item(POWER_MARKER_ROW_PEAK_MIN, POWER_MARKER_COL_DELTA_X)->setData(Qt::DisplayRole, 0.0);
                ui->powerMarkerTable->item(POWER_MARKER_ROW_PEAK_MIN, POWER_MARKER_COL_DELTA_Y)->setData(Qt::DisplayRole, 0.0);
                ui->powerMarkerTable->item(POWER_MARKER_ROW_PEAK_MIN, POWER_MARKER_COL_DELTA_TO)->setData(Qt::DisplayRole, "Max");
            }
        }

        if (power > m_powerMaxY)
        {
            m_powerMaxY = power;
            m_powerMaxX = dateTime.toMSecsSinceEpoch();
            m_powerPeakSeries->replace(0, m_powerMaxX, m_powerMaxY);
            QDateTime dt = QDateTime::fromMSecsSinceEpoch(m_powerMaxX);
            ui->powerMarkerTable->item(POWER_MARKER_ROW_PEAK_MAX, POWER_MARKER_COL_DATE)->setData(Qt::DisplayRole, dt.date());
            ui->powerMarkerTable->item(POWER_MARKER_ROW_PEAK_MAX, POWER_MARKER_COL_TIME)->setData(Qt::DisplayRole, dt.time());
            ui->powerMarkerTable->item(POWER_MARKER_ROW_PEAK_MAX, POWER_MARKER_COL_VALUE)->setData(Qt::DisplayRole, m_powerMaxY);
            calcPowerPeakDelta();
        }
        else if (power < m_powerMinY)
        {
            m_powerMinY = power;
            m_powerMinX = dateTime.toMSecsSinceEpoch();
            m_powerPeakSeries->replace(1, m_powerMinX, m_powerMinY);
            QDateTime dt = QDateTime::fromMSecsSinceEpoch(m_powerMinX);
            ui->powerMarkerTable->item(POWER_MARKER_ROW_PEAK_MIN, POWER_MARKER_COL_DATE)->setData(Qt::DisplayRole, dt.date());
            ui->powerMarkerTable->item(POWER_MARKER_ROW_PEAK_MIN, POWER_MARKER_COL_TIME)->setData(Qt::DisplayRole, dt.time());
            ui->powerMarkerTable->item(POWER_MARKER_ROW_PEAK_MIN, POWER_MARKER_COL_VALUE)->setData(Qt::DisplayRole, m_powerMinY);
            calcPowerPeakDelta();
        }

        // Update markers (E.g. if scale changes)
        int c = m_powerSeries->count();
        if (c >= 2)
        {
            QPointF p1 = m_powerSeries->at(c-2);
            QPointF p2 = m_powerSeries->at(c-1);
            if (m_powerM1Valid && (m_powerM1X >= p1.x()) && (m_powerM1X < p2.x()))
            {
                m_powerM1Y = Interpolation::interpolate(p1.x(), p1.y(), p2.x(), p2.y(), m_powerM1X);
                m_powerMarkerSeries->insert(0, QPointF(dateTime.toMSecsSinceEpoch(), m_powerM1Y));
                ui->powerMarkerTable->item(POWER_MARKER_ROW_M1, POWER_MARKER_COL_VALUE)->setData(Qt::DisplayRole, m_powerM1Y);
                calcPowerMarkerDelta();
            }
            if (m_powerM2Valid && (m_powerM2X >= p1.x()) && (m_powerM2X < p2.x()))
            {
                m_powerM2Y = Interpolation::interpolate(p1.x(), p1.y(), p2.x(), p2.y(), m_powerM2X);
                m_powerMarkerSeries->append(dateTime.toMSecsSinceEpoch(), m_powerM2Y);
                ui->powerMarkerTable->item(POWER_MARKER_ROW_M2, POWER_MARKER_COL_VALUE)->setData(Qt::DisplayRole, m_powerM2Y);
                calcPowerMarkerDelta();
            }
        }

        if (!skipCalcs)
        {
            // Set X axis format to include date if measurements span over different days
            // Seems there's a QT bug here, if we call m_powerXAxis->setFormat, the chart isn't
            // redrawn properly, so we have to redraw the whole thing
            QDateTime minDateTime = m_powerXAxis->min();
            QDateTime maxDateTime = m_powerXAxis->max();
            bool sameDay = minDateTime.date() == maxDateTime.date();
            if (!sameDay && m_powerXAxisSameDay)
            {
                m_powerXAxisSameDay = true;
                QTimer::singleShot(100, this, SLOT(plotPowerChart()));
            }
        }

        if (!skipCalcs && ui->powerShowAvg->isChecked()) {
            calcAverages();
        }
    }
    if (m_powerSeries->count() <= 1)    // Don't check skipCalcs here, as that will be set for first data
    {
        ui->powerStartTime->setMinimumDateTime(fft->m_dateTime);
        ui->powerEndTime->setMinimumDateTime(fft->m_dateTime);
        if (m_settings.m_powerAutoscale)
        {
            ui->powerStartTime->setDateTime(fft->m_dateTime);
            ui->powerEndTime->setDateTime(fft->m_dateTime);
        }
    }
    if (!skipCalcs)
    {
        ui->powerStartTime->setMaximumDateTime(fft->m_dateTime);
        ui->powerEndTime->setMaximumDateTime(fft->m_dateTime);
        if (m_settings.m_powerAutoscale) {
            ui->powerEndTime->setDateTime(fft->m_dateTime);
        }
    }
}

double RadioAstronomyGUI::degreesToSteradian(double deg) const
{
    // https://en.wikipedia.org/wiki/Steradian - Other properties
    double s = sin(Units::degreesToRadians(deg) / 4.0);
    return 4.0 * M_PI * s * s;
}

double RadioAstronomyGUI::hpbwToSteradians(double hpbw) const
{
    // https://www.cv.nrao.edu/~sransom/web/Ch3.html#E118
    double theta = Units::degreesToRadians(hpbw);
    return theta * theta * M_PI / (4.0 * M_LN2);
}

double RadioAstronomyGUI::calcOmegaA() const
{
    return hpbwToSteradians(m_beamWidth);
}

double RadioAstronomyGUI::calcOmegaS() const
{
    if (m_settings.m_sourceType == RadioAstronomySettings::UNKNOWN)
    {
        return 0.0;
    }
    else if (m_settings.m_sourceType == RadioAstronomySettings::EXTENDED)
    {
        return calcOmegaA();
    }
    else
    {
        return m_settings.m_omegaSUnits == RadioAstronomySettings::STERRADIANS ? m_settings.m_omegaS : degreesToSteradian(m_settings.m_omegaS);
    }
}

double RadioAstronomyGUI::beamFillingFactor() const
{
    if (m_settings.m_sourceType == RadioAstronomySettings::EXTENDED)
    {
        return 1.0;
    }
    else
    {
        // https://www.cv.nrao.edu/~sransom/web/Ch3.html#E55
        return calcOmegaS() / calcOmegaA();
    }
}

void RadioAstronomyGUI::powerMeasurementReceived(FFTMeasurement *fft, bool skipCalcs)
{
    ui->powerTable->setSortingEnabled(false);
    int row = ui->powerTable->rowCount();
    ui->powerTable->setRowCount(row + 1);

    QTableWidgetItem* dateItem = new QTableWidgetItem();
    QTableWidgetItem* timeItem = new QTableWidgetItem();
    QTableWidgetItem* powerItem = new QTableWidgetItem();
    QTableWidgetItem* powerDBItem = new QTableWidgetItem();
    QTableWidgetItem* powerdBmItem = new QTableWidgetItem();
    QTableWidgetItem* tSysItem = new QTableWidgetItem();
    QTableWidgetItem* tSys0Item = new QTableWidgetItem();
    QTableWidgetItem* tSourceItem = new QTableWidgetItem();
    QTableWidgetItem* tBItem = new QTableWidgetItem();
    QTableWidgetItem* tSkyItem = new QTableWidgetItem();
    QTableWidgetItem* fluxItem = new QTableWidgetItem();
    QTableWidgetItem* sigmaTItem = new QTableWidgetItem();
    QTableWidgetItem* sigmaSItem = new QTableWidgetItem();
    QTableWidgetItem* omegaAItem = new QTableWidgetItem();
    QTableWidgetItem* omegaSItem = new QTableWidgetItem();
    QTableWidgetItem* raItem = new QTableWidgetItem();
    QTableWidgetItem* decItem = new QTableWidgetItem();
    QTableWidgetItem* lonItem = new QTableWidgetItem();
    QTableWidgetItem* latItem = new QTableWidgetItem();
    QTableWidgetItem* azItem = new QTableWidgetItem();
    QTableWidgetItem* elItem = new QTableWidgetItem();
    QTableWidgetItem* vBCRSItem = new QTableWidgetItem();
    QTableWidgetItem* vLSRItem = new QTableWidgetItem();
    QTableWidgetItem* solarFluxItem = new QTableWidgetItem();
    QTableWidgetItem* airTempItem = new QTableWidgetItem();
    QTableWidgetItem* sensor1Item = new QTableWidgetItem();
    QTableWidgetItem* sensor2Item = new QTableWidgetItem();
    QTableWidgetItem* utcItem = new QTableWidgetItem();

    ui->powerTable->setItem(row, POWER_COL_DATE, dateItem);
    ui->powerTable->setItem(row, POWER_COL_TIME, timeItem);
    ui->powerTable->setItem(row, POWER_COL_POWER, powerItem);
    ui->powerTable->setItem(row, POWER_COL_POWER_DB, powerDBItem);
    ui->powerTable->setItem(row, POWER_COL_POWER_DBM, powerdBmItem);
    ui->powerTable->setItem(row, POWER_COL_TSYS, tSysItem);
    ui->powerTable->setItem(row, POWER_COL_TSYS0, tSys0Item);
    ui->powerTable->setItem(row, POWER_COL_TSOURCE, tSourceItem);
    ui->powerTable->setItem(row, POWER_COL_TB, tBItem);
    ui->powerTable->setItem(row, POWER_COL_TSKY, tSkyItem);
    ui->powerTable->setItem(row, POWER_COL_FLUX, fluxItem);
    ui->powerTable->setItem(row, POWER_COL_SIGMA_T, sigmaTItem);
    ui->powerTable->setItem(row, POWER_COL_SIGMA_S, sigmaSItem);
    ui->powerTable->setItem(row, POWER_COL_OMEGA_A, omegaAItem);
    ui->powerTable->setItem(row, POWER_COL_OMEGA_S, omegaSItem);
    ui->powerTable->setItem(row, POWER_COL_RA, raItem);
    ui->powerTable->setItem(row, POWER_COL_DEC, decItem);
    ui->powerTable->setItem(row, POWER_COL_GAL_LON, lonItem);
    ui->powerTable->setItem(row, POWER_COL_GAL_LAT, latItem);
    ui->powerTable->setItem(row, POWER_COL_AZ, azItem);
    ui->powerTable->setItem(row, POWER_COL_EL, elItem);
    ui->powerTable->setItem(row, POWER_COL_VBCRS, vBCRSItem);
    ui->powerTable->setItem(row, POWER_COL_VLSR, vLSRItem);
    ui->powerTable->setItem(row, POWER_COL_SOLAR_FLUX, solarFluxItem);
    ui->powerTable->setItem(row, POWER_COL_AIR_TEMP, airTempItem);
    ui->powerTable->setItem(row, POWER_COL_SENSOR_1, sensor1Item);
    ui->powerTable->setItem(row, POWER_COL_SENSOR_2, sensor2Item);
    ui->powerTable->setItem(row, POWER_COL_UTC, utcItem);

    ui->powerTable->setSortingEnabled(true);

    QDateTime dateTime = fft->m_dateTime;
    dateItem->setData(Qt::DisplayRole, dateTime.date());
    timeItem->setData(Qt::DisplayRole, dateTime.time());
    utcItem->setData(Qt::DisplayRole, dateTime.toUTC());

    powerItem->setData(Qt::DisplayRole, fft->m_totalPower);
    powerDBItem->setData(Qt::DisplayRole, fft->m_totalPowerdBFS);
    if (fft->m_tSys != 0.0f) {
        powerdBmItem->setData(Qt::DisplayRole, fft->m_totalPowerdBm);
    }
    if (fft->m_temp) {
        updatePowerColumns(row, fft);
    }
    if (fft->m_coordsValid)
    {
        raItem->setData(Qt::DisplayRole, fft->m_ra);
        decItem->setData(Qt::DisplayRole, fft->m_dec);
        latItem->setData(Qt::DisplayRole, fft->m_b);
        lonItem->setData(Qt::DisplayRole, fft->m_l);
        azItem->setData(Qt::DisplayRole, fft->m_azimuth);
        elItem->setData(Qt::DisplayRole, fft->m_elevation);
        vBCRSItem->setData(Qt::DisplayRole, fft->m_vBCRS);
        vLSRItem->setData(Qt::DisplayRole, fft->m_vLSR);
        tSkyItem->setData(Qt::DisplayRole, fft->m_skyTemp);
    }
    solarFluxItem->setData(Qt::DisplayRole, fft->m_solarFlux);
    airTempItem->setData(Qt::DisplayRole, fft->m_airTemp);
    sensor1Item->setData(Qt::DisplayRole, fft->m_sensor[0]);
    sensor2Item->setData(Qt::DisplayRole, fft->m_sensor[1]);

    addToPowerSeries(fft, skipCalcs);
}

void RadioAstronomyGUI::powerAutoscale()
{
    if (m_settings.m_powerAutoscale)
    {
        on_powerAutoscaleX_clicked();
        on_powerAutoscaleY_clicked();
    }
}

// Scale X and Y axis according to min and max values
void RadioAstronomyGUI::on_powerAutoscale_toggled(bool checked)
{
    m_settings.m_powerAutoscale = checked;
    ui->powerAutoscaleX->setEnabled(!m_settings.m_powerAutoscale);
    ui->powerAutoscaleY->setEnabled(!m_settings.m_powerAutoscale);
    ui->powerReference->setEnabled(!m_settings.m_powerAutoscale);
    ui->powerRange->setEnabled(!m_settings.m_powerAutoscale);
    ui->powerStartTime->setEnabled(!m_settings.m_powerAutoscale);
    ui->powerEndTime->setEnabled(!m_settings.m_powerAutoscale);
    powerAutoscale();
    applySettings();
}

void RadioAstronomyGUI::powerAutoscaleY(bool adjustAxis)
{
    double min = m_powerMin;
    double max = m_powerMax;
    double range = max - min;
    // Round to 1 or 2 decimal places
    if (range > 1.0)
    {
        min = std::floor(min * 10.0) / 10.0;
        max = std::ceil(max * 10.0) / 10.0;
    }
    else
    {
        min = std::floor(min * 100.0) / 100.0;
        max = std::ceil(max * 100.0) / 100.0;
    }
    range = max - min;
    max += range * 0.2; // Add 20% space for markers
    range = max - min;
    range = std::max(0.1, range);     // Don't be smaller than minimum value we can set in GUI

    if (adjustAxis) {
        m_powerYAxis->setRange(min, max);
    }
    ui->powerRange->setValue(range); // Call before setting reference, so number of decimals are adjusted
    ui->powerReference->setValue(max);
}

// Scale Y axis according to min and max values
void RadioAstronomyGUI::on_powerAutoscaleY_clicked()
{
    if (m_powerYAxis) {
        powerAutoscaleY(true);
    }
}

// Scale X axis according to min and max values in series
void RadioAstronomyGUI::on_powerAutoscaleX_clicked()
{
    if (m_powerSeries && (m_powerSeries->count() > 0))
    {
        QDateTime start = QDateTime::fromMSecsSinceEpoch(m_powerSeries->at(0).x());
        QDateTime end = QDateTime::fromMSecsSinceEpoch(m_powerSeries->at(m_powerSeries->count()-1).x());
        ui->powerStartTime->setDateTime(start);
        ui->powerEndTime->setDateTime(end);
    }
}

void RadioAstronomyGUI::on_powerReference_valueChanged(double value)
{
    m_settings.m_powerReference = value;
    if (m_powerYAxis) {
        m_powerYAxis->setRange(m_settings.m_powerReference - m_settings.m_powerRange, m_settings.m_powerReference);
    }
    applySettings();
}

void RadioAstronomyGUI::on_powerRange_valueChanged(double value)
{
    m_settings.m_powerRange = value;
    if (m_settings.m_powerRange <= 1.0)
    {
        ui->powerRange->setSingleStep(0.1);
        ui->powerRange->setDecimals(2);
        ui->powerReference->setDecimals(2);
    }
    else
    {
        ui->powerRange->setSingleStep(1.0);
        ui->powerRange->setDecimals(1);
        ui->powerReference->setDecimals(1);
    }
    if (m_powerYAxis) {
        m_powerYAxis->setRange(m_settings.m_powerReference - m_settings.m_powerRange, m_settings.m_powerReference);
    }
    applySettings();
}

void RadioAstronomyGUI::on_powerStartTime_dateTimeChanged(QDateTime value)
{
    if (m_powerXAxis) {
        m_powerXAxis->setMin(value);
    }
}

void RadioAstronomyGUI::on_powerEndTime_dateTimeChanged(QDateTime value)
{
    if (m_powerXAxis) {
        m_powerXAxis->setMax(value);
    }
}

// Columns in table reordered
void RadioAstronomyGUI::powerTable_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex)
{
    (void) oldVisualIndex;

    m_settings.m_powerTableColumnIndexes[logicalIndex] = newVisualIndex;
}

// Column in table resized (when hidden size is 0)
void RadioAstronomyGUI::powerTable_sectionResized(int logicalIndex, int oldSize, int newSize)
{
    (void) oldSize;

    m_settings.m_powerTableColumnSizes[logicalIndex] = newSize;
}

// Right click in table header - show column select menu
void RadioAstronomyGUI::powerTableColumnSelectMenu(QPoint pos)
{
    powerTableMenu->popup(ui->powerTable->horizontalHeader()->viewport()->mapToGlobal(pos));
}

// Hide/show column when menu selected
void RadioAstronomyGUI::powerTableColumnSelectMenuChecked(bool checked)
{
    (void) checked;

    QAction* action = qobject_cast<QAction*>(sender());
    if (action != nullptr)
    {
        int idx = action->data().toInt(nullptr);
        ui->powerTable->setColumnHidden(idx, !action->isChecked());
    }
}

// Create column select menu item
QAction *RadioAstronomyGUI::createCheckableItem(QString &text, int idx, bool checked, const char *slot)
{
    QAction *action = new QAction(text, this);
    action->setCheckable(true);
    action->setChecked(checked);
    action->setData(QVariant(idx));
    connect(action, SIGNAL(triggered()), this, slot);
    return action;
}

RadioAstronomyGUI* RadioAstronomyGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    RadioAstronomyGUI* gui = new RadioAstronomyGUI(pluginAPI, deviceUISet, rxChannel);
    return gui;
}

void RadioAstronomyGUI::destroy()
{
    delete this;
}

void RadioAstronomyGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray RadioAstronomyGUI::serialize() const
{
    return m_settings.serialize();
}

bool RadioAstronomyGUI::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data)) {
        displaySettings();
        applySettings(true);
        return true;
    } else {
        resetToDefaults();
        return false;
    }
}

void RadioAstronomyGUI::updateAvailableFeatures()
{
    QString currentText = ui->starTracker->currentText();
    ui->starTracker->blockSignals(true);
    ui->starTracker->clear();

    for (const auto& feature : m_availableFeatures) {
        ui->starTracker->addItem(tr("F%1:%2 %3").arg(feature.m_featureSetIndex).arg(feature.m_featureIndex).arg(feature.m_type));
    }

    if (currentText.isEmpty())
    {
        if (m_availableFeatures.size() > 0) {
            ui->starTracker->setCurrentIndex(0);
        }
    }
    else
    {
        ui->starTracker->setCurrentIndex(ui->starTracker->findText(currentText));
    }

    ui->starTracker->blockSignals(false);
    QString newText = ui->starTracker->currentText();

    if (currentText != newText)
    {
       m_settings.m_starTracker = newText;
       applySettings();
    }
}

bool RadioAstronomyGUI::handleMessage(const Message& message)
{
    if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) message;
        m_basebandSampleRate = notif.getSampleRate();
        m_centerFrequency = notif.getCenterFrequency();
        ui->deltaFrequency->setValueRange(false, 7, -m_basebandSampleRate/2, m_basebandSampleRate/2);
        ui->deltaFrequencyLabel->setToolTip(tr("Range %1 %L2 Hz").arg(QChar(0xB1)).arg(m_basebandSampleRate/2));
        updateAbsoluteCenterFrequency();

        if (m_settings.m_tempGalLink) {
            calcGalacticBackgroundTemp();
        }

        updateTSys0();

        return true;
    }
    else if (RadioAstronomy::MsgReportAvailableFeatures::match(message))
    {
        qDebug("RadioAstronomyGUI::handleMessage: MsgReportAvailableFeatures");
        RadioAstronomy::MsgReportAvailableFeatures& report = (RadioAstronomy::MsgReportAvailableFeatures&) message;
        m_availableFeatures = report.getFeatures();
        updateAvailableFeatures();
        return true;
    }
    else if (MainCore::MsgStarTrackerTarget::match(message))
    {
        MainCore::MsgStarTrackerTarget& msg = (MainCore::MsgStarTrackerTarget&)message;
        SWGSDRangel::SWGStarTrackerTarget *target = msg.getSWGStarTrackerTarget();
        m_coordsValid = true;
        m_ra = target->getRa();
        m_dec = target->getDec();
        m_azimuth = target->getAzimuth();
        m_elevation = target->getElevation();
        m_l = target->getL();
        m_b = target->getB();
        m_vBCRS = target->getEarthRotationVelocity() + target->getEarthOrbitVelocityBcrs();
        m_vLSR = target->getSunVelocityLsr() + m_vBCRS;
        m_solarFlux = target->getSolarFlux();
        double airTemp = target->getAirTemperature();
        m_skyTemp = target->getSkyTemperature();
        m_beamWidth = target->getHpbw();

        if (m_settings.m_elevationLink) {
            ui->elevation->setValue(m_elevation);
        }
        if (m_settings.m_tempAirLink) {
            ui->tempAir->setValue(airTemp);
        }
        SensorMeasurement* sm = new SensorMeasurement(QDateTime::currentDateTime(), airTemp);
        m_airTemps.append(sm);
        updateTSys0();
        updateOmegaA();

        return true;
    }
    else if (RadioAstronomy::MsgConfigureRadioAstronomy::match(message))
    {
        const RadioAstronomy::MsgConfigureRadioAstronomy& cfg = (RadioAstronomy::MsgConfigureRadioAstronomy&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (RadioAstronomy::MsgMeasurementProgress::match(message))
    {
        RadioAstronomy::MsgMeasurementProgress& progress = (RadioAstronomy::MsgMeasurementProgress&) message;
        ui->measurementProgress->setValue(progress.getPercentComplete());
        return true;
    }
    else if (RadioAstronomy::MsgSweepStatus::match(message))
    {
        RadioAstronomy::MsgSweepStatus& status = (RadioAstronomy::MsgSweepStatus&) message;
        ui->sweepStatus->setText(status.getStatus());
        return true;
    }
    else if (RadioAstronomy::MsgSweepComplete::match(message))
    {
        ui->startStop->blockSignals(true);
        ui->startStop->setChecked(false);
        ui->startStop->blockSignals(false);
        ui->startStop->setStyleSheet("QToolButton { background-color : blue; }");
        return true;
    }
    else if (RadioAstronomy::MsgCalComplete::match(message))
    {
        RadioAstronomy::MsgCalComplete& measurement = (RadioAstronomy::MsgCalComplete&) message;
        calCompletetReceived(measurement);
        return true;
    }
    else if (RadioAstronomy::MsgFFTMeasurement::match(message))
    {
        RadioAstronomy::MsgFFTMeasurement& measurement = (RadioAstronomy::MsgFFTMeasurement&) message;
        fftMeasurementReceived(measurement);
        if (m_settings.m_runMode == RadioAstronomySettings::SINGLE)
        {
            ui->startStop->blockSignals(true);
            ui->startStop->setChecked(false);
            ui->startStop->blockSignals(false);
            ui->startStop->setStyleSheet("QToolButton { background-color : blue; }");
        }
        return true;
    }
    else if (RadioAstronomy::MsgSensorMeasurement::match(message))
    {
        RadioAstronomy::MsgSensorMeasurement& measurement = (RadioAstronomy::MsgSensorMeasurement&) message;
        sensorMeasurementReceived(measurement);
        return true;
    }
    else if (RadioAstronomy::MsgReportAvailableRotators::match(message))
    {
        RadioAstronomy::MsgReportAvailableRotators& report = (RadioAstronomy::MsgReportAvailableRotators&) message;
        updateRotatorList(report.getFeatures());
        return true;
    }

    return false;
}

void RadioAstronomyGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()) != 0)
    {
        if (handleMessage(*message))
        {
            delete message;
        }
    }
}

void RadioAstronomyGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void RadioAstronomyGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

// Calculate Tsys0 - i.e. receiver noise temperature when there's no source signal, just unwanted noise
void RadioAstronomyGUI::updateTSys0()
{
    double tSys0 = calcTSys0();
    ui->tSys0->setText(QString("%1").arg(round(tSys0)));
    double sigmaT = calcSigmaT(tSys0);
    double sigmaS = calcSigmaS(tSys0);
    ui->sigmaTSys0->setText(QString("%1").arg(sigmaT, 0, 'f', 1));
    ui->sigmaSSys0->setText(QString("%1").arg(sigmaS, 0, 'f', 1));
}

// Estimate of system noise temperature due to all sources of unwanted noise, from user settings
double RadioAstronomyGUI::calcTSys0() const
{
    return m_settings.m_tempRX + m_settings.m_tempCMB + m_settings.m_tempGal + m_settings.m_tempSP + m_settings.m_tempAtm;
}

// Calculate measurement time
double RadioAstronomyGUI::calcTau() const
{
    return m_settings.m_integration / (m_settings.m_sampleRate / (double)m_settings.m_fftSize);
}

double RadioAstronomyGUI::calcTau(const FFTMeasurement* fft) const
{
    return fft->m_integration / (fft->m_sampleRate / (double)fft->m_fftSize);
}

// Calculate variation in Tsys due to random noise fluctuations, including receiver gain variations
// Minimum temp we can reliably detect will be ~5x this
// Uses practical total-power radiometer equation: https://www.cv.nrao.edu/~sransom/web/Ch3.html#E158

double RadioAstronomyGUI::calcSigmaT(double tSys) const
{
    double tau = calcTau();
    return tSys * sqrt(1.0/(m_settings.m_rfBandwidth * tau) + m_settings.m_gainVariation * m_settings.m_gainVariation);
}

double RadioAstronomyGUI::calcSigmaT(const FFTMeasurement* fft) const
{
    double tau = calcTau(fft);
    return fft->m_tSys * sqrt(1.0/(fft->m_rfBandwidth * tau) + m_settings.m_gainVariation * m_settings.m_gainVariation);
}

// Calculate variations in flux due to random noise fluctuations, including receiver gain variations
// Minimum flux we can reliably detect will be ~5x this

double RadioAstronomyGUI::calcSigmaS(double tSys) const
{
    double omegaA = hpbwToSteradians(m_beamWidth);
    double lambda = Astronomy::m_speedOfLight / (double)m_centerFrequency;
    double flux = 2.0 * Astronomy::m_boltzmann * tSys * omegaA / (lambda * lambda); // Should we use Aeff here instead?
    double tau = calcTau();
    double sigma = flux * sqrt(1.0/(m_settings.m_rfBandwidth * tau) + m_settings.m_gainVariation * m_settings.m_gainVariation);
    return Units::wattsPerMetrePerHertzToJansky(sigma);
}

double RadioAstronomyGUI::calcSigmaS(const FFTMeasurement* fft) const
{
    double omegaA = fft->m_omegaA;
    double lambda = Astronomy::m_speedOfLight / (double)fft->m_centerFrequency;
    double flux = 2.0 * Astronomy::m_boltzmann * fft->m_tSys * omegaA / (lambda * lambda); // Should we use Aeff here instead?
    double tau = calcTau(fft);
    double sigma = flux * sqrt(1.0/(fft->m_rfBandwidth * tau) + m_settings.m_gainVariation * m_settings.m_gainVariation);
    return Units::wattsPerMetrePerHertzToJansky(sigma);
}

// Calculate and display how long a single measurement will take
void RadioAstronomyGUI::updateIntegrationTime()
{
    double secs = calcTau();
    if (secs >= 60) {
         ui->integrationTime->setText(QString("%1m").arg(secs/60, 0, 'f', 1));
    } else {
        ui->integrationTime->setText(QString("%1s").arg(secs, 0, 'f', 1));
    }
    updateTSys0();
}

// Limit bandwidth to be less than sample rate
void RadioAstronomyGUI::updateBWLimits()
{
    qint64 sr = (qint64) m_settings.m_sampleRate;
    int digits = ceil(log10(sr+1));
    ui->rfBW->setValueRange(true, digits, 100, sr);
}

void RadioAstronomyGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings();
}

void RadioAstronomyGUI::on_sampleRate_changed(qint64 value)
{
    float sr = value;
    m_settings.m_sampleRate = sr;
    updateBWLimits();
    updateIntegrationTime();
    applySettings();
}

void RadioAstronomyGUI::on_rfBW_changed(qint64 value)
{
    float bw = value;
    m_channelMarker.setBandwidth(bw);
    m_settings.m_rfBandwidth = bw;
    applySettings();
}

void RadioAstronomyGUI::on_integration_changed(qint64 value)
{
    m_settings.m_integration = value;
    updateIntegrationTime();
    applySettings();
}

void RadioAstronomyGUI::on_recalibrate_toggled(bool checked)
{
    m_settings.m_recalibrate = checked;
    applySettings();
    if (checked) {
        recalibrate();
    }
}

void RadioAstronomyGUI::on_showCalSettings_clicked()
{
    RadioAstronomyCalibrationDialog dialog(&m_settings);
    if (dialog.exec() == QDialog::Accepted) {
        applySettings();
    }
}

// Start hot calibration
void RadioAstronomyGUI::on_startCalHot_clicked()
{
    if (ui->startStop->isChecked()) {
        ui->startStop->click();
    }
    m_radioAstronomy->getInputMessageQueue()->push(RadioAstronomy::MsgStartCal::create(true));
    ui->startCalHot->setStyleSheet("QToolButton { background-color : green; }");
}

// Start cold calibration
void RadioAstronomyGUI::on_startCalCold_clicked()
{
    if (ui->startStop->isChecked()) {
        ui->startStop->click();
    }
    m_radioAstronomy->getInputMessageQueue()->push(RadioAstronomy::MsgStartCal::create(false));
    ui->startCalCold->setStyleSheet("QToolButton { background-color : green; }");
}

// Clear all measurements (but not calibration data)
void RadioAstronomyGUI::clearData()
{
    ui->powerTable->setRowCount(0);
    m_powerSeries->clear();
    m_powerPeakSeries->clear();
    m_powerMarkerSeries->clear();
    m_powerTsys0Series->clear();
    m_powerFilteredSeries->clear();
    m_airTemps.clear();
    for (int i = 0; i < RADIOASTRONOMY_SENSORS; i++) {
        m_sensors[i].clear();
    }
    for (int row = 0; row < POWER_MARKER_ROWS; row++)
    {
        for (int col = POWER_MARKER_COL_DATE; col <= POWER_MARKER_COL_DELTA_TO; col++)
        {
            ui->powerMarkerTable->item(row, col)->setText("");
        }
    }
    m_powerM1Valid = false;
    m_powerM2Valid = false;

    qDeleteAll(m_fftMeasurements);
    m_fftMeasurements.clear();
    m_fftSeries->clear();
    m_fftPeakSeries->clear();
    m_fftMarkerSeries->clear();
    for (int row = 0; row < SPECTRUM_MARKER_ROWS; row++)
    {
        for (int col = SPECTRUM_MARKER_COL_FREQ; col <= SPECTRUM_MARKER_COL_D; col++)
        {
            ui->spectrumMarkerTable->item(row, col)->setText("");
        }
    }
    m_spectrumM1Valid = false;
    m_spectrumM2Valid = false;
    clearLoSMarker("Max");
    clearLoSMarker("M1");
    clearLoSMarker("M2");

    ui->spectrumIndex->setRange(0, 0);
    ui->spectrumDateTime->setDateTime(QDateTime::currentDateTime());
    ui->powerMean->setText("");
    ui->powerRMS->setText("");
    ui->powerSD->setText("");
    plotPowerVsTimeChart(); // To ensure min/max/peaks are reset

    create2DImage();
    plotPowerChart();

    ui->measurementProgress->setValue(0);
    ui->sweepStatus->setText("");
}

// Clear calibration data
void RadioAstronomyGUI::clearCalData()
{
    delete m_calHot;
    delete m_calCold;
    delete m_calG;
    m_calHot = nullptr;
    m_calCold = nullptr;
    m_calG = nullptr;
    m_calHotSeries->clear();
    m_calColdSeries->clear();
    ui->calAvgDiff->setText("");
}

// deleteRowsComplete should be called after all rows are deleted
// Returns if the row being deleted is the currently displayed FFT
bool RadioAstronomyGUI::deleteRow(int row)
{
    ui->powerTable->removeRow(row);
    delete m_fftMeasurements[row];
    m_fftMeasurements.removeAt(row);
    return row == ui->spectrumIndex->value();
}

// Updates GUI after rows have been deleted
void RadioAstronomyGUI::deleteRowsComplete(bool deletedCurrent, int next)
{
    if (m_fftMeasurements.size() == 0)
    {
        clearData();
    }
    else
    {
        if (deletedCurrent) {
            ui->spectrumIndex->setValue(next);
        }
        plotPowerChart();
        powerAutoscale();
    }
}

// Calculate average difference in hot and cold cal data - so we can easily validate results
void RadioAstronomyGUI::calcCalAvgDiff()
{
    if ((m_calHot && m_calCold) && (m_calHot->m_fftSize == m_calCold->m_fftSize))
    {
        Real sum = 0.0f;
        for (int i = 0; i < m_calHot->m_fftSize; i++) {
            sum += CalcDb::dbPower(m_calHot->m_fftData[i]) - CalcDb::dbPower(m_calCold->m_fftData[i]);
        }
        Real avg = sum / m_calHot->m_fftSize;
        ui->calAvgDiff->setText(QString::number(avg, 'f', 1));
    }
    else
    {
        ui->calAvgDiff->setText("");
    }
}

void RadioAstronomyGUI::calcCalibrationScaleFactors()
{
    if (m_calHot)
    {
        delete[] m_calG;
        m_calG = new double[m_calHot->m_fftSize];
        // Calculate scaling factors from FFT mag to temperature
        // FIXME: This assumes cal hot is fixed reference temp - E.g. 50Ohm term
        for (int i = 0; i < m_calHot->m_fftSize; i++) {
            m_calG[i] = (m_settings.m_tCalHot + m_settings.m_tempRX) / m_calHot->m_fftData[i];
        }
    }
}

void RadioAstronomyGUI::calibrate()
{
    if (m_calHotSeries)
    {
        calcCalibrationScaleFactors();
        calcCalTrx();
        calcCalTsp();

        if (m_settings.m_recalibrate)
        {
            // Apply new calibration to existing measurements
            recalibrate();
        }
    }
}

// Apply calibration to all existing measurements
void RadioAstronomyGUI::recalibrate()
{
    for (int i = 0; i < m_fftMeasurements.size(); i++)
    {
        FFTMeasurement* fft = m_fftMeasurements[i];
        // Recalibrate
        calcFFTTemperatures(fft);
        calcFFTTotalTemperature(fft);
        // Update table
        if (fft->m_tSys != 0.0f) {
            ui->powerTable->item(i, POWER_COL_POWER_DBM)->setData(Qt::DisplayRole, fft->m_totalPowerdBm);
        }
        if (fft->m_temp) {
            updatePowerColumns(i, fft);
        }
    }
    // Update charts
    plotFFTMeasurement();
    plotPowerChart();
}

// Calculate Trx using Y-factor method
void RadioAstronomyGUI::calcCalTrx()
{
    if ((m_calHot && m_calCold) && (m_calHot->m_fftSize == m_calCold->m_fftSize))
    {
        // y=Ph/Pc
        double sumH = 0.0;
        double sumC = 0.0;
        for (int i = 0; i < m_calHot->m_fftSize; i++)
        {
            sumH += m_calHot->m_fftData[i];
            sumC += m_calCold->m_fftData[i];
        }
        double y = sumH/sumC;
        // Use y to calculate Trx, which should be the same for both calibration points
        double Trx = (m_settings.m_tCalHot - (m_settings.m_tCalCold * y)) / (y - 1.0);
        ui->calYFactor->setText(QString::number(y, 'f', 2));
        ui->calTrx->setText(QString::number(Trx, 'f', 1));
    }
    else
    {
        ui->calYFactor->setText("");
        ui->calTrx->setText("");
    }
}

// Estimate spillover temperature (This is typically very Az/El depenedent as ground noise will vary)
void RadioAstronomyGUI::calcCalTsp()
{
    if (!ui->calTrx->text().isEmpty() && !ui->calTsky->text().isEmpty() && !ui->calYFactor->text().isEmpty())
    {
        double Trx = ui->calTrx->text().toDouble();
        double Tsky = ui->calTsky->text().toDouble();
        double y = ui->calYFactor->text().toDouble();
        double atmosphericAbsorbtion = std::exp(-m_settings.m_zenithOpacity/cos(Units::degreesToRadians(90.0f - m_settings.m_elevation)));

        double Tsp = (m_settings.m_tCalHot + Trx) / y - (Tsky*atmosphericAbsorbtion) - m_settings.m_tempAtm - Trx;

        ui->calTsp->setText(QString::number(Tsp, 'f', 1));
    }
    else
    {
        ui->calTsp->setText("");
    }
}

void RadioAstronomyGUI::on_clearData_clicked()
{
    clearData();
}

void RadioAstronomyGUI::on_clearCal_clicked()
{
    clearCalData();
}

// Save power data in table to a CSV file
void RadioAstronomyGUI::on_savePowerData_clicked()
{
    // Get filename to save to
    QFileDialog fileDialog(nullptr, "Select file to save data to", "", "*.csv");
    fileDialog.setAcceptMode(QFileDialog::AcceptSave);
    if (fileDialog.exec())
    {
        QStringList fileNames = fileDialog.selectedFiles();
        if (fileNames.size() > 0)
        {
            QFile file(fileNames[0]);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QMessageBox::critical(this, "Radio Astronomy", QString("Failed to open file %1").arg(fileNames[0]));
                return;
            }
            QTextStream out(&file);

            // Create a CSV file from the values in the table
            for (int i = 0; i < ui->powerTable->horizontalHeader()->count(); i++)
            {
                QString text = ui->powerTable->horizontalHeaderItem(i)->text();
                out << text << ",";
            }
            out << "\n";
            for (int i = 0; i < ui->powerTable->rowCount(); i++)
            {
                for (int j = 0; j < ui->powerTable->horizontalHeader()->count(); j++)
                {
                    out << ui->powerTable->item(i,j)->data(Qt::DisplayRole).toString() << ",";
                }
                out << "\n";
            }
        }
    }
}

// Create a hash mapping from column name to array index
QHash<QString,int> RadioAstronomyGUI::csvHeadersToHash(QStringList cols)
{
    QHash<QString,int> hash;
    for (int i = 0; i < cols.size(); i++) {
        hash.insert(cols[i], i);
    }
    return hash;
}

// Get data from column with given name, if available
QString RadioAstronomyGUI::csvData(QHash<QString,int> hash, QStringList cols, QString col)
{
    QString s;
    if (hash.contains(col))
    {
        int idx = hash[col];
        if (idx < cols.size()) {
            s = cols[idx];
        }
    }
    return s;
}

bool RadioAstronomyGUI::hasNeededFFTData(QHash<QString,int> hash)
{
    return hash.contains("FFT Size") && hash.contains("Data");
}

// Write FFTMeasurement to a stream
void RadioAstronomyGUI::saveFFT(QTextStream& out, const FFTMeasurement* fft)
{
    out << fft->m_dateTime.toString();
    out << ",";
    out << fft->m_centerFrequency;
    out << ",";
    out << fft->m_sampleRate;
    out << ",";
    out << fft->m_integration;
    out << ",";
    out << fft->m_rfBandwidth;
    out << ",";
    out << fft->m_omegaA;
    out << ",";
    out << fft->m_omegaS;
    out << ",";
    out << fft->m_totalPower;
    out << ",";
    out << fft->m_totalPowerdBFS;
    out << ",";
    out << fft->m_totalPowerdBm;
    out << ",";
    out << fft->m_totalPowerWatts;
    out << ",";
    out << fft->m_tSys;
    out << ",";
    out << fft->m_tSys0;
    out << ",";
    out << fft->m_tSource;
    out << ",";
    out << fft->m_flux;
    out << ",";
    out << fft->m_sigmaT;
    out << ",";
    out << fft->m_sigmaS;
    out << ",";
    out << fft->m_tempMin;
    out << ",";
    out << fft->m_baseline;
    out << ",";
    out << fft->m_ra;
    out << ",";
    out << fft->m_dec;
    out << ",";
    out << fft->m_azimuth;
    out << ",";
    out << fft->m_elevation;
    out << ",";
    out << fft->m_l;
    out << ",";
    out << fft->m_b;
    out << ",";
    out << fft->m_vBCRS;
    out << ",";
    out << fft->m_vLSR;
    out << ",";
    out << fft->m_solarFlux;
    out << ",";
    out << fft->m_airTemp;
    out << ",";
    out << fft->m_skyTemp;
    out << ",";
    out << fft->m_sensor[0];
    out << ",";
    out << fft->m_sensor[1];
    out << ",";
    out << fft->m_fftSize;
    out << ",";
    for (int j = 0; j < fft->m_fftSize; j++)
    {
        out << fft->m_fftData[j];
        out << ",";
    }
    if (fft->m_snr)
    {
        for (int j = 0; j < fft->m_fftSize; j++)
        {
            out << fft->m_snr[j];
            out << ",";
        }
    }
    if (fft->m_temp)
    {
        for (int j = 0; j < fft->m_fftSize; j++)
        {
            out << fft->m_temp[j];
            out << ",";
        }
    }
    out << "\n";
}

// Create a FFTMeasurement from data read from CSV file
RadioAstronomyGUI::FFTMeasurement* RadioAstronomyGUI::loadFFT(QHash<QString,int> hash, QStringList cols)
{
    int fftSize = csvData(hash, cols, "FFT Size").toInt();
    int fftDataIdx = hash["Data"];
    if ((fftSize > 0) && (cols.size() >= fftDataIdx + fftSize))
    {
        FFTMeasurement* fft = new FFTMeasurement();
        fft->m_dateTime = QDateTime::fromString(csvData(hash, cols, "Date Time"));
        fft->m_centerFrequency = csvData(hash, cols, "Centre Freq").toLongLong();
        fft->m_sampleRate = csvData(hash, cols, "Sample Rate").toInt();
        fft->m_integration = csvData(hash, cols, "Integration").toInt();
        fft->m_rfBandwidth = csvData(hash, cols, "Bandwidth").toInt();
        fft->m_omegaA = csvData(hash, cols, "OmegaA").toFloat();
        fft->m_omegaS = csvData(hash, cols, "OmegaS").toFloat();

        fft->m_fftSize = fftSize;
        fft->m_fftData = new Real[fftSize];
        fft->m_db = new Real[fftSize];
        for (int i = 0; i < fftSize; i++)
        {
            fft->m_fftData[i] = cols[fftDataIdx+i].toFloat();
            fft->m_db[i] = (Real)CalcDb::dbPower(fft->m_fftData[i]);
        }
        if (cols.size() >= fftDataIdx + 2*fftSize)
        {
            fft->m_snr = new Real[fftSize];
            for (int i = 0; i < fftSize; i++) {
                fft->m_snr[i] = cols[fftDataIdx+fftSize+i].toFloat();
            }
            if (cols.size() >= fftDataIdx + 3*fftSize)
            {
                fft->m_temp = new Real[fftSize];
                for (int i = 0; i < fftSize; i++) {
                    fft->m_temp[i] = cols[fftDataIdx+2*fftSize+i].toFloat();
                }
            }
        }
        fft->m_totalPower = csvData(hash, cols, "Power (FFT)").toFloat();
        fft->m_totalPowerdBFS = csvData(hash, cols, "Power (dBFS)").toFloat();
        fft->m_totalPowerdBm = csvData(hash, cols, "Power (dBm)").toFloat();
        fft->m_totalPowerWatts = csvData(hash, cols, "Power (Watts)").toFloat();
        fft->m_tSys = csvData(hash, cols, "Tsys").toFloat();
        fft->m_tSys0 = csvData(hash, cols, "Tsys0").toFloat();
        fft->m_tSource = csvData(hash, cols, "Tsource").toFloat();
        fft->m_flux = csvData(hash, cols, "Sv").toFloat();
        fft->m_sigmaT = csvData(hash, cols, "SigmaTsys").toFloat();
        fft->m_sigmaS = csvData(hash, cols, "SigmaSsys").toFloat();
        fft->m_tempMin = csvData(hash, cols, "Min Temp").toFloat();
        fft->m_baseline = (RadioAstronomySettings::SpectrumBaseline)csvData(hash, cols, "Baseline").toInt();


        fft->m_ra = csvData(hash, cols, "RA").toFloat();
        fft->m_dec = csvData(hash, cols, "Dec").toFloat();
        fft->m_azimuth = csvData(hash, cols, "Azimuth").toFloat();
        fft->m_elevation = csvData(hash, cols, "Elevation").toFloat();
        fft->m_l = csvData(hash, cols, "l").toFloat();
        fft->m_b = csvData(hash, cols, "b").toFloat();
        if ((fft->m_ra != 0.0) || (fft->m_dec != 0.0) || (fft->m_azimuth != 0.0) || (fft->m_elevation != 0.0) || (fft->m_l != 0.0) || (fft->m_b != 0.0)) {
            fft->m_coordsValid = true;
        }
        fft->m_vBCRS = csvData(hash, cols, "vBCRS").toFloat();
        fft->m_vLSR = csvData(hash, cols, "vLSR").toFloat();
        fft->m_solarFlux = csvData(hash, cols, "Solar Flux").toFloat();
        fft->m_airTemp = csvData(hash, cols, "Air Temp").toFloat();
        fft->m_skyTemp = csvData(hash, cols, "Sky Temp").toFloat();
        fft->m_sensor[0] = csvData(hash, cols, "Sensor 1").toFloat();
        fft->m_sensor[1] = csvData(hash, cols, "Sensor 2").toFloat();

        if (fft->m_rfBandwidth == 0)
        {
            fft->m_rfBandwidth = 0.9 * fft->m_sampleRate; // Older files don't have this column and we need a value for min
            calcFFTTotalPower(fft);
            /*calcFFTMinTemperature(fft);
            calcFFTTotalTemperature(fft);*/
        }
        return fft;
    }
    else
    {
        return nullptr;
    }
}

void RadioAstronomyGUI::on_saveSpectrumData_clicked()
{
    // Get filename to save to
    QFileDialog fileDialog(nullptr, "Select file to save data to", "", "*.csv");
    fileDialog.setAcceptMode(QFileDialog::AcceptSave);
    if (fileDialog.exec())
    {
        QStringList fileNames = fileDialog.selectedFiles();
        if (fileNames.size() > 0)
        {
            QFile file(fileNames[0]);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QMessageBox::critical(this, "Radio Astronomy", QString("Failed to open file %1").arg(fileNames[0]));
                return;
            }
            QTextStream out(&file);

            if (ui->spectrumChartSelect->currentIndex() == 0)
            {
                // Create a CSV file for all the spectrum data
                out << "Date Time,Centre Freq,Sample Rate,Integration,Bandwidth,OmegaA,OmegaS,Power (FFT),Power (dBFS),Power (dBm),Power (Watts),Tsys,Tsys0,Tsource,Sv,SigmaTsys,SigmaSsys,Min Temp,Baseline,RA,Dec,Azimuth,Elevation,l,b,vBCRS,vLSR,Solar Flux,Air Temp,Sky Temp,Sensor 1,Sensor 2,FFT Size,Data\n";
                for (int i = 0; i < m_fftMeasurements.size(); i++) {
                    saveFFT(out, m_fftMeasurements[i]);
                }
            }
            else
            {
                // Create a CSV file for calibration data
                out << "Cal,Cal Temp,Date Time,Centre Freq,Sample Rate,Integration,Bandwidth,OmegaA,OmegaS,Power (FFT),Power (dBFS),Power (dBm),Power (Watts),Tsys,Tsys0,Tsource,Sv,SigmaTsys,SigmaSsys,Min Temp,Baseline,RA,Dec,Azimuth,Elevation,l,b,vBCRS,vLSR,Solar Flux,Air Temp,Sky Temp,Sensor 1,Sensor 2,FFT Size,Data\n";
                if (m_calHot)
                {
                    out << "Hot,";
                    out << m_settings.m_tCalHot;
                    out << ",";
                    saveFFT(out, m_calHot);
                }
                if (m_calCold)
                {
                    out << "Cold,";
                    out << m_settings.m_tCalCold;
                    out << ",";
                    saveFFT(out, m_calCold);
                }
            }
        }
    }
}

void RadioAstronomyGUI::on_loadSpectrumData_clicked()
{
    // Get filename to load from
    QFileDialog fileDialog(nullptr, "Select file to load data from", "", "*.csv");
    fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
    if (fileDialog.exec())
    {
        QStringList fileNames = fileDialog.selectedFiles();
        if (fileNames.size() > 0)
        {
            QFile file(fileNames[0]);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QMessageBox::critical(this, "Radio Astronomy", QString("Failed to open file %1").arg(fileNames[0]));
                return;
            }

            // Get column names
            QTextStream in(&file);
            QString header = in.readLine();
            QStringList colNames = header.split(",");
            QHash<QString,int> hash = csvHeadersToHash(colNames);

            if (ui->spectrumChartSelect->currentIndex() == 0)
            {
                // Load data from CSV file
                if (hasNeededFFTData(hash))
                {
                    // Remove old data - we could support multiple series for comparison
                    clearData();
                    // Read in FFT data from file
                    ui->spectrumIndex->blockSignals(true); // Prevent every spectrum from being displayed
                    while (!in.atEnd())
                    {
                        QString row = in.readLine();
                        QStringList cols = row.split(",");
                        FFTMeasurement* fft = loadFFT(hash, cols);
                        if (fft) {
                            addFFT(fft, true);
                        }
                    }
                    ui->spectrumIndex->blockSignals(false);
                    // Add data from FFT to sensor measurements
                    for (int i = 0; i < m_fftMeasurements.size(); i++)
                    {
                        SensorMeasurement* sm;
                        sm = new SensorMeasurement(m_fftMeasurements[i]->m_dateTime, m_fftMeasurements[i]->m_airTemp);
                        m_airTemps.append(sm);
                        for (int j = 0; j < RADIOASTRONOMY_SENSORS; j++)
                        {
                            sm = new SensorMeasurement(m_fftMeasurements[i]->m_dateTime, m_fftMeasurements[i]->m_sensor[j]);
                            m_sensors[j].append(sm);
                        }
                    }
                    // If we're loading data from scratch, autoscale both axis
                    if ((ui->spectrumCenterFreq->value() == 0.0) || m_settings.m_spectrumAutoscale)
                    {
                        on_spectrumAutoscaleY_clicked();
                        on_spectrumAutoscaleX_clicked();
                    }
                    // Ensure both charts are redrawn fully, as we've disabled some updates/calcs during load
                    on_spectrumIndex_valueChanged(m_fftMeasurements.size() - 1);   // Don't call setValue, as it already has this value
                    plotPowerChart();
                    // As signals were blocked above, power axis may not match up with GUI. Manually update
                    // Just calling autoscale will not work, as the GUI values may not change
                    on_powerStartTime_dateTimeChanged(ui->powerStartTime->dateTime());
                    on_powerEndTime_dateTimeChanged(ui->powerEndTime->dateTime());
                    on_powerRange_valueChanged(m_settings.m_powerRange);
                    on_powerReference_valueChanged(m_settings.m_powerReference);
                }
            }
            else
            {
                // Load calibration data from CSV file
                if (hasNeededFFTData(hash) && hash.contains("Cal"))
                {
                    while (!in.atEnd())
                    {
                        QString row = in.readLine();
                        QStringList cols = row.split(",");

                        QString calName = csvData(hash, cols, "Cal");

                        FFTMeasurement** calp = nullptr;
                        FFTMeasurement* cal = nullptr;
                        if (calName == "Hot") {
                            calp = &m_calHot;
                        } else if (calName == "Cold") {
                            calp = &m_calCold;
                        } else {
                            qDebug() << "RadioAstronomyGUI::on_loadSpectrumData_clicked: Skipping unknown calibration " << calName;
                        }
                        if (calp)
                        {
                            cal = loadFFT(hash, cols);
                            if (cal)
                            {
                                delete *calp;
                                *calp = cal;
                                qDebug() << "RadioAstronomyGUI::on_loadSpectrumData_clicked: Loaded calibration " << calName;
                                if (calName == "Cold") {
                                    ui->calTsky->setText(QString::number(cal->m_skyTemp, 'f', 1));
                                }
                                QString calTempString = csvData(hash, cols, "Cal Temp");
                                bool ok;
                                double calTemp = calTempString.toDouble(&ok);
                                if (ok)
                                {
                                    if (calName == "Cold")
                                    {
                                        ui->tCalColdSelect->setCurrentIndex(0);
                                        ui->tCalCold->setValue(calTemp);
                                    }
                                    else
                                    {
                                        ui->tCalHotSelect->setCurrentIndex(0);
                                        ui->tCalHot->setValue(calTemp);
                                    }
                                }
                            }
                        }
                    }
                    calcCalAvgDiff();
                    calibrate();
                    plotCalMeasurements();
                }
                else
                {
                    QMessageBox::critical(this, "Radio Astronomy", QString("Missing required columns in file %1").arg(fileNames[0]));
                    return;
                }
            }
        }
    }
}

void RadioAstronomyGUI::on_powerTable_cellDoubleClicked(int row, int column)
{
    if ((column >= POWER_COL_RA) && (column >= POWER_COL_EL))
    {
        // Display target in Star Tracker
        QList<ObjectPipe*> starTrackerPipes;
        MainCore::instance()->getMessagePipes().getMessagePipes(this, "startracker.display", starTrackerPipes);

        for (const auto& pipe : starTrackerPipes)
        {
            MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
            SWGSDRangel::SWGStarTrackerDisplaySettings *swgSettings = new SWGSDRangel::SWGStarTrackerDisplaySettings();
            QDateTime dt(ui->powerTable->item(row, POWER_COL_DATE)->data(Qt::DisplayRole).toDate(),
                            ui->powerTable->item(row, POWER_COL_TIME)->data(Qt::DisplayRole).toTime());
            swgSettings->setDateTime(new QString(dt.toString(Qt::ISODateWithMs)));
            swgSettings->setAzimuth(ui->powerTable->item(row, POWER_COL_AZ)->data(Qt::DisplayRole).toFloat());
            swgSettings->setElevation(ui->powerTable->item(row, POWER_COL_EL)->data(Qt::DisplayRole).toFloat());
            messageQueue->push(MainCore::MsgStarTrackerDisplaySettings::create(m_radioAstronomy, swgSettings));
        }
    }
    else
    {
        // Display in Spectrometer
        ui->spectrumIndex->setValue(row);
    }
}

void RadioAstronomyGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void RadioAstronomyGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuChannelSettings)
    {
        BasicChannelSettingsDialog dialog(&m_channelMarker, this);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIDeviceIndex(m_settings.m_reverseAPIDeviceIndex);
        dialog.setReverseAPIChannelIndex(m_settings.m_reverseAPIChannelIndex);
        dialog.setDefaultTitle(m_displayedName);

        if (m_deviceUISet->m_deviceMIMOEngine)
        {
            dialog.setNumberOfStreams(m_radioAstronomy->getNumberOfDeviceStreams());
            dialog.setStreamIndex(m_settings.m_streamIndex);
        }

        dialog.move(p);
        new DialogPositioner(&dialog, false);
        dialog.exec();

        m_settings.m_rgbColor = m_channelMarker.getColor().rgb();
        m_settings.m_title = m_channelMarker.getTitle();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIDeviceIndex = dialog.getReverseAPIDeviceIndex();
        m_settings.m_reverseAPIChannelIndex = dialog.getReverseAPIChannelIndex();

        setWindowTitle(m_settings.m_title);
        setTitle(m_channelMarker.getTitle());
        setTitleColor(m_settings.m_rgbColor);

        if (m_deviceUISet->m_deviceMIMOEngine)
        {
            m_settings.m_streamIndex = dialog.getSelectedStreamIndex();
            m_channelMarker.clearStreamIndexes();
            m_channelMarker.addStreamIndex(m_settings.m_streamIndex);
            updateIndexLabel();
        }

        applySettings();
    }

    resetContextMenuType();
}

RadioAstronomyGUI::RadioAstronomyGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::RadioAstronomyGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_doApplySettings(true),
    m_basebandSampleRate(0),
    m_centerFrequency(0),
    m_tickCount(0),
    m_powerChart(nullptr),
    m_powerSeries(nullptr),
    m_powerXAxis(nullptr),
    m_powerYAxis(nullptr),
    m_powerPeakSeries(nullptr),
    m_powerMarkerSeries(nullptr),
    m_powerTsys0Series(nullptr),
    m_powerGaussianSeries(nullptr),
    m_powerFilteredSeries(nullptr),
    m_powerPeakValid(false),
    m_2DChart(nullptr),
    m_2DXAxis(nullptr),
    m_2DYAxis(nullptr),
    m_2DMapIntensity(nullptr),
    m_sweepIndex(0),
    m_calChart(nullptr),
    m_calXAxis(nullptr),
    m_calYAxis(nullptr),
    m_calHotSeries(nullptr),
    m_calColdSeries(nullptr),
    m_calHot(nullptr),
    m_calCold(nullptr),
    m_calG(nullptr),
    m_fftChart(nullptr),
    m_fftSeries(nullptr),
    m_fftHlineSeries(nullptr),
    m_fftPeakSeries(nullptr),
    m_fftMarkerSeries(nullptr),
    m_fftGaussianSeries(nullptr),
    m_fftLABSeries(nullptr),
    m_fftXAxis(nullptr),
    m_fftYAxis(nullptr),
    m_fftDopplerAxis(nullptr),
    m_powerM1Valid(false),
    m_powerM2Valid(false),
    m_spectrumM1Valid(false),
    m_spectrumM2Valid(false),
    m_coordsValid(false),
    m_ra(0.0f),
    m_dec(0.0f),
    m_azimuth(0.0f),
    m_elevation(0.0f),
    m_l(0.0f),
    m_b(0.0f),
    m_vBCRS(0.0f),
    m_vLSR(0.0f),
    m_solarFlux(0.0f),
    m_beamWidth(5.6f),
    m_lLAB(0.0f),
    m_bLAB(0.0f),
    m_downloadingLAB(false),
    m_window(nullptr),
    m_windowSorted(nullptr),
    m_windowIdx(0),
    m_windowCount(0)
{
    qDebug("RadioAstronomyGUI::RadioAstronomyGUI");
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/radioastronomy/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_radioAstronomy = reinterpret_cast<RadioAstronomy*>(rxChannel);
    m_radioAstronomy->setMessageQueueToGUI(getInputMessageQueue());

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick())); // 50 ms

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &RadioAstronomyGUI::networkManagerFinished
    );
    connect(&m_dlm, &HttpDownloadManager::downloadComplete, this, &RadioAstronomyGUI::downloadFinished);

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);

    // Need to setValue before calling setValueRange, otherwise valueChanged is called
    // overwriting the default settings (could also blockSignals)
    // Also, set bandwidth before sampleRate
    ui->rfBW->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->rfBW->setValue(m_settings.m_rfBandwidth);
    ui->rfBW->setValueRange(true, 8, 100, 99999999);
    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->sampleRate->setValue(m_settings.m_sampleRate);
    ui->sampleRate->setValueRange(true, 8, 1000, 99999999);
    ui->integration->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->integration->setValue(m_settings.m_integration);
    ui->integration->setValueRange(true, 7, 1, 99999999);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::yellow);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle("Radio Astronomy");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    setTitleColor(m_channelMarker.getColor());
    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);

    m_deviceUISet->addChannelMarker(&m_channelMarker);

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    // Resize the table using dummy data
    resizePowerTable();
    // Allow user to reorder columns
    ui->powerTable->horizontalHeader()->setSectionsMovable(true);
    // Allow user to sort table by clicking on headers
    ui->powerTable->setSortingEnabled(true);
    // Add context menu to allow hiding/showing of columns
    powerTableMenu = new QMenu(ui->powerTable);
    for (int i = 0; i < ui->powerTable->horizontalHeader()->count(); i++)
    {
        QString text = ui->powerTable->horizontalHeaderItem(i)->text();
        powerTableMenu->addAction(createCheckableItem(text, i, true, SLOT(powerTableColumnSelectMenuChecked())));
    }
    ui->powerTable->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->powerTable->horizontalHeader(), SIGNAL(customContextMenuRequested(QPoint)), SLOT(powerTableColumnSelectMenu(QPoint)));
    // Get signals when columns change
    connect(ui->powerTable->horizontalHeader(), SIGNAL(sectionMoved(int, int, int)), SLOT(powerTable_sectionMoved(int, int, int)));
    connect(ui->powerTable->horizontalHeader(), SIGNAL(sectionResized(int, int, int)), SLOT(powerTable_sectionResized(int, int, int)));
    ui->powerTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->powerTable, SIGNAL(customContextMenuRequested(QPoint)), SLOT(customContextMenuRequested(QPoint)));

    ui->powerTable->setItemDelegateForColumn(POWER_COL_TIME, new TimeDelegate());
    //ui->powerTable->setItemDelegateForColumn(POWER_COL_POWER, new DecimalDelegate(6));
    ui->powerTable->setItemDelegateForColumn(POWER_COL_POWER_DB, new DecimalDelegate(1));
    ui->powerTable->setItemDelegateForColumn(POWER_COL_POWER_DBM, new DecimalDelegate(1));
    ui->powerTable->setItemDelegateForColumn(POWER_COL_TSYS, new DecimalDelegate(0));
    ui->powerTable->setItemDelegateForColumn(POWER_COL_TSYS0, new DecimalDelegate(0));
    ui->powerTable->setItemDelegateForColumn(POWER_COL_TSOURCE, new DecimalDelegate(0));
    ui->powerTable->setItemDelegateForColumn(POWER_COL_TB, new DecimalDelegate(0));
    ui->powerTable->setItemDelegateForColumn(POWER_COL_TSKY, new DecimalDelegate(0));
    ui->powerTable->setItemDelegateForColumn(POWER_COL_FLUX, new DecimalDelegate(2));
    ui->powerTable->setItemDelegateForColumn(POWER_COL_SIGMA_T, new DecimalDelegate(2));
    ui->powerTable->setItemDelegateForColumn(POWER_COL_SIGMA_S, new DecimalDelegate(1));
    ui->powerTable->setItemDelegateForColumn(POWER_COL_RA, new HMSDelegate());
    ui->powerTable->setItemDelegateForColumn(POWER_COL_DEC, new DMSDelegate());
    ui->powerTable->setItemDelegateForColumn(POWER_COL_GAL_LAT, new DecimalDelegate(0));
    ui->powerTable->setItemDelegateForColumn(POWER_COL_GAL_LON, new DecimalDelegate(0));
    ui->powerTable->setItemDelegateForColumn(POWER_COL_AZ, new DecimalDelegate(0));
    ui->powerTable->setItemDelegateForColumn(POWER_COL_EL, new DecimalDelegate(0));
    ui->powerTable->setItemDelegateForColumn(POWER_COL_VBCRS, new DecimalDelegate(1));
    ui->powerTable->setItemDelegateForColumn(POWER_COL_VLSR, new DecimalDelegate(1));
    ui->powerTable->setItemDelegateForColumn(POWER_COL_AIR_TEMP, new DecimalDelegate(1));

    resizeSpectrumMarkerTable();
    ui->spectrumMarkerTable->setItemDelegateForColumn(SPECTRUM_MARKER_COL_FREQ, new DecimalDelegate(6));
    ui->spectrumMarkerTable->setItemDelegateForColumn(SPECTRUM_MARKER_COL_VALUE, new DecimalDelegate(1));
    ui->spectrumMarkerTable->setItemDelegateForColumn(SPECTRUM_MARKER_COL_DELTA_X, new DecimalDelegate(6));
    ui->spectrumMarkerTable->setItemDelegateForColumn(SPECTRUM_MARKER_COL_DELTA_Y, new DecimalDelegate(1));
    ui->spectrumMarkerTable->setItemDelegateForColumn(SPECTRUM_MARKER_COL_VR, new DecimalDelegate(2));
    ui->spectrumMarkerTable->setItemDelegateForColumn(SPECTRUM_MARKER_COL_R, new DecimalDelegate(1));
    ui->spectrumMarkerTable->setItemDelegateForColumn(SPECTRUM_MARKER_COL_D, new DecimalDelegate(1));
    ui->spectrumMarkerTable->setItemDelegateForColumn(SPECTRUM_MARKER_COL_R_MIN, new DecimalDelegate(1));
    ui->spectrumMarkerTable->setItemDelegateForColumn(SPECTRUM_MARKER_COL_V, new DecimalDelegate(1));

    // Create blank marker table
    ui->spectrumMarkerTable->setRowCount(SPECTRUM_MARKER_ROWS); // 1 peak and two markers
    for (int row = 0; row < SPECTRUM_MARKER_ROWS; row++)
    {
        ui->spectrumMarkerTable->setItem(row, SPECTRUM_MARKER_COL_NAME, new QTableWidgetItem());
        ui->spectrumMarkerTable->setItem(row, SPECTRUM_MARKER_COL_FREQ, new QTableWidgetItem());
        ui->spectrumMarkerTable->setItem(row, SPECTRUM_MARKER_COL_VALUE, new QTableWidgetItem());
        ui->spectrumMarkerTable->setItem(row, SPECTRUM_MARKER_COL_DELTA_X, new QTableWidgetItem());
        ui->spectrumMarkerTable->setItem(row, SPECTRUM_MARKER_COL_DELTA_Y, new QTableWidgetItem());
        ui->spectrumMarkerTable->setItem(row, SPECTRUM_MARKER_COL_DELTA_TO, new QTableWidgetItem());
        ui->spectrumMarkerTable->setItem(row, SPECTRUM_MARKER_COL_VR, new QTableWidgetItem());
        ui->spectrumMarkerTable->setItem(row, SPECTRUM_MARKER_COL_R, new QTableWidgetItem());
        ui->spectrumMarkerTable->setItem(row, SPECTRUM_MARKER_COL_D, new QTableWidgetItem());
        // It seems clearing Qt::ItemIsUserCheckable doesn't remove the checkbox, so once set, we always have it
        QTableWidgetItem* item = new QTableWidgetItem();
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
        ui->spectrumMarkerTable->setItem(row, SPECTRUM_MARKER_COL_PLOT_MAX, item);
        ui->spectrumMarkerTable->setItem(row, SPECTRUM_MARKER_COL_R_MIN, new QTableWidgetItem());
        ui->spectrumMarkerTable->setItem(row, SPECTRUM_MARKER_COL_V, new QTableWidgetItem());
    }
    ui->spectrumMarkerTable->item(SPECTRUM_MARKER_ROW_PEAK, SPECTRUM_MARKER_COL_NAME)->setText("Max");
    ui->spectrumMarkerTable->item(SPECTRUM_MARKER_ROW_M1, SPECTRUM_MARKER_COL_NAME)->setText("M1");
    ui->spectrumMarkerTable->item(SPECTRUM_MARKER_ROW_M2, SPECTRUM_MARKER_COL_NAME)->setText("M2");
    connect(ui->spectrumMarkerTable, &QTableWidget::itemChanged, this, &RadioAstronomyGUI::spectrumMarkerTableItemChanged);

    resizePowerMarkerTable();
    ui->powerMarkerTable->setItemDelegateForColumn(POWER_MARKER_COL_TIME, new TimeDelegate());
    ui->powerMarkerTable->setItemDelegateForColumn(POWER_MARKER_COL_VALUE, new DecimalDelegate(1));
    ui->powerMarkerTable->setItemDelegateForColumn(POWER_MARKER_COL_DELTA_X, new TimeDeltaDelegate());
    ui->powerMarkerTable->setItemDelegateForColumn(POWER_MARKER_COL_DELTA_Y, new DecimalDelegate(1));

    // Create blank marker table
    ui->powerMarkerTable->setRowCount(POWER_MARKER_ROWS); // 1 peak and two markers
    for (int row = 0; row < POWER_MARKER_ROWS; row++)
    {
        ui->powerMarkerTable->setItem(row, POWER_MARKER_COL_NAME, new QTableWidgetItem());
        ui->powerMarkerTable->setItem(row, POWER_MARKER_COL_DATE, new QTableWidgetItem());
        ui->powerMarkerTable->setItem(row, POWER_MARKER_COL_TIME, new QTableWidgetItem());
        ui->powerMarkerTable->setItem(row, POWER_MARKER_COL_VALUE, new QTableWidgetItem());
        ui->powerMarkerTable->setItem(row, POWER_MARKER_COL_DELTA_X, new QTableWidgetItem());
        ui->powerMarkerTable->setItem(row, POWER_MARKER_COL_DELTA_Y, new QTableWidgetItem());
        ui->powerMarkerTable->setItem(row, POWER_MARKER_COL_DELTA_TO, new QTableWidgetItem());
    }
    ui->powerMarkerTable->item(POWER_MARKER_ROW_PEAK_MAX, POWER_MARKER_COL_NAME)->setText("Max");
    ui->powerMarkerTable->item(POWER_MARKER_ROW_PEAK_MIN, POWER_MARKER_COL_NAME)->setText("Min");
    ui->powerMarkerTable->item(POWER_MARKER_ROW_M1, POWER_MARKER_COL_NAME)->setText("M1");
    ui->powerMarkerTable->item(POWER_MARKER_ROW_M2, POWER_MARKER_COL_NAME)->setText("M2");

    ui->sweepStartDateTime->setMinimumDateTime(QDateTime::currentDateTime());
    ui->spectrumDateTime->setDateTime(QDateTime::currentDateTime());

    ui->startStop->setStyleSheet("QToolButton { background-color : blue; }");

    displaySettings();
    makeUIConnections();
    applySettings(true);

    create2DImage();

    plotCalSpectrum();
    plotSpectrum();
    plotPowerChart();
}

void RadioAstronomyGUI::customContextMenuRequested(QPoint pos)
{
    QTableWidgetItem *item = ui->powerTable->itemAt(pos);
    if (item)
    {
        QMenu* tableContextMenu = new QMenu(ui->powerTable);
        connect(tableContextMenu, &QMenu::aboutToHide, tableContextMenu, &QMenu::deleteLater);

        // Copy cell contents to clipboard
        QAction* copyAction = new QAction("Copy cell", tableContextMenu);
        const QString text = item->text();
        connect(copyAction, &QAction::triggered, this, [text]()->void {
            QClipboard *clipboard = QGuiApplication::clipboard();
            clipboard->setText(text);
        });
        tableContextMenu->addAction(copyAction);

        // Delete selected rows
        QAction* delAction = new QAction("Delete rows", tableContextMenu);
        connect(delAction, &QAction::triggered, this, [this]()->void {
            QModelIndexList rowIndexes = ui->powerTable->selectionModel()->selectedRows();
            if (rowIndexes.size() > 0)
            {
                // Delete in reverse row order
                std::vector<int> rows;
                foreach (auto rowIndex, rowIndexes) {
                    rows.push_back(rowIndex.row());
                }
                std::sort(rows.begin(), rows.end(), std::greater<int>());
                bool deletedCurrent = false;
                int next;
                foreach (auto row, rows) {
                    next = row - 1;
                    if (deleteRow(row)) {
                        deletedCurrent = true;
                    }
                }
                deleteRowsComplete(deletedCurrent, next);
            }
        });
        tableContextMenu->addAction(delAction);

        // Update rows with new Tsys0 and baseline
        QAction* updateTSysAction = new QAction(QString("Update Tsys0 / baseline / %1").arg(QChar(937)), tableContextMenu);
        connect(updateTSysAction, &QAction::triggered, this, [this]()->void {
            QModelIndexList rowIndexes = ui->powerTable->selectionModel()->selectedRows();
            if (rowIndexes.size() > 0)
            {
                foreach (auto rowIndex, rowIndexes)
                {
                    int row = rowIndex.row();
                    m_fftMeasurements[row]->m_tSys0 = calcTSys0();
                    m_fftMeasurements[row]->m_baseline = m_settings.m_spectrumBaseline;
                    m_fftMeasurements[row]->m_omegaA = calcOmegaA();
                    m_fftMeasurements[row]->m_omegaS = calcOmegaS();
                    calcFFTTotalTemperature(m_fftMeasurements[row]);
                    updatePowerColumns(row, m_fftMeasurements[row]);
                }
                plotFFTMeasurement();
            }
        });
        tableContextMenu->addAction(updateTSysAction);

        tableContextMenu->popup(ui->powerTable->viewport()->mapToGlobal(pos));
    }
}

RadioAstronomyGUI::~RadioAstronomyGUI()
{
    delete ui;
    delete m_calHot;
    delete m_calCold;
    qDeleteAll(m_dataLAB);
    m_dataLAB.clear();
    delete[] m_2DMapIntensity;
    delete[] m_window;
    delete[] m_windowSorted;
}

void RadioAstronomyGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void RadioAstronomyGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        RadioAstronomy::MsgConfigureRadioAstronomy* message = RadioAstronomy::MsgConfigureRadioAstronomy::create( m_settings, force);
        m_radioAstronomy->getInputMessageQueue()->push(message);
    }
}

int RadioAstronomyGUI::fftSizeToIndex(int size)
{
    switch (size)
    {
    case 16:
        return 0;
    case 32:
        return 1;
    case 64:
        return 2;
    case 128:
        return 3;
    case 256:
        return 4;
    case 512:
        return 5;
    case 1024:
        return 6;
    case 2048:
        return 7;
    case 4096:
        return 8;
    }
    return 0;
}

void RadioAstronomyGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());

    blockApplySettings(true);
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    float rfBW = m_settings.m_rfBandwidth; // Save value, as it may be corrupted when setting sampleRate
    ui->sampleRate->setValue(m_settings.m_sampleRate);
    ui->rfBW->setValue(rfBW);
    updateBWLimits();

    ui->integration->setValue(m_settings.m_integration);
    ui->fftSize->setCurrentIndex(fftSizeToIndex(m_settings.m_fftSize));
    ui->fftWindow->setCurrentIndex((int)m_settings.m_fftWindow);
    ui->filterFreqs->setText(m_settings.m_filterFreqs);

    int idx = ui->starTracker->findText(m_settings.m_starTracker);
    if (idx != -1) {
        ui->starTracker->setCurrentIndex(idx);
    }
    idx = ui->rotator->findText(m_settings.m_rotator);
    if (idx != -1) {
        ui->rotator->setCurrentIndex(idx);
    }

    ui->tempRXSelect->setCurrentIndex(0);
    ui->tempRX->setValue(m_settings.m_tempRX);
    ui->tempRXUnitsLabel->setText("K");
    ui->tempCMB->setValue(m_settings.m_tempCMB);
    ui->tempGal->setValue(m_settings.m_tempGal);
    ui->tempGal->setEnabled(!m_settings.m_tempGalLink);
    ui->tempGalLink->setChecked(m_settings.m_tempGalLink);
    ui->tempSP->setValue(m_settings.m_tempSP);
    ui->tempAtm->setValue(m_settings.m_tempAtm);
    ui->tempAtm->setEnabled(!m_settings.m_tempAtmLink);
    ui->tempAtmLink->setChecked(m_settings.m_tempAtmLink);
    ui->tempAir->setValue(m_settings.m_tempAir);
    ui->tempAir->setEnabled(!m_settings.m_tempAirLink);
    ui->tempAirLink->setChecked(m_settings.m_tempAirLink);
    ui->zenithOpacity->setValue(m_settings.m_zenithOpacity);
    ui->elevation->setValue(m_settings.m_elevation);
    ui->elevation->setEnabled(!m_settings.m_elevationLink);
    ui->elevationLink->setChecked(m_settings.m_elevationLink);

    ui->gainVariation->setValue(m_settings.m_gainVariation);
    ui->sourceType->setCurrentIndex((int)m_settings.m_sourceType);
    ui->omegaS->setValue(m_settings.m_omegaS);
    ui->omegaSUnits->setCurrentIndex((int)m_settings.m_omegaSUnits);
    ui->omegaAUnits->setCurrentIndex((int)m_settings.m_omegaAUnits);

    ui->recalibrate->setChecked(m_settings.m_recalibrate);
    ui->tCalHot->setValue(m_settings.m_tCalHot);
    ui->tCalCold->setValue(m_settings.m_tCalCold);

    ui->spectrumAutoscale->setChecked(m_settings.m_spectrumAutoscale);
    ui->spectrumReference->setValue(m_settings.m_spectrumReference);
    ui->spectrumRange->setValue(m_settings.m_spectrumRange);
    FFTMeasurement* fft = currentFFT();
    if (fft) {
        ui->spectrumCenterFreq->setValue(fft->m_centerFrequency/1e6 + m_settings.m_spectrumCenterFreqOffset);
    } else {
        ui->spectrumCenterFreq->setValue(m_centerFrequency/1e6 + m_settings.m_spectrumCenterFreqOffset);
    }
    ui->spectrumSpan->setValue(m_settings.m_spectrumSpan);
    ui->spectrumYUnits->setCurrentIndex((int)m_settings.m_spectrumYScale);
    ui->spectrumBaseline->setCurrentIndex((int)m_settings.m_spectrumBaseline);
    ui->spectrumAutoscaleX->setEnabled(!m_settings.m_spectrumAutoscale);
    ui->spectrumAutoscaleY->setEnabled(!m_settings.m_spectrumAutoscale);
    ui->spectrumReference->setEnabled(!m_settings.m_spectrumAutoscale);
    ui->spectrumRange->setEnabled(!m_settings.m_spectrumAutoscale);
    ui->spectrumCenterFreq->setEnabled(!m_settings.m_spectrumAutoscale);
    ui->spectrumSpan->setEnabled(!m_settings.m_spectrumAutoscale);

    ui->powerAutoscale->setChecked(m_settings.m_powerAutoscale);
    ui->powerReference->setValue(m_settings.m_powerReference);
    ui->powerRange->setValue(m_settings.m_powerRange);
    ui->powerShowPeak->setChecked(m_settings.m_powerPeaks);
    if (m_powerPeakSeries)
    {
        m_powerPeakSeries->setVisible(m_settings.m_powerPeaks);
        m_powerChart->legend()->markers(m_powerPeakSeries)[0]->setVisible(false);
    }
    ui->powerShowMarker->setChecked(m_settings.m_powerMarkers);
    if (m_powerMarkerSeries)
    {
        m_powerMarkerSeries->setVisible(m_settings.m_powerMarkers);
        m_powerChart->legend()->markers(m_powerMarkerSeries)[0]->setVisible(false);
    }
    ui->powerShowAvg->setChecked(m_settings.m_powerAvg);
    ui->powerChartAvgWidgets->setVisible(m_settings.m_powerAvg);
    ui->powerShowGaussian->setChecked(m_settings.m_powerShowGaussian);
    ui->powerGaussianWidgets->setVisible(m_settings.m_powerShowGaussian);
    if (m_powerGaussianSeries) {
        m_powerGaussianSeries->setVisible(m_settings.m_powerShowGaussian);
    }
    ui->powerShowLegend->setChecked(m_settings.m_powerLegend);
    if (m_powerChart) {
        m_powerChart->legend()->setVisible(m_settings.m_powerLegend);
    }
    ui->powerChartSelect->setCurrentIndex((int)m_settings.m_powerYData);
    ui->powerYUnits->setCurrentIndex(powerYUnitsToIndex(m_settings.m_powerYUnits));
    ui->powerShowTsys0->setChecked(m_settings.m_powerShowTsys0);
    ui->powerShowAirTemp->setChecked(m_settings.m_powerShowAirTemp);
    m_airTemps.clicked(m_settings.m_powerShowAirTemp);
    ui->powerShowSensor1->setChecked(m_settings.m_sensorVisible[0]);
    m_sensors[0].setName(m_settings.m_sensorName[0]);
    m_sensors[0].clicked(m_settings.m_sensorVisible[0]);
    ui->powerShowSensor2->setChecked(m_settings.m_sensorVisible[1]);
    m_sensors[1].setName(m_settings.m_sensorName[1]);
    m_sensors[1].clicked(m_settings.m_sensorVisible[1]);
    ui->powerShowFiltered->setChecked(m_settings.m_powerShowFiltered);
    if (m_powerFilteredSeries) {
        m_powerFilteredSeries->setVisible(m_settings.m_powerShowFiltered);
    }
    ui->powerFilterWidgets->setVisible(m_settings.m_powerShowFiltered);
    ui->powerFilter->setCurrentIndex((int)m_settings.m_powerFilter);
    ui->powerFilterN->setValue(m_settings.m_powerFilterN);
    ui->powerShowMeasurement->setChecked(m_settings.m_powerShowMeasurement);
    if (m_powerSeries) {
        m_powerSeries->setVisible(m_settings.m_powerShowMeasurement);
    }

    ui->power2DLinkSweep->setChecked(m_settings.m_power2DLinkSweep);
    ui->power2DSweepType->setCurrentIndex((int)m_settings.m_power2DSweepType);
    ui->power2DWidth->setValue(m_settings.m_power2DWidth);
    ui->power2DHeight->setValue(m_settings.m_power2DHeight);
    ui->power2DXMin->setValue(m_settings.m_power2DXMin);
    ui->power2DXMax->setValue(m_settings.m_power2DXMax);
    ui->power2DYMin->setValue(m_settings.m_power2DYMin);
    ui->power2DYMax->setValue(m_settings.m_power2DYMax);
    ui->powerColourAutoscale->setChecked(m_settings.m_powerColourAutoscale);
    ui->powerColourScaleMin->setValue(m_settings.m_powerColourScaleMin);
    ui->powerColourScaleMin->setEnabled(!m_settings.m_powerColourAutoscale);
    ui->powerColourScaleMax->setValue(m_settings.m_powerColourScaleMax);
    ui->powerColourScaleMax->setEnabled(!m_settings.m_powerColourAutoscale);
    ui->powerColourPalette->setCurrentIndex(ui->powerColourPalette->findText(m_settings.m_powerColourPalette));

    ui->spectrumReverseXAxis->setChecked(m_settings.m_spectrumReverseXAxis);
    ui->spectrumPeak->setChecked(m_settings.m_spectrumPeaks);
    ui->spectrumMarker->setChecked(m_settings.m_spectrumMarkers);
    ui->spectrumTemp->setChecked(m_settings.m_spectrumTemp);
    if (m_fftGaussianSeries) {
        m_fftGaussianSeries->setVisible(m_settings.m_spectrumTemp);
    }
    ui->spectrumShowRefLine->setChecked(m_settings.m_spectrumRefLine);
    if (m_fftHlineSeries)
    {
        m_fftHlineSeries->setVisible(m_settings.m_spectrumRefLine);
        m_fftDopplerAxis->setVisible(m_settings.m_spectrumRefLine);
    }
    ui->spectrumShowLAB->setChecked(m_settings.m_spectrumLAB);
    if (m_fftLABSeries) {
        m_fftLABSeries->setVisible(m_settings.m_spectrumLAB);
    }
    ui->spectrumShowDistance->setChecked(m_settings.m_spectrumDistance);
    updateDistanceColumns();
    ui->spectrumShowLegend->setChecked(m_settings.m_spectrumLegend);
    if (m_fftChart) {
        m_fftChart->legend()->setVisible(m_settings.m_spectrumLegend);
    }
    if (m_calChart) {
        m_calChart->legend()->setVisible(m_settings.m_spectrumLegend);
    }


    ui->refFrame->setCurrentIndex((int)m_settings.m_refFrame);
    ui->spectrumLine->setCurrentIndex((int)m_settings.m_line);
    ui->sunDistanceToGC->setValue(m_settings.m_sunDistanceToGC);
    ui->sunOrbitalVelocity->setValue(m_settings.m_sunOrbitalVelocity);
    displaySpectrumLineFrequency();
    updateSpectrumSelect();
    updatePowerSelect();

    // Updates visibility of widgets
    updateSpectrumMarkerTableVisibility();
    updatePowerMarkerTableVisibility();
    updatePowerChartWidgetsVisibility();
    updateSpectrumChartWidgetsVisibility();

    updateIntegrationTime();

    ui->runMode->setCurrentIndex((int)m_settings.m_runMode);
    ui->sweepStartAtTime->setCurrentIndex(m_settings.m_sweepStartAtTime ? 1 : 0);
    ui->sweepStartDateTime->setDateTime(m_settings.m_sweepStartDateTime);
    ui->sweepStartDateTime->setVisible(m_settings.m_sweepStartAtTime);
    ui->sweepType->setCurrentIndex((int)m_settings.m_sweepType);
    ui->sweep1Start->setValue(m_settings.m_sweep1Start);
    ui->sweep1Stop->setValue(m_settings.m_sweep1Stop);
    ui->sweep1Step->setValue(m_settings.m_sweep1Step);
    ui->sweep1Delay->setValue(m_settings.m_sweep1Delay);
    ui->sweep2Start->setValue(m_settings.m_sweep2Start);
    ui->sweep2Stop->setValue(m_settings.m_sweep2Stop);
    ui->sweep2Step->setValue(m_settings.m_sweep2Step);
    ui->sweep2Delay->setValue(m_settings.m_sweep2Delay);
    displayRunModeSettings();

    updateIndexLabel();

    // Order and size columns
    QHeaderView *header = ui->powerTable->horizontalHeader();
    for (int i = 0; i < RADIOASTRONOMY_POWERTABLE_COLUMNS; i++)
    {
        bool hidden = m_settings.m_powerTableColumnSizes[i] == 0;
        header->setSectionHidden(i, hidden);
        powerTableMenu->actions().at(i)->setChecked(!hidden);
        if (m_settings.m_powerTableColumnSizes[i] > 0)
            ui->powerTable->setColumnWidth(i, m_settings.m_powerTableColumnSizes[i]);
        header->moveSection(header->visualIndex(i), m_settings.m_powerTableColumnIndexes[i]);
    }

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
    getRollupContents()->arrangeRollups();
}

void RadioAstronomyGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void RadioAstronomyGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void RadioAstronomyGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_radioAstronomy->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    double powDbAvg = CalcDb::dbPower(magsqAvg);

    if (m_tickCount % 4 == 0) {
        ui->channelPower->setText(QString::number(powDbAvg, 'f', 1));
    }

    m_tickCount++;
}

void RadioAstronomyGUI::updateRotatorList(const QList<RadioAstronomySettings::AvailableFeature>& rotators)
{
    // Update list of rotators
    ui->rotator->blockSignals(true);
    ui->rotator->clear();
    ui->rotator->addItem("None");

    for (const auto& rotator : rotators)
    {
        QString name = QString("F%1:%2 %3").arg(rotator.m_featureSetIndex).arg(rotator.m_featureIndex).arg(rotator.m_type);
        ui->rotator->addItem(name);
    }

    // Rotator feature can be created after this plugin, so select it
    // if the chosen rotator appears
    int rotatorIndex = ui->rotator->findText(m_settings.m_rotator);

    if (rotatorIndex >= 0) {
        ui->rotator->setCurrentIndex(rotatorIndex);
    } else {
        ui->rotator->setCurrentIndex(0); // return to None
    }

    ui->rotator->blockSignals(false);
}

void RadioAstronomyGUI::on_fftSize_currentIndexChanged(int index)
{
    m_settings.m_fftSize = 1 << (4+index);
    applySettings();
    updateIntegrationTime();
}

void RadioAstronomyGUI::on_fftWindow_currentIndexChanged(int index)
{
    m_settings.m_fftWindow = (RadioAstronomySettings::FFTWindow)index;
    applySettings();
}

void RadioAstronomyGUI::on_filterFreqs_editingFinished()
{
    m_settings.m_filterFreqs = ui->filterFreqs->text();
    applySettings();
}

void RadioAstronomyGUI::on_gainVariation_valueChanged(double value)
{
    m_settings.m_gainVariation = value;
    applySettings();
    updateTSys0();
}

// Can we allow user to enter text that is automatically looked up on SIMBAD?
void RadioAstronomyGUI::on_sourceType_currentIndexChanged(int index)
{
    m_settings.m_sourceType = (RadioAstronomySettings::SourceType)index;
    applySettings();
    if (m_settings.m_sourceType == RadioAstronomySettings::SUN)
    {
        // Mean diameter of Sun in degrees from Earth
        ui->omegaS->setValue(0.53);
        ui->omegaSUnits->setCurrentIndex(0);
    }
    else if (m_settings.m_sourceType == RadioAstronomySettings::CAS_A)
    {
        // Diameter of Cas A in degrees http://simbad.u-strasbg.fr/simbad/sim-id?Ident=Cassiopeia+A
        ui->omegaS->setValue(0.08333);
        ui->omegaSUnits->setCurrentIndex(0);
    }
    bool visible = index == 1 || index >= 3;
    ui->omegaS->setVisible(visible);
    ui->omegaSUnits->setVisible(visible);
}

void RadioAstronomyGUI::on_omegaS_valueChanged(double value)
{
    m_settings.m_omegaS = value;
    if ((m_settings.m_sourceType == RadioAstronomySettings::SUN) && (value != 0.53)) {
        ui->sourceType->setCurrentIndex((int)RadioAstronomySettings::COMPACT);
    } else if ((m_settings.m_sourceType == RadioAstronomySettings::CAS_A) && (value != 0.08333)) {
        ui->sourceType->setCurrentIndex((int)RadioAstronomySettings::COMPACT);
    }
    applySettings();
}

void RadioAstronomyGUI::updateOmegaA()
{
    if (m_settings.m_omegaAUnits == RadioAstronomySettings::DEGREES) {
        ui->omegaA->setText(QString("%1").arg(m_beamWidth, 0, 'f', 1));
    } else {
        ui->omegaA->setText(QString("%1").arg(hpbwToSteradians(m_beamWidth), 0, 'f', 4));
    }
}

void RadioAstronomyGUI::on_omegaAUnits_currentIndexChanged(int index)
{
    m_settings.m_omegaAUnits = (RadioAstronomySettings::AngleUnits)index;
    updateOmegaA();
    if (m_settings.m_omegaAUnits == RadioAstronomySettings::DEGREES) {
        ui->omegaALabel->setText("HPBW");
    } else {
        ui->omegaALabel->setText(QString("%1<sub>A</sub>").arg(QChar(937)));
    }
    applySettings();
}

void RadioAstronomyGUI::on_omegaSUnits_currentIndexChanged(int index)
{
    m_settings.m_omegaSUnits = (RadioAstronomySettings::AngleUnits)index;
    if (  (   (m_settings.m_sourceType == RadioAstronomySettings::SUN)
           || (m_settings.m_sourceType == RadioAstronomySettings::CAS_A)
          )
        && (m_settings.m_omegaSUnits != RadioAstronomySettings::DEGREES)
       )
    {
        ui->sourceType->setCurrentIndex((int)RadioAstronomySettings::COMPACT);
    }
    applySettings();
}

void RadioAstronomyGUI::on_starTracker_currentTextChanged(const QString& text)
{
    m_settings.m_starTracker = text;
    applySettings();
}

void RadioAstronomyGUI::on_rotator_currentTextChanged(const QString& text)
{
    m_settings.m_rotator = text;
    applySettings();
}

void RadioAstronomyGUI::on_showSensors_clicked()
{
    RadioAstronomySensorDialog dialog(&m_settings);
    if (dialog.exec() == QDialog::Accepted)
    {
        m_sensors[0].setName(m_settings.m_sensorName[0]);
        m_sensors[1].setName(m_settings.m_sensorName[1]);
        applySettings();
    }
}

void RadioAstronomyGUI::sensorMeasurementReceived(const RadioAstronomy::MsgSensorMeasurement& measurement)
{
    int sensor = measurement.getSensor();
    double value = measurement.getValue();
    QDateTime dateTime = measurement.getDateTime();
    SensorMeasurement* sm = new SensorMeasurement(dateTime, value);
    m_sensors[sensor].append(sm);
}

void RadioAstronomyGUI::on_powerChartSelect_currentIndexChanged(int index)
{
    m_settings.m_powerYData = (RadioAstronomySettings::PowerYData)index;
    ui->powerYUnits->clear();
    switch (m_settings.m_powerYData)
    {
    case RadioAstronomySettings::PY_POWER:
        ui->powerYUnits->addItem("dBFS");
        ui->powerYUnits->addItem("dBm");
        ui->powerYUnits->addItem("Watts");
        break;
    case RadioAstronomySettings::PY_TSYS:
    case RadioAstronomySettings::PY_TSOURCE:
        ui->powerYUnits->addItem("K");
        break;
    case RadioAstronomySettings::PY_FLUX:
        ui->powerYUnits->addItem("SFU");
        ui->powerYUnits->addItem("Jy");
        break;
    case RadioAstronomySettings::PY_2D_MAP:
        ui->powerYUnits->addItem("dBFS");
        ui->powerYUnits->addItem("dBm");
        //ui->powerYUnits->addItem("Watts"); // No watts for now, as range spin boxes can't handle scientific notation
        ui->powerYUnits->addItem("K");
        break;
    default:
        break;
    }
    updatePowerMarkerTableVisibility();
    updatePowerChartWidgetsVisibility();
    plotPowerChart();
    applySettings();
}

void RadioAstronomyGUI::updatePowerChartWidgetsVisibility()
{
    bool powerChart;
    if (m_settings.m_powerYData != RadioAstronomySettings::PY_2D_MAP) {
        powerChart = true;
    } else {
        powerChart = false;
    }
    ui->powerShowLegend->setVisible(powerChart);
    ui->powerShowSensor2->setVisible(powerChart);
    ui->powerShowSensor1->setVisible(powerChart);
    ui->powerShowAirTemp->setVisible(powerChart);
    ui->powerShowTsys0->setVisible(powerChart);
    ui->powerShowAvg->setVisible(powerChart);
    ui->powerShowGaussian->setVisible(powerChart);
    ui->powerShowMarker->setVisible(powerChart);
    ui->powerShowPeak->setVisible(powerChart);
    ui->powerScaleWidgets->setVisible(powerChart);
    ui->powerGaussianWidgets->setVisible(powerChart && m_settings.m_powerShowGaussian);
    ui->powerMarkerTableWidgets->setVisible(powerChart && (m_settings.m_powerPeaks || m_settings.m_powerMarkers));
    ui->power2DScaleWidgets->setVisible(!powerChart);
    ui->power2DColourScaleWidgets->setVisible(!powerChart);
    getRollupContents()->arrangeRollups();
}

int RadioAstronomyGUI::powerYUnitsToIndex(RadioAstronomySettings::PowerYUnits units)
{
    switch (units)
    {
    case RadioAstronomySettings::PY_DBFS:
        return 0;
    case RadioAstronomySettings::PY_DBM:
        return 1;
    case RadioAstronomySettings::PY_WATTS:
        return 2;
    case RadioAstronomySettings::PY_KELVIN:
        return 0;
    case RadioAstronomySettings::PY_SFU:
        return 0;
    case RadioAstronomySettings::PY_JANSKY:
        return 1;
    }
    return -1;
}

void RadioAstronomyGUI::on_powerYUnits_currentIndexChanged(int index)
{
    (void) index;

    QString text = ui->powerYUnits->currentText();
    if (text == "dBFS")
    {
        m_settings.m_powerYUnits = RadioAstronomySettings::PY_DBFS;
        ui->powerMarkerTable->horizontalHeaderItem(POWER_MARKER_COL_VALUE)->setText("Power (dBFS)");
        ui->powerColourScaleMin->setDecimals(2);
        ui->powerColourScaleMax->setDecimals(2);
    }
    else if (text == "dBm")
    {
        m_settings.m_powerYUnits = RadioAstronomySettings::PY_DBM;
        ui->powerMarkerTable->horizontalHeaderItem(POWER_MARKER_COL_VALUE)->setText("Power (dBm)");
        ui->powerColourScaleMin->setDecimals(2);
        ui->powerColourScaleMax->setDecimals(2);
    }
    else if (text == "Watts")
    {
        m_settings.m_powerYUnits = RadioAstronomySettings::PY_WATTS;
        ui->powerMarkerTable->horizontalHeaderItem(POWER_MARKER_COL_VALUE)->setText("Power (W)");
    }
    else if (text == "K")
    {
        m_settings.m_powerYUnits = RadioAstronomySettings::PY_KELVIN;
        ui->powerMarkerTable->horizontalHeaderItem(POWER_MARKER_COL_VALUE)->setText("Temp (K)");
        ui->powerColourScaleMin->setDecimals(0);
        ui->powerColourScaleMax->setDecimals(0);
    }
    else if (text == "SFU")
    {
        m_settings.m_powerYUnits = RadioAstronomySettings::PY_SFU;
        ui->powerMarkerTable->horizontalHeaderItem(POWER_MARKER_COL_VALUE)->setText("Flux (SFU)");
    }
    else if (text == "Jy")
    {
        m_settings.m_powerYUnits = RadioAstronomySettings::PY_JANSKY;
        ui->powerMarkerTable->horizontalHeaderItem(POWER_MARKER_COL_VALUE)->setText("Flux (Jy)");
    }
    if (text == "dBFS")
    {
        ui->powerColourScaleMinUnits->setText("dB");
        ui->powerColourScaleMaxUnits->setText("dB");
    }
    else
    {
        ui->powerColourScaleMinUnits->setText(text);
        ui->powerColourScaleMaxUnits->setText(text);
    }
    applySettings();
    plotPowerChart();
}

void RadioAstronomyGUI::on_spectrumChartSelect_currentIndexChanged(int index)
{
    updateSpectrumMarkerTableVisibility();  // If this follows updateSpectrumChartWidgetsVisibility, widgets are not redrawn properly.
    updateSpectrumChartWidgetsVisibility();  // when switching from cal to spectrum if table is visible
    if (index == 0)
    {
        if (m_fftChart) {
            ui->spectrumChart->setChart(m_fftChart);
        }
    }
    else
    {
        if (m_calChart) {
            ui->spectrumChart->setChart(m_calChart);
        }
    }
}

void RadioAstronomyGUI::updateSpectrumChartWidgetsVisibility()
{
    bool fft = ui->spectrumChartSelect->currentIndex() == 0;
    ui->spectrumYUnits->setVisible(fft);
    ui->spectrumScaleWidgets->setVisible(fft);
    ui->spectrumSelectWidgets->setVisible(fft);
    ui->spectrumRefLineWidgets->setVisible(fft && m_settings.m_spectrumRefLine);

    ui->spectrumGaussianWidgets->setVisible(fft && m_settings.m_spectrumTemp);
    ui->calWidgets->setVisible(!fft);
    ui->recalibrate->setVisible(!fft);
    ui->startCalHot->setVisible(!fft);
    ui->startCalCold->setVisible(!fft);
    ui->clearCal->setVisible(!fft);
    ui->showCalSettings->setVisible(!fft);
    ui->spectrumShowRefLine->setVisible(fft);
    ui->spectrumShowDistance->setVisible(fft);
    ui->spectrumShowLAB->setVisible(fft);
    ui->spectrumTemp->setVisible(fft);
    ui->spectrumMarker->setVisible(fft);
    ui->spectrumPeak->setVisible(fft);
    ui->saveSpectrumChartImages->setVisible(fft);

    getRollupContents()->arrangeRollups();
}

// Calulate mean, RMS and standard deviation
// Currently this is for all data - but could make it only for visible data
void RadioAstronomyGUI::calcAverages()
{
    qreal sum = 0.0;
    qreal sumSq = 0.0;
    QVector<QPointF> points = m_powerSeries->pointsVector();
    for (int i = 0; i < points.size(); i++)
    {
        QPointF point = points.at(i);
        qreal y = point.y();
        sum += y;
        sumSq += y * y;
    }
    qreal mean = sum / points.size();
    qreal rms = std::sqrt(sumSq / points.size());
    qreal sumSqDiff = 0.0;
    for (int i = 0; i < points.size(); i++)
    {
        QPointF point = points.at(i);
        qreal y = point.y();
        qreal diff = y - mean;
        sumSqDiff += diff * diff;
    }
    qreal sigma = std::sqrt(sumSqDiff / points.size());

    ui->powerMean->setText(QString::number(mean));
    ui->powerRMS->setText(QString::number(rms));
    ui->powerSD->setText(QString::number(sigma));
}

QRgb RadioAstronomyGUI::intensityToColor(float intensity)
{
    QRgb c1, c2;
    float scale;

    if (std::isnan(intensity)) {
        return qRgb(0, 0, 0);
    }
    // Get in range 0-1
    intensity = (intensity - m_settings.m_powerColourScaleMin) / (m_settings.m_powerColourScaleMax - m_settings.m_powerColourScaleMin);
    intensity = std::min(intensity, 1.0f);
    intensity = std::max(intensity, 0.0f);

    if (m_settings.m_powerColourPalette[0] == 'C')
    {
        // Colour heat map gradient
        if (intensity <= 0.25f)
        {
            c1 = qRgb(0, 0, 0x80); // Navy
            c2 = qRgb(0, 0, 0xff); // Blue
            scale = intensity * 4.0f;
        }
        else if (intensity <= 0.5f)
        {
            c1 = qRgb(0, 0, 0xff); // Blue
            c2 = qRgb(0, 0xff, 0); // Green
            scale = (intensity - 0.25f) * 4.0f;
        }
        else if (intensity <= 0.75f)
        {
            c1 = qRgb(0, 0xff, 0); // Green
            c2 = qRgb(0xff, 0xff, 0); // Yellow
            scale = (intensity - 0.5f) * 4.0f;
        }
        else
        {
            c1 = qRgb(0xff, 0xff, 0); // Yellow
            c2 = qRgb(0xff, 0, 0); // Red
            scale = (intensity - 0.75f) * 4.0f;
        }

        int r, g, b;
        r = (qRed(c2)-qRed(c1))*scale+qRed(c1);
        g = (qGreen(c2)-qGreen(c1))*scale+qGreen(c1);
        b = (qBlue(c2)-qBlue(c1))*scale+qBlue(c1);

        return qRgb(r, g, b);
    }
    else
    {
        // Greyscale
        int g = 255 * intensity;
        return qRgb(g, g, g);
    }
}

void RadioAstronomyGUI::plot2DChart()
{
    // Only plot if visible
    if (ui->powerChartSelect->currentIndex() == 4)
    {
        QChart *oldChart = m_2DChart;

        m_2DChart = new QChart();

        m_2DChart->layout()->setContentsMargins(0, 0, 0, 0);
        m_2DChart->setMargins(QMargins(1, 1, 1, 1));
        m_2DChart->setTheme(QChart::ChartThemeDark);
        m_2DChart->setTitle("");

        m_2DXAxis = new QValueAxis();
        m_2DYAxis = new QValueAxis();

        m_2DXAxis->setGridLineVisible(false);
        m_2DYAxis->setGridLineVisible(false);
        set2DAxisTitles();
        m_2DXAxis->setRange(m_settings.m_power2DXMin, m_settings.m_power2DXMax);
        m_2DYAxis->setRange(m_settings.m_power2DYMin, m_settings.m_power2DYMax);

        m_2DChart->addAxis(m_2DXAxis, Qt::AlignBottom);
        m_2DChart->addAxis(m_2DYAxis, Qt::AlignLeft);

        m_2DMap.fill(qRgb(0, 0, 0));
        for (int i = 0; i < m_fftMeasurements.size(); i++) {
            update2DImage(m_fftMeasurements[i], i < m_fftMeasurements.size() - 1);
        }
        if (m_settings.m_powerColourAutoscale) {
            powerColourAutoscale();
        }

        connect(m_2DChart, SIGNAL(plotAreaChanged(QRectF)), this, SLOT(plotAreaChanged(QRectF)));

        ui->powerChart->setChart(m_2DChart);

        delete oldChart;
    }
}

void RadioAstronomyGUI::set2DAxisTitles()
{
    if (m_settings.m_power2DSweepType == RadioAstronomySettings::SWP_LB)
    {
        m_2DXAxis->setTitleText(QString("Galactic longitude (%1)").arg(QChar(0xb0)));
        m_2DYAxis->setTitleText(QString("Galactic latitude (%1)").arg(QChar(0xb0)));
    }
    else
    {
        m_2DXAxis->setTitleText(QString("Azimuth (%1)").arg(QChar(0xb0)));
        m_2DYAxis->setTitleText(QString("Elevation (%1)").arg(QChar(0xb0)));
    }
}

void RadioAstronomyGUI::update2DSettingsFromSweep()
{
    if (m_settings.m_runMode == RadioAstronomySettings::SWEEP)
    {
        ui->power2DSweepType->setCurrentIndex((int)m_settings.m_sweepType);

        // Calculate width and height of image - 1 pixel per sweep measurement
        float sweep1Start, sweep1Stop;
        sweep1Start = m_settings.m_sweep1Start;
        sweep1Stop = m_settings.m_sweep1Stop;
        // Handle azimuth/l sweep through 0. E.g. 340deg -> 20deg with +vs step, or 20deg -> 340deg with -ve step
        if ((m_settings.m_sweep1Stop < m_settings.m_sweep1Start) && (m_settings.m_sweep1Step > 0)) {
            sweep1Stop = m_settings.m_sweep1Stop + 360.0;
        } else if ((m_settings.m_sweep1Stop > m_settings.m_sweep1Start) && (m_settings.m_sweep1Step < 0)) {
            sweep1Start += 360.0;
        }
        int width = abs((sweep1Stop - sweep1Start) / m_settings.m_sweep1Step) + 1;
        int height = abs((m_settings.m_sweep2Stop - m_settings.m_sweep2Start) / m_settings.m_sweep2Step) + 1;
        ui->power2DWidth->setValue(width);
        ui->power2DHeight->setValue(height);

        // Subtract/add half a step so that pixels are centred on coordinates
        int start1 = m_settings.m_sweep1Start - m_settings.m_sweep1Step / 2;
        int stop1 = m_settings.m_sweep1Stop + m_settings.m_sweep1Step / 2;
        if (start1 < stop1)
        {
            ui->power2DXMin->setValue(start1);
            ui->power2DXMax->setValue(stop1);
        }
        else
        {
            ui->power2DXMin->setValue(stop1);
            ui->power2DXMax->setValue(start1);
        }

        int start2 = m_settings.m_sweep2Start - m_settings.m_sweep2Step / 2;
        int stop2 = m_settings.m_sweep2Stop + m_settings.m_sweep2Step / 2;
        if (start2 < stop2)
        {
            ui->power2DYMin->setValue(start2);
            ui->power2DYMax->setValue(stop2);
        }
        else
        {
            ui->power2DYMin->setValue(stop2);
            ui->power2DYMax->setValue(start2);
        }

        m_sweepIndex = 0;
    }
}

void RadioAstronomyGUI::create2DImage()
{
    // Intensity array holds power/temperature values which are then colourised in to a QImage
    delete m_2DMapIntensity;
    int size = m_settings.m_power2DWidth * m_settings.m_power2DHeight;
    if (size > 0)
    {
        m_2DMapIntensity = new float[size];
        for (int i = 0; i < size; i++) {
            m_2DMapIntensity[i] = NAN;
        }
        m_2DMapMin = std::numeric_limits<float>::max();
        m_2DMapMax = -std::numeric_limits<float>::max();
        QImage map(m_settings.m_power2DWidth, m_settings.m_power2DHeight, QImage::Format_ARGB32);
        map.fill(qRgb(0, 0, 0));
        m_2DMap = map;
    }
    else
    {
        m_2DMapIntensity = nullptr;
        m_2DMap = QImage();
    }
}

void RadioAstronomyGUI::update2DImage(FFTMeasurement* fft, bool skipCalcs)
{
    if (m_2DMap.width() > 0)
    {
        int x, y;
        if (m_settings.m_power2DSweepType == RadioAstronomySettings::SWP_OFFSET)
        {
            y = fft->m_sweepIndex / m_2DMap.width();
            x = fft->m_sweepIndex % m_2DMap.width();

            if (m_settings.m_sweep2Step >= 0) {
                y = m_2DMap.height() - 1 - y;
            }
            if (m_settings.m_sweep1Step < 0) {
                x = m_2DMap.width() - 1 - x;
            }
        }
        else
        {
            if (m_settings.m_power2DSweepType == RadioAstronomySettings::SWP_LB)
            {
                x = (int)round(fft->m_l);
                y = (int)round(fft->m_b);
            }
            else
            {
                x = (int)round(fft->m_azimuth);
                y = (int)round(fft->m_elevation);
            }

            // Map coordinates to pixels
            float xRange = m_settings.m_power2DXMax - m_settings.m_power2DXMin;
            float yRange = m_settings.m_power2DYMax - m_settings.m_power2DYMin;

            x = (x - m_settings.m_power2DXMin) * m_settings.m_power2DWidth / xRange;
            y = (y - m_settings.m_power2DYMin) * m_settings.m_power2DHeight / yRange;

            if (yRange >= 0) {
                y = m_2DMap.height() - 1 - y;
            }
            if (xRange < 0) {
                x = m_2DMap.width() - 1 - x;
            }
        }

        if ((x >= 0) && (x < m_2DMap.width()) && (y >= 0) && (y < m_2DMap.height()))
        {
            float intensity;
            switch (m_settings.m_powerYUnits)
            {
            case RadioAstronomySettings::PY_DBFS:
                intensity = fft->m_totalPowerdBFS;
                break;
            case RadioAstronomySettings::PY_DBM:
                intensity = fft->m_totalPowerdBm;
                break;
            case RadioAstronomySettings::PY_WATTS:
                intensity = fft->m_totalPowerWatts;
                break;
            case RadioAstronomySettings::PY_KELVIN:
                intensity = fft->m_tSys;
                break;
            default:
                break;
            }

            float newMin = std::min(m_2DMapMin, intensity);
            float newMax = std::max(m_2DMapMax, intensity);
            bool rescale = false;
            if ((newMin != m_2DMapMin) || (newMax != m_2DMapMax)) {
                rescale = m_settings.m_powerColourAutoscale;
            }
            m_2DMapMin = newMin;
            m_2DMapMax = newMax;
            m_2DMapIntensity[y*m_2DMap.width()+x] = intensity;
            m_2DMap.setPixel(x, y, intensityToColor(intensity));
            if (rescale && !skipCalcs) {
                powerColourAutoscale();
            }
            if (m_2DChart && !skipCalcs) {
                plotAreaChanged(m_2DChart->plotArea());
            }
        }
    }
}

void RadioAstronomyGUI::recolour2DImage()
{
    for (int y = 0; y < m_2DMap.height(); y++)
    {
        for (int x = 0; x < m_2DMap.width(); x++) {
            m_2DMap.setPixel(x, y, intensityToColor(m_2DMapIntensity[y*m_2DMap.width()+x]));
        }
    }
    if (m_2DChart) {
        plotAreaChanged(m_2DChart->plotArea());
    }
}

void RadioAstronomyGUI::plotAreaChanged(const QRectF& plotArea)
{
    if (ui->powerChartSelect->currentIndex() == 4)
    {
        int width = static_cast<int>(plotArea.width());
        int height = static_cast<int>(plotArea.height());
        int viewW = static_cast<int>(ui->powerChart->width());
        int viewH = static_cast<int>(ui->powerChart->height());

        // Scale the image to fit plot area
        QImage image = m_2DMap.scaled(QSize(width, height), Qt::IgnoreAspectRatio);
        QImage translated(viewW, viewH, QImage::Format_ARGB32);
        translated.fill(Qt::black);
        QPainter painter(&translated);
        painter.drawImage(plotArea.topLeft(), image);

        m_2DChart->setPlotAreaBackgroundBrush(translated);
        m_2DChart->setPlotAreaBackgroundVisible(true);
    }
}

// Find min and max coordinates from existing data
void RadioAstronomyGUI::power2DAutoscale()
{
    if (m_fftMeasurements.size() > 0)
    {
        float minX = std::numeric_limits<float>::max();
        float maxX = -std::numeric_limits<float>::max();
        float minY = std::numeric_limits<float>::max();
        float maxY = -std::numeric_limits<float>::max();

        for (int i = 0; i < m_fftMeasurements.size(); i++)
        {
            FFTMeasurement* fft = m_fftMeasurements[i];
            float x, y;
            if (m_settings.m_power2DSweepType == RadioAstronomySettings::SWP_LB)
            {
                x = fft->m_l;
                y = fft->m_b;
            }
            else
            {
                x = fft->m_azimuth;
                y = fft->m_elevation;
            }
            minX = std::min(minX, x);
            maxX = std::max(maxX, x);
            minY = std::min(minY, y);
            maxY = std::max(maxY, y);
        }

        // Adjust so pixels are centered
        float xAdjust = (maxX - minX) / m_2DMap.width() / 2;
        float yAdjust = (maxY - minY) / m_2DMap.height() / 2;

        ui->power2DXMin->setValue(minX - xAdjust);
        ui->power2DXMax->setValue(maxX + xAdjust);
        ui->power2DYMin->setValue(minY - yAdjust);
        ui->power2DYMax->setValue(maxY + xAdjust);
    }
}

void RadioAstronomyGUI::on_power2DAutoscale_clicked()
{
    power2DAutoscale();
    plot2DChart();
}

void RadioAstronomyGUI::on_power2DLinkSweep_toggled(bool checked)
{
    m_settings.m_power2DLinkSweep = checked;
    applySettings();
}

void RadioAstronomyGUI::on_power2DSweepType_currentIndexChanged(int index)
{
    m_settings.m_power2DSweepType = (RadioAstronomySettings::SweepType)index;
    applySettings();
    plot2DChart();
}

void RadioAstronomyGUI::on_power2DWidth_valueChanged(int value)
{
    m_settings.m_power2DWidth = value;
    applySettings();
    create2DImage();
    plot2DChart();
}

void RadioAstronomyGUI::on_power2DHeight_valueChanged(int value)
{
    m_settings.m_power2DHeight = value;
    applySettings();
    create2DImage();
    plot2DChart();
}

void RadioAstronomyGUI::on_power2DXMin_valueChanged(double value)
{
    m_settings.m_power2DXMin = value;
    applySettings();
    if (m_2DXAxis)
    {
        m_2DXAxis->setMin(m_settings.m_power2DXMin);
        plot2DChart();
    }
}

void RadioAstronomyGUI::on_power2DXMax_valueChanged(double value)
{
    m_settings.m_power2DXMax = value;
    applySettings();
    if (m_2DXAxis)
    {
        m_2DXAxis->setMax(m_settings.m_power2DXMax);
        plot2DChart();
    }
}

void RadioAstronomyGUI::on_power2DYMin_valueChanged(double value)
{
    m_settings.m_power2DYMin = value;
    applySettings();
    if (m_2DYAxis)
    {
        m_2DYAxis->setMin(m_settings.m_power2DYMin);
        plot2DChart();
    }
}

void RadioAstronomyGUI::on_power2DYMax_valueChanged(double value)
{
    m_settings.m_power2DYMax = value;
    applySettings();
    if (m_2DYAxis)
    {
        m_2DYAxis->setMax(m_settings.m_power2DYMax);
        plot2DChart();
    }
}

void RadioAstronomyGUI::powerColourAutoscale()
{
    int width = m_2DMap.width();
    int height = m_2DMap.height();
    float newMin = std::numeric_limits<float>::max();
    float newMax = -std::numeric_limits<float>::max();
    for (int i = 0; i < width * height; i++)
    {
        if (!std::isnan(m_2DMapIntensity[i]))
        {
            newMin = std::min(newMin, m_2DMapIntensity[i]);
            newMax = std::max(newMax, m_2DMapIntensity[i]);
        }
    }

    if ((newMin != ui->powerColourScaleMin->value()) || (newMax != ui->powerColourScaleMax->value()))
    {
        ui->powerColourScaleMin->setValue(std::floor(newMin * 10.0) / 10.0);
        ui->powerColourScaleMax->setValue(std::ceil(newMax * 10.0) / 10.0);
    }
}

void RadioAstronomyGUI::on_powerColourAutoscale_toggled(bool checked)
{
    m_settings.m_powerColourAutoscale = checked;
    applySettings();
    if (m_settings.m_powerColourAutoscale) {
        powerColourAutoscale();
    }
    ui->powerColourScaleMin->setEnabled(!m_settings.m_powerColourAutoscale);
    ui->powerColourScaleMax->setEnabled(!m_settings.m_powerColourAutoscale);
}

void RadioAstronomyGUI::updatePowerColourScaleStep()
{
    float diff = abs(m_settings.m_powerColourScaleMax - m_settings.m_powerColourScaleMin);
    double step = (diff <= 1.0f) ? 0.1 : 1.0f;
    ui->powerColourScaleMin->setSingleStep(step);
    ui->powerColourScaleMax->setSingleStep(step);
}

void RadioAstronomyGUI::on_powerColourScaleMin_valueChanged(double value)
{
    m_settings.m_powerColourScaleMin = value;
    updatePowerColourScaleStep();
    applySettings();
    recolour2DImage();
}

void RadioAstronomyGUI::on_powerColourScaleMax_valueChanged(double value)
{
    m_settings.m_powerColourScaleMax = value;
    updatePowerColourScaleStep();
    applySettings();
    recolour2DImage();
}

void RadioAstronomyGUI::on_powerColourPalette_currentIndexChanged(int index)
{
    (void) index;
    m_settings.m_powerColourPalette = ui->powerColourPalette->currentText();
    applySettings();
    recolour2DImage();
}

void RadioAstronomyGUI::plotPowerChart()
{
    if (ui->powerChartSelect->currentIndex() == 4) {
        plot2DChart();
    } else {
        plotPowerVsTimeChart();
    }
}

void RadioAstronomyGUI::plotPowerVsTimeChart()
{
    QChart *oldChart = m_powerChart;

    m_powerChart = new QChart();

    m_powerChart->layout()->setContentsMargins(0, 0, 0, 0);
    m_powerChart->setMargins(QMargins(1, 1, 1, 1));
    m_powerChart->setTheme(QChart::ChartThemeDark);

    m_powerChart->legend()->setAlignment(Qt::AlignRight);
    m_powerChart->legend()->setVisible(m_settings.m_powerLegend);

    // Create measurement data series
    m_powerSeries = new QLineSeries();
    m_powerSeries->setVisible(m_settings.m_powerShowMeasurement);
    connect(m_powerSeries, &QXYSeries::clicked, this, &RadioAstronomyGUI::powerSeries_clicked);

    // Plot peak info
    m_powerPeakSeries = new QScatterSeries();
    m_powerPeakSeries->setName("Peak");
    m_powerPeakSeries->setPointLabelsVisible(true);
    m_powerPeakSeries->setPointLabelsFormat("@yPoint"); // Qt can't display dates, so omit @xPoint
    m_powerPeakSeries->setMarkerSize(5);
    m_powerPeakSeries->setVisible(m_settings.m_powerPeaks);

    // Markers
    m_powerMarkerSeries = new QScatterSeries();
    m_powerMarkerSeries->setName("Marker");
    m_powerMarkerSeries->setPointLabelsVisible(true);
    m_powerMarkerSeries->setPointLabelsFormat("@yPoint");
    m_powerMarkerSeries->setMarkerSize(5);
    m_powerMarkerSeries->setVisible(m_settings.m_powerMarkers);

    // Noise
    m_powerTsys0Series = new QLineSeries();
    m_powerTsys0Series->setName("Tsys0");
    m_powerTsys0Series->setVisible(m_settings.m_powerShowTsys0);

    // Air temperature
    m_airTemps.init("Air temp", m_settings.m_powerShowAirTemp);

    // Gaussian
    m_powerGaussianSeries = new QLineSeries();
    m_powerGaussianSeries->setName("Gaussian fit");
    m_powerGaussianSeries->setVisible(m_settings.m_powerShowGaussian);

    // Filtered measurement
    m_powerFilteredSeries = new QLineSeries();
    m_powerFilteredSeries->setName("Filtered");
    m_powerFilteredSeries->setVisible(m_settings.m_powerShowFiltered);
    plotPowerFiltered();

    // Sensors
    for (int i = 0; i < RADIOASTRONOMY_SENSORS; i++) {
        m_sensors[i].init(m_settings.m_sensorName[i], m_settings.m_sensorVisible[i]);
    }

    // Reset min/max and peaks
    m_powerMin = std::numeric_limits<double>::max();
    m_powerMax = -std::numeric_limits<double>::max();
    m_powerPeakValid = false;

    // Create X axis
    m_powerXAxis = new QDateTimeAxis();
    int rows = ui->powerTable->rowCount();
    QString dateTimeFormat = "hh:mm:ss";
    m_powerXAxisSameDay = true;
    if (rows > 1)
    {
        QDate start = ui->powerTable->item(0, POWER_COL_DATE)->data(Qt::DisplayRole).toDate();
        QDate end = ui->powerTable->item(rows-1, POWER_COL_DATE)->data(Qt::DisplayRole).toDate();
        if (start != end)
        {
            dateTimeFormat = QString("%1 hh:mm").arg(QLocale::system().dateFormat(QLocale::ShortFormat));
            m_powerXAxisSameDay = false;
        }
    }
    m_powerXAxis->setFormat(dateTimeFormat);
    m_powerXAxis->setRange(ui->powerStartTime->dateTime(), ui->powerEndTime->dateTime());
    ui->powerStartTime->setDisplayFormat(dateTimeFormat);
    ui->powerEndTime->setDisplayFormat(dateTimeFormat);

    // Create Y axis
    m_powerYAxis = new QValueAxis();

    m_powerXAxis->setTitleText("Time");
    calcPowerChartTickCount(size().width());
    switch (m_settings.m_powerYData)
    {
    case RadioAstronomySettings::PY_POWER:
        m_powerSeries->setName("Measurement");
        switch (m_settings.m_powerYUnits)
        {
        case RadioAstronomySettings::PY_DBFS:
            m_powerYAxis->setTitleText("Power (dBFS)");
            break;
        case RadioAstronomySettings::PY_DBM:
            m_powerYAxis->setTitleText("Power (dBm)");
            break;
        case RadioAstronomySettings::PY_WATTS:
            m_powerYAxis->setTitleText("Power (Watts)");
            break;
        default:
            break;
        }
        break;
    case RadioAstronomySettings::PY_TSYS:
        m_powerSeries->setName("Tsys");
        m_powerYAxis->setTitleText("Tsys (K)");
        break;
    case RadioAstronomySettings::PY_TSOURCE:
        m_powerSeries->setName("Tsource");
        m_powerYAxis->setTitleText("Tsource (K)");
        break;
    case RadioAstronomySettings::PY_FLUX:
        m_powerSeries->setName("Flux density");
        switch (m_settings.m_powerYUnits)
        {
        case RadioAstronomySettings::PY_SFU:
            m_powerYAxis->setTitleText("Flux density (SFU)");
            break;
        case RadioAstronomySettings::PY_JANSKY:
            m_powerYAxis->setTitleText("Flux density (Jy)");
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }

    m_powerChart->addAxis(m_powerXAxis, Qt::AlignBottom);
    m_powerChart->addAxis(m_powerYAxis, Qt::AlignLeft);
    m_powerChart->addAxis(m_airTemps.yAxis(), Qt::AlignRight);
    for (int i = 0; i < RADIOASTRONOMY_SENSORS; i++) {
        m_powerChart->addAxis(m_sensors[i].yAxis(), Qt::AlignRight);
    }

    // Add data to series and calculate peaks
    for (int i = 0; i < m_fftMeasurements.size(); i++) {
        addToPowerSeries(m_fftMeasurements[i], i < (m_fftMeasurements.size() - 1));
    }
    m_airTemps.addAllToSeries();
    for (int i = 0; i < RADIOASTRONOMY_SENSORS; i++) {
        m_sensors[i].addAllToSeries();
    }

    m_powerChart->addSeries(m_powerSeries);
    m_powerSeries->attachAxis(m_powerXAxis);
    m_powerSeries->attachAxis(m_powerYAxis);

    m_powerChart->addSeries(m_powerTsys0Series);
    m_powerTsys0Series->attachAxis(m_powerXAxis);
    m_powerTsys0Series->attachAxis(m_powerYAxis);

    m_powerChart->addSeries(m_powerGaussianSeries);
    m_powerGaussianSeries->attachAxis(m_powerXAxis);
    m_powerGaussianSeries->attachAxis(m_powerYAxis);

    m_airTemps.addToChart(m_powerChart, m_powerXAxis);
    for (int i = 0; i < RADIOASTRONOMY_SENSORS; i++) {
        m_sensors[i].addToChart(m_powerChart, m_powerXAxis);
    }

    m_powerChart->addSeries(m_powerFilteredSeries);
    m_powerFilteredSeries->attachAxis(m_powerXAxis);
    m_powerFilteredSeries->attachAxis(m_powerYAxis);

    m_powerChart->addSeries(m_powerPeakSeries);
    m_powerPeakSeries->attachAxis(m_powerXAxis);
    m_powerPeakSeries->attachAxis(m_powerYAxis);

    m_powerChart->addSeries(m_powerMarkerSeries);
    m_powerMarkerSeries->attachAxis(m_powerXAxis);
    m_powerMarkerSeries->attachAxis(m_powerYAxis);

    // Dark theme only has 5 colours for series, so use an extra unique colour (purple)
    QPen pen(QColor(qRgb(146, 65, 146)), 2, Qt::SolidLine);
    m_sensors[1].setPen(pen);

    // Don't have peaks and markers in legend
    m_powerChart->legend()->markers(m_powerPeakSeries)[0]->setVisible(false);
    m_powerChart->legend()->markers(m_powerMarkerSeries)[0]->setVisible(false);

    ui->powerChart->setChart(m_powerChart);

    delete oldChart;
}

void RadioAstronomyGUI::calCompletetReceived(const RadioAstronomy::MsgCalComplete& measurement)
{
    bool hot = measurement.getHot();
    int size = measurement.getSize();
    Real* data = measurement.getCal();

    FFTMeasurement* fft = new FFTMeasurement();
    if (hot)
    {
        delete m_calHot;
        m_calHot = fft;
        ui->startCalHot->setStyleSheet("QToolButton { background: none; }");
    }
    else
    {
        delete m_calCold;
        m_calCold = fft;
        ui->startCalCold->setStyleSheet("QToolButton { background: none; }");
    }
    fft->m_fftData = data;
    fft->m_fftSize = size;
    fft->m_dateTime = measurement.getDateTime();
    fft->m_centerFrequency = m_centerFrequency;
    fft->m_sampleRate = m_settings.m_sampleRate;
    fft->m_integration = m_settings.m_integration;
    fft->m_rfBandwidth = m_settings.m_rfBandwidth;
    fft->m_omegaA = calcOmegaA();
    fft->m_omegaS = calcOmegaS();
    fft->m_coordsValid = m_coordsValid;
    fft->m_ra = m_ra;
    fft->m_dec = m_dec;
    fft->m_azimuth = m_azimuth;
    fft->m_elevation = m_elevation;
    fft->m_l = m_l;
    fft->m_b = m_b;
    fft->m_vBCRS = m_vBCRS;
    fft->m_vLSR = m_vLSR;
    fft->m_solarFlux = m_solarFlux;
    fft->m_airTemp = m_airTemps.lastValue();
    fft->m_skyTemp = m_skyTemp;
    for (int i = 0; i < RADIOASTRONOMY_SENSORS; i++) {
        fft->m_sensor[i] = m_sensors[i].lastValue();
    }
    fft->m_tSys0 = calcTSys0();
    fft->m_baseline = m_settings.m_spectrumBaseline;

    if (!hot) {
        ui->calTsky->setText(QString::number(m_skyTemp, 'f', 1));
    }

    calcFFTTotalPower(fft);
    calcCalAvgDiff();
    calibrate();
    calcFFTTemperatures(fft);
    calcFFTTotalTemperature(fft);
    plotCalMeasurements();
}

void RadioAstronomyGUI::plotCalMeasurements()
{
    m_calHotSeries->clear();
    m_calColdSeries->clear();

    if (m_calHot || m_calCold)
    {
        float minVal = std::numeric_limits<float>::max();
        float maxVal = -std::numeric_limits<float>::max();

        double size;
        double sampleRate;
        double centerFrequency;
        if (m_calCold && m_calHot)
        {
            size = (double)std::min(m_calCold->m_fftSize, m_calHot->m_fftSize);
            sampleRate = (double)m_calCold->m_sampleRate;
            centerFrequency = (double)m_calCold->m_centerFrequency;
        }
        else if (m_calCold)
        {
            size = (double)m_calCold->m_fftSize;
            sampleRate = (double)m_calCold->m_sampleRate;
            centerFrequency = (double)m_calCold->m_centerFrequency;
        }
        else
        {
            size = (double)m_calHot->m_fftSize;
            sampleRate = (double)m_calHot->m_sampleRate;
            centerFrequency = (double)m_calHot->m_centerFrequency;
        }

        double binRes = sampleRate / size;
        double startFreq = centerFrequency - sampleRate / 2.0;
        double freq = startFreq;
        for (int i = 0; i < size; i++)
        {
            float value;
            bool hotValid = m_calHot && (i < m_calHot->m_fftSize);
            bool coldValid = m_calCold && (i < m_calCold->m_fftSize);

            if (hotValid)
            {
                value = CalcDb::dbPower(m_calHot->m_fftData[i]);
                m_calHotSeries->append(freq / 1e6, value);
                minVal = std::min(minVal, value);
                maxVal = std::max(maxVal, value);
            }
            if (coldValid)
            {
                value = CalcDb::dbPower(m_calCold->m_fftData[i]);
                m_calColdSeries->append(freq / 1e6, value);
                minVal = std::min(minVal, value);
                maxVal = std::max(maxVal, value);
            }
            freq += binRes;
        }

        m_calYAxis->setRange(minVal, maxVal);

        double startFreqMHz = centerFrequency/1e6 - sampleRate/ 1e6 / 2.0;
        double endFreqMHz = centerFrequency/1e6 + sampleRate/ 1e6 / 2.0;
        m_calXAxis->setRange(startFreqMHz, endFreqMHz);
        m_calXAxis->setReverse(m_settings.m_spectrumReverseXAxis);
    }
}

void RadioAstronomyGUI::plotCalSpectrum()
{
    QChart *oldChart = m_calChart;

    m_calChart = new QChart();

    m_calChart->layout()->setContentsMargins(0, 0, 0, 0);
    m_calChart->setMargins(QMargins(1, 1, 1, 1));
    m_calChart->setTheme(QChart::ChartThemeDark);

    m_calChart->legend()->setAlignment(Qt::AlignRight);
    m_calChart->legend()->setVisible(m_settings.m_spectrumLegend);

    m_calHotSeries = new QLineSeries();
    m_calHotSeries->setName("Hot");
    m_calColdSeries = new QLineSeries();
    m_calColdSeries->setName("Cold");

    m_calXAxis = new QValueAxis();
    m_calYAxis = new QValueAxis();

    m_calChart->addAxis(m_calXAxis, Qt::AlignBottom);
    m_calChart->addAxis(m_calYAxis, Qt::AlignLeft);

    m_calXAxis->setTitleText("Frequency (MHz)");
    calcSpectrumChartTickCount(m_calXAxis, size().width());
    m_calYAxis->setTitleText("Power (dBFS)");

    m_calChart->addSeries(m_calHotSeries);
    m_calHotSeries->attachAxis(m_calXAxis);
    m_calHotSeries->attachAxis(m_calYAxis);

    m_calChart->addSeries(m_calColdSeries);
    m_calColdSeries->attachAxis(m_calXAxis);
    m_calColdSeries->attachAxis(m_calYAxis);

    plotCalMeasurements();

    ui->spectrumChart->setChart(m_calChart);

    delete oldChart;
}

void RadioAstronomyGUI::on_spectrumReference_valueChanged(double value)
{
    m_settings.m_spectrumReference = value;
    spectrumUpdateYRange();
    if (!m_settings.m_spectrumAutoscale) {
        applySettings();
    }
}

void RadioAstronomyGUI::on_spectrumRange_valueChanged(double value)
{
    m_settings.m_spectrumRange = value;
    if (m_settings.m_spectrumRange <= 1.0)
    {
        ui->spectrumRange->setSingleStep(0.1);
        ui->spectrumRange->setDecimals(2);
        ui->spectrumReference->setDecimals(2);
    }
    else
    {
        ui->spectrumRange->setSingleStep(1.0);
        ui->spectrumRange->setDecimals(1);
        ui->spectrumReference->setDecimals(1);
    }
    spectrumUpdateYRange();
    if (!m_settings.m_spectrumAutoscale) {
        applySettings();
    }
}

void RadioAstronomyGUI::on_spectrumSpan_valueChanged(double value)
{
    m_settings.m_spectrumSpan = value;
    spectrumUpdateXRange();
    applySettings();
}

void RadioAstronomyGUI::on_spectrumCenterFreq_valueChanged(double value)
{
    double offset;
    FFTMeasurement* fft = currentFFT();
    if (fft) {
        offset = value - fft->m_centerFrequency/1e6;
    } else {
        offset = value - m_centerFrequency/1e6;
    }
    m_settings.m_spectrumCenterFreqOffset = offset;
    spectrumUpdateXRange();
    applySettings();
}

void RadioAstronomyGUI::spectrumUpdateXRange(FFTMeasurement* fft)
{
    if (!fft) {
        fft = currentFFT();
    }
    if (m_fftXAxis && fft)
    {
        double startFreqMHz = fft->m_centerFrequency/1e6 - m_settings.m_spectrumSpan / 2.0;
        double endFreqMHz = fft->m_centerFrequency/1e6 + m_settings.m_spectrumSpan / 2.0;
        m_fftXAxis->setRange(startFreqMHz + m_settings.m_spectrumCenterFreqOffset, endFreqMHz + m_settings.m_spectrumCenterFreqOffset);
        double lineFreqMHz = ui->spectrumLineFrequency->value();
        double lineFreq = lineFreqMHz * 1e6;
        double startFreq = fft->m_centerFrequency + m_settings.m_spectrumSpan*1e6 / 2.0 + m_settings.m_spectrumCenterFreqOffset*1e6;
        double endFreq = fft->m_centerFrequency - m_settings.m_spectrumSpan*1e6 / 2.0 + m_settings.m_spectrumCenterFreqOffset*1e6;
        m_fftDopplerAxis->setRange(dopplerToVelocity(lineFreq, startFreq, fft),
                                   dopplerToVelocity(lineFreq, endFreq, fft));
    }
}

void RadioAstronomyGUI::spectrumUpdateYRange(FFTMeasurement* fft)
{
    if (!fft) {
        fft = currentFFT();
    }
    if (m_fftYAxis && fft) {
        m_fftYAxis->setRange(m_settings.m_spectrumReference - m_settings.m_spectrumRange, m_settings.m_spectrumReference);
    }
}

void RadioAstronomyGUI::spectrumAutoscale()
{
    if (m_settings.m_spectrumAutoscale)
    {
        on_spectrumAutoscaleX_clicked();
        on_spectrumAutoscaleY_clicked();
    }
}

// Scale Y axis according to min and max values
void RadioAstronomyGUI::on_spectrumAutoscale_toggled(bool checked)
{
    m_settings.m_spectrumAutoscale = checked;
    ui->spectrumAutoscaleX->setEnabled(!m_settings.m_spectrumAutoscale);
    ui->spectrumAutoscaleY->setEnabled(!m_settings.m_spectrumAutoscale);
    ui->spectrumReference->setEnabled(!m_settings.m_spectrumAutoscale);
    ui->spectrumRange->setEnabled(!m_settings.m_spectrumAutoscale);
    ui->spectrumCenterFreq->setEnabled(!m_settings.m_spectrumAutoscale);
    ui->spectrumSpan->setEnabled(!m_settings.m_spectrumAutoscale);
    spectrumAutoscale();
    applySettings();
}

// Get minimum and maximum values in a series
// Assumes minVal and maxVal are initialised
static bool seriesMinAndMax(QLineSeries* series, qreal& minVal, qreal& maxVal)
{
    QVector<QPointF> points = series->pointsVector();
    for (int i = 0; i < points.size(); i++)
    {
        qreal power = points[i].y();
        minVal = std::min(minVal, power);
        maxVal = std::max(maxVal, power);
    }
    return points.size() > 0;
}

// Scale Y axis according to min and max values
void RadioAstronomyGUI::on_spectrumAutoscaleY_clicked()
{
    double minVal = std::numeric_limits<double>::max();
    double maxVal = -std::numeric_limits<double>::max();
    bool v0 = false, v1 = false;
    if (m_fftSeries) {
        v0 = seriesMinAndMax(m_fftSeries, minVal, maxVal);
    }
    if (m_fftLABSeries && m_settings.m_spectrumLAB) {
        v1 = seriesMinAndMax(m_fftLABSeries, minVal, maxVal);
    }
    if (v0 || v1)
    {
        double range = (maxVal - minVal) * 1.2; // 20% wider than signal range, for space for markers
        range = std::max(0.1, range);     // Don't be smaller than minimum value we can set in GUI
        ui->spectrumRange->setValue(range); // Call before setting reference, so number of decimals are adjusted
        ui->spectrumReference->setValue(maxVal + range * 0.15);
    }
}

// Scale X axis according to min and max values
void RadioAstronomyGUI::on_spectrumAutoscaleX_clicked()
{
    FFTMeasurement* fft = currentFFT();
    if (fft)
    {
        ui->spectrumSpan->setValue(fft->m_sampleRate/1e6);
        ui->spectrumCenterFreq->setValue(fft->m_centerFrequency/1e6);
    }
    else
    {
        ui->spectrumSpan->setValue(m_basebandSampleRate/1e6);
        ui->spectrumCenterFreq->setValue(m_centerFrequency/1e6);
    }
}

RadioAstronomyGUI::FFTMeasurement* RadioAstronomyGUI::currentFFT()
{
    int index = ui->spectrumIndex->value();
    if ((index >= 0) && (index < m_fftMeasurements.size())) {
        return m_fftMeasurements[index];
    } else {
        return nullptr;
    }
}

void RadioAstronomyGUI::on_spectrumYUnits_currentIndexChanged(int index)
{
    (void) index;

    QString text = ui->spectrumYUnits->currentText();
    if (text == "dBFS")
    {
        m_settings.m_spectrumYScale = RadioAstronomySettings::SY_DBFS;
        ui->spectrumMarkerTable->horizontalHeaderItem(SPECTRUM_MARKER_COL_VALUE)->setText("Power (dBFS)");
    }
    else if (text == "SNR")
    {
        m_settings.m_spectrumYScale = RadioAstronomySettings::SY_SNR;
        ui->spectrumMarkerTable->horizontalHeaderItem(SPECTRUM_MARKER_COL_VALUE)->setText("SNR");
    }
    else if (text == "dBm")
    {
        m_settings.m_spectrumYScale = RadioAstronomySettings::SY_DBM;
        ui->spectrumMarkerTable->horizontalHeaderItem(SPECTRUM_MARKER_COL_VALUE)->setText("Power (dBm)");
    }
    else if (text == "Tsys K")
    {
        m_settings.m_spectrumYScale = RadioAstronomySettings::SY_TSYS;
        ui->spectrumMarkerTable->horizontalHeaderItem(SPECTRUM_MARKER_COL_VALUE)->setText("Tsys (K)");
    }
    else
    {
        m_settings.m_spectrumYScale = RadioAstronomySettings::SY_TSOURCE;
        ui->spectrumMarkerTable->horizontalHeaderItem(SPECTRUM_MARKER_COL_VALUE)->setText("Tsource (K)");
    }
    plotFFTMeasurement();
    applySettings();
}

void RadioAstronomyGUI::on_spectrumBaseline_currentIndexChanged(int index)
{
    m_settings.m_spectrumBaseline = (RadioAstronomySettings::SpectrumBaseline)index;
    plotFFTMeasurement();
    if ((m_settings.m_powerYData == RadioAstronomySettings::PY_TSOURCE) || (m_settings.m_powerYData == RadioAstronomySettings::PY_FLUX)) {
        plotPowerChart();
    }
    applySettings();
}

// Convert frequency shift to velocity in km/s (+ve approaching)
static double lineDopplerVelocity(double centre, double f)
{
    return Astronomy::dopplerToVelocity(f, centre) / 1000.0f;
}

// Convert frequency shift to velocity (+ve receeding - which seems to be the astronomical convention)
double RadioAstronomyGUI::dopplerToVelocity(double centre, double f, FFTMeasurement *fft)
{
    double v = lineDopplerVelocity(centre, f);
    // Adjust in to selected reference frame
    switch (m_settings.m_refFrame)
    {
    case RadioAstronomySettings::BCRS:
        v -= fft->m_vBCRS;
        break;
    case RadioAstronomySettings::LSR:
        v -= fft->m_vLSR;
        break;
    default:
        break;
    }
    // Make +ve receeding
    return -v;
}

// Replot current FFT
void RadioAstronomyGUI::plotFFTMeasurement()
{
    plotFFTMeasurement(ui->spectrumIndex->value());
}

// Plot FFT with specified index
void RadioAstronomyGUI::plotFFTMeasurement(int index)
{
    if (index < m_fftMeasurements.size())
    {
        FFTMeasurement *fft = m_fftMeasurements[index];

        m_fftSeries->clear();
        m_fftHlineSeries->clear();
        m_fftGaussianSeries->clear();
        m_fftLABSeries->clear();
        m_fftPeakSeries->clear();

        double binRes = fft->m_sampleRate / (double)fft->m_fftSize;
        double startFreq = fft->m_centerFrequency - fft->m_sampleRate / 2.0;

        // Plot reference spectral line and Doppler axis
        plotRefLine(fft);

        // Plot Gaussian for temp estimation
        plotTempGaussian(startFreq, binRes, fft->m_fftSize);

        // Plot LAB reference data
        if (   fft->m_coordsValid && m_settings.m_spectrumLAB
            && (   (m_settings.m_spectrumYScale == RadioAstronomySettings::SY_TSYS)
                || (m_settings.m_spectrumYScale == RadioAstronomySettings::SY_TSOURCE)
               )
           )
        {
            plotLAB(fft->m_l, fft->m_b, m_beamWidth);
        }

        if (   ((m_settings.m_spectrumYScale == RadioAstronomySettings::SY_SNR) && !fft->m_snr)
            || ((m_settings.m_spectrumYScale == RadioAstronomySettings::SY_DBM) && !fft->m_temp)
            || ((m_settings.m_spectrumYScale == RadioAstronomySettings::SY_TSYS) && !fft->m_temp)
            || ((m_settings.m_spectrumYScale == RadioAstronomySettings::SY_TSOURCE) && !fft->m_temp)
           )
        {
            m_fftChart->setTitle("No cal data: Run calibration or set units to dBFS.");
        }
        else
        {
            m_fftChart->setTitle("");

            if (fft->m_coordsValid)
            {
                m_fftChart->setTitle(QString("RA: %1 Dec: %2 l: %3%7 b: %4%7 Az: %5%7 El: %6%7")
                                            .arg(Units::decimalHoursToHoursMinutesAndSeconds(fft->m_ra))
                                            .arg(Units::decimalDegreesToDegreeMinutesAndSeconds(fft->m_dec))
                                            .arg(QString::number(fft->m_l, 'f', 1))
                                            .arg(QString::number(fft->m_b, 'f', 1))
                                            .arg(QString::number(fft->m_azimuth, 'f', 0))
                                            .arg(QString::number(fft->m_elevation, 'f', 0))
                                            .arg(QChar(0xb0)));
            }

            int peakIdx = 0;
            double peakValue = -std::numeric_limits<double>::max();

            double freq = startFreq;   // Main spectrum seems to use bin midpoint - this uses lowest frequency, so we're tone at centre freq appears in centre of plot

            // Plot power/temp
            for (int i = 0; i < fft->m_fftSize; i++)
            {
                qreal value;
                switch (m_settings.m_spectrumYScale)
                {
                case RadioAstronomySettings::SY_DBFS:
                    value = fft->m_db[i];
                    break;
                case RadioAstronomySettings::SY_SNR:
                    value = fft->m_snr[i];
                    break;
                case RadioAstronomySettings::SY_DBM:
                    value = Astronomy::noisePowerdBm(fft->m_temp[i], fft->m_sampleRate/(double)fft->m_fftSize);
                    break;
                case RadioAstronomySettings::SY_TSYS:
                    value = fft->m_temp[i];
                    break;
                case RadioAstronomySettings::SY_TSOURCE:
                    switch (m_settings.m_spectrumBaseline)
                    {
                    case RadioAstronomySettings::SBL_TSYS0:
                        value = fft->m_temp[i] - fft->m_tSys0;
                        break;
                    case RadioAstronomySettings::SBL_TMIN:
                        value = fft->m_temp[i] - fft->m_tempMin;
                        break;
                    case RadioAstronomySettings::SBL_CAL_COLD:
                        if (m_calCold) {
                            value = m_calG[i] * (fft->m_fftData[i] - m_calCold->m_fftData[i]);
                        } else {
                            value = 0.0;
                        }
                        break;
                    }
                    break;
                }
                if (value > peakValue)
                {
                    peakValue = value;
                    peakIdx = i;
                }

                m_fftSeries->append(freq / 1e6, value);
                freq += binRes;
            }

            double startFreqMHz = fft->m_centerFrequency/1e6 - m_settings.m_spectrumSpan / 2.0;

            spectrumUpdateXRange(fft);
            spectrumUpdateYRange(fft);
            m_fftXAxis->setReverse(m_settings.m_spectrumReverseXAxis);

            switch (m_settings.m_spectrumYScale)
            {
            case RadioAstronomySettings::SY_DBFS:
                m_fftYAxis->setTitleText("Power (dBFS)");
                break;
            case RadioAstronomySettings::SY_SNR:
                m_fftYAxis->setTitleText("SNR");
                break;
            case RadioAstronomySettings::SY_DBM:
                m_fftYAxis->setTitleText("Power (dBm)");
                break;
            case RadioAstronomySettings::SY_TSYS:
                m_fftYAxis->setTitleText("Tsys (K)");
                break;
            case RadioAstronomySettings::SY_TSOURCE:
                m_fftYAxis->setTitleText("Tsource (K)");
                break;
            }

            // Plot peaks
            if (m_settings.m_spectrumPeaks)
            {
                double peakFreqMHz = (startFreq + peakIdx * binRes) / 1e6;
                double peakFreq = peakFreqMHz * 1e6;

                m_fftPeakSeries->append(peakFreqMHz, peakValue);

                ui->spectrumMarkerTable->item(SPECTRUM_MARKER_ROW_PEAK, SPECTRUM_MARKER_COL_FREQ)->setData(Qt::DisplayRole, peakFreqMHz);
                ui->spectrumMarkerTable->item(SPECTRUM_MARKER_ROW_PEAK, SPECTRUM_MARKER_COL_VALUE)->setData(Qt::DisplayRole, peakValue);

                calcVrAndDistanceToPeak(peakFreq, fft, SPECTRUM_MARKER_ROW_PEAK);
            }

            // Update markers to track current data
            if (m_spectrumM1Valid)
            {
                m_fftMarkerSeries->clear();
                int idx;
                idx = (m_spectrumM1X - startFreqMHz) / (binRes/1e6);
                if ((idx >= 0) && (idx < m_fftSeries->count()))
                {
                    m_spectrumM1Y = m_fftSeries->at(idx).y();
                    ui->spectrumMarkerTable->item(SPECTRUM_MARKER_ROW_M1, SPECTRUM_MARKER_COL_VALUE)->setData(Qt::DisplayRole, m_spectrumM1Y);
                    m_fftMarkerSeries->append(m_spectrumM1X, m_spectrumM1Y);
                    calcVrAndDistanceToPeak(m_spectrumM1X*1e6, fft, SPECTRUM_MARKER_ROW_M1);
                }
                if (m_spectrumM2Valid)
                {
                    idx = (m_spectrumM2X - startFreqMHz) / (binRes/1e6);
                    if (idx < m_fftSeries->count())
                    {
                        m_spectrumM2Y = m_fftSeries->at(idx).y();
                        ui->spectrumMarkerTable->item(SPECTRUM_MARKER_ROW_M2, SPECTRUM_MARKER_COL_VALUE)->setData(Qt::DisplayRole, m_spectrumM2Y);
                        m_fftMarkerSeries->append(m_spectrumM2X, m_spectrumM2Y);
                        calcVrAndDistanceToPeak(m_spectrumM2X*1e6, fft, SPECTRUM_MARKER_ROW_M2);
                        calcSpectrumMarkerDelta();
                    }
                }
            }

            spectrumAutoscale();
        }
    }
}

bool RadioAstronomyGUI::losMarkerEnabled(const QString& name)
{
    if (m_settings.m_spectrumDistance && m_settings.m_spectrumRefLine)
    {
        if (name == "Max") {
            return m_settings.m_spectrumPeaks;
        } else if (name == "M1") {
            return m_settings.m_spectrumMarkers;
        } else {
            return m_settings.m_spectrumMarkers;
        }
    }
    return false;
}

void RadioAstronomyGUI::showLoSMarker(const QString& name)
{
    if (losMarkerEnabled(name))
    {
        if (name == "Max") {
            showLoSMarker(SPECTRUM_MARKER_ROW_PEAK);
        } else if (name == "M1") {
            showLoSMarker(SPECTRUM_MARKER_ROW_M1);
        } else {
            showLoSMarker(SPECTRUM_MARKER_ROW_M2);
        }
    }
}

void RadioAstronomyGUI::showLoSMarker(int row)
{
    bool ok;
    float d = ui->spectrumMarkerTable->item(row, SPECTRUM_MARKER_COL_D)->data(Qt::DisplayRole).toFloat(&ok);
    if (ok)
    {
        FFTMeasurement *fft = currentFFT();
        if (fft)
        {
            QString name = ui->spectrumMarkerTable->item(row, SPECTRUM_MARKER_COL_NAME)->text();
            updateLoSMarker(name, fft->m_l, fft->m_b, d);
        }
    }
}

void RadioAstronomyGUI::updateLoSMarker(const QString& name, float l, float b, float d)
{
    // Send to Star Tracker
    QList<ObjectPipe*> starTrackerPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "startracker.display", starTrackerPipes);

    for (const auto& pipe : starTrackerPipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
        SWGSDRangel::SWGStarTrackerDisplayLoSSettings *swgSettings = new SWGSDRangel::SWGStarTrackerDisplayLoSSettings();
        swgSettings->setName(new QString(name));
        swgSettings->setL(l);
        swgSettings->setB(b);
        swgSettings->setD(d);
        messageQueue->push(MainCore::MsgStarTrackerDisplayLoSSettings::create(m_radioAstronomy, swgSettings));
    }
}

void RadioAstronomyGUI::clearLoSMarker(const QString& name)
{
    // Set d to 0 to clear
    updateLoSMarker(name, 0.0f, 0.0f, 0.0f);
}

void RadioAstronomyGUI::calcVrAndDistanceToPeak(double freq, FFTMeasurement *fft, int row)
{
    double lineFreq = ui->spectrumLineFrequency->value() * 1e6;

    // Calculate radial velocity (along line-of-sight) from Doppler shift
    double vR = dopplerToVelocity(lineFreq, freq, fft);
    ui->spectrumMarkerTable->item(row, SPECTRUM_MARKER_COL_VR)->setData(Qt::DisplayRole, vR);

    // Tangent point method only valid for Galactic quadrants I (0-90) and IV (270-360)
    if ((fft->m_l < 90.0) || (fft->m_l > 270.0))
    {
        // Calculate minimum distance to Galactic centre along line of sight (tangential radius)
        double rMin = m_settings.m_sunDistanceToGC * sin(Units::degreesToRadians(fft->m_l));
        ui->spectrumMarkerTable->item(row, SPECTRUM_MARKER_COL_R_MIN)->setData(Qt::DisplayRole, rMin);

        // Calculate orbital velocity at tangent/minimum point
        // This is the velocity to plot for the rotation curve
        double w0 = m_settings.m_sunOrbitalVelocity / m_settings.m_sunDistanceToGC;
        double vOrb = vR + rMin * w0;
        ui->spectrumMarkerTable->item(row, SPECTRUM_MARKER_COL_V)->setData(Qt::DisplayRole, vOrb);
    }
    else
    {
        ui->spectrumMarkerTable->item(row, SPECTRUM_MARKER_COL_R_MIN)->setText("");
        ui->spectrumMarkerTable->item(row, SPECTRUM_MARKER_COL_V)->setText("");
    }

    // Calculate distance of HI cloud (as indicated by a peak) to Sun and Galactic center
    double r, d1, d2;
    int solutions = calcDistanceToPeak(vR, fft->m_l, fft->m_b, r, d1, d2);
    if (solutions == 0)
    {
        ui->spectrumMarkerTable->item(row, SPECTRUM_MARKER_COL_R)->setText("");
        ui->spectrumMarkerTable->item(row, SPECTRUM_MARKER_COL_D)->setText("");
    }
    else if (solutions == 1)
    {
        ui->spectrumMarkerTable->item(row, SPECTRUM_MARKER_COL_R)->setData(Qt::DisplayRole, r);
        ui->spectrumMarkerTable->item(row, SPECTRUM_MARKER_COL_D)->setData(Qt::DisplayRole, d1);
    }
    else
    {
        ui->spectrumMarkerTable->item(row, SPECTRUM_MARKER_COL_R)->setData(Qt::DisplayRole, r);
        ui->spectrumMarkerTable->item(row, SPECTRUM_MARKER_COL_D)->setText(QString("%1/%2").arg(QString::number(d1, 'f', 1)).arg(QString::number(d2, 'f', 1)));
    }

    // Send to Star Tracker
    QString name = ui->spectrumMarkerTable->item(row, SPECTRUM_MARKER_COL_NAME)->text();
    if (losMarkerEnabled(name))
    {
        if ((solutions == 0) || std::isnan(d1))
        {
            updateLoSMarker(name, fft->m_l, fft->m_b, 0.0f);
        }
        else if (solutions == 1)
        {
            updateLoSMarker(name, fft->m_l, fft->m_b, d1);
        }
        else
        {
            bool plotMax = ui->spectrumMarkerTable->item(row, SPECTRUM_MARKER_COL_PLOT_MAX)->checkState() == Qt::Checked;
            if ((plotMax && (d1 > d2)) || (!plotMax && (d1 < d2))) {
                updateLoSMarker(name, fft->m_l, fft->m_b, d1);
            } else {
                updateLoSMarker(name, fft->m_l, fft->m_b, d2);
            }
        }
    }
}

int RadioAstronomyGUI::calcDistanceToPeak(double vr, float l, float b, double& r, double &d1, double &d2)
{
    // Radio Astronomy 4th edition - Burke - p343
    double r0 = m_settings.m_sunDistanceToGC; // Distance of Sun to Galactic centre in kpc
    double v0 = m_settings.m_sunOrbitalVelocity; // Orbital velocity of the Sun around Galactic centre in km/s
    double w0 = v0/r0;   // Angular velocity of the Sun
    double gl = Units::degreesToRadians(l);
    double gb = Units::degreesToRadians(b);
    double w = vr/(r0*sin(gl)*cos(gb))+w0;
    r = v0/w; // Assume constant v, regardless of distance from GC - Dark matter magic - Not valid <1kpc from GC
    if (r < 0) {
        return 0;
    }
    // https://en.wikipedia.org/wiki/Solution_of_triangles#Two_sides_and_non-included_angle_given_(SSA)
    double beta = gl;
    double d = sin(beta)*r0/r;
    if (d > 1.0) {
        // No solutions
        return 0;
    }
    if ((r <= r0) && (beta >= M_PI/2.0)) {
        return 0;
    }
    double gamma = asin(d);
    double alpha1 = M_PI - beta - gamma;
    d1 = r * sin(alpha1)/sin(beta); // Distance from Sun to peak
    if (r < r0)
    {
        double alpha2 = M_PI - beta - (M_PI-gamma);
        d2 = r * sin(alpha2)/sin(beta); // Distance from Sun to peak
        return 2;
    }
    else
    {
        return 1;
    }
}

void RadioAstronomyGUI::spectrumMarkerTableItemChanged(QTableWidgetItem *item)
{
    if (item->column() == SPECTRUM_MARKER_COL_PLOT_MAX) {
        // Plot max checkbox clicked
        calcDistances();
    }
}

void RadioAstronomyGUI::calcDistances()
{
    double freq;
    bool ok;
    FFTMeasurement* fft = currentFFT();
    if (fft)
    {
        freq = ui->spectrumMarkerTable->item(SPECTRUM_MARKER_ROW_PEAK, SPECTRUM_MARKER_COL_FREQ)->data(Qt::DisplayRole).toDouble(&ok);
        if (ok) {
            calcVrAndDistanceToPeak(freq*1e6, fft, SPECTRUM_MARKER_ROW_PEAK);
        }
        freq = ui->spectrumMarkerTable->item(SPECTRUM_MARKER_ROW_M1, SPECTRUM_MARKER_COL_FREQ)->data(Qt::DisplayRole).toDouble(&ok);
        if (ok) {
            calcVrAndDistanceToPeak(freq*1e6, fft, SPECTRUM_MARKER_ROW_M1);
        }
        freq = ui->spectrumMarkerTable->item(SPECTRUM_MARKER_ROW_M2, SPECTRUM_MARKER_COL_FREQ)->data(Qt::DisplayRole).toDouble(&ok);
        if (ok) {
            calcVrAndDistanceToPeak(freq*1e6, fft, SPECTRUM_MARKER_ROW_M2);
        }
    }
}

void RadioAstronomyGUI::plotRefLine(FFTMeasurement *fft)
{
    double lineFreqMHz = ui->spectrumLineFrequency->value();
    double lineFreq = lineFreqMHz * 1e6;
    QString refFrame[] = {"Topocentric", "Barycentric", "LSR"};
    m_fftDopplerAxis->setTitleText(QString("%1 radial velocity (km/s - +ve receeding)").arg(refFrame[m_settings.m_refFrame]));
    m_fftHlineSeries->setName(QString("%1 line").arg(ui->spectrumLine->currentText()));
    m_fftHlineSeries->append(0.0f, -200.0f); // For dB
    m_fftHlineSeries->append(0.0f, 10000.0f); // For temp can be >1e6?
    double startFreq = fft->m_centerFrequency + m_settings.m_spectrumSpan*1e6 / 2.0 + m_settings.m_spectrumCenterFreqOffset*1e6;
    double endFreq = fft->m_centerFrequency - m_settings.m_spectrumSpan*1e6 / 2.0 + m_settings.m_spectrumCenterFreqOffset*1e6;
    m_fftDopplerAxis->setRange(dopplerToVelocity(lineFreq, startFreq, fft),
                               dopplerToVelocity(lineFreq, endFreq, fft));
    m_fftDopplerAxis->setReverse(!m_settings.m_spectrumReverseXAxis);
    m_fftDopplerAxis->setVisible(m_settings.m_spectrumRefLine);
}

void RadioAstronomyGUI::plotTempGaussian(double startFreq, double freqStep, int steps)
{
    m_fftGaussianSeries->clear();
    double f0 = ui->spectrumGaussianFreq->value() * 1e6;
    double a = ui->spectrumGaussianAmp->value();
    double floor = ui->spectrumGaussianFloor->value();
    double fwhm = ui->spectrumGaussianFWHM->value();
    double fwhm_sq = fwhm*fwhm;
    double freq = startFreq;
    for (int i = 0; i < steps; i++)
    {
        double fd = freq - f0;
        double g = a * std::exp(-4.0*M_LN2*fd*fd/fwhm_sq) + floor;
        m_fftGaussianSeries->append(freq/1e6, g);
        freq += freqStep;
    }
}

void RadioAstronomyGUI::calcFFTPower(FFTMeasurement* fft)
{
    // Convert linear to dB
    for (int i = 0; i < fft->m_fftSize; i++) {
        fft->m_db[i] = CalcDb::dbPower(fft->m_fftData[i]);
    }
}

void RadioAstronomyGUI::calcFFTTotalPower(FFTMeasurement* fft)
{
    double total = 0.0;
    for (int i = 0; i < fft->m_fftSize; i++) {
        total += fft->m_fftData[i];
    }
    fft->m_totalPower = total;
    fft->m_totalPowerdBFS = CalcDb::dbPower(total);
}

void RadioAstronomyGUI::calcFFTTemperatures(FFTMeasurement* fft)
{
    if (m_calCold && !fft->m_snr) {
        fft->m_snr = new Real[fft->m_fftSize];
    }
    if (m_calG && !fft->m_temp) {
        fft->m_temp = new Real[fft->m_fftSize];
    }
    for (int i = 0; i < fft->m_fftSize; i++)
    {
        if (fft->m_snr && m_calCold)
        {
            // Calculate SNR (relative to cold cal)
            fft->m_snr[i] = fft->m_fftData[i] / m_calCold->m_fftData[i];
        }
        if (m_calG)
        {
            // Calculate temperature using scaling from hot cal
            fft->m_temp[i] = m_calG[i] * fft->m_fftData[i];
            // Calculate temperature using linear interpolation from hot/cold cal
            //fft->m_temp[i] = m_calG[i] * (fft->m_fftData[i] - m_calCold->m_fftData[i]) + m_settings.m_tCalCold;
            //fft->m_temp[i] = std::max(fft->m_temp[i], 0.0f); // Can't have negative temperatures
        }
    }
    calcFFTMinTemperature(fft);
}

void RadioAstronomyGUI::calcFFTMinTemperature(FFTMeasurement* fft)
{
    fft->m_tempMin = 0;
    if (fft->m_temp)
    {
        // Select minimum from within band. 95% of that to account for a little bit of inband rolloff
        float tempMin = std::numeric_limits<float>::max();
        double pc = 0.95 * fft->m_rfBandwidth / (double)fft->m_sampleRate;
        int count = (int)(pc * fft->m_fftSize);
        int start = (fft->m_fftSize - count) / 2;
        for (int i = 0; i < count; i++)
        {
            int idx = i + start;
            tempMin = std::min(tempMin, fft->m_temp[idx]);
        }
        if (tempMin != std::numeric_limits<float>::max()) {
            fft->m_tempMin = tempMin;
        }
    }
}

// Estimate Tsource by subtracting selected baseline
double RadioAstronomyGUI::calcTSource(FFTMeasurement *fft) const
{
    switch (fft->m_baseline)
    {
    case RadioAstronomySettings::SBL_TSYS0:
        return fft->m_tSys - fft->m_tSys0;
    case RadioAstronomySettings::SBL_TMIN:
        return fft->m_tSys - fft->m_tempMin;
    case RadioAstronomySettings::SBL_CAL_COLD:
        if (m_calCold) {
            return fft->m_tSys - m_calCold->m_tSys;
        }
        break;
    }
    return fft->m_tSys;
}

// Calculate spectral flux density of source
double RadioAstronomyGUI::calcFlux(double Ta, const FFTMeasurement *fft) const
{
    // Factor of 2 here assumes single polarization
    // See equation 5.13 and 5.20 in Radio Astronomy 4th edition - Burke
    double lambda = Astronomy::m_speedOfLight / (double)fft->m_centerFrequency;
    return 2.0 * Astronomy::m_boltzmann * Ta * fft->m_omegaA / (lambda * lambda);
}

void RadioAstronomyGUI::calcFFTTotalTemperature(FFTMeasurement* fft)
{
    if (fft->m_temp)
    {
        double tempSum = 0.0;
        for (int i = 0; i < fft->m_fftSize; i++) {
            tempSum += fft->m_temp[i];
        }

        // Convert from temperature to power in Watts and dBm
        Real bw = fft->m_sampleRate/(Real)fft->m_fftSize;
        fft->m_totalPowerWatts = Astronomy::m_boltzmann * tempSum * bw;
        fft->m_totalPowerdBm = Astronomy::noisePowerdBm(tempSum, bw);
        fft->m_tSys = tempSum/fft->m_fftSize;

        // Esimate source temperature
        fft->m_tSource = calcTSource(fft);

        // Calculate error due to thermal noise and gain variation
        fft->m_sigmaT = calcSigmaT(fft);
        fft->m_sigmaS = calcSigmaS(fft);

        // Calculate spectral flux density of source
        fft->m_flux = calcFlux(fft->m_tSource, fft);
    }
}

void RadioAstronomyGUI::updatePowerColumns(int row, FFTMeasurement* fft)
{
    ui->powerTable->item(row, POWER_COL_TSYS)->setData(Qt::DisplayRole, fft->m_tSys);
    ui->powerTable->item(row, POWER_COL_TSYS0)->setData(Qt::DisplayRole, fft->m_tSys0);
    ui->powerTable->item(row, POWER_COL_TSOURCE)->setData(Qt::DisplayRole, fft->m_tSource);
    if (m_settings.m_sourceType != RadioAstronomySettings::UNKNOWN) {
        ui->powerTable->item(row, POWER_COL_TB)->setData(Qt::DisplayRole, fft->m_tSource/beamFillingFactor());
    } else {
        ui->powerTable->item(row, POWER_COL_TB)->setText("");
    }
    ui->powerTable->item(row, POWER_COL_FLUX)->setData(Qt::DisplayRole, Units::wattsPerMetrePerHertzToJansky(fft->m_flux));
    ui->powerTable->item(row, POWER_COL_SIGMA_T)->setData(Qt::DisplayRole, fft->m_sigmaT);
    ui->powerTable->item(row, POWER_COL_SIGMA_S)->setData(Qt::DisplayRole, fft->m_sigmaS);
    ui->powerTable->item(row, POWER_COL_OMEGA_A)->setData(Qt::DisplayRole, fft->m_omegaA);
    ui->powerTable->item(row, POWER_COL_OMEGA_S)->setData(Qt::DisplayRole, fft->m_omegaS);
}

void RadioAstronomyGUI::addFFT(FFTMeasurement *fft, bool skipCalcs)
{
    m_fftMeasurements.append(fft);

    powerMeasurementReceived(fft, skipCalcs); // Call before ui->spectrumIndex->setValue, so table row is valid
    update2DImage(fft, skipCalcs);

    ui->spectrumIndex->setRange(0, m_fftMeasurements.size() - 1);
    if ((ui->spectrumIndex->value() == m_fftMeasurements.size() - 2) || (m_fftMeasurements.size() == 1)) {
        ui->spectrumIndex->setValue(m_fftMeasurements.size() - 1);
    }
    if ( m_fftMeasurements.size() == 1)
    {
        // Force drawing for first measurement
        on_spectrumIndex_valueChanged(0);
    }
}

void RadioAstronomyGUI::fftMeasurementReceived(const RadioAstronomy::MsgFFTMeasurement& measurement)
{
    FFTMeasurement *fft = new FFTMeasurement();
    fft->m_fftData = measurement.getFFT();
    fft->m_fftSize = measurement.getSize();
    fft->m_dateTime = measurement.getDateTime();
    fft->m_centerFrequency = m_centerFrequency;
    fft->m_sampleRate = m_settings.m_sampleRate;
    fft->m_integration = m_settings.m_integration;
    fft->m_rfBandwidth = m_settings.m_rfBandwidth;
    fft->m_omegaA = calcOmegaA();
    fft->m_omegaS = calcOmegaS();
    fft->m_coordsValid = m_coordsValid;
    fft->m_ra = m_ra;
    fft->m_dec = m_dec;
    fft->m_azimuth = m_azimuth;
    fft->m_elevation = m_elevation;
    fft->m_l = m_l;
    fft->m_b = m_b;
    fft->m_vBCRS = m_vBCRS;
    fft->m_vLSR = m_vLSR;
    fft->m_solarFlux = m_solarFlux;
    fft->m_airTemp = m_airTemps.lastValue();
    fft->m_skyTemp = m_skyTemp;
    for (int i = 0; i < RADIOASTRONOMY_SENSORS; i++) {
        fft->m_sensor[i] = m_sensors[i].lastValue();
    }
    fft->m_db = new Real[fft->m_fftSize];
    fft->m_sweepIndex = m_sweepIndex++;
    fft->m_tSys0 = calcTSys0();
    fft->m_baseline = m_settings.m_spectrumBaseline;

    calcFFTPower(fft);
    calcFFTTotalPower(fft);
    calcFFTTemperatures(fft);
    calcFFTTotalTemperature(fft);
    addFFT(fft);
}

void RadioAstronomyGUI::on_spectrumIndex_valueChanged(int value)
{
    if (value < m_fftMeasurements.size())
    {
        plotFFTMeasurement(value);

        // Highlight in table
        ui->powerTable->selectRow(value);
        ui->powerTable->scrollTo(ui->powerTable->model()->index(value, 0));
        ui->spectrumDateTime->setDateTime(m_fftMeasurements[value]->m_dateTime);

        // Display target in Star Tracker
        QList<ObjectPipe*> starTrackerPipes;
        MainCore::instance()->getMessagePipes().getMessagePipes(this, "startracker.display", starTrackerPipes);

        for (const auto& pipe : starTrackerPipes)
        {
            MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
            SWGSDRangel::SWGStarTrackerDisplaySettings *swgSettings = new SWGSDRangel::SWGStarTrackerDisplaySettings();
            swgSettings->setDateTime(new QString(m_fftMeasurements[value]->m_dateTime.toString(Qt::ISODateWithMs)));
            swgSettings->setAzimuth(m_fftMeasurements[value]->m_azimuth);
            swgSettings->setElevation(m_fftMeasurements[value]->m_elevation);
            messageQueue->push(MainCore::MsgStarTrackerDisplaySettings::create(m_radioAstronomy, swgSettings));
        }
    }
}

void RadioAstronomyGUI::plotSpectrum()
{
    QChart *oldChart = m_fftChart;

    m_fftChart = new QChart();

    m_fftChart->layout()->setContentsMargins(0, 0, 0, 0);
    m_fftChart->setMargins(QMargins(1, 1, 1, 1));
    m_fftChart->setTheme(QChart::ChartThemeDark);

    m_fftChart->legend()->setAlignment(Qt::AlignRight);
    m_fftChart->legend()->setVisible(m_settings.m_spectrumLegend);

    m_fftSeries = new QLineSeries();
    m_fftSeries->setName("Measurement");
    connect(m_fftSeries, &QXYSeries::clicked, this, &RadioAstronomyGUI::spectrumSeries_clicked);

    // Plot vertical reference spectral line
    m_fftHlineSeries = new QLineSeries();
    m_fftHlineSeries->setName(QString("%1 line").arg(ui->spectrumLine->currentText()));
    m_fftHlineSeries->setVisible(m_settings.m_spectrumRefLine);

    // Plot peak info
    m_fftPeakSeries = new QScatterSeries();
    m_fftPeakSeries->setPointLabelsVisible(true);
    m_fftPeakSeries->setMarkerSize(5);
    m_fftPeakSeries->setName("Max");

    // Markers
    m_fftMarkerSeries = new QScatterSeries();
    m_fftMarkerSeries->setPointLabelsVisible(true);
    m_fftMarkerSeries->setMarkerSize(5);
    m_fftMarkerSeries->setName("Markers");

    // Gaussian
    m_fftGaussianSeries = new QLineSeries();
    m_fftGaussianSeries->setName("Gaussian fit");
    m_fftGaussianSeries->setVisible(m_settings.m_spectrumTemp);

    m_fftLABSeries  = new QLineSeries();
    m_fftLABSeries->setName("LAB reference");
    m_fftLABSeries->setVisible(m_settings.m_spectrumLAB);

    m_fftXAxis = new QValueAxis();
    m_fftYAxis = new QValueAxis();
    m_fftDopplerAxis = new QValueAxis();

    m_fftChart->addAxis(m_fftXAxis, Qt::AlignBottom);
    m_fftChart->addAxis(m_fftYAxis, Qt::AlignLeft);
    m_fftChart->addAxis(m_fftDopplerAxis, Qt::AlignTop);

    m_fftXAxis->setTitleText("Frequency (MHz)");
    calcSpectrumChartTickCount(m_fftXAxis, size().width());
    calcSpectrumChartTickCount(m_fftDopplerAxis, size().width());
    m_fftYAxis->setTitleText("Power");

    m_fftChart->addSeries(m_fftSeries);
    m_fftSeries->attachAxis(m_fftXAxis);
    m_fftSeries->attachAxis(m_fftYAxis);

    m_fftChart->addSeries(m_fftHlineSeries);
    //m_fftHlineSeries->attachAxis(m_fftXAxis);
    m_fftHlineSeries->attachAxis(m_fftDopplerAxis);
    m_fftHlineSeries->attachAxis(m_fftYAxis);

    m_fftChart->addSeries(m_fftGaussianSeries);
    m_fftGaussianSeries->attachAxis(m_fftXAxis);
    m_fftGaussianSeries->attachAxis(m_fftYAxis);

    m_fftChart->addSeries(m_fftLABSeries);
    //m_fftLABSeries->attachAxis(m_fftXAxis);
    m_fftLABSeries->attachAxis(m_fftDopplerAxis);
    m_fftLABSeries->attachAxis(m_fftYAxis);

    m_fftChart->addSeries(m_fftPeakSeries);
    m_fftPeakSeries->attachAxis(m_fftXAxis);
    m_fftPeakSeries->attachAxis(m_fftYAxis);

    m_fftChart->addSeries(m_fftMarkerSeries);
    m_fftMarkerSeries->attachAxis(m_fftXAxis);
    m_fftMarkerSeries->attachAxis(m_fftYAxis);

    // Don't have peaks and markers in legend
    m_fftChart->legend()->markers(m_fftPeakSeries)[0]->setVisible(false);
    m_fftChart->legend()->markers(m_fftMarkerSeries)[0]->setVisible(false);

    ui->spectrumChart->setChart(m_fftChart);

    delete oldChart;
}

// Calculate galactic background temperature based on center frequency
void RadioAstronomyGUI::calcGalacticBackgroundTemp()
{
    // https://arxiv.org/ftp/arxiv/papers/1912/1912.12699.pdf - page 6
    // 17.1, 25.2 and 54.8 K for the 10th, 50th and 90th percentile of the all-sky distribution
    // See also ITU-R P.372-7 section 6
    // If this is used for cold calibration, we don't want to use the higher value
    double temp = 25.2 * std::pow(m_centerFrequency/408000000.0, -2.75);
    ui->tempGal->setValue(temp);
}

// Calculate athmospheric noise temperature based on air temperature, zenith opacity and elevation
void RadioAstronomyGUI::calcAtmosphericTemp()
{
    float el = m_settings.m_elevation;
    if (m_settings.m_elevation < 1.0f) {
        el = 1.0f; // Avoid divide by 0 and limit max value to match ITU-R P.372-7 figure 5
    }
    double temp = Units::celsiusToKelvin(m_settings.m_tempAir) * (1.0 - std::exp(-m_settings.m_zenithOpacity/cos(Units::degreesToRadians(90.0f - el))));
    ui->tempAtm->setValue(temp);
}

void RadioAstronomyGUI::on_tempRXSelect_currentIndexChanged(int value)
{
    if (value == 0)
    {
        // T_RX
        ui->tempRX->setValue(m_settings.m_tempRX);
        ui->tempRXUnitsLabel->setText("K");
    }
    else
    {
        // NF
        ui->tempRX->setValue(Units::noiseTempToNoiseFigureTo(m_settings.m_tempRX));
        ui->tempRXUnitsLabel->setText("dB");
    }
}

void RadioAstronomyGUI::on_tempRX_valueChanged(double value)
{
    if (ui->tempRXSelect->currentIndex() == 0) {
        m_settings.m_tempRX = value;
    } else {
        m_settings.m_tempRX = Units::noiseFigureToNoiseTemp(value);
    }
    updateTSys0();
    applySettings();
}

void RadioAstronomyGUI::on_tempCMB_valueChanged(double value)
{
    m_settings.m_tempCMB = value;
    updateTSys0();
    applySettings();
}

void RadioAstronomyGUI::on_tempGal_valueChanged(double value)
{
    m_settings.m_tempGal = value;
    updateTSys0();
    applySettings();
}

void RadioAstronomyGUI::on_tempSP_valueChanged(double value)
{
    m_settings.m_tempSP = value;
    updateTSys0();
    applySettings();
}

void RadioAstronomyGUI::on_tempAtm_valueChanged(double value)
{
    m_settings.m_tempAtm = value;
    updateTSys0();
    applySettings();
}

void RadioAstronomyGUI::on_tempAir_valueChanged(double value)
{
    m_settings.m_tempAir = value;
    if (m_settings.m_tempAtmLink) {
        calcAtmosphericTemp();
    }
    applySettings();
}

void RadioAstronomyGUI::on_zenithOpacity_valueChanged(double value)
{
    m_settings.m_zenithOpacity = value;
    if (m_settings.m_tempAtmLink) {
        calcAtmosphericTemp();
    }
    applySettings();
}

void RadioAstronomyGUI::on_elevation_valueChanged(double value)
{
    m_settings.m_elevation = value;
    if (m_settings.m_tempAtmLink) {
        calcAtmosphericTemp();
    }
    applySettings();
}

void RadioAstronomyGUI::on_elevationLink_toggled(bool checked)
{
    m_settings.m_elevationLink = checked;
    ui->elevation->setValue(m_elevation);
    ui->elevation->setEnabled(!m_settings.m_elevationLink);
    applySettings();
}

void RadioAstronomyGUI::on_tempAtmLink_toggled(bool checked)
{
    m_settings.m_tempAtmLink = checked;
    ui->tempAtm->setEnabled(!m_settings.m_tempAtmLink);
    if (checked) {
        calcAtmosphericTemp();
    }
    applySettings();
}

void RadioAstronomyGUI::on_tempAirLink_toggled(bool checked)
{
    m_settings.m_tempAirLink = checked;
    ui->tempAir->setEnabled(!m_settings.m_tempAirLink);
    if (checked)
    {
        ui->tempAir->setValue(m_airTemps.lastValue());
        calcAtmosphericTemp();
    }
    applySettings();
}

void RadioAstronomyGUI::on_tempGalLink_toggled(bool checked)
{
    m_settings.m_tempGalLink = checked;
    if (checked) {
        calcGalacticBackgroundTemp();
    }
    ui->tempGal->setEnabled(!m_settings.m_tempGalLink);
    applySettings();
}

void RadioAstronomyGUI::on_tCalHotSelect_currentIndexChanged(int value)
{
    if (value == 0)
    {
        // Thot
        ui->tCalHot->setValue(m_settings.m_tCalHot);
        ui->tCalHotUnitsLabel->setText("K");
    }
    else
    {
        // Phot
        double power = Astronomy::noisePowerdBm(m_settings.m_tCalHot, m_settings.m_sampleRate);
        ui->tCalHot->setValue(power);
        ui->tCalHotUnitsLabel->setText("dBm");
    }
}

void RadioAstronomyGUI::on_tCalHot_valueChanged(double value)
{
    double temp;
    if (ui->tCalHotSelect->currentIndex() == 0) {
        temp = value;
    } else {
        temp = Astronomy::noiseTemp(value, m_settings.m_sampleRate);
    }
    m_settings.m_tCalHot = (float)temp;
    calibrate();
    applySettings();
}

void RadioAstronomyGUI::on_tCalColdSelect_currentIndexChanged(int value)
{
    if (value == 0)
    {
        // Tcold
        ui->tCalCold->setValue(m_settings.m_tCalCold);
        ui->tCalColdUnitsLabel->setText("K");
    }
    else
    {
        // Pcold
        double power = Astronomy::noisePowerdBm(m_settings.m_tCalCold, m_settings.m_sampleRate);
        ui->tCalCold->setValue(power);
        ui->tCalColdUnitsLabel->setText("dBm");
    }
}

void RadioAstronomyGUI::on_tCalCold_valueChanged(double value)
{
    double temp;
    if (ui->tCalColdSelect->currentIndex() == 0) {
        temp = value;
    } else {
        temp = Astronomy::noiseTemp(value, m_settings.m_sampleRate);
    }
    m_settings.m_tCalCold = (float)temp;
    calibrate();
    applySettings();
}

void RadioAstronomyGUI::on_spectrumLine_currentIndexChanged(int value)
{
    m_settings.m_line = (RadioAstronomySettings::Line)value;
    displaySpectrumLineFrequency();
    plotFFTMeasurement();
    applySettings();
}

void RadioAstronomyGUI::displaySpectrumLineFrequency()
{
    switch (m_settings.m_line)
    {
    case RadioAstronomySettings::HI:
        ui->spectrumLineFrequency->setValue(Astronomy::m_hydrogenLineFrequency / 1e6);
        ui->spectrumLineFrequency->setEnabled(false);
        break;
    case RadioAstronomySettings::OH:
        ui->spectrumLineFrequency->setValue(Astronomy::m_hydroxylLineFrequency / 1e6);
        ui->spectrumLineFrequency->setEnabled(false);
        break;
    case RadioAstronomySettings::DI:
        ui->spectrumLineFrequency->setValue(Astronomy::m_deuteriumLineFrequency / 1e6);
        ui->spectrumLineFrequency->setEnabled(false);
        break;
    case RadioAstronomySettings::CUSTOM_LINE:
        ui->spectrumLineFrequency->setValue(m_settings.m_lineCustomFrequency / 1e6);
        ui->spectrumLineFrequency->setEnabled(true);
        break;
    }
}

void RadioAstronomyGUI::on_spectrumLineFrequency_valueChanged(double value)
{
    m_settings.m_lineCustomFrequency = value * 1e6;
    plotFFTMeasurement();
    applySettings();
}

void RadioAstronomyGUI::on_refFrame_currentIndexChanged(int value)
{
    m_settings.m_refFrame = (RadioAstronomySettings::RefFrame)value;
    plotFFTMeasurement();
    applySettings();
}

void RadioAstronomyGUI::on_sunDistanceToGC_valueChanged(double value)
{
    m_settings.m_sunDistanceToGC = value;
    applySettings();
    calcDistances();
}

void RadioAstronomyGUI::on_sunOrbitalVelocity_valueChanged(double value)
{
    m_settings.m_sunOrbitalVelocity = value;
    applySettings();
    calcDistances();
}

void RadioAstronomyGUI::on_savePowerChartImage_clicked()
{
    QFileDialog fileDialog(nullptr, "Select file to save image to", "", "*.png;*.jpg;*.jpeg;*.bmp;*.ppm;*.xbm;*.xpm");
    fileDialog.setAcceptMode(QFileDialog::AcceptSave);
    if (fileDialog.exec())
    {
        QStringList fileNames = fileDialog.selectedFiles();
        if (fileNames.size() > 0)
        {
            QImage image(ui->powerChart->size(), QImage::Format_ARGB32);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            ui->powerChart->render(&painter);
            if (!image.save(fileNames[0])) {
                QMessageBox::critical(this, "Radio Astronomy", QString("Failed to save image to %1").arg(fileNames[0]));
            }
        }
    }
}

void RadioAstronomyGUI::on_saveSpectrumChartImage_clicked()
{
    QFileDialog fileDialog(nullptr, "Select file to save image to", "", "*.png;*.jpg;*.jpeg;*.bmp;*.ppm;*.xbm;*.xpm");
    fileDialog.setAcceptMode(QFileDialog::AcceptSave);
    if (fileDialog.exec())
    {
        QStringList fileNames = fileDialog.selectedFiles();
        if (fileNames.size() > 0)
        {
            QImage image(ui->spectrumChart->size(), QImage::Format_ARGB32);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            ui->spectrumChart->render(&painter);
            if (!image.save(fileNames[0])) {
                QMessageBox::critical(this, "Radio Astronomy", QString("Failed to save image to %1").arg(fileNames[0]));
            }
        }
    }
}

void RadioAstronomyGUI::on_saveSpectrumChartImages_clicked()
{
    if (m_fftMeasurements.size() > 1)
    {
        // Get filename of animation file
        QFileDialog fileDialog(nullptr, "Select file to save animation to", "", "*.png");
        fileDialog.setAcceptMode(QFileDialog::AcceptSave);
        if (fileDialog.exec())
        {
            QStringList fileNames = fileDialog.selectedFiles();
            if (fileNames.size() > 0)
            {
                // Create animation file
                APNG apng(m_fftMeasurements.size());

                // Plot each FFT to a temp .png file (in memory) then append to animation file
                for (int i = 0; i < m_fftMeasurements.size(); i++)
                {
                    plotFFTMeasurement(i);
                    QApplication::processEvents(); // To get chart title to be updated
                    QImage image(ui->spectrumChart->size(), QImage::Format_ARGB32);
                    image.fill(Qt::transparent);
                    QPainter painter(&image);
                    ui->spectrumChart->render(&painter);
                    apng.addImage(image);
                }
                if (!apng.save(fileNames[0])) {
                    QMessageBox::critical(this, "Radio Astronomy", QString("Failed to write to file %1").arg(fileNames[0]));
                }
            }
        }
    }
}

void RadioAstronomyGUI::on_spectrumReverseXAxis_toggled(bool checked)
{
    m_settings.m_spectrumReverseXAxis = checked;
    applySettings();
    if (ui->spectrumChartSelect->currentIndex() == 0) {
        plotFFTMeasurement();
    } else {
        m_calXAxis->setReverse(m_settings.m_spectrumReverseXAxis);
    }
}

void RadioAstronomyGUI::on_powerShowPeak_toggled(bool checked)
{
    m_settings.m_powerPeaks = checked;
    updatePowerMarkerTableVisibility();
    applySettings();
    if (m_powerPeakSeries)
    {
        m_powerPeakSeries->setVisible(checked);
        if (checked) {
            m_powerChart->legend()->markers(m_powerPeakSeries)[0]->setVisible(false);
        }
    }
    getRollupContents()->arrangeRollups();
}

void RadioAstronomyGUI::on_spectrumPeak_toggled(bool checked)
{
    m_settings.m_spectrumPeaks = checked;
    updateSpectrumMarkerTableVisibility();
    plotFFTMeasurement();
    applySettings();
    if (m_fftChart)
    {
        if (checked) {
            m_fftChart->legend()->markers(m_fftPeakSeries)[0]->setVisible(false);
            showLoSMarker("Max");
        } else {
            clearLoSMarker("Max");
        }
    }
    getRollupContents()->arrangeRollups();
}

void RadioAstronomyGUI::on_powerShowMarker_toggled(bool checked)
{
    m_settings.m_powerMarkers = checked;
    updatePowerMarkerTableVisibility();
    applySettings();
    if (m_powerMarkerSeries)
    {
        m_powerMarkerSeries->setVisible(checked);
        if (checked) {
            m_powerChart->legend()->markers(m_powerMarkerSeries)[0]->setVisible(false);
        }
    }
    updatePowerSelect();
    getRollupContents()->arrangeRollups();
}

void RadioAstronomyGUI::on_powerShowAvg_toggled(bool checked)
{
    m_settings.m_powerAvg = checked;
    applySettings();
    ui->powerChartAvgWidgets->setVisible(checked);
    getRollupContents()->arrangeRollups();
    if (checked) {
        calcAverages();
    }
}

void RadioAstronomyGUI::on_powerShowLegend_toggled(bool checked)
{
    m_settings.m_powerLegend = checked;
    applySettings();
    if (m_powerChart)
    {
        if (checked) {
            m_powerChart->legend()->show();
        } else {
            m_powerChart->legend()->hide();
        }
    }
}

void RadioAstronomyGUI::on_powerShowTsys0_toggled(bool checked)
{
    m_settings.m_powerShowTsys0 = checked;
    applySettings();
    if (m_powerTsys0Series) {
        m_powerTsys0Series->setVisible(checked);
    }
}

void RadioAstronomyGUI::on_powerShowAirTemp_toggled(bool checked)
{
    m_settings.m_powerShowAirTemp = checked;
    applySettings();
    m_airTemps.clicked(checked);
}

void RadioAstronomyGUI::on_powerShowSensor1_toggled(bool checked)
{
    m_settings.m_sensorVisible[0] = checked;
    applySettings();
    m_sensors[0].clicked(checked);
}

void RadioAstronomyGUI::on_powerShowSensor2_toggled(bool checked)
{
    m_settings.m_sensorVisible[1] = checked;
    applySettings();
    m_sensors[1].clicked(checked);
}

void RadioAstronomyGUI::updatePowerMarkerTableVisibility()
{
    ui->powerMarkerTableWidgets->setVisible(m_settings.m_powerPeaks || m_settings.m_powerMarkers);
    if (m_settings.m_powerPeaks)
    {
        ui->powerMarkerTable->showRow(POWER_MARKER_ROW_PEAK_MAX);
        ui->powerMarkerTable->showRow(POWER_MARKER_ROW_PEAK_MIN);
    }
    else
    {
        ui->powerMarkerTable->hideRow(POWER_MARKER_ROW_PEAK_MAX);
        ui->powerMarkerTable->hideRow(POWER_MARKER_ROW_PEAK_MIN);
    }
    if (m_settings.m_powerMarkers)
    {
        ui->powerMarkerTable->showRow(POWER_MARKER_ROW_M1);
        ui->powerMarkerTable->showRow(POWER_MARKER_ROW_M2);
    }
    else
    {
        ui->powerMarkerTable->hideRow(POWER_MARKER_ROW_M1);
        ui->powerMarkerTable->hideRow(POWER_MARKER_ROW_M2);
    }
    ui->powerMarkerTableWidgets->updateGeometry(); // Without this, widgets aren't resized properly
}

void RadioAstronomyGUI::updateSpectrumMarkerTableVisibility()
{
    bool fft = ui->spectrumChartSelect->currentIndex() == 0;
    ui->spectrumMarkerTableWidgets->setVisible(fft && (m_settings.m_spectrumPeaks || m_settings.m_spectrumMarkers));
    if (m_settings.m_spectrumPeaks)
    {
        ui->spectrumMarkerTable->showRow(SPECTRUM_MARKER_ROW_PEAK);
    }
    else
    {
        ui->spectrumMarkerTable->hideRow(SPECTRUM_MARKER_ROW_PEAK);
    }
    if (m_settings.m_spectrumMarkers)
    {
        ui->spectrumMarkerTable->showRow(SPECTRUM_MARKER_ROW_M1);
        ui->spectrumMarkerTable->showRow(SPECTRUM_MARKER_ROW_M2);
    }
    else
    {
        ui->spectrumMarkerTable->hideRow(SPECTRUM_MARKER_ROW_M1);
        ui->spectrumMarkerTable->hideRow(SPECTRUM_MARKER_ROW_M2);
    }
    ui->spectrumMarkerTableWidgets->updateGeometry(); // Without this, widgets aren't resized properly
}

void RadioAstronomyGUI::on_spectrumMarker_toggled(bool checked)
{
    m_settings.m_spectrumMarkers = checked;
    applySettings();
    updateSpectrumMarkerTableVisibility();
    m_fftMarkerSeries->setVisible(checked);
    if (checked)
    {
        m_fftChart->legend()->markers(m_fftMarkerSeries)[0]->setVisible(false);
        showLoSMarker("M1");
        showLoSMarker("M2");
    }
    else
    {
        clearLoSMarker("M1");
        clearLoSMarker("M2");
    }
    updateSpectrumSelect();
    getRollupContents()->arrangeRollups();
}

void RadioAstronomyGUI::on_spectrumTemp_toggled(bool checked)
{
    m_settings.m_spectrumTemp = checked;
    applySettings();
    ui->spectrumGaussianWidgets->setVisible(checked);
    m_fftGaussianSeries->setVisible(checked);
    updateSpectrumSelect();
    getRollupContents()->arrangeRollups();
}

void RadioAstronomyGUI::on_spectrumShowLegend_toggled(bool checked)
{
    m_settings.m_spectrumLegend = checked;
    applySettings();
    if (m_fftChart)
    {
        m_fftChart->legend()->setVisible(checked);
        m_calChart->legend()->setVisible(checked);
    }
}

void RadioAstronomyGUI::on_spectrumShowRefLine_toggled(bool checked)
{
    m_settings.m_spectrumRefLine = checked;
    applySettings();
    ui->spectrumRefLineWidgets->setVisible(checked);
    if (m_fftHlineSeries)
    {
        m_fftHlineSeries->setVisible(m_settings.m_spectrumRefLine);
        m_fftDopplerAxis->setVisible(m_settings.m_spectrumRefLine);
    }
    updateDistanceColumns();
    getRollupContents()->arrangeRollups();
}

void RadioAstronomyGUI::on_spectrumShowLAB_toggled(bool checked)
{
    m_settings.m_spectrumLAB = checked;
    applySettings();
    m_fftLABSeries->setVisible(m_settings.m_spectrumLAB);
    if (m_settings.m_spectrumLAB) {
        plotLAB(); // Replot incase data needs to be downloaded
    }
    spectrumAutoscale();
}

void RadioAstronomyGUI::updateDistanceColumns()
{
    if (m_settings.m_spectrumDistance && m_settings.m_spectrumRefLine)
    {
        ui->spectrumMarkerTable->showColumn(SPECTRUM_MARKER_COL_R);
        ui->spectrumMarkerTable->showColumn(SPECTRUM_MARKER_COL_D);
        ui->spectrumMarkerTable->showColumn(SPECTRUM_MARKER_COL_PLOT_MAX);
        ui->spectrumMarkerTable->showColumn(SPECTRUM_MARKER_COL_R_MIN);
        ui->spectrumMarkerTable->showColumn(SPECTRUM_MARKER_COL_V);
        showLoSMarker("Max");
        showLoSMarker("M1");
        showLoSMarker("M2");
        ui->sunDistanceToGCLine->setVisible(true);
        ui->sunDistanceToGCLabel->setVisible(true);
        ui->sunDistanceToGC->setVisible(true);
        ui->sunDistanceToGCUnits->setVisible(true);
        ui->sunOrbitalVelocityLine->setVisible(true);
        ui->sunOrbitalVelocityLabel->setVisible(true);
        ui->sunOrbitalVelocity->setVisible(true);
        ui->sunOrbitalVelocityUnits->setVisible(true);
    }
    else
    {
        ui->spectrumMarkerTable->hideColumn(SPECTRUM_MARKER_COL_R);
        ui->spectrumMarkerTable->hideColumn(SPECTRUM_MARKER_COL_D);
        ui->spectrumMarkerTable->hideColumn(SPECTRUM_MARKER_COL_PLOT_MAX);
        ui->spectrumMarkerTable->hideColumn(SPECTRUM_MARKER_COL_R_MIN);
        ui->spectrumMarkerTable->hideColumn(SPECTRUM_MARKER_COL_V);
        clearLoSMarker("Max");
        clearLoSMarker("M1");
        clearLoSMarker("M2");
        ui->sunDistanceToGCLine->setVisible(false);
        ui->sunDistanceToGCLabel->setVisible(false);
        ui->sunDistanceToGC->setVisible(false);
        ui->sunDistanceToGCUnits->setVisible(false);
        ui->sunOrbitalVelocityLine->setVisible(false);
        ui->sunOrbitalVelocityLabel->setVisible(false);
        ui->sunOrbitalVelocity->setVisible(false);
        ui->sunOrbitalVelocityUnits->setVisible(false);
    }
}

void RadioAstronomyGUI::on_spectrumShowDistance_toggled(bool checked)
{
    m_settings.m_spectrumDistance = checked;
    applySettings();
    if (m_settings.m_spectrumDistance && !m_settings.m_spectrumRefLine) {
        ui->spectrumShowRefLine->setChecked(true);
    }
    updateDistanceColumns();
}

// point isn't necessarily a point in the series - may be interpolated
void RadioAstronomyGUI::powerSeries_clicked(const QPointF &point)
{
    QString selection = ui->powerSelect->currentText();
    if (selection.startsWith("M"))
    {
        if (selection == "M1")
        {
            // Place marker 1
            m_powerM1X = point.x();
            m_powerM1Y = point.y();
            if (m_powerM1Valid) {
                m_powerMarkerSeries->replace(0, m_powerM1X, m_powerM1Y);
            } else {
                m_powerMarkerSeries->insert(0, QPointF(m_powerM1X, m_powerM1Y));
            }
            m_powerM1Valid = true;
            QDateTime dt = QDateTime::fromMSecsSinceEpoch(m_powerM1X);
            ui->powerMarkerTable->item(POWER_MARKER_ROW_M1, POWER_MARKER_COL_DATE)->setData(Qt::DisplayRole, dt.date());
            ui->powerMarkerTable->item(POWER_MARKER_ROW_M1, POWER_MARKER_COL_TIME)->setData(Qt::DisplayRole, dt.time());
            ui->powerMarkerTable->item(POWER_MARKER_ROW_M1, POWER_MARKER_COL_VALUE)->setData(Qt::DisplayRole, m_powerM1Y);
            calcPowerMarkerDelta();
        }
        else if (selection == "M2")
        {
            // Place marker 2
            m_powerM2X = point.x();
            m_powerM2Y = point.y();
            if (m_powerM2Valid) {
                m_powerMarkerSeries->replace(1, m_powerM2X, m_powerM2Y);
            } else {
                m_powerMarkerSeries->insert(1, QPointF(m_powerM2X, m_powerM2Y));
            }
            m_powerM2Valid = true;
            QDateTime dt = QDateTime::fromMSecsSinceEpoch(m_powerM2X);
            ui->powerMarkerTable->item(POWER_MARKER_ROW_M2, POWER_MARKER_COL_DATE)->setData(Qt::DisplayRole, dt.date());
            ui->powerMarkerTable->item(POWER_MARKER_ROW_M2, POWER_MARKER_COL_TIME)->setData(Qt::DisplayRole, dt.time());
            ui->powerMarkerTable->item(POWER_MARKER_ROW_M2, POWER_MARKER_COL_VALUE)->setData(Qt::DisplayRole, m_powerM2Y);
            calcPowerMarkerDelta();
        }
    }
    else if (selection == "Gaussian")
    {
        // Fit a Gaussian assuming point clicked is the peak
        ui->powerGaussianCenter->setDateTime(QDateTime::fromMSecsSinceEpoch(point.x()));
        // Calculate noise floor - take average of lowest 10%
        qreal floor = calcSeriesFloor(m_powerSeries);
        ui->powerGaussianFloor->setValue(floor);
        // Set amplitude to achieve selected point
        ui->powerGaussianAmp->setValue(point.y() - floor);
    }
    else
    {
        if (m_fftMeasurements.size() > 1)
        {
            // Select row closest to clicked data
            QDateTime dt = QDateTime::fromMSecsSinceEpoch(point.x());
            int i = 0;
            while ((i < m_fftMeasurements.size()) && (dt > m_fftMeasurements[i]->m_dateTime)) {
                i++;
            }
            if (i < m_fftMeasurements.size()) {
                ui->spectrumIndex->setValue(i);
            }
        }
    }
}

qreal RadioAstronomyGUI::calcSeriesFloor(QXYSeries *series, int percent)
{
    QList<qreal> minValues;
    double count = series->count() * percent / 100.0;
    for (int i = 0; i < series->count(); i++)
    {
        qreal y = series->at(i).y();
        if (minValues.size() < count)
        {
            minValues.append(y);
            std::sort(minValues.begin(), minValues.end());
        }
        else if (y < minValues.last())
        {
            minValues.append(y);
            std::sort(minValues.begin(), minValues.end());
        }
    }
    qreal sum = std::accumulate(minValues.begin(), minValues.end(), 0.0);
    return sum / minValues.size();
}

void RadioAstronomyGUI::updateSpectrumSelect()
{
    ui->spectrumSelect->clear();
    if (m_settings.m_spectrumMarkers)
    {
        ui->spectrumSelect->addItem("M1");
        ui->spectrumSelect->addItem("M2");
    }
    if (m_settings.m_spectrumTemp)
    {
        ui->spectrumSelect->addItem("Gaussian");
    }
    bool visible = ui->spectrumSelect->count() != 0;
    ui->spectrumSelectLabel->setVisible(visible);
    ui->spectrumSelect->setVisible(visible);
}

void RadioAstronomyGUI::updatePowerSelect()
{
    ui->powerSelect->clear();
    if (m_settings.m_powerMarkers || m_settings.m_powerShowGaussian) {
        ui->powerSelect->addItem("Row");
    }
    if (m_settings.m_powerMarkers)
    {
        ui->powerSelect->addItem("M1");
        ui->powerSelect->addItem("M2");
    }
    if (m_settings.m_powerShowGaussian) {
        ui->powerSelect->addItem("Gaussian");
    }
    bool visible = ui->powerSelect->count() != 0;
    ui->powerSelectLabel->setVisible(visible);
    ui->powerSelect->setVisible(visible);
}

void RadioAstronomyGUI::spectrumSeries_clicked(const QPointF &point)
{
    QString selection = ui->spectrumSelect->currentText();
    if (selection.startsWith("M"))
    {
        FFTMeasurement *fft = currentFFT();
        if (selection == "M1")
        {
            m_spectrumM1X = point.x();
            m_spectrumM1Y = point.y();
            m_spectrumM1Valid = true;
            ui->spectrumMarkerTable->item(SPECTRUM_MARKER_ROW_M1, SPECTRUM_MARKER_COL_FREQ)->setData(Qt::DisplayRole, m_spectrumM1X);
            ui->spectrumMarkerTable->item(SPECTRUM_MARKER_ROW_M1, SPECTRUM_MARKER_COL_VALUE)->setData(Qt::DisplayRole, m_spectrumM1Y);
            calcVrAndDistanceToPeak(m_spectrumM1X*1e6, fft, SPECTRUM_MARKER_ROW_M1);
        }
        else if (selection == "M2")
        {
            m_spectrumM2X = point.x();
            m_spectrumM2Y = point.y();
            m_spectrumM2Valid = true;
            ui->spectrumMarkerTable->item(SPECTRUM_MARKER_ROW_M2, SPECTRUM_MARKER_COL_FREQ)->setData(Qt::DisplayRole, m_spectrumM2X);
            ui->spectrumMarkerTable->item(SPECTRUM_MARKER_ROW_M2, SPECTRUM_MARKER_COL_VALUE)->setData(Qt::DisplayRole, m_spectrumM2Y);
            calcVrAndDistanceToPeak(m_spectrumM2X*1e6, fft, SPECTRUM_MARKER_ROW_M2);
        }
        calcSpectrumMarkerDelta();

        m_fftMarkerSeries->clear();
        if (m_spectrumM1Valid) {
            m_fftMarkerSeries->append(m_spectrumM1X, m_spectrumM1Y);
        }
        if (m_spectrumM2Valid) {
            m_fftMarkerSeries->append(m_spectrumM2X, m_spectrumM2Y);
        }
    }
    else if (selection == "Gaussian")
    {
        ui->spectrumGaussianFreq->setValue(point.x());
        // Calculate noise floor - take average of lowest 10%
        qreal floor = calcSeriesFloor(m_fftSeries);
        ui->spectrumGaussianFloor->setValue(floor);
        // Set amplitude to achieve selected point
        ui->spectrumGaussianAmp->setValue(point.y() - floor);
        plotFFTMeasurement();
    }
}

void RadioAstronomyGUI::on_spectrumGaussianFreq_valueChanged(double value)
{
    (void) value;
    calcFWHM();
    plotFFTMeasurement();
}

void RadioAstronomyGUI::on_spectrumGaussianAmp_valueChanged(double value)
{
    (void) value;
    plotFFTMeasurement();
    calcColumnDensity();
}

void RadioAstronomyGUI::on_spectrumGaussianFloor_valueChanged(double value)
{
    (void) value;
    plotFFTMeasurement();
}

void RadioAstronomyGUI::on_spectrumGaussianFWHM_valueChanged(double fFWHM)
{
    double c = Astronomy::m_speedOfLight;
    double k = Astronomy::m_boltzmann;
    double m = Astronomy::m_hydrogenMass;
    double f0 = ui->spectrumGaussianFreq->value() * 1e6;
    double vTurb = ui->spectrumGaussianTurb->value() * 1e3; // RSM turbulent velocties - Convert to m/s
    const double bf = 2.0*sqrt(M_LN2);  // Convert from Doppler parameter to FWHM

    double fr = fFWHM * c / (bf*f0);
    double frs = fr*fr;

    double T = m * (frs-vTurb*vTurb) / (2.0*k);

    ui->spectrumTemperature->blockSignals(true);
    ui->spectrumTemperature->setValue(T);
    ui->spectrumTemperature->blockSignals(false);
    plotFFTMeasurement();
    calcColumnDensity();
}

void RadioAstronomyGUI::on_spectrumGaussianTurb_valueChanged(double value)
{
    (void) value;
    calcFWHM();
    plotFFTMeasurement();
}

void RadioAstronomyGUI::on_spectrumTemperature_valueChanged(double value)
{
   (void) value;
   calcFWHM();
   plotFFTMeasurement();
}

void RadioAstronomyGUI::calcFWHM()
{
    double c = Astronomy::m_speedOfLight;
    double k = Astronomy::m_boltzmann;
    double m = Astronomy::m_hydrogenMass;
    double f0 = ui->spectrumGaussianFreq->value() * 1e6;
    double vTurb = ui->spectrumGaussianTurb->value() * 1e3; // RSM turbulent velocties - Convert to m/s
    double T = ui->spectrumTemperature->value();
    const double bf = 2.0*sqrt(M_LN2);  // Convert from Doppler parameter to FWHM

    double fFWHM = bf * f0/c * sqrt((2*k*T)/m + vTurb * vTurb);

    ui->spectrumGaussianFWHM->blockSignals(true);
    ui->spectrumGaussianFWHM->setValue(fFWHM);
    ui->spectrumGaussianFWHM->blockSignals(false);
    calcColumnDensity();
}

// Assumes optically thin
void RadioAstronomyGUI::calcColumnDensity()
{
    double f0 = ui->spectrumLineFrequency->value() * 1e6;
    double f = f0 + ui->spectrumGaussianFWHM->value() / 2.0;
    double v = lineDopplerVelocity(f0, f) * 2.0;
    double a = ui->spectrumGaussianAmp->value();
    double integratedIntensity = v * a;
    double columnDensity = 1.81e18 * integratedIntensity;
    ui->columnDensity->setText(QString::number(columnDensity, 'g', 2));
}

void RadioAstronomyGUI::on_powerShowGaussian_clicked(bool checked)
{
    m_settings.m_powerShowGaussian = checked;
    applySettings();
    ui->powerGaussianWidgets->setVisible(checked);
    m_powerGaussianSeries->setVisible(checked);
    updatePowerSelect();
    getRollupContents()->arrangeRollups();
    update();
}

void RadioAstronomyGUI::plotPowerGaussian()
{
    m_powerGaussianSeries->clear();
    double dt0 = ui->powerGaussianCenter->dateTime().toMSecsSinceEpoch();
    double a = ui->powerGaussianAmp->value();
    double floor = ui->powerGaussianFloor->value();
    double fwhm = ui->powerGaussianFWHM->value() * 1000; // Convert from s to ms
    double fwhm_sq = fwhm*fwhm;
    qint64 dt = m_powerXAxis->min().toMSecsSinceEpoch();
    qint64 end = m_powerXAxis->max().toMSecsSinceEpoch();
    int steps = 256;
    qint64 step = (end - dt) / steps;
    for (int i = 0; i < steps; i++)
    {
        double fd = dt - dt0;
        double g = a * std::exp(-4.0*M_LN2*fd*fd/fwhm_sq) + floor;
        m_powerGaussianSeries->append(dt, g);
        dt += step;
    }
}

// Calculate antenna HPBW from Sun's FWHM time
void RadioAstronomyGUI::calcHPBWFromFWHM()
{
    double fwhmSeconds = ui->powerGaussianFWHM->value();
    double sunDegPerSecond = 360.0/(24.0*60.0*60.0);
    double hpbwDeg = fwhmSeconds * sunDegPerSecond;
    ui->powerGaussianHPBW->setValue(hpbwDeg);
}

// Calculate Sun's FWHM time for anntena HPBW
void RadioAstronomyGUI::calcFHWMFromHPBW()
{
    double hpwmDeg = ui->powerGaussianHPBW->value();
    double sunDegPerSecond = 360.0/(24.0*60.0*60.0);
    double fwhmSeconds = hpwmDeg / sunDegPerSecond;
    ui->powerGaussianFWHM->setValue(fwhmSeconds);
}

void RadioAstronomyGUI::on_powerGaussianCenter_dateTimeChanged(QDateTime dateTime)
{
    (void) dateTime;
    plotPowerGaussian();
}

void RadioAstronomyGUI::on_powerGaussianAmp_valueChanged(double value)
{
    (void) value;
    plotPowerGaussian();
}

void RadioAstronomyGUI::on_powerGaussianFloor_valueChanged(double value)
{
    (void) value;
    plotPowerGaussian();
}

void RadioAstronomyGUI::on_powerGaussianFWHM_valueChanged(double value)
{
    (void) value;
    plotPowerGaussian();
    ui->powerGaussianHPBW->blockSignals(true);
    calcHPBWFromFWHM();
    ui->powerGaussianHPBW->blockSignals(false);
}

void RadioAstronomyGUI::on_powerGaussianHPBW_valueChanged(double value)
{
    (void) value;
    calcFHWMFromHPBW();
    ui->powerGaussianFWHM->blockSignals(true);
    plotPowerGaussian();
    ui->powerGaussianFWHM->blockSignals(false);
}

void RadioAstronomyGUI::addToPowerFilter(qreal x, qreal y)
{
    // Add data to circular buffer
    m_window[m_windowIdx] = y;
    m_windowIdx = (m_windowIdx + 1) % m_settings.m_powerFilterN;
    if (m_windowCount < m_settings.m_powerFilterN) {
        m_windowCount++;
    }

    // Filter
    if (m_settings.m_powerFilter == RadioAstronomySettings::FILT_MOVING_AVERAGE)
    {
        // Moving average
        qreal sum = 0.0;
        for (int i = 0; i < m_windowCount; i++) {
            sum += m_window[i];
        }
        qreal mean = sum / m_windowCount;
        y = mean;
    }
    else
    {
        // Median
        std::partial_sort_copy(m_window, m_window + m_windowCount, m_windowSorted, m_windowSorted + m_windowCount);
        qreal median;
        if ((m_windowCount & 1) == 1) {
            median = m_windowSorted[m_windowCount / 2];
        } else {
            median = (m_windowSorted[m_windowCount / 2 - 1] + m_windowSorted[m_windowCount / 2]) / 2.0;
        }
        y = median;
    }

    // Add to series for chart
    m_powerFilteredSeries->append(x, y);
}

void RadioAstronomyGUI::plotPowerFiltered()
{
    delete[] m_window;
    delete[] m_windowSorted;
    m_window = new qreal[m_settings.m_powerFilterN];
    m_windowSorted = new qreal[m_settings.m_powerFilterN];
    m_windowIdx = 0;
    m_windowCount = 0;

    m_powerFilteredSeries->clear();
    QVector<QPointF> powerSeries = m_powerSeries->pointsVector();
    for (int i = 0; i < powerSeries.size(); i++)
    {
        QPointF point = powerSeries.at(i);
        addToPowerFilter(point.x(), point.y());
    }
}

void RadioAstronomyGUI::on_powerShowFiltered_clicked(bool checked)
{
    m_settings.m_powerShowFiltered = checked;
    applySettings();
    ui->powerFilterWidgets->setVisible(checked);
    m_powerFilteredSeries->setVisible(checked);
    getRollupContents()->arrangeRollups();
    update();
}

void RadioAstronomyGUI::on_powerFilter_currentIndexChanged(int index)
{
    m_settings.m_powerFilter = (RadioAstronomySettings::PowerFilter)index;
    applySettings();
    plotPowerFiltered();
}

void RadioAstronomyGUI::on_powerFilterN_valueChanged(int value)
{
    m_settings.m_powerFilterN = value;
    applySettings();
    plotPowerFiltered();
}

void RadioAstronomyGUI::on_powerShowMeasurement_clicked(bool checked)
{
    m_settings.m_powerShowMeasurement = checked;
    applySettings();
    m_powerSeries->setVisible(checked);
}

RadioAstronomyGUI::LABData* RadioAstronomyGUI::parseLAB(QFile* file, float l, float b)
{
    LABData *data = new LABData();
    data->read(file, l, b);
    m_dataLAB.append(data);
    return data;
}

void RadioAstronomyGUI::plotLAB()
{
    int index = ui->spectrumIndex->value();
    if (index < m_fftMeasurements.size())
    {
        FFTMeasurement *fft = m_fftMeasurements[index];
        plotLAB(fft->m_l, fft->m_b, m_beamWidth);
    }
}

void RadioAstronomyGUI::plotLAB(float l, float b, float beamWidth)
{
    // Assume a beamwidth >1deg
    l = round(l);
    b = round(b);

    // Check if we already have the data in memory
    LABData* data = nullptr;
    for (int i = 0; i < m_dataLAB.size(); i++)
    {
        if ((m_dataLAB[i]->m_l == l) && (m_dataLAB[i]->m_b == b))
        {
            data = m_dataLAB[i];
            break;
        }
    }

    if (!data)
    {
        // Try to open previously downloaded data
        QString filenameLAB = HttpDownloadManager::downloadDir() + "/" + QString("lab_l_%1_b_%2.txt").arg(l).arg(b);
        QFile file(filenameLAB);
        if (file.open(QIODevice::ReadOnly))
        {
            qDebug() << "RadioAstronomyGUI::plotLAB: Using cached file: " << filenameLAB;
            data = parseLAB(&file, l, b);
        }
        else
        {
            // Only download one file at a time, so we don't overload the server
            if (!m_downloadingLAB)
            {
                m_downloadingLAB = true;
                m_lLAB = l;
                m_bLAB = b;
                m_filenameLAB = filenameLAB;

                // Request data be generated via web server
                QNetworkRequest request(QUrl("https://www.astro.uni-bonn.de/hisurvey/euhou/LABprofile/index.php"));
                request.setRawHeader("Content-Type", "application/x-www-form-urlencoded");

                QUrlQuery params;
                params.addQueryItem("coordinates", "lb");
                params.addQueryItem("ral", QString::number(l));
                params.addQueryItem("decb",  QString::number(b));
                params.addQueryItem("beam",  QString::number(beamWidth));
                params.addQueryItem("vmin", "-100.0" );
                params.addQueryItem("vmax", "100.0" );
                params.addQueryItem("search", "Search data" );

                m_networkManager->post(request, params.query(QUrl::FullyEncoded).toUtf8());
            }
        }
    }

    if (data)
    {
        data->toSeries(m_fftLABSeries);
        spectrumAutoscale();
    }
}

void RadioAstronomyGUI::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "RadioAstronomyGUI::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
        m_downloadingLAB = false;
    }
    else
    {
        QString answer = reply->readAll();
        QRegExp re("a href=\\\"download.php([^\"]*)\"");
        if (re.indexIn(answer) != -1)
        {
            QString filename = re.capturedTexts()[1];
            qDebug() << "RadioAstronomyGUI: Downloading LAB reference data: " << filename;
            m_dlm.download(QUrl("https://www.astro.uni-bonn.de/hisurvey/euhou/LABprofile/download.php" + filename), m_filenameLAB);
        }
        else
        {
            qDebug() << "RadioAstronomyGUI::networkManagerFinished - No filename found: " << answer;
            m_downloadingLAB = false;
        }
    }

    reply->deleteLater();
}

void RadioAstronomyGUI::downloadFinished(const QString& filename, bool success)
{
    if (success)
    {
        QFile file(filename);
        if (file.open(QIODevice::ReadOnly))
        {
            LABData *data = parseLAB(&file, m_lLAB, m_bLAB);
            file.close();
            // Check if the data we've downloaded is for the current FFT being displayed
            int index = ui->spectrumIndex->value();
            if (index < m_fftMeasurements.size())
            {
                FFTMeasurement *fft = m_fftMeasurements[index];
                if (m_lLAB == fft->m_l && m_bLAB == fft->m_b)
                {
                    data->toSeries(m_fftLABSeries);
                    spectrumAutoscale();
                    m_downloadingLAB = false;
                }
                else
                {
                    // Try ploting for current FFT (as we only allow one download at a time, so may have been skipped)
                    m_downloadingLAB = false;
                    plotLAB(fft->m_l, fft->m_b, m_beamWidth);
                    // Don't clear m_downloadingLAB after this point
                }
            }
        } else {
            qDebug() << "RadioAstronomyGUI::downloadFinished: Failed to open downloaded file: " << filename;
            m_downloadingLAB = false;
        }
    }
    else
    {
        qDebug() << "RadioAstronomyGUI::downloadFinished: Failed to download: " << filename;
        m_downloadingLAB = false;
    }
}

void RadioAstronomyGUI::displayRunModeSettings()
{
    bool sweep = m_settings.m_runMode == RadioAstronomySettings::SWEEP;
    ui->sweep1CoordLabel->setVisible(sweep);
    ui->sweepType->setVisible(sweep);
    ui->sweep1StartLabel->setVisible(sweep);
    ui->sweep1Start->setVisible(sweep);
    ui->sweep1StopLabel->setVisible(sweep);
    ui->sweep1Stop->setVisible(sweep);
    ui->sweep1StepLabel->setVisible(sweep);
    ui->sweep1Step->setVisible(sweep);
    ui->sweep1DelayLabel->setVisible(sweep);
    ui->sweep1Delay->setVisible(sweep);
    ui->sweep2CoordLabel->setVisible(sweep);
    ui->sweep2StartLabel->setVisible(sweep);
    ui->sweep2Start->setVisible(sweep);
    ui->sweep2StopLabel->setVisible(sweep);
    ui->sweep2Stop->setVisible(sweep);
    ui->sweep2StepLabel->setVisible(sweep);
    ui->sweep2Step->setVisible(sweep);
    ui->sweep2DelayLabel->setVisible(sweep);
    ui->sweep2Delay->setVisible(sweep);
    ui->sweepStatus->setVisible(sweep);
    ui->runLayout->activate();         // Needed otherwise height of rollup doesn't seem to be reduced
    ui->statusLayout->activate();      // going from sweep to single/continuous
    getRollupContents()->arrangeRollups();
}

void RadioAstronomyGUI::on_runMode_currentIndexChanged(int index)
{
    m_settings.m_runMode = (RadioAstronomySettings::RunMode)index;
    applySettings();
    displayRunModeSettings();
}

void RadioAstronomyGUI::on_sweepType_currentIndexChanged(int index)
{
    m_settings.m_sweepType = (RadioAstronomySettings::SweepType)index;
    if ((index == 0) || (index == 2))
    {
        ui->sweep1CoordLabel->setText("Az");
        ui->sweep2CoordLabel->setText("El");
    }
    else if (index == 1)
    {
        ui->sweep1CoordLabel->setText("l");
        ui->sweep2CoordLabel->setText("b");
    }
}

void RadioAstronomyGUI::on_sweep1Start_valueChanged(double value)
{
    m_settings.m_sweep1Start = value;
    applySettings();
}

void RadioAstronomyGUI::on_sweep1Stop_valueChanged(double value)
{
    m_settings.m_sweep1Stop = value;
    applySettings();
}

void RadioAstronomyGUI::on_sweep1Step_valueChanged(double value)
{
    m_settings.m_sweep1Step = value;
    applySettings();
}

void RadioAstronomyGUI::on_sweep1Delay_valueChanged(double value)
{
    m_settings.m_sweep1Delay = value;
    applySettings();
}

void RadioAstronomyGUI::on_sweep2Start_valueChanged(double value)
{
    m_settings.m_sweep2Start = value;
    applySettings();
}

void RadioAstronomyGUI::on_sweep2Stop_valueChanged(double value)
{
    m_settings.m_sweep2Stop = value;
    applySettings();
}

void RadioAstronomyGUI::on_sweep2Step_valueChanged(double value)
{
    m_settings.m_sweep2Step = value;
    applySettings();
}

void RadioAstronomyGUI::on_sweep2Delay_valueChanged(double value)
{
    m_settings.m_sweep2Delay = value;
    applySettings();
}

void RadioAstronomyGUI::on_sweepStartAtTime_currentIndexChanged(int index)
{
    m_settings.m_sweepStartAtTime = ui->sweepStartAtTime->currentIndex() == 1;
    ui->sweepStartDateTime->setVisible(index == 1);
    getRollupContents()->arrangeRollups();
    applySettings();
}

void RadioAstronomyGUI::on_sweepStartDateTime_dateTimeChanged(const QDateTime& dateTime)
{
    m_settings.m_sweepStartDateTime = dateTime;
    applySettings();
}

void RadioAstronomyGUI::on_startStop_clicked(bool checked)
{
    if (checked)
    {
        ui->startStop->setStyleSheet("QToolButton { background-color : green; }");
        applySettings();
        if (m_settings.m_power2DLinkSweep)
        {
            update2DSettingsFromSweep();
            create2DImage();
        }
        m_radioAstronomy->getInputMessageQueue()->push(RadioAstronomy::MsgStartSweep::create());
    }
    else
    {
        m_radioAstronomy->getInputMessageQueue()->push(RadioAstronomy::MsgStopSweep::create());
        if (m_settings.m_runMode != RadioAstronomySettings::SWEEP) {
            ui->startStop->setStyleSheet("QToolButton { background-color : blue; }");
        }
    }
}

void RadioAstronomyGUI::calcPowerChartTickCount(int width)
{
    // These values should probably be dependent on the font used
    if (m_powerXAxis) {
        if (m_powerXAxisSameDay) {
            m_powerXAxis->setTickCount(width > 700 ? 10 : 5);
        } else {
            m_powerXAxis->setTickCount(width > 1200 ? 10 : 5);
        }
    }
}

void RadioAstronomyGUI::calcSpectrumChartTickCount(QValueAxis *axis, int width)
{
    if (axis) {
        axis->setTickCount(width > 700 ? 10 : 5);
    }
}

void RadioAstronomyGUI::resizeEvent(QResizeEvent* size)
{
    int width = size->size().width();
    calcPowerChartTickCount(width);
    calcSpectrumChartTickCount(m_fftXAxis, width);
    calcSpectrumChartTickCount(m_fftDopplerAxis, width);
    calcSpectrumChartTickCount(m_calXAxis, width);
    ChannelGUI::resizeEvent(size);
}

void RadioAstronomyGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &RadioAstronomyGUI::on_deltaFrequency_changed);
    QObject::connect(ui->sampleRate, &ValueDialZ::changed, this, &RadioAstronomyGUI::on_sampleRate_changed);
    QObject::connect(ui->rfBW, &ValueDialZ::changed, this, &RadioAstronomyGUI::on_rfBW_changed);
    QObject::connect(ui->integration, &ValueDialZ::changed, this, &RadioAstronomyGUI::on_integration_changed);
    QObject::connect(ui->fftSize, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RadioAstronomyGUI::on_fftSize_currentIndexChanged);
    QObject::connect(ui->fftWindow, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RadioAstronomyGUI::on_fftWindow_currentIndexChanged);
    QObject::connect(ui->filterFreqs, &QLineEdit::editingFinished, this, &RadioAstronomyGUI::on_filterFreqs_editingFinished);
    QObject::connect(ui->starTracker, &QComboBox::currentTextChanged, this, &RadioAstronomyGUI::on_starTracker_currentTextChanged);
    QObject::connect(ui->rotator, &QComboBox::currentTextChanged, this, &RadioAstronomyGUI::on_rotator_currentTextChanged);
    QObject::connect(ui->showSensors, &QToolButton::clicked, this, &RadioAstronomyGUI::on_showSensors_clicked);
    QObject::connect(ui->tempRXSelect, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RadioAstronomyGUI::on_tempRXSelect_currentIndexChanged);
    QObject::connect(ui->tempRX, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RadioAstronomyGUI::on_tempRX_valueChanged);
    QObject::connect(ui->tempCMB, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RadioAstronomyGUI::on_tempCMB_valueChanged);
    QObject::connect(ui->tempGal, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RadioAstronomyGUI::on_tempGal_valueChanged);
    QObject::connect(ui->tempSP, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RadioAstronomyGUI::on_tempSP_valueChanged);
    QObject::connect(ui->tempAtm, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RadioAstronomyGUI::on_tempAtm_valueChanged);
    QObject::connect(ui->tempAir, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RadioAstronomyGUI::on_tempAir_valueChanged);
    QObject::connect(ui->zenithOpacity, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RadioAstronomyGUI::on_zenithOpacity_valueChanged);
    QObject::connect(ui->elevation, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RadioAstronomyGUI::on_elevation_valueChanged);
    QObject::connect(ui->tempAtmLink, &ButtonSwitch::toggled, this, &RadioAstronomyGUI::on_tempAtmLink_toggled);
    QObject::connect(ui->tempAirLink, &ButtonSwitch::toggled, this, &RadioAstronomyGUI::on_tempAirLink_toggled);
    QObject::connect(ui->tempGalLink, &ButtonSwitch::toggled, this, &RadioAstronomyGUI::on_tempGalLink_toggled);
    QObject::connect(ui->elevationLink, &ButtonSwitch::toggled, this, &RadioAstronomyGUI::on_elevationLink_toggled);
    QObject::connect(ui->gainVariation, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RadioAstronomyGUI::on_gainVariation_valueChanged);
    QObject::connect(ui->omegaAUnits, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RadioAstronomyGUI::on_omegaAUnits_currentIndexChanged);
    QObject::connect(ui->sourceType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RadioAstronomyGUI::on_sourceType_currentIndexChanged);
    QObject::connect(ui->omegaS, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RadioAstronomyGUI::on_omegaS_valueChanged);
    QObject::connect(ui->omegaSUnits, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RadioAstronomyGUI::on_omegaSUnits_currentIndexChanged);
    QObject::connect(ui->spectrumChartSelect, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RadioAstronomyGUI::on_spectrumChartSelect_currentIndexChanged);
    QObject::connect(ui->showCalSettings, &QToolButton::clicked, this, &RadioAstronomyGUI::on_showCalSettings_clicked);
    QObject::connect(ui->startCalHot, &QToolButton::clicked, this, &RadioAstronomyGUI::on_startCalHot_clicked);
    QObject::connect(ui->startCalCold, &QToolButton::clicked, this, &RadioAstronomyGUI::on_startCalCold_clicked);
    QObject::connect(ui->recalibrate, &ButtonSwitch::toggled, this, &RadioAstronomyGUI::on_recalibrate_toggled);
    QObject::connect(ui->spectrumShowLegend, &ButtonSwitch::toggled, this, &RadioAstronomyGUI::on_spectrumShowLegend_toggled);
    QObject::connect(ui->spectrumShowRefLine, &ButtonSwitch::toggled, this, &RadioAstronomyGUI::on_spectrumShowRefLine_toggled);
    QObject::connect(ui->spectrumTemp, &ButtonSwitch::toggled, this, &RadioAstronomyGUI::on_spectrumTemp_toggled);
    QObject::connect(ui->spectrumMarker, &ButtonSwitch::toggled, this, &RadioAstronomyGUI::on_spectrumMarker_toggled);
    QObject::connect(ui->spectrumPeak, &ButtonSwitch::toggled, this, &RadioAstronomyGUI::on_spectrumPeak_toggled);
    QObject::connect(ui->spectrumReverseXAxis, &ButtonSwitch::toggled, this, &RadioAstronomyGUI::on_spectrumReverseXAxis_toggled);
    QObject::connect(ui->savePowerData, &QToolButton::clicked, this, &RadioAstronomyGUI::on_savePowerData_clicked);
    QObject::connect(ui->savePowerChartImage, &QToolButton::clicked, this, &RadioAstronomyGUI::on_savePowerChartImage_clicked);
    QObject::connect(ui->saveSpectrumData, &QToolButton::clicked, this, &RadioAstronomyGUI::on_saveSpectrumData_clicked);
    QObject::connect(ui->loadSpectrumData, &QToolButton::clicked, this, &RadioAstronomyGUI::on_loadSpectrumData_clicked);
    QObject::connect(ui->saveSpectrumChartImage, &QToolButton::clicked, this, &RadioAstronomyGUI::on_saveSpectrumChartImage_clicked);
    QObject::connect(ui->saveSpectrumChartImages, &QToolButton::clicked, this, &RadioAstronomyGUI::on_saveSpectrumChartImages_clicked);
    QObject::connect(ui->clearData, &QToolButton::clicked, this, &RadioAstronomyGUI::on_clearData_clicked);
    QObject::connect(ui->clearCal, &QToolButton::clicked, this, &RadioAstronomyGUI::on_clearCal_clicked);
    QObject::connect(ui->spectrumAutoscale, &QToolButton::toggled, this, &RadioAstronomyGUI::on_spectrumAutoscale_toggled);
    QObject::connect(ui->spectrumAutoscaleX, &QToolButton::clicked, this, &RadioAstronomyGUI::on_spectrumAutoscaleX_clicked);
    QObject::connect(ui->spectrumAutoscaleY, &QToolButton::clicked, this, &RadioAstronomyGUI::on_spectrumAutoscaleY_clicked);
    QObject::connect(ui->spectrumReference, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RadioAstronomyGUI::on_spectrumReference_valueChanged);
    QObject::connect(ui->spectrumRange, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RadioAstronomyGUI::on_spectrumRange_valueChanged);
    QObject::connect(ui->spectrumCenterFreq, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RadioAstronomyGUI::on_spectrumCenterFreq_valueChanged);
    QObject::connect(ui->spectrumSpan, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RadioAstronomyGUI::on_spectrumSpan_valueChanged);
    QObject::connect(ui->spectrumYUnits, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RadioAstronomyGUI::on_spectrumYUnits_currentIndexChanged);
    QObject::connect(ui->spectrumBaseline, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RadioAstronomyGUI::on_spectrumBaseline_currentIndexChanged);
    QObject::connect(ui->spectrumIndex, &QSlider::valueChanged, this, &RadioAstronomyGUI::on_spectrumIndex_valueChanged);
    QObject::connect(ui->spectrumLine, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RadioAstronomyGUI::on_spectrumLine_currentIndexChanged);
    QObject::connect(ui->spectrumLineFrequency, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RadioAstronomyGUI::on_spectrumLineFrequency_valueChanged);
    QObject::connect(ui->refFrame, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RadioAstronomyGUI::on_refFrame_currentIndexChanged);
    QObject::connect(ui->sunDistanceToGC, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RadioAstronomyGUI::on_sunDistanceToGC_valueChanged);
    QObject::connect(ui->sunOrbitalVelocity, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RadioAstronomyGUI::on_sunOrbitalVelocity_valueChanged);
    QObject::connect(ui->spectrumGaussianFreq, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RadioAstronomyGUI::on_spectrumGaussianFreq_valueChanged);
    QObject::connect(ui->spectrumGaussianAmp, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RadioAstronomyGUI::on_spectrumGaussianAmp_valueChanged);
    QObject::connect(ui->spectrumGaussianFloor, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RadioAstronomyGUI::on_spectrumGaussianFloor_valueChanged);
    QObject::connect(ui->spectrumGaussianFWHM, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RadioAstronomyGUI::on_spectrumGaussianFWHM_valueChanged);
    QObject::connect(ui->spectrumGaussianTurb, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RadioAstronomyGUI::on_spectrumGaussianTurb_valueChanged);
    QObject::connect(ui->spectrumTemperature, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RadioAstronomyGUI::on_spectrumTemperature_valueChanged);
    QObject::connect(ui->spectrumShowLAB, &ButtonSwitch::toggled, this, &RadioAstronomyGUI::on_spectrumShowLAB_toggled);
    QObject::connect(ui->spectrumShowDistance, &ButtonSwitch::toggled, this, &RadioAstronomyGUI::on_spectrumShowDistance_toggled);
    QObject::connect(ui->tCalHotSelect, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RadioAstronomyGUI::on_tCalHotSelect_currentIndexChanged);
    QObject::connect(ui->tCalHot, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RadioAstronomyGUI::on_tCalHot_valueChanged);
    QObject::connect(ui->tCalColdSelect, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RadioAstronomyGUI::on_tCalColdSelect_currentIndexChanged);
    QObject::connect(ui->tCalCold, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RadioAstronomyGUI::on_tCalCold_valueChanged);
    QObject::connect(ui->powerChartSelect, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RadioAstronomyGUI::on_powerChartSelect_currentIndexChanged);
    QObject::connect(ui->powerYUnits, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RadioAstronomyGUI::on_powerYUnits_currentIndexChanged);
    QObject::connect(ui->powerShowMarker, &ButtonSwitch::toggled, this, &RadioAstronomyGUI::on_powerShowMarker_toggled);
    QObject::connect(ui->powerShowAirTemp, &ButtonSwitch::toggled, this, &RadioAstronomyGUI::on_powerShowAirTemp_toggled);
    QObject::connect(ui->powerShowPeak, &ButtonSwitch::toggled, this, &RadioAstronomyGUI::on_powerShowPeak_toggled);
    QObject::connect(ui->powerShowAvg, &ButtonSwitch::toggled, this, &RadioAstronomyGUI::on_powerShowAvg_toggled);
    QObject::connect(ui->powerShowLegend, &ButtonSwitch::toggled, this, &RadioAstronomyGUI::on_powerShowLegend_toggled);
    QObject::connect(ui->powerAutoscale, &QToolButton::toggled, this, &RadioAstronomyGUI::on_powerAutoscale_toggled);
    QObject::connect(ui->powerAutoscaleY, &QToolButton::clicked, this, &RadioAstronomyGUI::on_powerAutoscaleY_clicked);
    QObject::connect(ui->powerAutoscaleX, &QToolButton::clicked, this, &RadioAstronomyGUI::on_powerAutoscaleX_clicked);
    QObject::connect(ui->powerReference, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RadioAstronomyGUI::on_powerReference_valueChanged);
    QObject::connect(ui->powerRange, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RadioAstronomyGUI::on_powerRange_valueChanged);
    QObject::connect(ui->powerStartTime, &WrappingDateTimeEdit::dateTimeChanged, this, &RadioAstronomyGUI::on_powerStartTime_dateTimeChanged);
    QObject::connect(ui->powerEndTime, &WrappingDateTimeEdit::dateTimeChanged, this, &RadioAstronomyGUI::on_powerEndTime_dateTimeChanged);
    QObject::connect(ui->powerShowGaussian, &ButtonSwitch::clicked, this, &RadioAstronomyGUI::on_powerShowGaussian_clicked);
    QObject::connect(ui->powerGaussianCenter, &WrappingDateTimeEdit::dateTimeChanged, this, &RadioAstronomyGUI::on_powerGaussianCenter_dateTimeChanged);
    QObject::connect(ui->powerGaussianAmp, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RadioAstronomyGUI::on_powerGaussianAmp_valueChanged);
    QObject::connect(ui->powerGaussianFloor, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RadioAstronomyGUI::on_powerGaussianFloor_valueChanged);
    QObject::connect(ui->powerGaussianFWHM, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RadioAstronomyGUI::on_powerGaussianFWHM_valueChanged);
    QObject::connect(ui->powerGaussianHPBW, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RadioAstronomyGUI::on_powerGaussianHPBW_valueChanged);
    QObject::connect(ui->runMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RadioAstronomyGUI::on_runMode_currentIndexChanged);
    QObject::connect(ui->sweepType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RadioAstronomyGUI::on_sweepType_currentIndexChanged);
    QObject::connect(ui->startStop, &ButtonSwitch::clicked, this, &RadioAstronomyGUI::on_startStop_clicked);
    QObject::connect(ui->sweepStartAtTime, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RadioAstronomyGUI::on_sweepStartAtTime_currentIndexChanged);
    QObject::connect(ui->sweepStartDateTime, &QDateTimeEdit::dateTimeChanged, this, &RadioAstronomyGUI::on_sweepStartDateTime_dateTimeChanged);
    QObject::connect(ui->powerColourAutoscale, &QToolButton::toggled, this, &RadioAstronomyGUI::on_powerColourAutoscale_toggled);
    QObject::connect(ui->powerColourScaleMin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RadioAstronomyGUI::on_powerColourScaleMin_valueChanged);
    QObject::connect(ui->powerColourScaleMax, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RadioAstronomyGUI::on_powerColourScaleMax_valueChanged);
    QObject::connect(ui->powerColourPalette, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RadioAstronomyGUI::on_powerColourPalette_currentIndexChanged);
    QObject::connect(ui->powerTable, &QTableWidget::cellDoubleClicked, this, &RadioAstronomyGUI::on_powerTable_cellDoubleClicked);
}

void RadioAstronomyGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_centerFrequency + m_settings.m_inputFrequencyOffset);
}
