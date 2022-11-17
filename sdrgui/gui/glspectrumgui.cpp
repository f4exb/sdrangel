///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2020 F4EXB                                                 //
// written by Edouard Griffiths                                                  //
//                                                                               //
// OpenGL interface modernization.                                               //
// See: http://doc.qt.io/qt-5/qopenglshaderprogram.html                          //
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

#include <QLineEdit>
#include <QToolTip>
#include <QFileDialog>
#include <QMessageBox>
#include <QScreen>

#include "gui/glspectrumgui.h"
#include "dsp/fftwindow.h"
#include "dsp/spectrumvis.h"
#include "gui/glspectrum.h"
#include "gui/glspectrumview.h"
#include "gui/crightclickenabler.h"
#include "gui/wsspectrumsettingsdialog.h"
#include "gui/spectrummarkersdialog.h"
#include "gui/spectrumcalibrationpointsdialog.h"
#include "gui/spectrummeasurementsdialog.h"
#include "gui/spectrummeasurements.h"
#include "gui/flowlayout.h"
#include "util/colormap.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "ui_glspectrumgui.h"

const int GLSpectrumGUI::m_fpsMs[] = {500, 200, 100, 50, 20, 10, 5};

GLSpectrumGUI::GLSpectrumGUI(QWidget* parent) :
    QWidget(parent),
    ui(new Ui::GLSpectrumGUI),
    m_spectrumVis(nullptr),
    m_glSpectrum(nullptr),
    m_doApplySettings(true),
    m_calibrationShiftdB(0.0),
    m_markersDialog(nullptr)
{
    ui->setupUi(this);

    // Use the custom flow layout for the 3 main horizontal layouts (lines)
    ui->verticalLayout->removeItem(ui->Line6Layout);
    ui->verticalLayout->removeItem(ui->Line5Layout);
    ui->verticalLayout->removeItem(ui->Line4Layout);
    ui->verticalLayout->removeItem(ui->Line3Layout);
    ui->verticalLayout->removeItem(ui->Line2Layout);
    ui->verticalLayout->removeItem(ui->Line1Layout);
    FlowLayout *flowLayout = new FlowLayout(nullptr, 1, 1, 1);
    flowLayout->addItem(ui->Line1Layout);
    flowLayout->addItem(ui->Line2Layout);
    flowLayout->addItem(ui->Line3Layout);
    flowLayout->addItem(ui->Line4Layout);
    flowLayout->addItem(ui->Line5Layout);
    flowLayout->addItem(ui->Line6Layout);
    ui->verticalLayout->addItem(flowLayout);

    on_linscale_toggled(false);
    //displayMeasurementGUI();

    QString levelStyle = QString(
        "QSpinBox {background-color: rgb(79, 79, 79);}"
        "QLineEdit {color: white; background-color: rgb(79, 79, 79); border: 1px solid gray; border-radius: 4px;}"
        "QTooltip {color: white; background-color: balck;}"
    );
    ui->refLevel->setStyleSheet(levelStyle);
    ui->levelRange->setStyleSheet(levelStyle);
    ui->fftOverlap->setStyleSheet(levelStyle);

    ui->colorMap->addItems(ColorMap::getColorMapNames());
    ui->colorMap->setCurrentText("Angel");

    connect(&m_messageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    CRightClickEnabler *wsSpectrumRightClickEnabler = new CRightClickEnabler(ui->wsSpectrum);
    connect(wsSpectrumRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(openWebsocketSpectrumSettingsDialog(const QPoint &)));

    CRightClickEnabler *calibrationPointsRightClickEnabler = new CRightClickEnabler(ui->calibration);
    connect(calibrationPointsRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(openCalibrationPointsDialog(const QPoint &)));

    displaySettings();
    setAveragingCombo();
    applySettings();
}

GLSpectrumGUI::~GLSpectrumGUI()
{
    if (m_markersDialog) {
        delete m_markersDialog;
    }

    delete ui;
}

void GLSpectrumGUI::setBuddies(SpectrumVis* spectrumVis, GLSpectrum* glSpectrum)
{
    m_spectrumVis = spectrumVis;
    m_glSpectrum = glSpectrum;
    m_glSpectrum->setSpectrumVis(spectrumVis);
    m_glSpectrum->setMessageQueueToGUI(&m_messageQueue);
    m_spectrumVis->setMessageQueueToGUI(&m_messageQueue);
}

void GLSpectrumGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings();
}

QByteArray GLSpectrumGUI::serialize() const
{
    const_cast<GLSpectrumGUI*>(this)->m_settings.getHistogramMarkers() = m_glSpectrum->getHistogramMarkers();
    const_cast<GLSpectrumGUI*>(this)->m_settings.getWaterfallMarkers() = m_glSpectrum->getWaterfallMarkers();
    return m_settings.serialize();
}

bool GLSpectrumGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        m_glSpectrum->setHistogramMarkers(m_settings.getHistogramMarkers());
        m_glSpectrum->setWaterfallMarkers(m_settings.getWaterfallMarkers());
        setAveragingCombo();
        displaySettings(); // ends with blockApplySettings(false)
        applySettings();
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void GLSpectrumGUI::formatTo(SWGSDRangel::SWGObject *swgObject) const
{
    m_settings.formatTo(swgObject);
}

