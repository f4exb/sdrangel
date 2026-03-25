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

#include <QColorDialog>

#include "spectrumdisplaysettingsdialog.h"
#include "glspectrum.h"

#include "ui_spectrumdisplaysettingsdialog.h"

static QString rgbToColor(quint32 rgb)
{
    QColor color = QColor::fromRgba(rgb);
    return QString("%1,%2,%3").arg(color.red()).arg(color.green()).arg(color.blue());
}

static QString backgroundCSS(quint32 rgb)
{
    // Must specify a border, otherwise we end up with a gradient instead of solid background
    return QString("QToolButton { background-color: rgb(%1); border: none; }").arg(rgbToColor(rgb));
}

SpectrumDisplaySettingsDialog::SpectrumDisplaySettingsDialog(GLSpectrum *glSpectrum, SpectrumSettings *settings, int sampleRate, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SpectrumDisplaySettingsDialog),
    m_glSpectrum(glSpectrum),
    m_settings(settings),
    m_sampleRate(sampleRate)
{
    ui->setupUi(this);

    m_spectrumColor = m_settings->m_spectrumColor;
    ui->spectrumColor->setStyleSheet(backgroundCSS(m_spectrumColor));

    for (int i = 0; i < settings->m_spectrumMemory.size(); i++)
    {
        MemorySettings memorySetting = {settings->m_spectrumMemory[i].m_color, settings->m_spectrumMemory[i].m_label};
        m_memorySettings.append(memorySetting);
    }

    ui->waterfallScrollBar->setChecked(m_settings->m_scrollBar);
    ui->scrollLength->setValue(m_settings->m_scrollLength);

    ui->waterfallVerticalAxisUnits->setCurrentIndex((int) m_settings->m_waterfallTimeUnits);
    ui->waterfallVerticalAxisFormat->setText(m_settings->m_waterfallTimeFormat);

    ui->displayRBW->setChecked(m_settings->m_displayRBW);
    ui->displayCursorStats->setChecked(m_settings->m_displayCursorStats);
    ui->displayPeakStats->setChecked(m_settings->m_displayPeakStats);

    displaySettings();
}

SpectrumDisplaySettingsDialog::~SpectrumDisplaySettingsDialog()
{}

void SpectrumDisplaySettingsDialog::accept()
{
    m_settings->m_waterfallTimeUnits = (SpectrumSettings::WaterfallTimeUnits) ui->waterfallVerticalAxisUnits->currentIndex();
    m_settings->m_waterfallTimeFormat = ui->waterfallVerticalAxisFormat->text();
    m_settings->m_scrollBar = ui->waterfallScrollBar->isChecked();
    m_settings->m_scrollLength = ui->scrollLength->value();
    m_settings->m_displayRBW = ui->displayRBW->isChecked();
    m_settings->m_displayCursorStats = ui->displayCursorStats->isChecked();
    m_settings->m_displayPeakStats = ui->displayPeakStats->isChecked();
    m_settings->m_spectrumColor = m_spectrumColor;

    for (int i = 0; i < m_settings->m_spectrumMemory.size(); i++)
    {
        m_settings->m_spectrumMemory[i].m_color = m_memorySettings[i].m_color;
        m_settings->m_spectrumMemory[i].m_label = m_memorySettings[i].m_label;
    }

    QDialog::accept();
}

