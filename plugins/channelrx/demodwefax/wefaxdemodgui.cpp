///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#include "wefaxdemodgui.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

#include <QAbstractButton>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPixmap>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

#include "device/deviceuiset.h"
#include "dsp/basebandsamplesink.h"
#include "dsp/dspcommands.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/dialogpositioner.h"
#include "gui/rollupcontents.h"
#include "maincore.h"
#include "ui_wefaxdemodgui.h"
#include "util/db.h"
#include "wefaxdemod.h"
#include "wefaximageadjust.h"

WefaxDemodGUI *WefaxDemodGUI::create(
    PluginAPI *pluginAPI,
    DeviceUISet *deviceUISet,
    BasebandSampleSink *rxChannel)
{
    return new WefaxDemodGUI(pluginAPI, deviceUISet, rxChannel);
}

WefaxDemodGUI::WefaxDemodGUI(
    PluginAPI *pluginAPI,
    DeviceUISet *deviceUISet,
    BasebandSampleSink *rxChannel,
    QWidget *parent) :
    ChannelGUI(parent),
    ui(new Ui::WefaxDemodGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_wefaxDemod(reinterpret_cast<WefaxDemod *>(rxChannel)),
    m_channelMarker(this),
    m_doApplySettings(true),
    m_basebandSampleRate(0),
    m_deviceCenterFrequency(0),
    m_tickCount(0),
    m_decoderState(WefaxDecoder::State::Idle),
    m_panning(false),
    m_panStartHorizontal(0),
    m_panStartVertical(0)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/demodwefax/readme.md";
    RollupContents *rollupContents = getRollupContents();
    ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    ui->ioc->addItem("IOC 576", 576);
    ui->ioc->addItem("IOC 288", 288);
    for (int lpm : {60, 90, 100, 120, 180, 240}) {
        ui->lpm->addItem(QString("%1 LPM").arg(lpm), lpm);
    }
    for (int angle : {0, 90, 180, 270}) {
        ui->displayRotation->addItem(QString("%1°").arg(angle), angle);
    }
    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);
    ui->imageScrollAreaContents->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui->imageScrollArea->viewport()->installEventFilter(this);
    connect(ui->imageScrollArea->horizontalScrollBar(), &QScrollBar::rangeChanged,
        this, [this]() { updatePanCursor(); });
    connect(ui->imageScrollArea->verticalScrollBar(), &QScrollBar::rangeChanged,
        this, [this]() { updatePanCursor(); });
    makeUIConnections();

    m_wefaxDemod->setMessageQueueToGUI(getInputMessageQueue());
    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(QColor::fromRgb(m_settings.m_rgbColor));
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true);
    m_deviceUISet->addChannelMarker(&m_channelMarker);

    setTitle(m_settings.m_title);
    setTitleColor(QColor::fromRgb(m_settings.m_rgbColor));
    setDisplayedame("WEFAX");
    setStatusText("Idle");

    connect(&m_channelMarker, &ChannelMarker::changedByCursor,
        this, &WefaxDemodGUI::channelMarkerChangedByCursor);
    connect(&m_channelMarker, &ChannelMarker::highlightedByCursor,
        this, &WefaxDemodGUI::channelMarkerHighlightedByCursor);
    connect(&m_inputMessageQueue, &MessageQueue::messageEnqueued,
        this, &WefaxDemodGUI::handleInputMessages);
    connect(this, &QWidget::customContextMenuRequested,
        this, &WefaxDemodGUI::onMenuDialogCalled);
    connect(rollupContents, &RollupContents::widgetRolled,
        this, &WefaxDemodGUI::onWidgetRolled);
    connect(&MainCore::instance()->getMasterTimer(), &QTimer::timeout,
        this, &WefaxDemodGUI::tick);

    displaySettings();
    applySettings(QStringList(), true);
    getRollupContents()->arrangeRollups();
    m_resizer.enableChildMouseTracking();
}

WefaxDemodGUI::~WefaxDemodGUI()
{
    delete ui;
}

