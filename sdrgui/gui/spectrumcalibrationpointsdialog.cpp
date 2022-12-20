///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 F4EXB                                                      //
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

#include <QFileDialog>
#include <QFile>
#include <QStandardPaths>

#include "util/db.h"
#include "util/csv.h"
#include "dsp/spectrummarkers.h"
#include "gui/dialpopup.h"

#include "spectrumcalibrationpointsdialog.h"

#include "ui_spectrumcalibrationpointsdialog.h"

SpectrumCalibrationPointsDialog::SpectrumCalibrationPointsDialog(
    QList<SpectrumCalibrationPoint>& calibrationPoints,
    SpectrumSettings::CalibrationInterpolationMode& calibrationInterpMode,
    const SpectrumHistogramMarker *markerZero,
    QWidget* parent
) :
    QDialog(parent),
    ui(new Ui::SpectrumCalibrationPointsDialog),
    m_calibrationPoints(calibrationPoints),
    m_calibrationInterpMode(calibrationInterpMode),
    m_markerZero(markerZero),
    m_calibrationPointIndex(0),
    m_centerFrequency(0),
    m_globalCorrection(0.0),
    m_setElseCorrectGlobal(false)
{
    ui->setupUi(this);
    ui->calibPointFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->calibPointFrequency->setValueRange(false, 10, -9999999999L, 9999999999L);
    ui->calibrationGlobalCorr->setColorMapper(ColorMapper(ColorMapper::GrayYellow));
    ui->calibrationGlobalCorr->setValueRange(false, 5, -20000L, 4000L, 2);
    ui->relativePower->setColorMapper(ColorMapper(ColorMapper::GrayYellow));
    ui->relativePower->setValueRange(false, 5, -20000L, 4000L, 2);
    ui->calibratedPower->setColorMapper(ColorMapper(ColorMapper::GrayYellow));
    ui->calibratedPower->setValueRange(false, 5, -20000L, 4000L, 2);
    ui->calibPoint->setMaximum(m_calibrationPoints.size() - 1);
    ui->calibInterpMode->blockSignals(true);
    ui->calibInterpMode->setCurrentIndex((int) m_calibrationInterpMode);
    ui->calibInterpMode->blockSignals(false);
    ui->calibrationGlobalCorr->setValue(m_globalCorrection * 100.0);
    ui->corrOrSet->setText("Cor");
    displayCalibrationPoint();
    DialPopup::addPopupsToChildDials(this);
}

SpectrumCalibrationPointsDialog::~SpectrumCalibrationPointsDialog()
{}

void SpectrumCalibrationPointsDialog::displayCalibrationPoint()
{
    ui->calibPointFrequency->blockSignals(true);
    ui->calibPoint->blockSignals(true);
    ui->relativePower->blockSignals(true);
    ui->calibratedPower->blockSignals(true);

    if (m_calibrationPoints.size() == 0)
    {
        ui->calibPoint->setEnabled(false);
        ui->calibPointDel->setEnabled(false);
        ui->relativePower->setEnabled(false);
        ui->calibratedPower->setEnabled(false);
        ui->calibPointFrequency->setEnabled(false);
        ui->importMarkerZero->setEnabled(false);
        ui->centerFrequency->setEnabled(false);
    }
    else
    {
        ui->calibPoint->setEnabled(true);
        ui->calibPointDel->setEnabled(true);
        ui->relativePower->setEnabled(true);
        ui->calibratedPower->setEnabled(true);
        ui->calibPointFrequency->setEnabled(true);
        ui->importMarkerZero->setEnabled(true);
        ui->centerFrequency->setEnabled(true);
        ui->calibPoint->setValue(m_calibrationPointIndex);
        ui->calibPointText->setText(tr("%1").arg(m_calibrationPointIndex));
        double powerDB = CalcDb::dbPower(m_calibrationPoints[m_calibrationPointIndex].m_powerRelativeReference, 1e-20);
        ui->relativePower->setValue(round(powerDB*100.0)); // fixed point 2 decimals
        powerDB = CalcDb::dbPower(m_calibrationPoints[m_calibrationPointIndex].m_powerCalibratedReference, 1e-20);
        ui->calibratedPower->setValue(round(powerDB*100.0)); // fixed point 2 decimals
        ui->calibPointFrequency->setValue(m_calibrationPoints[m_calibrationPointIndex].m_frequency);
    }

    ui->calibPointFrequency->blockSignals(false);
    ui->calibPoint->blockSignals(false);
    ui->relativePower->blockSignals(false);
    ui->calibratedPower->blockSignals(false);
}