void GLSpectrumGUI::updateFrom(const QStringList& keys, const SWGSDRangel::SWGObject *swgObject)
{
    m_settings.updateFrom(keys, swgObject);
}

void GLSpectrumGUI::updateSettings()
{
    displaySettings();
    applySettings();
}

void GLSpectrumGUI::displaySettings()
{
    blockApplySettings(true);
    ui->refLevel->setValue(m_settings.m_refLevel + m_calibrationShiftdB);
    ui->levelRange->setValue(m_settings.m_powerRange);
    ui->decay->setSliderPosition(m_settings.m_decay);
    ui->decayDivisor->setSliderPosition(m_settings.m_decayDivisor);
    ui->stroke->setSliderPosition(m_settings.m_histogramStroke);
    ui->waterfall->setChecked(m_settings.m_displayWaterfall);
    ui->spectrogram->setChecked(m_settings.m_display3DSpectrogram);
    ui->spectrogramStyle->setCurrentIndex((int) m_settings.m_3DSpectrogramStyle);
    ui->spectrogramStyle->setVisible(m_settings.m_display3DSpectrogram);
    ui->colorMap->setCurrentText(m_settings.m_colorMap);
    ui->currentLine->blockSignals(true);
    ui->currentFill->blockSignals(true);
    ui->currentGradient->blockSignals(true);
    ui->currentLine->setChecked(m_settings.m_displayCurrent && (m_settings.m_spectrumStyle == SpectrumSettings::SpectrumStyle::Line));
    ui->currentFill->setChecked(m_settings.m_displayCurrent && (m_settings.m_spectrumStyle == SpectrumSettings::SpectrumStyle::Fill));
    ui->currentGradient->setChecked(m_settings.m_displayCurrent && (m_settings.m_spectrumStyle == SpectrumSettings::SpectrumStyle::Gradient));
    ui->currentLine->blockSignals(false);
    ui->currentFill->blockSignals(false);
    ui->currentGradient->blockSignals(false);
    ui->maxHold->setChecked(m_settings.m_displayMaxHold);
    ui->histogram->setChecked(m_settings.m_displayHistogram);
    ui->invertWaterfall->setChecked(m_settings.m_invertedWaterfall);
    ui->grid->setChecked(m_settings.m_displayGrid);
    ui->gridIntensity->setSliderPosition(m_settings.m_displayGridIntensity);
    ui->truncateScale->setChecked(m_settings.m_truncateFreqScale);

    ui->decay->setToolTip(QString("Decay: %1").arg(m_settings.m_decay));
    ui->decayDivisor->setToolTip(QString("Decay divisor: %1").arg(m_settings.m_decayDivisor));
    ui->stroke->setToolTip(QString("Stroke: %1").arg(m_settings.m_histogramStroke));
    ui->gridIntensity->setToolTip(QString("Grid intensity: %1").arg(m_settings.m_displayGridIntensity));
    ui->traceIntensity->setToolTip(QString("Trace intensity: %1").arg(m_settings.m_displayTraceIntensity));

    ui->fftWindow->blockSignals(true);
    ui->averaging->blockSignals(true);
    ui->averagingMode->blockSignals(true);
    ui->linscale->blockSignals(true);

    ui->fftWindow->setCurrentIndex(m_settings.m_fftWindow);

    for (int i = SpectrumSettings::m_log2FFTSizeMin; i <= SpectrumSettings::m_log2FFTSizeMax; i++)
    {
        if (m_settings.m_fftSize == (1 << i))
        {
            ui->fftSize->setCurrentIndex(i - SpectrumSettings::m_log2FFTSizeMin);
            break;
        }
    }

    setFFTSizeToolitp();
    unsigned int i = 0;

    for (; i < sizeof(m_fpsMs)/sizeof(m_fpsMs[0]); i++)
    {
        if (m_settings.m_fpsPeriodMs >= m_fpsMs[i]) {
            break;
        }
    }

    ui->fps->setCurrentIndex(i);

    ui->fftOverlap->setValue(m_settings.m_fftOverlap);
    setMaximumOverlap();
    ui->averaging->setCurrentIndex(m_settings.m_averagingIndex);
    ui->averagingMode->setCurrentIndex((int) m_settings.m_averagingMode);
    ui->linscale->setChecked(m_settings.m_linear);
    setAveragingToolitp();
    ui->calibration->setChecked(m_settings.m_useCalibration);
    displayGotoMarkers();

    ui->fftWindow->blockSignals(false);
    ui->averaging->blockSignals(false);
    ui->averagingMode->blockSignals(false);
    ui->linscale->blockSignals(false);
    blockApplySettings(false);

    updateMeasurements();
}

void GLSpectrumGUI::displayGotoMarkers()
{
    ui->gotoMarker->clear();
    ui->gotoMarker->addItem("Go to...");

    for (auto marker : m_settings.m_annoationMarkers)
    {
        if (marker.m_show != SpectrumAnnotationMarker::Hidden)
        {
            qint64 freq = marker.m_startFrequency + marker.m_bandwidth/2;
            QString freqString = displayScaled(freq, 'f', 3, true);
            ui->gotoMarker->addItem(QString("%1 - %2").arg(marker.m_text).arg(freqString));
        }
    }

    ui->gotoMarker->setVisible(m_glSpectrum && m_glSpectrum->isDeviceSpectrum() && (ui->gotoMarker->count() > 1));
}