void WefaxDemodGUI::makeUIConnections()
{
    connect(ui->deltaFrequency, &ValueDialZ::changed, this, [this](qint64 value) {
        m_settings.m_inputFrequencyOffset = value;
        m_channelMarker.setCenterFrequency(value);
        updateAbsoluteCenterFrequency();
        applySettings(QStringList({"inputFrequencyOffset"}));
    });
    connect(ui->rfBW, &QSlider::valueChanged, this, [this](int value) {
        m_settings.m_rfBandwidth = static_cast<Real>(value * 100.0);
        ui->rfBWText->setText(QString("%1k").arg(value / 10.0, 0, 'f', 1));
        m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
        applySettings(QStringList({"rfBandwidth"}));
    });
    connect(ui->fmDeviation, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
        m_settings.m_fmDeviation = static_cast<Real>(value);
        applySettings(QStringList({"fmDeviation"}));
    });
    connect(ui->ioc, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        m_settings.m_ioc = ui->ioc->itemData(index).toInt();
        applySettings(QStringList({"ioc"}));
    });
    connect(ui->lpm, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        m_settings.m_linesPerMinute = ui->lpm->itemData(index).toInt();
        applySettings(QStringList({"linesPerMinute"}));
    });
    connect(ui->autoMode, &QAbstractButton::toggled, this, [this](bool checked) {
        m_settings.m_autoMode = checked;
        updateStartStopControl();
        applySettings(QStringList({"autoMode"}));
    });
    connect(ui->inverted, &QAbstractButton::toggled, this, [this](bool checked) {
        m_settings.m_inverted = checked;
        // Like RTTY's mark/space button, show the tone order: low then high.
        ui->inverted->setText(checked ? "W-B" : "B-W");
        applySettings(QStringList({"inverted"}));
    });
    connect(ui->minimumPhasingLines, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
        m_settings.m_minimumPhasingLines = value;
        applySettings(QStringList({"minimumPhasingLines"}));
    });
    connect(ui->clockCorrection, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        m_settings.m_manualClockCorrectionPpm = value;
        applySettings(QStringList({"manualClockCorrectionPpm"}));
    });
    connect(ui->startConfirm, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        m_settings.m_startConfirmSeconds = value;
        applySettings(QStringList({"startConfirmSeconds"}));
    });
    connect(ui->stopConfirm, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        m_settings.m_stopConfirmSeconds = value;
        applySettings(QStringList({"stopConfirmSeconds"}));
    });
    QMenu *presetMenu = new QMenu(ui->presetMenu);
    ui->presetMenu->setMenu(presetMenu);
    const auto addPreset = [this, presetMenu](
        const QString& name, int ioc, int lpm, float shift, float bandwidth)
    {
        QAction *preset = presetMenu->addAction(name);
        connect(preset, &QAction::triggered, this, [this, ioc, lpm, shift, bandwidth]() {
            m_settings.m_ioc = ioc;
            m_settings.m_linesPerMinute = lpm;
            m_settings.m_fmDeviation = shift;
            m_settings.m_rfBandwidth = bandwidth;
            m_settings.m_autoMode = true;
            displaySettings();
            applySettings(QStringList({
                "ioc", "linesPerMinute", "fmDeviation", "rfBandwidth", "autoMode"}));
        });
    };
    addPreset(tr("Standard WMO — 120/576, 800 Hz"), 576, 120, 800.0f, 2400.0f);
    addPreset(tr("DWD — 120/576, 850 Hz"), 576, 120, 850.0f, 2400.0f);
    addPreset(tr("Weak signal — 120/576, 1.6 kHz BW"), 576, 120, 800.0f, 1600.0f);
    presetMenu->addSeparator();
    addPreset(tr("IOC 288 — 120 LPM"), 288, 120, 800.0f, 1600.0f);
    addPreset(tr("High speed — 240/576"), 576, 240, 800.0f, 3600.0f);
    connect(ui->autoSave, &QAbstractButton::toggled, this, [this](bool checked) {
        m_settings.m_autoSave = checked;
        applySettings(QStringList({"autoSave"}));
    });
    connect(ui->autoSavePath, &QLineEdit::editingFinished, this, [this]() {
        m_settings.m_autoSavePath = ui->autoSavePath->text().trimmed();
        applySettings(QStringList({"autoSavePath"}));
    });
    connect(ui->autoSavePathBrowse, &QAbstractButton::clicked,
        this, &WefaxDemodGUI::chooseAutoSaveDirectory);
    connect(ui->maxRows, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
        m_settings.m_maxRows = value;
        applySettings(QStringList({"maxRows"}));
    });
    connect(ui->startStop, &QAbstractButton::clicked, this, [this](bool checked) {
        const auto action = checked
            ? (m_settings.m_autoMode
                ? WefaxDemod::MsgDecoderAction::StartPhasing
                : WefaxDemod::MsgDecoderAction::StartReceiving)
            : WefaxDemod::MsgDecoderAction::Stop;
        sendAction(static_cast<int>(action));
    });
    const auto action = [this](QAbstractButton *button, WefaxDemod::MsgDecoderAction::Action decoderAction) {
        connect(button, &QAbstractButton::clicked, this, [this, decoderAction]() {
            sendAction(static_cast<int>(decoderAction));
        });
    };
    action(ui->clearButton, WefaxDemod::MsgDecoderAction::Clear);
    connect(ui->saveButton, &QAbstractButton::clicked, this, &WefaxDemodGUI::chooseSaveFile);
    connect(ui->zoomIn, &QAbstractButton::clicked, this, [this]() {
        stepDisplayZoom(1);
    });
    connect(ui->zoomOut, &QAbstractButton::clicked, this, [this]() {
        stepDisplayZoom(-1);
    });
    connect(ui->zoomAll, &QAbstractButton::clicked, this, [this](bool checked) {
        if (checked) {
            setDisplayZoomPercent(0);
        } else {
            ui->zoomAll->setChecked(true);
        }
    });
    connect(ui->displayRotation, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        m_settings.m_displayRotation = ui->displayRotation->itemData(index).toInt();
        applySettings(QStringList({"displayRotation"}));
        updateImage();
    });
    connect(ui->displayInvert, &QAbstractButton::toggled, this, [this](bool checked) {
        m_settings.m_displayInverted = checked;
        applySettings(QStringList({"displayInverted"}));
        updateImage();
    });
    connect(ui->autoScroll, &QAbstractButton::toggled, this, [this](bool checked) {
        m_settings.m_autoScroll = checked;
        applySettings(QStringList({"autoScroll"}));
        if (checked) {
            ui->imageScrollArea->verticalScrollBar()->setValue(
                ui->imageScrollArea->verticalScrollBar()->maximum());
        }
    });
    connect(ui->displayContrast, &QSlider::valueChanged, this, [this](int value) {
        m_settings.m_displayContrast = value;
        ui->displayContrastText->setText(QString::number(value));
        applySettings(QStringList({"displayContrast"}));
        updateImage();
    });
    connect(ui->displayThreshold, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
        m_settings.m_displayThreshold = value;
        applySettings(QStringList({"displayThreshold"}));
        updateImage();
    });
    connect(ui->horizontalAlignment, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
        m_settings.m_horizontalAlignment = value;
        applySettings(QStringList({"horizontalAlignment"}));
        updateImage();
    });
    connect(ui->displaySlantCorrection, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        m_settings.m_displaySlantCorrectionPpm = value;
        applySettings(QStringList({"displaySlantCorrectionPpm"}));
        updateImage();
    });
    connect(ui->autoSlant, &QAbstractButton::toggled, this, [this](bool checked) {
        m_settings.m_autoSlant = checked;
        applySettings(QStringList({"autoSlant"}));
    });
}
void WefaxDemodGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);
    displaySettings();
    applySettings(QStringList(), true);
}

