///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#include <QLocale>
#include <QFileDialog>
#include <QTime>

#include "device/deviceuiset.h"
#include "device/deviceapi.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "dsp/hbfilterchainconverter.h"
#include "dsp/dspcommands.h"
#include "mainwindow.h"

#include "sigmffilesinkmessages.h"
#include "sigmffilesink.h"
#include "sigmffilesinkgui.h"
#include "ui_sigmffilesinkgui.h"

SigMFFileSinkGUI* SigMFFileSinkGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *channelRx)
{
    SigMFFileSinkGUI* gui = new SigMFFileSinkGUI(pluginAPI, deviceUISet, channelRx);
    return gui;
}

void SigMFFileSinkGUI::destroy()
{
    delete this;
}

void SigMFFileSinkGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray SigMFFileSinkGUI::serialize() const
{
    return m_settings.serialize();
}

bool SigMFFileSinkGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        displaySettings();
        applySettings(true);
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

bool SigMFFileSinkGUI::handleMessage(const Message& message)
{
    if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification notif = (const DSPSignalNotification&) message;
        m_deviceCenterFrequency = notif.getCenterFrequency();
        m_basebandSampleRate = notif.getSampleRate();
        ui->deltaFrequency->setValueRange(false, 8, -m_basebandSampleRate/2, m_basebandSampleRate/2);
        ui->deltaFrequencyLabel->setToolTip(tr("Range %1 %L2 Hz").arg(QChar(0xB1)).arg(m_basebandSampleRate/2));
        displayRate();

        if (m_fixedPosition)
        {
            setFrequencyFromPos();
            applySettings();
        }
        else
        {
            setPosFromFrequency();
        }

        return true;
    }
    else if (SigMFFileSink::MsgConfigureSigMFFileSink::match(message))
    {
        const SigMFFileSink::MsgConfigureSigMFFileSink& cfg = (SigMFFileSink::MsgConfigureSigMFFileSink&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        ui->glSpectrumGUI->updateSettings();
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (SigMFFileSinkMessages::MsgConfigureSpectrum::match(message))
    {
        const SigMFFileSinkMessages::MsgConfigureSpectrum& cfg = (SigMFFileSinkMessages::MsgConfigureSpectrum&) message;
        ui->glSpectrum->setSampleRate(cfg.getSampleRate());
        ui->glSpectrum->setCenterFrequency(cfg.getCenterFrequency());
        return true;
    }
    else if (SigMFFileSinkMessages::MsgReportSquelch::match(message))
    {
        const SigMFFileSinkMessages::MsgReportSquelch& report = (SigMFFileSinkMessages::MsgReportSquelch&) message;

        if (report.getOpen()) {
            ui->squelchLevel->setStyleSheet("QDial { background-color : green; }");
        } else {
            ui->squelchLevel->setStyleSheet("QDial { background:rgb(79,79,79); }");
        }

        return true;
    }
    else if (SigMFFileSinkMessages::MsgReportRecording::match(message))
    {
        const SigMFFileSinkMessages::MsgReportSquelch& report = (SigMFFileSinkMessages::MsgReportSquelch&) message;
        qDebug("SigMFFileSinkGUI::handleMessage: FileSinkMessages::MsgReportRecording: %s", report.getOpen() ? "on" : "off");

        blockSignals(true);

        if (report.getOpen())
        {
            ui->record->setStyleSheet("QToolButton { background-color : red; }");
            ui->record->setChecked(true);
        }
        else
        {
            ui->record->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
            ui->record->setChecked(false);
        }

        blockSignals(false);
        return true;
    }
    else if (SigMFFileSink::MsgReportStartStop::match(message))
    {
        const SigMFFileSink::MsgReportStartStop& cfg = (SigMFFileSink::MsgReportStartStop&) message;
        m_running = cfg.getStartStop();
        blockSignals(true);
        ui->record->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
        ui->record->setChecked(false);
        ui->record->setEnabled(m_running && !m_settings.m_squelchRecordingEnable);
        blockSignals(false);

        return true;
    }
    else
    {
        return false;
    }
}

SigMFFileSinkGUI::SigMFFileSinkGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *channelrx, QWidget* parent) :
        ChannelGUI(parent),
        ui(new Ui::SigMFFileSinkGUI),
        m_pluginAPI(pluginAPI),
        m_deviceUISet(deviceUISet),
        m_channelMarker(this),
        m_deviceCenterFrequency(0),
        m_running(false),
        m_fixedShiftIndex(0),
        m_basebandSampleRate(0),
        m_fixedPosition(false),
        m_tickCount(0)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/sigmffilesink/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_sigMFFileSink = (SigMFFileSink*) channelrx;
    m_spectrumVis = m_sigMFFileSink->getSpectrumVis();
	m_spectrumVis->setGLSpectrum(ui->glSpectrum);
    m_sigMFFileSink->setMessageQueueToGUI(getInputMessageQueue());

	ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 8, -99999999, 99999999);
    ui->position->setEnabled(m_fixedPosition);
    ui->glSpectrumGUI->setBuddies(m_spectrumVis, ui->glSpectrum);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    m_channelMarker.setCenterFrequency(0);
	m_channelMarker.setBandwidth(m_basebandSampleRate);
    m_channelMarker.setTitle("SigMF File Sink");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    m_settings.setSpectrumGUI(ui->glSpectrumGUI);
    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);

    m_deviceUISet->addChannelMarker(&m_channelMarker);

	connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));
    connect(&(m_deviceUISet->m_deviceAPI->getMasterTimer()), SIGNAL(timeout()), this, SLOT(tick()));

    displaySettings();
    makeUIConnections();
    applySettings(true);
    DialPopup::addPopupsToChildDials(this);
}