QString GLSpectrumGUI::displayScaled(int64_t value, char type, int precision, bool showMult)
{
    int64_t posValue = (value < 0) ? -value : value;

    if (posValue < 1000) {
        return tr("%1").arg(QString::number(value, type, precision));
    } else if (posValue < 1000000) {
        return tr("%1%2").arg(QString::number(value / 1000.0, type, precision)).arg(showMult ? "k" : "");
    } else if (posValue < 1000000000) {
        return tr("%1%2").arg(QString::number(value / 1000000.0, type, precision)).arg(showMult ? "M" : "");
    } else if (posValue < 1000000000000) {
        return tr("%1%2").arg(QString::number(value / 1000000000.0, type, precision)).arg(showMult ? "G" : "");
    } else {
        return tr("%1").arg(QString::number(value, 'e', precision));
    }
}

void GLSpectrumGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void GLSpectrumGUI::applySettings()
{
    if (!m_doApplySettings) {
        return;
    }

    if (m_glSpectrum) {
        applySpectrumSettings();
    }

    if (m_spectrumVis)
    {
        SpectrumVis::MsgConfigureSpectrumVis *msg = SpectrumVis::MsgConfigureSpectrumVis::create(m_settings, false);
        m_spectrumVis->getInputMessageQueue()->push(msg);
    }
}

void GLSpectrumGUI::applySpectrumSettings()
{
    m_glSpectrum->setDisplayWaterfall(m_settings.m_displayWaterfall);
    m_glSpectrum->setDisplay3DSpectrogram(m_settings.m_display3DSpectrogram);
    m_glSpectrum->set3DSpectrogramStyle(m_settings.m_3DSpectrogramStyle);
    m_glSpectrum->setColorMapName(m_settings.m_colorMap);
    m_glSpectrum->setSpectrumStyle(m_settings.m_spectrumStyle);
    m_glSpectrum->setInvertedWaterfall(m_settings.m_invertedWaterfall);
    m_glSpectrum->setDisplayMaxHold(m_settings.m_displayMaxHold);
    m_glSpectrum->setDisplayCurrent(m_settings.m_displayCurrent);
    m_glSpectrum->setDisplayHistogram(m_settings.m_displayHistogram);
    m_glSpectrum->setDecay(m_settings.m_decay);
    m_glSpectrum->setDecayDivisor(m_settings.m_decayDivisor);
    m_glSpectrum->setHistoStroke(m_settings.m_histogramStroke);
    m_glSpectrum->setDisplayGrid(m_settings.m_displayGrid);
    m_glSpectrum->setDisplayGridIntensity(m_settings.m_displayGridIntensity);
    m_glSpectrum->setDisplayTraceIntensity(m_settings.m_displayTraceIntensity);
    m_glSpectrum->setWaterfallShare(m_settings.m_waterfallShare);

    if ((m_settings.m_averagingMode == SpectrumSettings::AvgModeFixed) || (m_settings.m_averagingMode == SpectrumSettings::AvgModeMax)) {
        m_glSpectrum->setTimingRate(SpectrumSettings::getAveragingValue(m_settings.m_averagingIndex, m_settings.m_averagingMode) == 0 ?
            1 :
           SpectrumSettings::getAveragingValue(m_settings.m_averagingIndex, m_settings.m_averagingMode));
    } else {
        m_glSpectrum->setTimingRate(1);
    }

    Real refLevel = m_settings.m_linear ? pow(10.0, m_settings.m_refLevel/10.0) : m_settings.m_refLevel;
    Real powerRange = m_settings.m_linear ? pow(10.0, m_settings.m_refLevel/10.0) :  m_settings.m_powerRange;
    qDebug("GLSpectrumGUI::applySettings: refLevel: %e powerRange: %e", refLevel, powerRange);
    m_glSpectrum->setReferenceLevel(refLevel);
    m_glSpectrum->setPowerRange(powerRange);
    m_glSpectrum->setFPSPeriodMs(m_settings.m_fpsPeriodMs);
    m_glSpectrum->setFreqScaleTruncationMode(m_settings.m_truncateFreqScale);
    m_glSpectrum->setLinear(m_settings.m_linear);
    m_glSpectrum->setUseCalibration(m_settings.m_useCalibration);

    m_glSpectrum->setHistogramMarkers(m_settings.m_histogramMarkers);
    m_glSpectrum->setWaterfallMarkers(m_settings.m_waterfallMarkers);
    m_glSpectrum->setAnnotationMarkers(m_settings.m_annoationMarkers);
    m_glSpectrum->setMarkersDisplay(m_settings.m_markersDisplay);
    m_glSpectrum->setCalibrationPoints(m_settings.m_calibrationPoints);
    m_glSpectrum->setCalibrationInterpMode(m_settings.m_calibrationInterpMode);
}

void GLSpectrumGUI::on_fftWindow_currentIndexChanged(int index)
{
    qDebug("GLSpectrumGUI::on_fftWindow_currentIndexChanged: %d", index);
    m_settings.m_fftWindow = (FFTWindow::Function) index;
    applySettings();
}

void GLSpectrumGUI::on_fftSize_currentIndexChanged(int index)
{
    qDebug("GLSpectrumGUI::on_fftSize_currentIndexChanged: %d", index);
    m_settings.m_fftSize = 1 << (SpectrumSettings::m_log2FFTSizeMin + index);
    setAveragingCombo();
    setMaximumOverlap();
    applySettings();
    setAveragingToolitp();
    setFFTSizeToolitp();
}