bool WefaxDemodGUI::deserialize(const QByteArray& data)
{
    if (!m_settings.deserialize(data)) {
        resetToDefaults();
        return false;
    }

    displaySettings();
    applySettings(QStringList(), true);
    return true;
}

void WefaxDemodGUI::displaySettings()
{
    m_doApplySettings = false;
    ui->deltaFrequency->setValue(m_settings.m_inputFrequencyOffset);
    ui->rfBW->setValue(qRound(m_settings.m_rfBandwidth / 100.0));
    ui->rfBWText->setText(QString("%1k").arg(m_settings.m_rfBandwidth / 1000.0, 0, 'f', 1));
    ui->fmDeviation->setValue(static_cast<int>(std::lround(m_settings.m_fmDeviation)));
    ui->ioc->setCurrentIndex(std::max(0, ui->ioc->findData(m_settings.m_ioc)));
    ui->lpm->setCurrentIndex(std::max(0, ui->lpm->findData(m_settings.m_linesPerMinute)));
    ui->autoMode->setChecked(m_settings.m_autoMode);
    ui->inverted->setChecked(m_settings.m_inverted);
    ui->inverted->setText(m_settings.m_inverted ? "W-B" : "B-W");
    ui->minimumPhasingLines->setValue(m_settings.m_minimumPhasingLines);
    ui->startConfirm->setValue(m_settings.m_startConfirmSeconds);
    ui->stopConfirm->setValue(m_settings.m_stopConfirmSeconds);
    ui->clockCorrection->setValue(m_settings.m_manualClockCorrectionPpm);
    ui->autoSave->setChecked(m_settings.m_autoSave);
    ui->autoSavePath->setText(m_settings.m_autoSavePath);
    ui->maxRows->setValue(m_settings.m_maxRows);
    ui->zoomAll->setChecked(m_settings.m_displayZoomPercent == 0);
    ui->displayRotation->setCurrentIndex(std::max(0, ui->displayRotation->findData(m_settings.m_displayRotation)));
    ui->displayInvert->setChecked(m_settings.m_displayInverted);
    ui->displayContrast->setValue(m_settings.m_displayContrast);
    ui->displayContrastText->setText(QString::number(m_settings.m_displayContrast));
    ui->displayThreshold->setValue(m_settings.m_displayThreshold);
    ui->horizontalAlignment->setValue(m_settings.m_horizontalAlignment);
    ui->displaySlantCorrection->setValue(m_settings.m_displaySlantCorrectionPpm);
    ui->autoScroll->setChecked(m_settings.m_autoScroll);
    ui->autoSlant->setChecked(m_settings.m_autoSlant);
    updateStartStopControl();
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setColor(QColor::fromRgb(m_settings.m_rgbColor));
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.clearStreamIndexes();
    m_channelMarker.addStreamIndex(m_settings.m_streamIndex);
    setTitle(m_settings.m_title);
    setTitleColor(QColor::fromRgb(m_settings.m_rgbColor));
    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    // Zoom has no widget of its own to trigger a redraw.
    updateImage();
    m_doApplySettings = true;
}