SigMFFileSinkGUI::~SigMFFileSinkGUI()
{
    m_sigMFFileSink->setMessageQueueToGUI(nullptr);
    delete ui;
}

void SigMFFileSinkGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void SigMFFileSinkGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        setTitleColor(m_channelMarker.getColor());

        SigMFFileSink::MsgConfigureSigMFFileSink* message = SigMFFileSink::MsgConfigureSigMFFileSink::create(m_settings, force);
        m_sigMFFileSink->getInputMessageQueue()->push(message);
    }
}

void SigMFFileSinkGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setBandwidth(m_basebandSampleRate / (1<<m_settings.m_log2Decim));
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());

    blockApplySettings(true);

    ui->record->setEnabled(!m_settings.m_squelchRecordingEnable);
    ui->squelchedRecording->setChecked(m_settings.m_squelchRecordingEnable);
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    ui->fileNameText->setText(m_settings.m_fileRecordName);
    ui->decimationFactor->setCurrentIndex(m_settings.m_log2Decim);
    ui->spectrumSquelch->setChecked(m_settings.m_spectrumSquelchMode);
    ui->squelchLevel->setValue(m_settings.m_spectrumSquelch);
    ui->squelchLevelText->setText(tr("%1").arg(m_settings.m_spectrumSquelch));
    ui->preRecordTime->setValue(m_settings.m_preRecordTime);
    ui->preRecordTimeText->setText(tr("%1").arg(m_settings.m_preRecordTime));
    ui->postSquelchTime->setValue(m_settings.m_squelchPostRecordTime);
    ui->postSquelchTimeText->setText(tr("%1").arg(m_settings.m_squelchPostRecordTime));
    ui->squelchedRecording->setChecked(m_settings.m_squelchRecordingEnable);

    if (!m_settings.m_spectrumSquelchMode)
    {
        ui->squelchLevel->setStyleSheet("QDial { background:rgb(79,79,79); }");
        ui->record->setEnabled(true);
        ui->squelchedRecording->blockSignals(true);
        ui->squelchedRecording->setChecked(false);
        ui->squelchedRecording->blockSignals(false);
    }

    updateIndexLabel();
    setPosFromFrequency();

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void SigMFFileSinkGUI::displayRate()
{
    double channelSampleRate = ((double) m_basebandSampleRate) / (1<<m_settings.m_log2Decim);
    ui->channelRateText->setText(tr("%1k").arg(QString::number(channelSampleRate / 1000.0, 'g', 5)));
    m_channelMarker.setBandwidth(channelSampleRate);
}

void SigMFFileSinkGUI::displayPos()
{
    ui->position->setValue(m_fixedShiftIndex);
    ui->filterChainIndex->setText(tr("%1").arg(m_fixedShiftIndex));
}

void SigMFFileSinkGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void SigMFFileSinkGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void SigMFFileSinkGUI::channelMarkerChangedByCursor()
{
    if (m_fixedPosition) {
        return;
    }

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    setPosFromFrequency();
	applySettings();
}

void SigMFFileSinkGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void SigMFFileSinkGUI::handleSourceMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()) != 0)
    {
        if (handleMessage(*message))
        {
            delete message;
        }
    }
}

void SigMFFileSinkGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void SigMFFileSinkGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuChannelSettings)
    {
        BasicChannelSettingsDialog dialog(&m_channelMarker, this);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIDeviceIndex(m_settings.m_reverseAPIDeviceIndex);
        dialog.setReverseAPIChannelIndex(m_settings.m_reverseAPIChannelIndex);
        dialog.setDefaultTitle(m_displayedName);

        if (m_deviceUISet->m_deviceMIMOEngine)
        {
            dialog.setNumberOfStreams(m_sigMFFileSink->getNumberOfDeviceStreams());
            dialog.setStreamIndex(m_settings.m_streamIndex);
        }

        dialog.move(p);
        new DialogPositioner(&dialog, false);
        dialog.exec();

        m_settings.m_rgbColor = m_channelMarker.getColor().rgb();
        m_settings.m_title = m_channelMarker.getTitle();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIDeviceIndex = dialog.getReverseAPIDeviceIndex();
        m_settings.m_reverseAPIChannelIndex = dialog.getReverseAPIChannelIndex();

        setWindowTitle(m_settings.m_title);
        setTitle(m_channelMarker.getTitle());
        setTitleColor(m_settings.m_rgbColor);

        if (m_deviceUISet->m_deviceMIMOEngine)
        {
            m_settings.m_streamIndex = dialog.getSelectedStreamIndex();
            m_channelMarker.clearStreamIndexes();
            m_channelMarker.addStreamIndex(m_settings.m_streamIndex);
            updateIndexLabel();
        }

        applySettings();
    }

    resetContextMenuType();
}

void SigMFFileSinkGUI::on_deltaFrequency_changed(qint64 value)
{
    if (!m_fixedPosition)
    {
        m_channelMarker.setCenterFrequency(value);
        m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
        updateAbsoluteCenterFrequency();
        setPosFromFrequency();
        applySettings();
    }
}

void SigMFFileSinkGUI::on_decimationFactor_currentIndexChanged(int index)
{
    m_settings.m_log2Decim = index;
    applyDecimation();
    displayRate();
    displayPos();
    applySettings();

    if (m_fixedPosition) {
        setFrequencyFromPos();
    } else {
        setPosFromFrequency();
    }
}

void SigMFFileSinkGUI::on_fixedPosition_toggled(bool checked)
{
    m_fixedPosition = checked;
    m_channelMarker.setMovable(!checked);
    ui->deltaFrequency->setEnabled(!checked);
    ui->position->setEnabled(checked);

    if (m_fixedPosition)
    {
        setFrequencyFromPos();
        applySettings();
    }
}

void SigMFFileSinkGUI::on_position_valueChanged(int value)
{
    m_fixedShiftIndex = value;
    displayPos();

    if (m_fixedPosition)
    {
        setFrequencyFromPos();
        applySettings();
    }
}

void SigMFFileSinkGUI::on_spectrumSquelch_toggled(bool checked)
{
    m_settings.m_spectrumSquelchMode = checked;

    if (!m_settings.m_spectrumSquelchMode)
    {
        m_settings.m_squelchRecordingEnable = false;
        ui->squelchLevel->setStyleSheet("QDial { background:rgb(79,79,79); }");
        ui->squelchedRecording->blockSignals(true);
        ui->squelchedRecording->setChecked(false);
        ui->squelchedRecording->blockSignals(false);
        ui->record->setEnabled(true);
    }

    ui->squelchedRecording->setEnabled(checked);

    applySettings();
}

void SigMFFileSinkGUI::on_squelchLevel_valueChanged(int value)
{
	m_settings.m_spectrumSquelch = value;
	ui->squelchLevelText->setText(tr("%1").arg(m_settings.m_spectrumSquelch));
    applySettings();
}

void SigMFFileSinkGUI::on_preRecordTime_valueChanged(int value)
{
    m_settings.m_preRecordTime = value;
    ui->preRecordTimeText->setText(tr("%1").arg(m_settings.m_preRecordTime));
    applySettings();
}

void SigMFFileSinkGUI::on_postSquelchTime_valueChanged(int value)
{
    m_settings.m_squelchPostRecordTime = value;
    ui->postSquelchTimeText->setText(tr("%1").arg(m_settings.m_squelchPostRecordTime));
    applySettings();
}

void SigMFFileSinkGUI::on_squelchedRecording_toggled(bool checked)
{
    ui->record->setEnabled(!checked);
    m_settings.m_squelchRecordingEnable = checked;
    applySettings();
}

void SigMFFileSinkGUI::on_record_toggled(bool checked)
{
    m_sigMFFileSink->record(checked);
}

void SigMFFileSinkGUI::on_showFileDialog_clicked(bool checked)
{
    (void) checked;
    QFileDialog fileDialog(
        this,
        tr("Save SigMF record file"),
        m_settings.m_fileRecordName,
        tr("SigMF Files (*.sigmf-meta)")
    );

    fileDialog.setOptions(QFileDialog::DontUseNativeDialog);
    fileDialog.setFileMode(QFileDialog::AnyFile);
    QStringList fileNames;

    if (fileDialog.exec())
    {
        fileNames = fileDialog.selectedFiles();

        if (fileNames.size() > 0)
        {
            m_settings.m_fileRecordName = fileNames.at(0);
		    ui->fileNameText->setText(m_settings.m_fileRecordName);
            applySettings();
        }
    }
}

