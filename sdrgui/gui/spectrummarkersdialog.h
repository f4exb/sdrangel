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

#include "dsp/spectrumsettings.h"
#include "dsp/spectrummarkers.h"
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
        SpectrumSettings::MarkersDisplay& markersDisplay,
        QWidget* parent = nullptr
    );
    ~SpectrumMarkersDialog();
    void setCenterFrequency(qint64 centerFrequency) { m_centerFrequency = centerFrequency; }
    void setPower(float power) { m_power = power; }
    void setTime(float time) { m_time = time; }

private:
    Ui::SpectrumMarkersDialog* ui;
    QList<SpectrumHistogramMarker>& m_histogramMarkers;
    QList<SpectrumWaterfallMarker>& m_waterfallMarkers;
    SpectrumSettings::MarkersDisplay& m_markersDisplay;
    int m_histogramMarkerIndex;
    int m_waterfallMarkerIndex;
    qint64 m_centerFrequency;
    float m_power;
    float m_time;

    void displayHistogramMarker();
    void displayWaterfallMarker();
    void displayTime(float time);
    float getTime() const;

private slots:
    void on_markerFrequency_changed(qint64 value);
    void on_centerFrequency_clicked();
    void on_markerColor_clicked();
    void on_showMarker_clicked(bool clicked);
    void on_fixedPower_valueChanged(int value);
    void on_marker_valueChanged(int value);
    void on_setReference_clicked();
    void on_markerAdd_clicked();
    void on_markerDel_clicked();
    void on_powerMode_currentIndexChanged(int index);
    void on_powerHoldReset_clicked();
    void on_wMarkerFrequency_changed(qint64 value);
    void on_timeCoarse_valueChanged(int value);
    void on_timeFine_valueChanged(int value);
    void on_timeExp_valueChanged(int value);
    void on_wCenterFrequency_clicked();
    void on_wMarkerColor_clicked();
    void on_wShowMarker_clicked(bool clicked);
    void on_wMarker_valueChanged(int value);
    void on_wSetReference_clicked();
    void on_wMarkerAdd_clicked();
    void on_wMarkerDel_clicked();
    void on_showSelect_currentIndexChanged(int index);

signals:
    void updateHistogram();
    void updateWaterfall();
};

#endif // SDRBASE_GUI_SPECTRUMMARKERSDIALOG_H_