void WefaxDemodGUI::applySettings(const QStringList& settingsKeys, bool force)
{
    if (m_doApplySettings) {
        m_wefaxDemod->getInputMessageQueue()->push(
            WefaxDemod::MsgConfigureWefaxDemod::create(settingsKeys, m_settings, force));
    }
}

void WefaxDemodGUI::sendAction(int action)
{
    m_wefaxDemod->getInputMessageQueue()->push(
        WefaxDemod::MsgDecoderAction::create(
            static_cast<WefaxDemod::MsgDecoderAction::Action>(action)));
}

bool WefaxDemodGUI::handleMessage(const Message& message)
{
    if (WefaxDemod::MsgConfigureWefaxDemod::match(message))
    {
        const auto& configure = static_cast<const WefaxDemod::MsgConfigureWefaxDemod&>(message);
        m_settings = configure.getSettings();
        m_settings.setChannelMarker(&m_channelMarker);
        m_settings.setRollupState(&m_rollupState);
        displaySettings();
        return true;
    }

    if (WefaxDemod::MsgImage::match(message))
    {
        const auto& report = static_cast<const WefaxDemod::MsgImage&>(message);
        const bool imageChanged = m_image.cacheKey() != report.getImage().cacheKey();
        if (imageChanged) {
            m_image = report.getImage();
        }
        m_decoderState = report.getState();
        if (imageChanged) {
            updateImage(true);
        }

        QString state;
        switch (report.getState())
        {
        case WefaxDecoder::State::Idle: state = tr("Idle"); break;
        case WefaxDecoder::State::Phasing: state = tr("Phasing"); break;
        case WefaxDecoder::State::Receiving: state = tr("Receiving"); break;
        }

        updateStartStopControl();

        // A short status for the current stage, with every detail in the
        // tooltip. Power is already shown above.
        QStringList parts;
        parts.append(tr("%1, image %2").arg(state).arg(report.getImageId()));
        if (report.getState() == WefaxDecoder::State::Phasing)
        {
            parts.append(tr("%1 phasing lines").arg(report.getPhasingLineCount()));
        }
        else if (report.getState() == WefaxDecoder::State::Receiving)
        {
            parts.append(tr("IOC %1 at %2 LPM").arg(report.getIOC()).arg(report.getLinesPerMinute()));
            parts.append(tr("clock %1 ppm (%2)")
                .arg(report.getAppliedClockCorrectionPpm(), 0, 'f', 1)
                .arg(report.getTimingSource() == "phasing" ? tr("phasing") : tr("manual")));
        }
        else if (report.getCompletionReason() != "none")
        {
            parts.append(tr("ended by %1").arg(report.getCompletionReason()));
        }
        if (std::abs(report.getTuningErrorHz()) >= 0.5) {
            parts.append(tr("tune %1 Hz").arg(report.getTuningErrorHz(), 0, 'f', 1));
        }
        if (report.getSlantCorrectionPpm() != 0.0) {
            parts.append(tr("slant %1 ppm").arg(report.getSlantCorrectionPpm(), 0, 'f', 1));
        }
        if (report.getAlignmentPx() != 0) {
            parts.append(tr("aligned %1 px").arg(report.getAlignmentPx()));
        }
        if (!report.getBandwidthSufficient()) {
            parts.append(tr("bandwidth low: needs %1 kHz").arg(report.getRequiredBandwidthHz() / 1000.0, 0, 'f', 1));
        }
        if (!report.getLastSaveError().isEmpty()) {
            parts.append(tr("save error: %1").arg(report.getLastSaveError()));
        }
        const QString status = parts.join(" | ");

        ui->status->setToolTip(tr(
            "State: %1\nImage: %2\nIOC %3, %4 LPM (%5 effective)\nPhasing lines: %6\n"
            "Samples per line: %7\nClock: %8 ppm measured, %9 applied\nTiming: %10 (%11), confidence %12\n"
            "Tuning error: %13 Hz\nSlant correction: %14 ppm\nAlignment: %15 px\nCompletion: %16")
            .arg(state)
            .arg(report.getImageId())
            .arg(report.getIOC())
            .arg(report.getLinesPerMinute())
            .arg(report.getEffectiveLinesPerMinute(), 0, 'f', 3)
            .arg(report.getPhasingLineCount())
            .arg(report.getSamplesPerLine(), 0, 'f', 3)
            .arg(report.getClockCorrectionPpm(), 0, 'f', 1)
            .arg(report.getAppliedClockCorrectionPpm(), 0, 'f', 1)
            .arg(report.getTimingSource())
            .arg(report.getTimingStatus())
            .arg(report.getConfidence(), 0, 'f', 3)
            .arg(report.getTuningErrorHz(), 0, 'f', 1)
            .arg(report.getSlantCorrectionPpm(), 0, 'f', 1)
            .arg(report.getAlignmentPx())
            .arg(report.getCompletionReason()));
        ui->status->setText(status);
        setStatusText(state);
        return true;
    }

    if (DSPSignalNotification::match(message))
    {
        const auto& notification = static_cast<const DSPSignalNotification&>(message);
        m_deviceCenterFrequency = notification.getCenterFrequency();
        m_basebandSampleRate = notification.getSampleRate();
        ui->deltaFrequency->setValueRange(
            false, 7, -m_basebandSampleRate / 2, m_basebandSampleRate / 2);
        ui->deltaFrequencyLabel->setToolTip(
            tr("Range %1 %L2 Hz").arg(QChar(0xB1)).arg(m_basebandSampleRate / 2));
        updateAbsoluteCenterFrequency();
        return true;
    }

    return false;
}