void SigMFFileSinkGUI::setFrequencyFromPos()
{
    int inputFrequencyOffset = SigMFFileSinkSettings::getOffsetFromFixedShiftIndex(
        m_basebandSampleRate,
        m_settings.m_log2Decim,
        m_fixedShiftIndex);
    m_channelMarker.setCenterFrequency(inputFrequencyOffset);
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
}

void SigMFFileSinkGUI::setPosFromFrequency()
{
    int fshift = SigMFFileSinkSettings::getHalfBand(m_basebandSampleRate, m_settings.m_log2Decim + 1);
    m_fixedShiftIndex = SigMFFileSinkSettings::getFixedShiftIndexFromOffset(
        m_basebandSampleRate,
        m_settings.m_log2Decim,
        m_settings.m_inputFrequencyOffset + (m_settings.m_inputFrequencyOffset < 0 ? -fshift : fshift)
    );
    displayPos();
}

void SigMFFileSinkGUI::applyDecimation()
{
    ui->position->setMaximum(SigMFFileSinkSettings::getNbFixedShiftIndexes(m_settings.m_log2Decim)-1);
    ui->position->setValue(m_fixedShiftIndex);
    m_fixedShiftIndex = ui->position->value();
}

void SigMFFileSinkGUI::tick()
{
    if (++m_tickCount == 20) // once per second
    {
        uint64_t msTime = m_sigMFFileSink->getMsCount();
        uint64_t bytes = m_sigMFFileSink->getByteCount();
        unsigned int nbTracks = m_sigMFFileSink->getNbTracks();
        QTime recordLength(0, 0, 0, 0);
        recordLength = recordLength.addSecs(msTime / 1000);
        recordLength = recordLength.addMSecs(msTime % 1000);
        QString s_time = recordLength.toString("HH:mm:ss");
        ui->recordTimeText->setText(s_time);
        ui->recordSizeText->setText(displayScaled(bytes, 2));
        ui->recordNbTracks->setText(tr("#%1").arg(nbTracks));
        m_tickCount = 0;
    }
}

QString SigMFFileSinkGUI::displayScaled(uint64_t value, int precision)
{
    if (value < 1000) {
        return tr("%1").arg(QString::number(value, 'f', precision));
    } else if (value < 1000000) {
        return tr("%1k").arg(QString::number(value / 1000.0, 'f', precision));
    } else if (value < 1000000000) {
        return tr("%1M").arg(QString::number(value / 1000000.0, 'f', precision));
    } else if (value < 1000000000000) {
        return tr("%1G").arg(QString::number(value / 1000000000.0, 'f', precision));
    } else {
        return tr("%1").arg(QString::number(value, 'e', precision));
    }
}

void SigMFFileSinkGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &SigMFFileSinkGUI::on_deltaFrequency_changed);
    QObject::connect(ui->decimationFactor, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SigMFFileSinkGUI::on_decimationFactor_currentIndexChanged);
    QObject::connect(ui->fixedPosition, &QCheckBox::toggled, this, &SigMFFileSinkGUI::on_fixedPosition_toggled);
    QObject::connect(ui->position, &QSlider::valueChanged, this, &SigMFFileSinkGUI::on_position_valueChanged);
    QObject::connect(ui->spectrumSquelch, &ButtonSwitch::toggled, this, &SigMFFileSinkGUI::on_spectrumSquelch_toggled);
    QObject::connect(ui->squelchLevel, &QDial::valueChanged, this, &SigMFFileSinkGUI::on_squelchLevel_valueChanged);
    QObject::connect(ui->preRecordTime, &QDial::valueChanged, this, &SigMFFileSinkGUI::on_preRecordTime_valueChanged);
    QObject::connect(ui->postSquelchTime, &QDial::valueChanged, this, &SigMFFileSinkGUI::on_postSquelchTime_valueChanged);
    QObject::connect(ui->squelchedRecording, &ButtonSwitch::toggled, this, &SigMFFileSinkGUI::on_squelchedRecording_toggled);
    QObject::connect(ui->record, &ButtonSwitch::toggled, this, &SigMFFileSinkGUI::on_record_toggled);
    QObject::connect(ui->showFileDialog, &QPushButton::clicked, this, &SigMFFileSinkGUI::on_showFileDialog_clicked);
}

void SigMFFileSinkGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