void SpectrumCalibrationPointsDialog::on_calibPoint_valueChanged(int value)
{
    if (m_calibrationPoints.size() == 0) {
        return;
    }

    m_calibrationPointIndex = value;
    displayCalibrationPoint();
}

void SpectrumCalibrationPointsDialog::on_calibPointAdd_clicked()
{
    m_calibrationPoints.append(SpectrumCalibrationPoint());
    m_calibrationPoints.back().m_frequency = m_centerFrequency;
    m_calibrationPointIndex = m_calibrationPoints.size() - 1;
    ui->calibPoint->setMaximum(m_calibrationPoints.size() - 1);
    ui->calibPoint->setMinimum(0);
    displayCalibrationPoint();
}

void SpectrumCalibrationPointsDialog::on_calibPointDel_clicked()
{
    if (m_calibrationPoints.size() == 0) {
        return;
    }

    m_calibrationPoints.removeAt(m_calibrationPointIndex);
    m_calibrationPointIndex = m_calibrationPointIndex < m_calibrationPoints.size() ?
        m_calibrationPointIndex : m_calibrationPointIndex - 1;
    ui->calibPoint->setMaximum(m_calibrationPoints.size() - 1);
    ui->calibPoint->setMinimum(0);
    displayCalibrationPoint();
}

void SpectrumCalibrationPointsDialog::on_relativePower_changed(qint64 value)
{
    if (m_calibrationPoints.size() == 0) {
        return;
    }

    float powerDB = value / 100.0f;
    m_calibrationPoints[m_calibrationPointIndex].m_powerRelativeReference = CalcDb::powerFromdB(powerDB);
    emit updateCalibrationPoints();
}

void SpectrumCalibrationPointsDialog::on_calibratedPower_changed(qint64 value)
{
    if (m_calibrationPoints.size() == 0) {
        return;
    }

    float powerDB = value / 100.0f;
    m_calibrationPoints[m_calibrationPointIndex].m_powerCalibratedReference = CalcDb::powerFromdB(powerDB);
    emit updateCalibrationPoints();
}

void SpectrumCalibrationPointsDialog::on_calibPointFrequency_changed(qint64 value)
{
    if (m_calibrationPoints.size() == 0) {
        return;
    }

    m_calibrationPoints[m_calibrationPointIndex].m_frequency = value;
    emit updateCalibrationPoints();
}

void SpectrumCalibrationPointsDialog::on_calibPointDuplicate_clicked()
{
    if (m_calibrationPoints.size() == 0) {
        return;
    }

    m_calibrationPoints.push_back(SpectrumCalibrationPoint(m_calibrationPoints[m_calibrationPointIndex]));
    ui->calibPoint->setMaximum(m_calibrationPoints.size() - 1);
    ui->calibPoint->setMinimum(0);
    m_calibrationPointIndex = m_calibrationPoints.size() - 1;
    displayCalibrationPoint();
}

void SpectrumCalibrationPointsDialog::on_calibPointsSort_clicked()
{
    if (m_calibrationPoints.size() == 0) {
        return;
    }

    std::sort(m_calibrationPoints.begin(), m_calibrationPoints.end(), calibrationPointsLessThan);
    m_calibrationPointIndex = 0;
    displayCalibrationPoint();
}

void SpectrumCalibrationPointsDialog::on_importMarkerZero_clicked()
{
    if ((m_calibrationPoints.size() == 0) || (m_markerZero == nullptr)) {
        return;
    }

    m_calibrationPoints[m_calibrationPointIndex].m_frequency = m_markerZero->m_frequency;
    m_calibrationPoints[m_calibrationPointIndex].m_powerRelativeReference = CalcDb::powerFromdB(m_markerZero->m_powerMax);
    displayCalibrationPoint();
    emit updateCalibrationPoints();
}