void WefaxDemodGUI::updateStartStopControl()
{
    const bool running = m_decoderState != WefaxDecoder::State::Idle;
    ui->startStop->setChecked(running);
    ui->startStop->setAccessibleName(running ? tr("Stop reception") : tr("Start reception"));
    ui->startStop->setToolTip(running
        ? tr("Stop WEFAX reception")
        : (m_settings.m_autoMode
            ? tr("Start phasing; reception begins automatically when phasing ends")
            : tr("Start reception immediately using nominal timing and manual clock correction")));
}

void WefaxDemodGUI::handleInputMessages()
{
    Message *message;
    while ((message = m_inputMessageQueue.pop()))
    {
        handleMessage(*message);
        delete message;
    }
}

void WefaxDemodGUI::updateImage(bool newRows)
{
    if (m_image.isNull())
    {
        if (m_panning) {
            ui->imageScrollArea->viewport()->releaseMouse();
            m_panning = false;
        }
        ui->imageLabel->clear();
        ui->imageLabel->setFixedSize(1, 1);
        ui->imageScrollAreaContents->setFixedSize(1, 1);
        updatePanCursor();
        return;
    }

    // Alignment, slant, contrast, inversion, threshold and rotation are
    // shared with the saved image; zoom is display only.
    QImage displayed = WefaxImageAdjust::apply(m_image, m_settings);
    int zoom = m_settings.m_displayZoomPercent;
    if (zoom == 0)
    {
        const QSize available = ui->imageScrollArea->viewport()->size() - QSize(4, 4);
        zoom = std::max(1, static_cast<int>(100.0 * std::min(
            available.width() / static_cast<double>(displayed.width()),
            available.height() / static_cast<double>(displayed.height()))));
    }
    const QSize scaledSize = displayed.size() * (zoom / 100.0);
    QPixmap pixmap = QPixmap::fromImage(displayed);
    if (scaledSize != displayed.size()) {
        pixmap = pixmap.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    ui->imageLabel->setPixmap(pixmap);
    ui->imageLabel->setFixedSize(scaledSize);
    ui->imageScrollAreaContents->setFixedSize(scaledSize);
    // Only follow new rows: an adjustment redraws the same image at the same
    // size, and keeping the scroll position lets its effect be compared on
    // the area being viewed.
    if (newRows && m_settings.m_autoScroll) {
        ui->imageScrollArea->verticalScrollBar()->setValue(
            ui->imageScrollArea->verticalScrollBar()->maximum());
    }
    updatePanCursor();
}

void WefaxDemodGUI::updatePanCursor()
{
    QWidget *viewport = ui->imageScrollArea->viewport();
    const bool canPan = (ui->imageScrollArea->horizontalScrollBar()->maximum() > 0)
        || (ui->imageScrollArea->verticalScrollBar()->maximum() > 0);
    viewport->setCursor(m_panning
        ? Qt::ClosedHandCursor
        : (canPan ? Qt::OpenHandCursor : Qt::ArrowCursor));
}

int WefaxDemodGUI::effectiveDisplayZoomPercent() const
{
    if (m_settings.m_displayZoomPercent != 0) {
        return m_settings.m_displayZoomPercent;
    }
    if (m_image.isNull()) {
        return 100;
    }

    QSize imageSize = m_image.size();
    if ((m_settings.m_displayRotation == 90) || (m_settings.m_displayRotation == 270)) {
        imageSize.transpose();
    }
    const QSize available = ui->imageScrollArea->viewport()->size() - QSize(4, 4);
    return std::max(1, static_cast<int>(100.0 * std::min(
        available.width() / static_cast<double>(imageSize.width()),
        available.height() / static_cast<double>(imageSize.height()))));
}

void WefaxDemodGUI::setDisplayZoomPercent(int percent)
{
    const bool changed = m_settings.m_displayZoomPercent != percent;
    m_settings.m_displayZoomPercent = percent;
    ui->zoomAll->setChecked(percent == 0);

    if (changed) {
        applySettings(QStringList({"displayZoomPercent"}));
        updateImage();
    }
}

void WefaxDemodGUI::stepDisplayZoom(int direction)
{
    static constexpr std::array<int, 7> zoomLevels {{5, 10, 25, 50, 100, 200, 400}};
    const int current = effectiveDisplayZoomPercent();
    int target = current;

    if (direction > 0)
    {
        target = zoomLevels.back();
        for (int level : zoomLevels) {
            if (level > current) {
                target = level;
                break;
            }
        }
    }
    else if (direction < 0)
    {
        target = zoomLevels.front();
        for (auto it = zoomLevels.rbegin(); it != zoomLevels.rend(); ++it) {
            if (*it < current) {
                target = *it;
                break;
            }
        }
    }

    setDisplayZoomPercent(target);
}

bool WefaxDemodGUI::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->imageScrollArea->viewport())
    {
        if ((event->type() == QEvent::Resize)
            && (m_settings.m_displayZoomPercent == 0)) {
            updateImage();
        } else if (event->type() == QEvent::MouseButtonPress) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            QScrollBar *horizontal = ui->imageScrollArea->horizontalScrollBar();
            QScrollBar *vertical = ui->imageScrollArea->verticalScrollBar();
            const bool canPan = (horizontal->maximum() > 0) || (vertical->maximum() > 0);
            if ((mouseEvent->button() == Qt::LeftButton) && canPan)
            {
                m_panning = true;
                m_panStartPosition = mouseEvent->pos();
                m_panStartHorizontal = horizontal->value();
                m_panStartVertical = vertical->value();
                ui->imageScrollArea->viewport()->grabMouse();
                updatePanCursor();
                mouseEvent->accept();
                return true;
            }
        } else if ((event->type() == QEvent::MouseMove) && m_panning) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            const QPoint delta = mouseEvent->pos() - m_panStartPosition;
            ui->imageScrollArea->horizontalScrollBar()->setValue(
                m_panStartHorizontal - delta.x());
            ui->imageScrollArea->verticalScrollBar()->setValue(
                m_panStartVertical - delta.y());
            mouseEvent->accept();
            return true;
        } else if ((event->type() == QEvent::MouseButtonRelease) && m_panning) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton)
            {
                m_panning = false;
                ui->imageScrollArea->viewport()->releaseMouse();
                updatePanCursor();
                mouseEvent->accept();
                return true;
            }
        } else if ((event->type() == QEvent::Wheel) && !m_image.isNull()) {
            auto *wheelEvent = static_cast<QWheelEvent *>(event);
            int delta = wheelEvent->angleDelta().y();
            if (delta == 0) {
                delta = wheelEvent->pixelDelta().y();
            }
            if (delta != 0)
            {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                const QPoint anchor = wheelEvent->position().toPoint();
#else
                const QPoint anchor = wheelEvent->pos();
#endif
                QScrollBar *horizontal = ui->imageScrollArea->horizontalScrollBar();
                QScrollBar *vertical = ui->imageScrollArea->verticalScrollBar();
                const int oldHorizontal = horizontal->value();
                const int oldVertical = vertical->value();
                const int oldZoom = effectiveDisplayZoomPercent();

                stepDisplayZoom(delta > 0 ? 1 : -1);

                const int newZoom = effectiveDisplayZoomPercent();
                const double scale = newZoom / static_cast<double>(oldZoom);
                horizontal->setValue(static_cast<int>(std::lround(
                    (oldHorizontal + anchor.x()) * scale - anchor.x())));
                vertical->setValue(static_cast<int>(std::lround(
                    (oldVertical + anchor.y()) * scale - anchor.y())));
                wheelEvent->accept();
                return true;
            }
        }
    }
    return ChannelGUI::eventFilter(watched, event);
}