void GLSpectrumGUI::on_fftOverlap_valueChanged(int value)
{
    qDebug("GLSpectrumGUI::on_fftOverlap_valueChanged: %d", value);
    m_settings.m_fftOverlap = value;
    setMaximumOverlap();
    applySettings();
    setAveragingToolitp();
}

void GLSpectrumGUI::on_autoscale_clicked(bool checked)
{
    (void) checked;

    if (!m_spectrumVis) {
        return;
    }

    std::vector<Real> psd;
    m_spectrumVis->getZoomedPSDCopy(psd);
    int avgRange = m_settings.m_fftSize / 32;

    if (psd.size() < (unsigned int) avgRange) {
        return;
    }

    std::sort(psd.begin(), psd.end());
    float max = psd[psd.size() - 1];
    float minSum = 0.0f;

    for (int i = 0; i < avgRange; i++) {
        minSum += psd[i];
    }

    float minAvg = minSum / avgRange;
    int minLvl = CalcDb::dbPower(minAvg*2);
    int maxLvl = CalcDb::dbPower(max*10);

    m_settings.m_refLevel = maxLvl;
    m_settings.m_powerRange = maxLvl - minLvl;
    ui->refLevel->setValue(m_settings.m_refLevel + m_calibrationShiftdB);
    ui->levelRange->setValue(m_settings.m_powerRange);
    // qDebug("GLSpectrumGUI::on_autoscale_clicked: max: %d min %d max: %e min: %e",
    //     maxLvl, minLvl, maxAvg, minAvg);

    applySettings();
}

void GLSpectrumGUI::on_averagingMode_currentIndexChanged(int index)
{
    qDebug("GLSpectrumGUI::on_averagingMode_currentIndexChanged: %d", index);
    m_settings.m_averagingMode = index < 0 ?
        SpectrumSettings::AvgModeNone :
        index > 3 ?
            SpectrumSettings::AvgModeMax :
            (SpectrumSettings::AveragingMode) index;

    setAveragingCombo();
    applySettings();
    setAveragingToolitp();
}

void GLSpectrumGUI::on_averaging_currentIndexChanged(int index)
{
    qDebug("GLSpectrumGUI::on_averaging_currentIndexChanged: %d", index);
    m_settings.m_averagingIndex = index;
    applySettings();
    setAveragingToolitp();
}

void GLSpectrumGUI::on_linscale_toggled(bool checked)
{
    qDebug("GLSpectrumGUI::on_averaging_currentIndexChanged: %s", checked ? "lin" : "log");
    m_settings.m_linear = checked;
    applySettings();
}

void GLSpectrumGUI::on_wsSpectrum_toggled(bool checked)
{
    if (m_spectrumVis)
    {
        SpectrumVis::MsgConfigureWSpectrumOpenClose *msg = SpectrumVis::MsgConfigureWSpectrumOpenClose::create(checked);
        m_spectrumVis->getInputMessageQueue()->push(msg);
    }
}

void GLSpectrumGUI::on_markers_clicked(bool checked)
{
    (void) checked;

    if (!m_glSpectrum || m_markersDialog) {
        return;
    }

    m_markersDialog = new SpectrumMarkersDialog(
        m_glSpectrum->getHistogramMarkers(),
        m_glSpectrum->getWaterfallMarkers(),
        m_glSpectrum->getAnnotationMarkers(),
        m_glSpectrum->getMarkersDisplay(),
        m_glSpectrum->getHistogramFindPeaks(),
        m_calibrationShiftdB,
        this
    );

    m_markersDialog->setCenterFrequency(m_glSpectrum->getCenterFrequency());
    m_markersDialog->setPower(m_glSpectrum->getPowerMax() / 2.0f);
    m_markersDialog->setTime(m_glSpectrum->getTimeMax() / 2.0f);

    connect(m_markersDialog, SIGNAL(updateHistogram()), this, SLOT(updateHistogramMarkers()));
    connect(m_markersDialog, SIGNAL(updateWaterfall()), this, SLOT(updateWaterfallMarkers()));
    connect(m_markersDialog, SIGNAL(updateAnnotations()), this, SLOT(updateAnnotationMarkers()));
    connect(m_markersDialog, SIGNAL(updateMarkersDisplay()), this, SLOT(updateMarkersDisplay()));
    connect(m_markersDialog, SIGNAL(finished(int)), this, SLOT(closeMarkersDialog()));

    QPoint globalCursorPos = QCursor::pos();
    QScreen *screen = QGuiApplication::screenAt(globalCursorPos);
    QRect mouseScreenGeometry = screen->geometry();
    QPoint localCursorPos = globalCursorPos - mouseScreenGeometry.topLeft();
    m_markersDialog->move(localCursorPos);

    m_markersDialog->show();
}

void GLSpectrumGUI::closeMarkersDialog()
{
    m_settings.m_histogramMarkers = m_glSpectrum->getHistogramMarkers();
    m_settings.m_waterfallMarkers = m_glSpectrum->getWaterfallMarkers();
    m_settings.m_annoationMarkers = m_glSpectrum->getAnnotationMarkers();
    m_settings.m_markersDisplay = m_glSpectrum->getMarkersDisplay();

    displayGotoMarkers();
    applySettings();

    delete m_markersDialog;
    m_markersDialog = nullptr;
}

