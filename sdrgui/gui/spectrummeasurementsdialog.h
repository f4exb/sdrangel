///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2016, 2018-2019 Edouard Griffiths, F4EXB <f4exb06@gmail.com> //
// Copyright (C) 2020, 2022 Jon Beniston, M7RCE <jon@beniston.com>               //
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

#ifndef SDRBASE_GUI_SPECTRUMMEASUREMENTSDIALOG_H_
#define SDRBASE_GUI_SPECTRUMMEASUREMENTSDIALOG_H_

#include <QDialog>

#include "dsp/spectrumsettings.h"
#include "export.h"

namespace Ui {
    class SpectrumMeasurementsDialog;
}

class GLSpectrum;

class SDRGUI_API SpectrumMeasurementsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SpectrumMeasurementsDialog(GLSpectrum *glSpectrum, SpectrumSettings *settings, QWidget *parent = nullptr);
    ~SpectrumMeasurementsDialog();

private:
    void displaySettings();

    Ui::SpectrumMeasurementsDialog *ui;
    GLSpectrum *m_glSpectrum;
    SpectrumSettings *m_settings;

private slots:
    void on_measurement_currentIndexChanged(int index);
    void on_precision_valueChanged(int value);
    void on_position_currentIndexChanged(int index);
    void on_highlight_toggled(bool checked);
    void on_resetMeasurements_clicked(bool checked);
    void on_centerFrequencyOffset_changed(qint64 value);
    void on_bandwidth_changed(qint64 value);
    void on_chSpacing_changed(qint64 value);
    void on_adjChBandwidth_changed(qint64 value);
    void on_harmonics_valueChanged(int value);
    void on_peaks_valueChanged(int value);

signals:
    void updateMeasurements();
};

#endif // SDRBASE_GUI_SPECTRUMMEASUREMENTSDIALOG_H_