void WefaxDemodGUI::channelMarkerChangedByCursor()
{
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    const QSignalBlocker blocker(ui->deltaFrequency);
    ui->deltaFrequency->setValue(m_settings.m_inputFrequencyOffset);
    updateAbsoluteCenterFrequency();
    applySettings(QStringList({"inputFrequencyOffset"}));
}

void WefaxDemodGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}

void WefaxDemodGUI::tick()
{
    double magSqAverage;
    double magSqPeak;
    int sampleCount;
    m_wefaxDemod->getMagSqLevels(magSqAverage, magSqPeak, sampleCount);
    const double powerDbAverage = CalcDb::dbPower(magSqAverage);
    const double powerDbPeak = CalcDb::dbPower(magSqPeak);

    ui->channelPowerMeter->levelChanged(
        (100.0 + powerDbAverage) / 100.0,
        (100.0 + powerDbPeak) / 100.0,
        sampleCount);

    if ((m_tickCount % 4) == 0) {
        ui->channelPower->setText(QString::number(powerDbAverage, 'f', 1));
    }

    ++m_tickCount;
}

void WefaxDemodGUI::onWidgetRolled(QWidget *widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;
    getRollupContents()->saveState(m_rollupState);
    applySettings(QStringList(), false);
}

void WefaxDemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void WefaxDemodGUI::chooseSaveFile()
{
    const QString fileName = QFileDialog::getSaveFileName(
        this,
        tr("Save WEFAX image"),
        QString(),
        tr("PNG images (*.png)"));
    if (!fileName.isEmpty()) {
        m_wefaxDemod->saveImage(fileName);
    }
}

