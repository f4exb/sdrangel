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

#ifndef SDRBASE_GUI_SPECTRUMCALIBRATIONPOINTSDIALOG_H_
#define SDRBASE_GUI_SPECTRUMCALIBRATIONPOINTSDIALOG_H_

#include <QDialog>
#include <QList>

#include "dsp/spectrumsettings.h"
#include "dsp/spectrumcalibrationpoint.h"
#include "export.h"

class SpectrumHistogramMarker;

namespace Ui {
    class SpectrumCalibrationPointsDialog;
}

class SDRGUI_API SpectrumCalibrationPointsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SpectrumCalibrationPointsDialog(
        QList<SpectrumCalibrationPoint>& calibrationPoints,
        const SpectrumHistogramMarker *markerZero,
        QWidget* parent = nullptr
    );
    ~SpectrumCalibrationPointsDialog();
    void setCenterFrequency(qint64 centerFrequency) { m_centerFrequency = centerFrequency; }

private:
    Ui::SpectrumCalibrationPointsDialog* ui;
    QList<SpectrumCalibrationPoint>& m_calibrationPoints;
    const SpectrumHistogramMarker *m_markerZero;
    int m_calibrationPointIndex;
    qint64 m_centerFrequency;

    void displayCalibrationPoint();

private slots:
    void on_calibPoint_valueChanged(int value);
    void on_calibPointAdd_clicked();
    void on_calibPointDel_clicked();
    void on_relativePower_valueChanged(int value);
    void on_absolutePower_valueChanged(int value);
    void on_calibPointFrequency_changed(qint64 value);
    void on_importMarkerZero_clicked();
    void on_centerFrequency_clicked();
    void on_calibPointsExport_clicked();
    void on_calibPointsImport_clicked();

signals:
    void updateCalibrationPoints();
};

#endif // SDRBASE_GUI_SPECTRUMCALIBRATIONPOINTSDIALOG_H_
