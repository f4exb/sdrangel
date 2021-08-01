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

#ifndef SDRBASE_GUI_SPECTRUMMARKERSDIALOG_H_
#define SDRBASE_GUI_SPECTRUMMARKERSDIALOG_H_

#include <QDialog>
#include <QList>

#include "gui/spectrummarkers.h"
#include "export.h"

namespace Ui {
    class SpectrumMarkersDialog;
}

class SDRGUI_API SpectrumMarkersDialog : public QDialog {
    Q_OBJECT

public:
    explicit SpectrumMarkersDialog(
        QList<SpectrumHistogramMarker>& histogramMarkers,
        QList<SpectrumWaterfallMarker>& waterfallMarkers,
        QWidget* parent = nullptr
    );
    ~SpectrumMarkersDialog();
    void setCenterFrequency(qint64 centerFrequency) { m_centerFrequency = centerFrequency; }
    void setPower(float power) { m_power = power; }

private:
    Ui::SpectrumMarkersDialog* ui;
    QList<SpectrumHistogramMarker>& m_histogramMarkers;
    QList<SpectrumWaterfallMarker>& m_waterfallMarkers;
    int m_histogramMarkerIndex;
    qint64 m_centerFrequency;
    float m_power;

    void displayHistogramMarker();

private slots:
    void on_markerFrequency_changed(qint64 value);
    void on_fixedPower_valueChanged(int value);
    void on_marker_valueChanged(int value);
    void on_setReference_clicked(bool checked);
    void on_markerAdd_clicked(bool checked);
    void on_markerDel_clicked(bool checked);
    void on_powerMode_currentIndexChanged(int index);

signals:
    void updateHistogram();
    void updateWaterfall();
};

#endif // SDRBASE_GUI_SPECTRUMMARKERSDIALOG_H_