void SpectrumDisplaySettingsDialog::displaySettings()
{
    bool enabled = ui->waterfallScrollBar->isChecked();
    ui->scrollLengthLabel->setEnabled(enabled);
    ui->scrollLength->setEnabled(enabled);
    ui->scrollRAMLabel->setEnabled(enabled);
    ui->scrollRAM->setEnabled(enabled);
    ui->scrollTimeLabel->setEnabled(enabled);
    ui->scrollTime->setEnabled(enabled);

    ui->waterfallVerticalAxisUnitsLabel->setEnabled(enabled);
    ui->waterfallVerticalAxisUnits->setEnabled(enabled);

    bool formatEnabled = ui->waterfallScrollBar->isChecked() && (ui->waterfallVerticalAxisUnits->currentIndex() != (int)SpectrumSettings::WaterfallTimeUnits::TimeOffset);
    if (!formatEnabled) {
        ui->waterfallVerticalAxisUnits->setCurrentIndex((int) SpectrumSettings::WaterfallTimeUnits::TimeOffset);
    }
    ui->waterfallVerticalAxisFormatLabel->setEnabled(formatEnabled);
    ui->waterfallVerticalAxisFormat->setEnabled(formatEnabled);

    // Calculate RAM usage

    std::size_t ram = ui->scrollLength->value() * (m_settings->m_fftSize * sizeof(float) + sizeof(Real *) + sizeof(int) + sizeof(qint64) + sizeof(QDateTime));

    ui->scrollRAM->setText(tr("%1 MB").arg(ram / 1024 / 1024));

    // Calculate time duration of complete scroll history

    float fftRate = m_sampleRate / (float) m_settings->m_fftSize * (100.0f - m_settings->m_fftOverlap) / 100.0f;
    if (m_settings->m_averagingMode == SpectrumSettings::AvgModeFixed) {
        fftRate /= m_settings->getAveragingValue(m_settings->m_averagingIndex, m_settings->m_averagingMode);
    }
    int secs = ui->scrollLength->value() / fftRate;
    int hours = secs / 3600;
    secs -= hours * 3600;
    int minutes = secs / 60;
    secs -= minutes * 60;

    ui->scrollTime->setText(QString("%1:%2:%3")
        .arg(hours, 2, 10, QChar('0'))
        .arg(minutes, 2, 10, QChar('0'))
        .arg(secs, 2, 10, QChar('0')));

    displayMemorySettings();
}

void SpectrumDisplaySettingsDialog::displayMemorySettings()
{
    int idx = ui->memIdx->currentIndex();
    ui->memColor->setStyleSheet(backgroundCSS(m_memorySettings[idx].m_color));
    ui->memLabel->setText(m_memorySettings[idx].m_label);
}

void SpectrumDisplaySettingsDialog::on_waterfallVerticalAxisUnits_currentIndexChanged(int index)
{
    (void) index;

    displaySettings();
}

void SpectrumDisplaySettingsDialog::on_waterfallScrollBar_clicked(bool checked)
{
    (void) checked;

    displaySettings();
}

void SpectrumDisplaySettingsDialog::on_scrollLength_valueChanged(int value)
{
    (void) value;

    displaySettings();
}

void SpectrumDisplaySettingsDialog::on_spectrumColor_clicked(bool checked)
{
    QColorDialog dialog(QColor::fromRgba(m_spectrumColor), ui->spectrumColor);
    if (dialog.exec() == QDialog::Accepted)
    {
        QRgb color = dialog.selectedColor().rgba();
        ui->spectrumColor->setStyleSheet(backgroundCSS(color));
        m_spectrumColor = color;
    }
}

void SpectrumDisplaySettingsDialog::on_memIdx_currentIndexChanged(int index)
{
    (void) index;

    displayMemorySettings();
}

void SpectrumDisplaySettingsDialog::on_memColor_clicked(bool checked)
{
    QColorDialog dialog(QColor::fromRgba(m_memorySettings[ui->memIdx->currentIndex()].m_color), ui->memColor);
    if (dialog.exec() == QDialog::Accepted)
    {
        QRgb color = dialog.selectedColor().rgba();
        ui->memColor->setStyleSheet(backgroundCSS(color));
        m_memorySettings[ui->memIdx->currentIndex()].m_color = color;
    }
}

void SpectrumDisplaySettingsDialog::on_memLabel_editingFinished()
{
    m_memorySettings[ui->memIdx->currentIndex()].m_label = ui->memLabel->text();
}