// Save spectrum data to a CSV file
void GLSpectrumGUI::on_save_clicked(bool checked)
{
    (void) checked;

    // Get filename to write
    QFileDialog fileDialog(nullptr, "Select file to save data to", "", "*.csv");
    fileDialog.setAcceptMode(QFileDialog::AcceptSave);
    if (fileDialog.exec())
    {
        QStringList fileNames = fileDialog.selectedFiles();
        if (fileNames.size() > 0)
        {
            // Get spectrum data (This vector can be larger than fftSize)
            std::vector<Real> spectrum;
            m_spectrumVis->getPowerSpectrumCopy(spectrum);

            // Write to text file
            QFile file(fileNames[0]);
            if (file.open(QIODevice::WriteOnly))
            {
                QTextStream out(&file);
                float frequency = m_glSpectrum->getCenterFrequency() - (m_glSpectrum->getSampleRate() / 2.0f);
                float rbw = m_glSpectrum->getSampleRate() / (float)m_settings.m_fftSize;
                out << "\"Frequency\",\"Power\"\n";
                for (int i = 0; i < m_settings.m_fftSize; i++)
                {
                    out << frequency << "," << spectrum[i] << "\n";
                    frequency += rbw;
                }
                file.close();
            }
            else
            {
                QMessageBox::critical(this, "Spectrum", QString("Failed to open file %1").arg(fileNames[0]));
            }
        }
    }
}

void GLSpectrumGUI::on_refLevel_valueChanged(int value)
{
    m_settings.m_refLevel = value - m_calibrationShiftdB;
    applySettings();
}

void GLSpectrumGUI::on_levelRange_valueChanged(int value)
{
    m_settings.m_powerRange = value;
    applySettings();
}

void GLSpectrumGUI::on_fps_currentIndexChanged(int index)
{
    m_settings.m_fpsPeriodMs = m_fpsMs[index];
    qDebug("GLSpectrumGUI::on_fps_currentIndexChanged: %d ms", m_settings.m_fpsPeriodMs);
    applySettings();
}

void GLSpectrumGUI::on_decay_valueChanged(int index)
{
    m_settings.m_decay = index;
    ui->decay->setToolTip(QString("Decay: %1").arg(m_settings.m_decay));
    applySettings();
}

void GLSpectrumGUI::on_decayDivisor_valueChanged(int index)
{
    m_settings.m_decayDivisor = index;
    ui->decayDivisor->setToolTip(QString("Decay divisor: %1").arg(m_settings.m_decayDivisor));
    applySettings();
}

void GLSpectrumGUI::on_stroke_valueChanged(int index)
{
    m_settings.m_histogramStroke = index;
    ui->stroke->setToolTip(QString("Stroke: %1").arg(m_settings.m_histogramStroke));
    applySettings();
}

void GLSpectrumGUI::on_spectrogramStyle_currentIndexChanged(int index)
{
    m_settings.m_3DSpectrogramStyle = (SpectrumSettings::SpectrogramStyle)index;
    applySettings();
}

void GLSpectrumGUI::on_colorMap_currentIndexChanged(int index)
{
    (void) index;
    m_settings.m_colorMap = ui->colorMap->currentText();
    applySettings();
}

void GLSpectrumGUI::on_waterfall_toggled(bool checked)
{
    m_settings.m_displayWaterfall = checked;
    if (checked)
    {
        blockApplySettings(true);
        ui->spectrogram->setChecked(false);
        blockApplySettings(false);
    }
    applySettings();
}

void GLSpectrumGUI::on_spectrogram_toggled(bool checked)
{
    m_settings.m_display3DSpectrogram = checked;
    if (checked)
    {
        blockApplySettings(true);
        ui->waterfall->setChecked(false);
        blockApplySettings(false);
    }
    ui->spectrogramStyle->setVisible(m_settings.m_display3DSpectrogram);
    applySettings();
}

void GLSpectrumGUI::on_histogram_toggled(bool checked)
{
    m_settings.m_displayHistogram = checked;
    applySettings();
}

void GLSpectrumGUI::on_maxHold_toggled(bool checked)
{
    m_settings.m_displayMaxHold = checked;
    applySettings();
}

void GLSpectrumGUI::on_currentLine_toggled(bool checked)
{
    ui->currentFill->blockSignals(true);
    ui->currentGradient->blockSignals(true);
    ui->currentFill->setChecked(false);
    ui->currentGradient->setChecked(false);
    ui->currentFill->blockSignals(false);
    ui->currentGradient->blockSignals(false);
    m_settings.m_spectrumStyle = SpectrumSettings::SpectrumStyle::Line;
    m_settings.m_displayCurrent = checked;
    applySettings();
}

void GLSpectrumGUI::on_currentFill_toggled(bool checked)
{
    ui->currentLine->blockSignals(true);
    ui->currentGradient->blockSignals(true);
    ui->currentLine->setChecked(false);
    ui->currentGradient->setChecked(false);
    ui->currentLine->blockSignals(false);
    ui->currentGradient->blockSignals(false);
    m_settings.m_spectrumStyle = SpectrumSettings::SpectrumStyle::Fill;
    m_settings.m_displayCurrent = checked;
    applySettings();
}