void SpectrumCalibrationPointsDialog::on_centerFrequency_clicked()
{
    if (m_calibrationPoints.size() == 0) {
        return;
    }

    m_calibrationPoints[m_calibrationPointIndex].m_frequency = m_centerFrequency;
    displayCalibrationPoint();
    emit updateCalibrationPoints();
}

void SpectrumCalibrationPointsDialog::on_calibInterpMode_currentIndexChanged(int index)
{
    m_calibrationInterpMode = (SpectrumSettings::CalibrationInterpolationMode) index;
    emit updateCalibrationPoints();
}

void SpectrumCalibrationPointsDialog::on_calibrationGlobalCorr_changed(qint64 value)
{
    m_globalCorrection = value / 100.0;
}

void SpectrumCalibrationPointsDialog::on_corrOrSet_toggled(bool checked)
{
    if (checked) {
        ui->corrOrSet->setText("Set");
    } else {
        ui->corrOrSet->setText("Cor");
    }

    m_setElseCorrectGlobal = checked;
}

void SpectrumCalibrationPointsDialog::on_globalRelativeCorrection_clicked()
{
    for (auto& calibrationPoint : m_calibrationPoints)
    {
        if (m_setElseCorrectGlobal) {
            calibrationPoint.m_powerRelativeReference = CalcDb::powerFromdB(m_globalCorrection);
        } else {
            calibrationPoint.m_powerRelativeReference *= CalcDb::powerFromdB(m_globalCorrection);
        }
    }

    displayCalibrationPoint();
    emit updateCalibrationPoints();
}

void SpectrumCalibrationPointsDialog::on_globalCalibratedCorrection_clicked()
{
    for (auto& calibrationPoint : m_calibrationPoints)
    {
        if (m_setElseCorrectGlobal) {
            calibrationPoint.m_powerCalibratedReference = CalcDb::powerFromdB(m_globalCorrection);
        } else {
            calibrationPoint.m_powerCalibratedReference *= CalcDb::powerFromdB(m_globalCorrection);
        }
    }

    displayCalibrationPoint();
    emit updateCalibrationPoints();
}

void SpectrumCalibrationPointsDialog::on_calibPointsExport_clicked()
{
    QFileDialog fileDialog(
        nullptr,
        "Select file to write calibration points to",
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
                stream << "Frequency,Relative,Calibrated\n";

                for (const auto &calibrationPint : m_calibrationPoints)
                {

                    stream << calibrationPint.m_frequency << ","
                        << CalcDb::dbPower(calibrationPint.m_powerRelativeReference, 1e-20) << ","
                        << CalcDb::dbPower(calibrationPint.m_powerCalibratedReference, 1e-20) << "\n";
                }

                stream.flush();
                file.close();
            }
        }
    }
}

void SpectrumCalibrationPointsDialog::on_calibPointsImport_clicked()
{
    QFileDialog fileDialog(
        nullptr,
        "Select .csv calibration points file to read",
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
                    {"Frequency", "Relative", "Calibrated"},
                    error
                );

                if (error.isEmpty())
                {
                    QStringList cols;
                    int frequencyCol = colIndexes.value("Frequency");
                    int referenceCol = colIndexes.value("Relative");
                    int absoluteCol = colIndexes.value("Calibrated");

                    m_calibrationPoints.clear();

                    while (CSV::readRow(in, &cols))
                    {
                        m_calibrationPoints.push_back(SpectrumCalibrationPoint());
                        m_calibrationPoints.back().m_frequency = cols[frequencyCol].toLongLong();
                        m_calibrationPoints.back().m_powerRelativeReference = CalcDb::powerFromdB(cols[referenceCol].toFloat());
                        m_calibrationPoints.back().m_powerCalibratedReference = CalcDb::powerFromdB(cols[absoluteCol].toFloat());
                    }

                    m_calibrationPointIndex = 0;
                    ui->calibPoint->setMaximum(m_calibrationPoints.size() - 1);
                    ui->calibPoint->setMinimum(0);
                    displayCalibrationPoint();
                    emit updateCalibrationPoints();
                }
            }
        }
    }
}
