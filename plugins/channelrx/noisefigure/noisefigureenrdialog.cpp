///////////////////////////////////////////////////////////////////////////////////
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

#include <QDebug>

#include <array>

#include <boost/version.hpp>
#include <boost/math/interpolators/barycentric_rational.hpp>

#include "noisefigureenrdialog.h"
#include "util/interpolation.h"

#if BOOST_VERSION < 107700
using namespace boost::math;
#else
using namespace boost::math::interpolators;
#endif

NoiseFigureENRDialog::NoiseFigureENRDialog(NoiseFigureSettings *settings, QWidget* parent) :
    QDialog(parent),
    m_settings(settings),
    m_chart(nullptr),
    ui(new Ui::NoiseFigureENRDialog)
{
    ui->setupUi(this);
    ui->enr->sortByColumn(0, Qt::AscendingOrder);
    for (int i = 0; i < m_settings->m_enr.size(); i++) {
        addRow( m_settings->m_enr[i]->m_frequency, m_settings->m_enr[i]->m_enr);
    }
    ui->interpolation->setCurrentIndex((int)m_settings->m_interpolation);
    plotChart();
}

NoiseFigureENRDialog::~NoiseFigureENRDialog()
{
    delete ui;
}

void NoiseFigureENRDialog::accept()
{
    QDialog::accept();
    qDeleteAll(m_settings->m_enr);
    m_settings->m_enr.clear();
    ui->enr->sortByColumn(0, Qt::AscendingOrder);
    for (int i = 0; i < ui->enr->rowCount(); i++)
    {
        QTableWidgetItem *freqItem = ui->enr->item(i, ENR_COL_FREQ);
        QTableWidgetItem *enrItem =  ui->enr->item(i, ENR_COL_ENR);
        double freqValue = freqItem->data(Qt::DisplayRole).toDouble();
        double enrValue = enrItem->data(Qt::DisplayRole).toDouble();

        NoiseFigureSettings::ENR *enr = new NoiseFigureSettings::ENR(freqValue, enrValue);
        m_settings->m_enr.append(enr);
    }
    m_settings->m_interpolation = (NoiseFigureSettings::Interpolation)ui->interpolation->currentIndex();
}

void NoiseFigureENRDialog::addRow(double freq, double enr)
{
    ui->enr->setSortingEnabled(false);
    ui->enr->blockSignals(true);
    int row = ui->enr->rowCount();
    ui->enr->setRowCount(row + 1);
    QTableWidgetItem *freqItem = new QTableWidgetItem();
    QTableWidgetItem *enrItem = new QTableWidgetItem();
    ui->enr->setItem(row, ENR_COL_FREQ, freqItem);
    ui->enr->setItem(row, ENR_COL_ENR, enrItem);
    freqItem->setData(Qt::DisplayRole, freq);
    enrItem->setData(Qt::DisplayRole, enr);
    ui->enr->blockSignals(false);
    ui->enr->setSortingEnabled(true);
}

void NoiseFigureENRDialog::on_addRow_clicked()
{
    addRow(0.0, 0.0);
}

void NoiseFigureENRDialog::on_deleteRow_clicked()
{
    QModelIndexList indexList = ui->enr->selectionModel()->selectedRows();
    if (!indexList.isEmpty())
    {
        int row = indexList.at(0).row();
        ui->enr->removeRow(row);
    }
    plotChart();
}

void NoiseFigureENRDialog::on_enr_cellChanged(int row, int column)
{
    (void) row;
    (void) column;
    plotChart();
}

void NoiseFigureENRDialog::on_start_valueChanged(int value)
{
    (void) value;
    plotChart();
}

void NoiseFigureENRDialog::on_stop_valueChanged(int value)
{
    (void) value;
    plotChart();
}

void NoiseFigureENRDialog::plotChart()
{
    QChart *oldChart = m_chart;

    m_chart = new QChart();

    m_chart->layout()->setContentsMargins(0, 0, 0, 0);
    m_chart->setMargins(QMargins(1, 1, 1, 1));
    m_chart->setTheme(QChart::ChartThemeDark);

    int size = ui->enr->rowCount();

    QLineSeries *linearSeries = new QLineSeries();
    QLineSeries *barySeries = new QLineSeries();
    double maxENR = 0.0;
    double minENR = 1000.0;

    if (size >= 2)
    {
        // Sort in to ascending frequency
        std::vector<std::array<double,2>> points;
        for (int i = 0; i < size; i++)
        {
            QTableWidgetItem *freqItem = ui->enr->item(i, ENR_COL_FREQ);
            QTableWidgetItem *enrItem =  ui->enr->item(i, ENR_COL_ENR);
            double freq = freqItem->data(Qt::DisplayRole).toDouble();
            double enr = enrItem->data(Qt::DisplayRole).toDouble();
            std::array<double,2> p = {freq, enr};
            points.push_back(p);
        }
        sort(points.begin(), points.end());

        // Copy to vectors for interpolation routines. Is there a better way?
        std::vector<double> x(size);
        std::vector<double> y(size);
        for (int i = 0; i < size; i++)
        {
            x[i] = points[i][0];
            y[i] = points[i][1];
        }
        int order = size - 1;
        barycentric_rational<double> interpolant(std::move(x), std::move(y), order);

        x.resize(size);
        y.resize(size);
        for (int i = 0; i < size; i++)
        {
            x[i] = points[i][0];
            y[i] = points[i][1];
        }

        // Plot interpolated ENR using both methods for comparison
        linearSeries->setName("Linear");
        barySeries->setName("Barycentric");
        double startFreq = ui->start->value();
        double stopFreq = ui->stop->value();
        double step = (stopFreq - startFreq) / 50.0;
        for (double freq = startFreq; freq < stopFreq; freq += step)
        {
            double enr;

            // Linear interpolation
            enr = Interpolation::linear(x.begin(), x.end(), y.begin(), freq);
            linearSeries->append(freq, enr);
            minENR = std::min(minENR, enr);
            maxENR = std::max(maxENR, enr);

            // Barycentric interpolation
            enr = interpolant(freq);
            barySeries->append(freq, enr);
            minENR = std::min(minENR, enr);
            maxENR = std::max(maxENR, enr);
        }
    }

    QValueAxis *xAxis = new QValueAxis();
    QValueAxis *yAxis = new QValueAxis();

    m_chart->addAxis(xAxis, Qt::AlignBottom);
    m_chart->addAxis(yAxis, Qt::AlignLeft);

    xAxis->setTitleText("Frequency (MHz)");
    yAxis->setTitleText("ENR (dB)");

    m_chart->addSeries(linearSeries);

    m_chart->addSeries(barySeries);

    linearSeries->attachAxis(xAxis);
    linearSeries->attachAxis(yAxis);

    barySeries->attachAxis(xAxis);
    barySeries->attachAxis(yAxis);

    yAxis->setRange(std::floor(minENR * 10.0)/10.0, std::ceil(maxENR * 10.0)/10.0);

    ui->chart->setChart(m_chart);

    delete oldChart;
}