void GLSpectrumGUI::on_currentGradient_toggled(bool checked)
{
    ui->currentLine->blockSignals(true);
    ui->currentFill->blockSignals(true);
    ui->currentLine->setChecked(false);
    ui->currentFill->setChecked(false);
    ui->currentLine->blockSignals(false);
    ui->currentFill->blockSignals(false);
    m_settings.m_spectrumStyle = SpectrumSettings::SpectrumStyle::Gradient;
    m_settings.m_displayCurrent = checked;
    applySettings();
}

void GLSpectrumGUI::on_invertWaterfall_toggled(bool checked)
{
    m_settings.m_invertedWaterfall = checked;
    applySettings();
}

void GLSpectrumGUI::on_grid_toggled(bool checked)
{
    m_settings.m_displayGrid = checked;
    applySettings();
}

void GLSpectrumGUI::on_gridIntensity_valueChanged(int index)
{
    m_settings.m_displayGridIntensity = index;
    ui->gridIntensity->setToolTip(QString("Grid intensity: %1").arg(m_settings.m_displayGridIntensity));
    applySettings();
}

void GLSpectrumGUI::on_truncateScale_toggled(bool checked)
{
    m_settings.m_truncateFreqScale = checked;
    qDebug("GLSpectrumGUI::on_truncateScale_toggled: m_truncateFreqScale: %s", (m_settings.m_truncateFreqScale ? "on" : "off"));
    applySettings();
}

void GLSpectrumGUI::on_traceIntensity_valueChanged(int index)
{
    m_settings.m_displayTraceIntensity = index;
    ui->traceIntensity->setToolTip(QString("Trace intensity: %1").arg(m_settings.m_displayTraceIntensity));
    applySettings();
}

void GLSpectrumGUI::on_clearSpectrum_clicked(bool checked)
{
    (void) checked;

    if (m_glSpectrum) {
        m_glSpectrum->clearSpectrumHistogram();
    }
}

void GLSpectrumGUI::on_freeze_toggled(bool checked)
{
    SpectrumVis::MsgStartStop *msg = SpectrumVis::MsgStartStop::create(!checked);
    m_spectrumVis->getInputMessageQueue()->push(msg);
}

void GLSpectrumGUI::on_calibration_toggled(bool checked)
{
    m_settings.m_useCalibration = checked;
    applySettings();
}

void GLSpectrumGUI::on_gotoMarker_currentIndexChanged(int index)
{
    if (index <= 0) {
        return;
    }
    int i = 1;
    for (auto marker : m_settings.m_annoationMarkers)
    {
        if (marker.m_show != SpectrumAnnotationMarker::Hidden)
        {
            if (i == index)
            {
                emit requestCenterFrequency(marker.m_startFrequency + marker.m_bandwidth/2);
                break;
            }
            i++;
        }
    }
    ui->gotoMarker->setCurrentIndex(0); // Redisplay "Goto..."
}

void GLSpectrumGUI::setAveragingCombo()
{
    int index = ui->averaging->currentIndex();
    ui->averaging->blockSignals(true);
    ui->averaging->clear();
    ui->averaging->addItem(QString("1"));
    uint64_t maxAveraging = SpectrumSettings::getMaxAveragingValue(m_settings.m_fftSize, m_settings.m_averagingMode);

    for (int i = 0; i <= SpectrumSettings::getAveragingMaxScale(m_settings.m_averagingMode); i++)
    {
        QString s;
        int m = pow(10.0, i);
        uint64_t x = 2*m;
        if (x > maxAveraging) {
            break;
        }
        setNumberStr(x, s);
        ui->averaging->addItem(s);
        x = 5*m;
        if (x > maxAveraging) {
            break;
        }
        setNumberStr(x, s);
        ui->averaging->addItem(s);
        x = 10*m;
        if (x > maxAveraging) {
            break;
        }
        setNumberStr(x, s);
        ui->averaging->addItem(s);
    }

    ui->averaging->setCurrentIndex(index >= ui->averaging->count() ? ui->averaging->count() - 1 : index);
    ui->averaging->blockSignals(false);
}

void GLSpectrumGUI::setNumberStr(int n, QString& s)
{
    if (n < 1000) {
        s = tr("%1").arg(n);
    } else if (n < 100000) {
        s = tr("%1k").arg(n/1000);
    } else if (n < 1000000) {
        s = tr("%1e5").arg(n/100000);
    } else if (n < 1000000000) {
        s = tr("%1M").arg(n/1000000);
    } else {
        s = tr("%1G").arg(n/1000000000);
    }
}

void GLSpectrumGUI::setNumberStr(float v, int decimalPlaces, QString& s)
{
    if (v < 1e-6) {
        s = tr("%1n").arg(v*1e9, 0, 'f', decimalPlaces);
    } else if (v < 1e-3) {
        s = tr("%1µ").arg(v*1e6, 0, 'f', decimalPlaces);
    } else if (v < 1.0) {
        s = tr("%1m").arg(v*1e3, 0, 'f', decimalPlaces);
    } else if (v < 1e3) {
        s = tr("%1").arg(v, 0, 'f', decimalPlaces);
    } else if (v < 1e6) {
        s = tr("%1k").arg(v*1e-3, 0, 'f', decimalPlaces);
    } else if (v < 1e9) {
        s = tr("%1M").arg(v*1e-6, 0, 'f', decimalPlaces);
    } else {
        s = tr("%1G").arg(v*1e-9, 0, 'f', decimalPlaces);
    }
}