void WefaxDemodGUI::chooseAutoSaveDirectory()
{
    const QString directory = QFileDialog::getExistingDirectory(
        this,
        tr("Select automatic WEFAX image folder"),
        m_settings.m_autoSavePath,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!directory.isEmpty()) {
        m_settings.m_autoSavePath = directory;
        ui->autoSavePath->setText(directory);
        applySettings(QStringList({"autoSavePath"}));
    }
}

void WefaxDemodGUI::onMenuDialogCalled(const QPoint& position)
{
    if (m_contextMenuType != ContextMenuType::ContextMenuChannelSettings)
    {
        resetContextMenuType();
        return;
    }

    BasicChannelSettingsDialog dialog(&m_channelMarker, this);
    dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
    dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
    dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
    dialog.setReverseAPIDeviceIndex(m_settings.m_reverseAPIDeviceIndex);
    dialog.setReverseAPIChannelIndex(m_settings.m_reverseAPIChannelIndex);
    dialog.setDefaultTitle("WEFAX");

    if (m_deviceUISet->m_deviceMIMOEngine)
    {
        dialog.setNumberOfStreams(m_wefaxDemod->getNumberOfDeviceStreams());
        dialog.setStreamIndex(m_settings.m_streamIndex);
    }

    dialog.move(position);
    new DialogPositioner(&dialog, false);
    dialog.exec();

    m_settings.m_rgbColor = m_channelMarker.getColor().rgb();
    m_settings.m_title = m_channelMarker.getTitle();
    m_settings.m_useReverseAPI = dialog.useReverseAPI();
    m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
    m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
    m_settings.m_reverseAPIDeviceIndex = dialog.getReverseAPIDeviceIndex();
    m_settings.m_reverseAPIChannelIndex = dialog.getReverseAPIChannelIndex();

    if (m_deviceUISet->m_deviceMIMOEngine)
    {
        m_settings.m_streamIndex = dialog.getSelectedStreamIndex();
        m_channelMarker.clearStreamIndexes();
        m_channelMarker.addStreamIndex(m_settings.m_streamIndex);
        updateIndexLabel();
    }

    setWindowTitle(m_settings.m_title);
    setTitle(m_settings.m_title);
    setTitleColor(QColor::fromRgb(m_settings.m_rgbColor));
    applySettings({
        "title",
        "rgbColor",
        "useReverseAPI",
        "reverseAPIAddress",
        "reverseAPIPort",
        "reverseAPIDeviceIndex",
        "reverseAPIChannelIndex",
        "streamIndex"
    });
    resetContextMenuType();
}
