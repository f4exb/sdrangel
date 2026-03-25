///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef SDRBASE_GUI_SPECTRUMDISPLAYSETTINGSDIALOG_H_
#define SDRBASE_GUI_SPECTRUMDISPLAYSETTINGSDIALOG_H_

#include <QDialog>

#include "dsp/spectrumsettings.h"
#include "export.h"

namespace Ui {
    class SpectrumDisplaySettingsDialog;
}

class GLSpectrum;

class SDRGUI_API SpectrumDisplaySettingsDialog : public QDialog {
    Q_OBJECT

    struct MemorySettings {
        QRgb m_color;
        QString m_label;
    };

public:
    explicit SpectrumDisplaySettingsDialog(GLSpectrum *glSpectrum, SpectrumSettings *settings, int sampleRate, QWidget *parent = nullptr);
    ~SpectrumDisplaySettingsDialog();

private:
    void displaySettings();
    void displayMemorySettings();

    Ui::SpectrumDisplaySettingsDialog *ui;
    GLSpectrum *m_glSpectrum;
    SpectrumSettings *m_settings;
    int m_sampleRate;
    QRgb m_spectrumColor;
    QList<MemorySettings> m_memorySettings;

private slots:
    void on_waterfallVerticalAxisUnits_currentIndexChanged(int index);
    void on_waterfallScrollBar_clicked(bool checked=false);
    void on_scrollLength_valueChanged(int value);
    void on_spectrumColor_clicked(bool checked=false);
    void on_memIdx_currentIndexChanged(int index);
    void on_memColor_clicked(bool checked=false);
    void on_memLabel_editingFinished();
    void accept() override;
};

#endif // SDRBASE_GUI_SPECTRUMDISPLAYSETTINGSDIALOG_H_