void GLSpectrumGUI::setAveragingToolitp()
{
    if (m_glSpectrum)
    {
        QString s;
        int averagingIndex = m_settings.m_averagingMode == SpectrumSettings::AvgModeNone ? 0 : m_settings.m_averagingIndex;
        float halfSize = m_settings.m_fftSize / 2;
        float overlapFactor = (halfSize - m_settings.m_fftOverlap) / halfSize;
        float averagingTime = (m_settings.m_fftSize * (SpectrumSettings::getAveragingValue(averagingIndex, m_settings.m_averagingMode) == 0 ?
            1 :
            SpectrumSettings::getAveragingValue(averagingIndex, m_settings.m_averagingMode))) / (float) m_glSpectrum->getSampleRate();
        setNumberStr(averagingTime*overlapFactor, 2, s);
        ui->averaging->setToolTip(QString("Number of averaging samples (avg time: %1s)").arg(s));
    }
    else
    {
        ui->averaging->setToolTip(QString("Number of averaging samples"));
    }
}

void GLSpectrumGUI::setFFTSizeToolitp()
{
    if (m_glSpectrum)
    {
        QString s;
        setNumberStr((float) m_glSpectrum->getSampleRate() / m_settings.m_fftSize, 2, s);
        ui->fftSize->setToolTip(QString("FFT size (resolution: %1Hz)").arg(s));
    }
    else
    {
        ui->fftSize->setToolTip(QString("FFT size"));
    }
}

void GLSpectrumGUI::setFFTSize(int log2FFTSize)
{
    ui->fftSize->setCurrentIndex(
        log2FFTSize < SpectrumSettings::m_log2FFTSizeMin ?
            0
            : log2FFTSize > SpectrumSettings::m_log2FFTSizeMax ?
                SpectrumSettings::m_log2FFTSizeMax - SpectrumSettings::m_log2FFTSizeMin
                : log2FFTSize - SpectrumSettings::m_log2FFTSizeMin
    );
}

void GLSpectrumGUI::setMaximumOverlap()
{
    int halfSize = m_settings.m_fftSize/2;
    ui->fftOverlap->setMaximum((halfSize)-1);
    int value = ui->fftOverlap->value();
    ui->fftOverlap->setValue(value);
    ui->fftOverlap->setToolTip(tr("FFT overlap %1 %").arg((value/(float)halfSize)*100.0f));

    if (m_glSpectrum) {
        m_glSpectrum->setFFTOverlap(value);
    }
}

bool GLSpectrumGUI::handleMessage(const Message& message)
{
    if (GLSpectrumView::MsgReportSampleRate::match(message))
    {
        setAveragingToolitp();
        setFFTSizeToolitp();
        return true;
    }
    else if (SpectrumVis::MsgConfigureSpectrumVis::match(message))
    {
        SpectrumVis::MsgConfigureSpectrumVis& cfg = (SpectrumVis::MsgConfigureSpectrumVis&) message;
        m_settings = cfg.getSettings();
        displaySettings();

        if (m_glSpectrum) {
            applySpectrumSettings();
        }

        return true;
    }
    else if (SpectrumVis::MsgConfigureWSpectrumOpenClose::match(message))
    {
        SpectrumVis::MsgConfigureWSpectrumOpenClose& notif = (SpectrumVis::MsgConfigureWSpectrumOpenClose&) message;
        ui->wsSpectrum->blockSignals(true);
        ui->wsSpectrum->doToggle(notif.getOpenClose());
        ui->wsSpectrum->blockSignals(false);
        return true;
    }
    else if (GLSpectrumView::MsgReportWaterfallShare::match(message))
    {
        const GLSpectrumView::MsgReportWaterfallShare& report = (const GLSpectrumView::MsgReportWaterfallShare&) message;
        m_settings.m_waterfallShare = report.getWaterfallShare();
        return true;
    }
    else if (GLSpectrumView::MsgReportFFTOverlap::match(message))
    {
        const GLSpectrumView::MsgReportFFTOverlap& report = (const GLSpectrumView::MsgReportFFTOverlap&) message;
        m_settings.m_fftOverlap = report.getOverlap();
        ui->fftOverlap->blockSignals(true);
        ui->fftOverlap->setValue(m_settings.m_fftOverlap);
        ui->fftOverlap->blockSignals(false);
        return true;
    }
    else if (GLSpectrumView::MsgReportPowerScale::match(message))
    {
        const GLSpectrumView::MsgReportPowerScale& report = (const GLSpectrumView::MsgReportPowerScale&) message;
        m_settings.m_refLevel = report.getRefLevel();
        m_settings.m_powerRange = report.getRange();
        ui->refLevel->blockSignals(true);
        ui->levelRange->blockSignals(true);
        ui->refLevel->setValue(m_settings.m_refLevel + m_calibrationShiftdB);
        ui->levelRange->setValue(m_settings.m_powerRange);
        ui->levelRange->blockSignals(false);
        ui->refLevel->blockSignals(false);
        return true;
    }
    else if (GLSpectrumView::MsgReportCalibrationShift::match(message))
    {
        const GLSpectrumView::MsgReportCalibrationShift& report = (GLSpectrumView::MsgReportCalibrationShift&) message;
        m_calibrationShiftdB = report.getCalibrationShiftdB();
        ui->refLevel->blockSignals(true);
        ui->refLevel->setValue(m_settings.m_refLevel + m_calibrationShiftdB);
        ui->refLevel->blockSignals(false);
        return true;
    }
    else if (GLSpectrumView::MsgReportHistogramMarkersChange::match(message))
    {
        if (m_markersDialog) {
            m_markersDialog->updateHistogramMarkersDisplay();
        }
        return true;
    }
    else if (GLSpectrumView::MsgReportWaterfallMarkersChange::match(message))
    {
        if (m_markersDialog) {
            m_markersDialog->updateWaterfallMarkersDisplay();
        }
        return true;
    }
    else if (SpectrumVis::MsgStartStop::match(message))
    {
        const SpectrumVis::MsgStartStop& msg = (SpectrumVis::MsgStartStop&) message;
        ui->freeze->blockSignals(true);
        ui->freeze->doToggle(!msg.getStartStop()); // this is a freeze so stop is true
        ui->freeze->blockSignals(false);
        return true;
    }

    return false;
}

void GLSpectrumGUI::handleInputMessages()
{
    Message* message;

    while ((message = m_messageQueue.pop()) != 0)
    {
        qDebug("GLSpectrumGUI::handleInputMessages: message: %s", message->getIdentifier());

        if (handleMessage(*message))
        {
            delete message;
        }
    }
}

void GLSpectrumGUI::openWebsocketSpectrumSettingsDialog(const QPoint& p)
{
    WebsocketSpectrumSettingsDialog dialog(this);
    dialog.setAddress(m_settings.m_wsSpectrumAddress);
    dialog.setPort(m_settings.m_wsSpectrumPort);

    dialog.move(p);
    dialog.exec();

    if (dialog.hasChanged())
    {
        m_settings.m_wsSpectrumAddress = dialog.getAddress();
        m_settings.m_wsSpectrumPort = dialog.getPort();

        applySettings();
    }
}

void GLSpectrumGUI::openCalibrationPointsDialog(const QPoint& p)
{
    SpectrumCalibrationPointsDialog dialog(
        m_glSpectrum->getCalibrationPoints(),
        m_glSpectrum->getCalibrationInterpMode(),
        m_glSpectrum->getHistogramMarkers().size() > 0 ? &m_glSpectrum->getHistogramMarkers()[0] : nullptr,
        this
    );

    dialog.setCenterFrequency(m_glSpectrum->getCenterFrequency());
    connect(&dialog, SIGNAL(updateCalibrationPoints()), this, SLOT(updateCalibrationPoints()));
    dialog.move(p);
    dialog.exec();

    m_settings.m_histogramMarkers = m_glSpectrum->getHistogramMarkers();
    m_settings.m_waterfallMarkers = m_glSpectrum->getWaterfallMarkers();
    m_settings.m_annoationMarkers = m_glSpectrum->getAnnotationMarkers();
    m_settings.m_markersDisplay = m_glSpectrum->getMarkersDisplay();
    m_settings.m_calibrationPoints = m_glSpectrum->getCalibrationPoints();
    m_settings.m_calibrationInterpMode = m_glSpectrum->getCalibrationInterpMode();

    applySettings();
}

void GLSpectrumGUI::updateHistogramMarkers()
{
    if (m_glSpectrum) {
        m_glSpectrum->updateHistogramMarkers();
    }
}

void GLSpectrumGUI::updateWaterfallMarkers()
{
    if (m_glSpectrum) {
        m_glSpectrum->updateWaterfallMarkers();
    }
}

void GLSpectrumGUI::updateAnnotationMarkers()
{
    if (m_glSpectrum) {
        m_glSpectrum->updateAnnotationMarkers();
    }
}

void GLSpectrumGUI::updateMarkersDisplay()
{
    if (m_glSpectrum) {
        m_glSpectrum->updateMarkersDisplay();
    }
}

void GLSpectrumGUI::updateCalibrationPoints()
{
    if (m_glSpectrum) {
        m_glSpectrum->updateCalibrationPoints();
    }
}

void GLSpectrumGUI::on_measure_clicked(bool checked)
{
    (void) checked;

    SpectrumMeasurementsDialog measurementsDialog(
        m_glSpectrum,
        &m_settings,
        this
    );

    connect(&measurementsDialog, &SpectrumMeasurementsDialog::updateMeasurements, this, &GLSpectrumGUI::updateMeasurements);

    measurementsDialog.exec();
}

void GLSpectrumGUI::updateMeasurements()
{
    if (m_glSpectrum)
    {
        m_glSpectrum->setMeasurementsVisible(m_settings.m_measurement != SpectrumSettings::MeasurementNone);
        m_glSpectrum->setMeasurementsPosition(m_settings.m_measurementsPosition);
        m_glSpectrum->setMeasurementParams(
            m_settings.m_measurement,
            m_settings.m_measurementCenterFrequencyOffset,
            m_settings.m_measurementBandwidth,
            m_settings.m_measurementChSpacing,
            m_settings.m_measurementAdjChBandwidth,
            m_settings.m_measurementHarmonics,
            m_settings.m_measurementPeaks,
            m_settings.m_measurementHighlight,
            m_settings.m_measurementPrecision
            );
    }
}
