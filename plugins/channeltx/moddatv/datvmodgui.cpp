///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#include <QDockWidget>
#include <QMainWindow>
#include <QFileDialog>
#include <QTime>
#include <QDebug>
#include <QMessageBox>

#include <cmath>

#include "device/deviceuiset.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "util/db.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "gui/dialogpositioner.h"
#include "maincore.h"

#include "ui_datvmodgui.h"
#include "datvmodgui.h"
#include "datvmodreport.h"

DATVModGUI* DATVModGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx)
{
    DATVModGUI* gui = new DATVModGUI(pluginAPI, deviceUISet, channelTx);
    return gui;
}

void DATVModGUI::destroy()
{
    delete this;
}

DATVModGUI::DATVModGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::DATVModGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_basebandSampleRate(1),
    m_doApplySettings(true),
    m_tickMsgOutstanding(false),
    m_streamLength(0),
    m_bitrate(1),
    m_frameCount(0),
    m_tickCount(0),
    m_enableNavTime(false)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channeltx/moddatv/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_datvMod = (DATVMod*) channelTx;
    m_datvMod->setMessageQueueToGUI(getInputMessageQueue());

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 8, -99999999, 99999999);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    m_channelMarker.setBandwidth(5000);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("DATV Modulator");
    m_channelMarker.setSourceOrSinkStream(false);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    setTitleColor(m_channelMarker.getColor());
    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);

    m_deviceUISet->addChannelMarker(&m_channelMarker);

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));

    resetToDefaults();

    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));

#ifndef _WIN32
    // Only currently works on Windows, so hide on other OSes
    ui->udpBufferUtilization->setVisible(false);
    ui->udpBufferUtilizationLine->setVisible(false);
#endif

    displaySettings();
    makeUIConnections();
    applySettings(true);
    if (!m_settings.m_tsFileName.isEmpty())
        configureTsFileName();
}

DATVModGUI::~DATVModGUI()
{
    delete ui;
}

void DATVModGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
}

QByteArray DATVModGUI::serialize() const
{
    return m_settings.serialize();
}

bool DATVModGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        displaySettings();
        applySettings(true);
        if (!m_settings.m_tsFileName.isEmpty())
            configureTsFileName();
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        displaySettings();
        applySettings(true);
        return false;
    }
}

bool DATVModGUI::handleMessage(const Message& message)
{
    if (DATVModReport::MsgReportTsFileSourceStreamData::match(message))
    {
        m_bitrate = ((DATVModReport::MsgReportTsFileSourceStreamData&)message).getBitrate();
        m_streamLength = ((DATVModReport::MsgReportTsFileSourceStreamData&)message).getStreamLength();
        m_frameCount = 0;
        ui->tsFileBitrate->setText(QString("%1kb/s").arg(m_bitrate/1000, 0, 'f', 2));
        updateWithStreamData();
        return true;
    }
    else if (DATVModReport::MsgReportTsFileSourceStreamTiming::match(message))
    {
        m_frameCount = ((DATVModReport::MsgReportTsFileSourceStreamTiming&)message).getFrameCount();
        updateWithStreamTime();
        m_tickMsgOutstanding = false;
        return true;
    }
    else if (DATVModReport::MsgReportRates::match(message))
    {
        DATVModReport::MsgReportRates& report = (DATVModReport::MsgReportRates&)message;
        m_channelSampleRate = report.getChannelSampleRate();
        m_sampleRate = report.getSampleRate();
        ui->channelSampleRateText->setText(tr("%1k").arg(m_sampleRate/1000.0f, 0, 'f', 2));
        int dataRate = report.getDataRate();
        ui->dataRateText->setText(tr("%1kb/s").arg(dataRate/1000.0f, 0, 'f', 2));
        setChannelMarkerBandwidth();
        return true;
    }
    else if (DATVModReport::MsgReportUDPBitrate::match(message))
    {
        DATVModReport::MsgReportUDPBitrate& report = (DATVModReport::MsgReportUDPBitrate&)message;
        ui->udpBitrate->setText(tr("%1kb/s").arg(report.getBitrate()/1000.0f, 0, 'f', 2));
        m_tickMsgOutstanding = false;
        return true;
    }
    else if (DATVModReport::MsgReportUDPBufferUtilization::match(message))
    {
        DATVModReport::MsgReportUDPBufferUtilization& report = (DATVModReport::MsgReportUDPBufferUtilization&)message;
        ui->udpBufferUtilization->setText(tr("%1%").arg(report.getUtilization(), 0, 'f', 1));
        m_tickMsgOutstanding = false;
        return true;
    }
    else if (DATVMod::MsgConfigureDATVMod::match(message))
    {
        const DATVMod::MsgConfigureDATVMod& cfg = (DATVMod::MsgConfigureDATVMod&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (DATVMod::MsgConfigureTsFileName::match(message))
    {
        const DATVMod::MsgConfigureTsFileName& cfg = (DATVMod::MsgConfigureTsFileName&) message;
        m_settings.m_tsFileName = cfg.getFileName();
        ui->tsFileText->setText(m_settings.m_tsFileName);
        return true;
    }
    else if (DSPSignalNotification::match(message))
    {
        const DSPSignalNotification& notif = (const DSPSignalNotification&) message;
        m_deviceCenterFrequency = notif.getCenterFrequency();
        m_basebandSampleRate = notif.getSampleRate();
        ui->deltaFrequency->setValueRange(false, 8, -m_basebandSampleRate/2, m_basebandSampleRate/2);
        ui->deltaFrequencyLabel->setToolTip(tr("Range %1 %L2 Hz").arg(QChar(0xB1)).arg(m_basebandSampleRate/2));
        updateAbsoluteCenterFrequency();
        return true;

    }
    else
    {
        return false;
    }
}

void DATVModGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void DATVModGUI::handleSourceMessages()
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

void DATVModGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = value;
    updateAbsoluteCenterFrequency();
    applySettings();
}

void DATVModGUI::on_dvbStandard_currentIndexChanged(int index)
{
    int idx;

    m_settings.m_standard = (DATVModSettings::DVBStandard) index;

    ui->rollOff->blockSignals(true);
    ui->modulation->blockSignals(true);

    ui->rollOff->clear();
    ui->modulation->clear();

    if (m_settings.m_standard == DATVModSettings::DVB_S)
    {
        ui->rollOff->addItem("0.35");
        ui->modulation->addItem("BPSK");
        ui->modulation->addItem("QPSK");
    }
    else
    {
        ui->rollOff->addItem("0.20");
        ui->rollOff->addItem("0.25");
        ui->rollOff->addItem("0.35");
        ui->modulation->addItem("QPSK");
        ui->modulation->addItem("8PSK");
        ui->modulation->addItem("16APSK");
        ui->modulation->addItem("32APSK");
    }

    ui->rollOff->blockSignals(false);
    ui->modulation->blockSignals(false);

    m_doApplySettings = false;

    idx = ui->rollOff->findText(QString("%1").arg(m_settings.m_rollOff, 0, 'f', 2));
    idx = idx == -1 ? 0 : idx;
    ui->rollOff->setCurrentIndex(idx);
    on_rollOff_currentIndexChanged(idx);
    idx = ui->modulation->findText(DATVModSettings::mapModulation(m_settings.m_modulation));
    idx = idx == -1 ? 0 : idx;
    ui->modulation->setCurrentIndex(idx);
    on_modulation_currentIndexChanged(idx);

    updateFEC();

    m_doApplySettings = true;

    applySettings();
}

void DATVModGUI::updateFEC()
{
    ui->fec->blockSignals(true);

    ui->fec->clear();
    if (m_settings.m_standard == DATVModSettings::DVB_S)
    {
        ui->fec->addItem("1/2");
        ui->fec->addItem("2/3");
        ui->fec->addItem("3/4");
        ui->fec->addItem("5/6");
        ui->fec->addItem("7/8");
    }
    else if (m_settings.m_modulation == DATVModSettings::QPSK)
    {
        ui->fec->addItem("1/4");
        ui->fec->addItem("1/3");
        ui->fec->addItem("2/5");
        ui->fec->addItem("1/2");
        ui->fec->addItem("3/5");
        ui->fec->addItem("2/3");
        ui->fec->addItem("3/4");
        ui->fec->addItem("4/5");
        ui->fec->addItem("5/6");
        ui->fec->addItem("8/9");
        ui->fec->addItem("9/10");
    }
    else if (m_settings.m_modulation == DATVModSettings::PSK8)
    {
        ui->fec->addItem("3/5");
        ui->fec->addItem("2/3");
        ui->fec->addItem("3/4");
        ui->fec->addItem("5/6");
        ui->fec->addItem("8/9");
        ui->fec->addItem("9/10");
    }
    else if (m_settings.m_modulation == DATVModSettings::APSK16)
    {
        ui->fec->addItem("2/3");
        ui->fec->addItem("3/4");
        ui->fec->addItem("4/5");
        ui->fec->addItem("5/6");
        ui->fec->addItem("8/9");
        ui->fec->addItem("9/10");
    }
    else if (m_settings.m_modulation == DATVModSettings::APSK32)
    {
        ui->fec->addItem("3/4");
        ui->fec->addItem("4/5");
        ui->fec->addItem("5/6");
        ui->fec->addItem("8/9");
        ui->fec->addItem("9/10");
    }

    ui->fec->blockSignals(false);

    int idx = ui->fec->findText(DATVModSettings::mapCodeRate(m_settings.m_fec));
    idx = idx == -1 ? 0 : idx;
    ui->fec->setCurrentIndex(idx);
    on_fec_currentIndexChanged(idx);
}

void DATVModGUI::on_modulation_currentIndexChanged(int index)
{
    if (m_settings.m_standard == DATVModSettings::DVB_S)
        m_settings.m_modulation = (DATVModSettings::DATVModulation) index;
    else
        m_settings.m_modulation = (DATVModSettings::DATVModulation) (index + 1);
    m_doApplySettings = false;
    updateFEC();
    m_doApplySettings = true;
    applySettings();
}

void DATVModGUI::on_rollOff_currentIndexChanged(int index)
{
    (void) index;
    m_settings.m_rollOff = ui->rollOff->currentText().toFloat();
    applySettings();
}

void DATVModGUI::on_fec_currentIndexChanged(int index)
{
    (void) index;
    m_settings.m_fec = DATVModSettings::mapCodeRate(ui->fec->currentText());
    applySettings();
}

void DATVModGUI::on_symbolRate_valueChanged(int value)
{
    m_settings.m_symbolRate = value;
    applySettings();
}

void DATVModGUI::on_rfBW_valueChanged(int value)
{
    m_settings.m_rfBandwidth = value * 100000;
    ui->rfBWText->setText(QString("%1M").arg(m_settings.m_rfBandwidth / 1e6, 0, 'f', 1));
    setChannelMarkerBandwidth();
    applySettings();
}

void DATVModGUI::setChannelMarkerBandwidth()
{
    if (m_channelSampleRate == m_sampleRate)
        m_channelMarker.setBandwidth(m_channelSampleRate);
    else
        m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setSidebands(ChannelMarker::dsb);
}

void DATVModGUI::on_inputSelect_currentIndexChanged(int index)
{
    m_settings.m_source = (DATVModSettings::DATVSource) index;
    applySettings();
}

void DATVModGUI::on_channelMute_toggled(bool checked)
{
    m_settings.m_channelMute = checked;
    applySettings();
}

void DATVModGUI::on_tsFileDialog_clicked(bool checked)
{
    (void) checked;
    QString fileName = QFileDialog::getOpenFileName(this,
        tr("Open MPEG transport stream file"), m_settings.m_tsFileName, tr("MPEG Transport Stream Files (*.ts)"),
        nullptr, QFileDialog::DontUseNativeDialog);

    if (fileName != "")
    {
        m_settings.m_tsFileName = fileName;
        ui->tsFileText->setText(m_settings.m_tsFileName);
        configureTsFileName();
    }
}

void DATVModGUI::on_playLoop_toggled(bool checked)
{
    m_settings.m_tsFilePlayLoop = checked;
    applySettings();
}

void DATVModGUI::on_playFile_toggled(bool checked)
{
    m_settings.m_tsFilePlay = checked;
    ui->navTimeSlider->setEnabled(!checked);
    m_enableNavTime = !checked;
    applySettings();
}

void DATVModGUI::on_navTimeSlider_valueChanged(int value)
{
    if (m_enableNavTime && ((value >= 0) && (value <= 100)))
    {
        DATVMod::MsgConfigureTsFileSourceSeek* message = DATVMod::MsgConfigureTsFileSourceSeek::create(value);
        m_datvMod->getInputMessageQueue()->push(message);
    }
}

void DATVModGUI::configureTsFileName()
{
    DATVMod::MsgConfigureTsFileName* message = DATVMod::MsgConfigureTsFileName::create(m_settings.m_tsFileName);
    m_datvMod->getInputMessageQueue()->push(message);
}

void DATVModGUI::on_udpAddress_editingFinished()
{
    m_settings.m_udpAddress = ui->udpAddress->text();
    applySettings();
}

void DATVModGUI::on_udpPort_valueChanged(int value)
{
    m_settings.m_udpPort = value;
    applySettings();
}

void DATVModGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void DATVModGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_datvMod->getNumberOfDeviceStreams());
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

void DATVModGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void DATVModGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        DATVMod::MsgConfigureSourceCenterFrequency *msgChan = DATVMod::MsgConfigureSourceCenterFrequency::create(
                m_channelMarker.getCenterFrequency());
        m_datvMod->getInputMessageQueue()->push(msgChan);

        DATVMod::MsgConfigureDATVMod *msg = DATVMod::MsgConfigureDATVMod::create(m_settings, force);
        m_datvMod->getInputMessageQueue()->push(msg);
    }
}

void DATVModGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle(m_settings.m_title);
    setChannelMarkerBandwidth();
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());
    updateIndexLabel();

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_settings.m_inputFrequencyOffset);

    ui->dvbStandard->setCurrentIndex((int) m_settings.m_standard);
    ui->fec->setCurrentIndex(ui->fec->findText(DATVModSettings::mapCodeRate(m_settings.m_fec)));
    ui->symbolRate->setValue(m_settings.m_symbolRate);
    ui->rollOff->setCurrentIndex(ui->rollOff->findText(QString("%1").arg(m_settings.m_rollOff, 0, 'f', 2)));
    ui->modulation->setCurrentIndex(ui->modulation->findText(DATVModSettings::mapModulation(m_settings.m_modulation)));

    ui->rfBW->setValue(m_settings.m_rfBandwidth/100000);
    ui->rfBWText->setText(QString("%1M").arg(m_settings.m_rfBandwidth / 1e6, 0, 'f', 1));

    ui->channelMute->setChecked(m_settings.m_channelMute);

    ui->inputSelect->setCurrentIndex((int) m_settings.m_source);

    if (m_settings.m_tsFileName.isEmpty())
        ui->tsFileText->setText("...");
    else
        ui->tsFileText->setText(m_settings.m_tsFileName);
    ui->playFile->setChecked(m_settings.m_tsFilePlay);
    ui->playLoop->setChecked(m_settings.m_tsFilePlayLoop);

    ui->udpAddress->setText(m_settings.m_udpAddress);
    ui->udpPort->setValue(m_settings.m_udpPort);

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void DATVModGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void DATVModGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void DATVModGUI::tick()
{
    double powDb = CalcDb::dbPower(m_datvMod->getMagSq());
    m_channelPowerDbAvg(powDb);
    ui->channelPower->setText(tr("%1 dB").arg(m_channelPowerDbAvg.asDouble(), 0, 'f', 1));

    // Use m_tickMsgOutstanding to prevent queuing lots of messsages while stopped/paused
    if (((++m_tickCount & 0xf) == 0) && !m_tickMsgOutstanding)
    {
        if (ui->inputSelect->currentIndex() == (int) DATVModSettings::SourceFile)
        {
            m_tickMsgOutstanding = true;
            m_datvMod->getInputMessageQueue()->push(DATVMod::MsgConfigureTsFileSourceStreamTiming::create());
        }
        else if (ui->inputSelect->currentIndex() == (int) DATVModSettings::SourceUDP)
        {
            m_tickMsgOutstanding = true;
            m_datvMod->getInputMessageQueue()->push(DATVMod::MsgGetUDPBitrate::create());
            m_datvMod->getInputMessageQueue()->push(DATVMod::MsgGetUDPBufferUtilization::create());
        }
    }
}

void DATVModGUI::updateWithStreamData()
{
    QTime recordLength(0, 0, 0, 0);
    recordLength = recordLength.addSecs(m_streamLength * 8 / m_bitrate);
    QString s_time = recordLength.toString("HH:mm:ss");
    ui->recordLengthText->setText(s_time);
    updateWithStreamTime();
}

void DATVModGUI::updateWithStreamTime()
{
    int t_sec = 0;
    int t_msec = 0;

    if (m_bitrate > 0.0f)
    {
        float secs = m_frameCount * 188 * 8 / m_bitrate;
        t_sec = (int) secs;
        t_msec = (int) ((secs - t_sec) * 1000.0f);
    }

    QTime t(0, 0, 0, 0);
    t = t.addSecs(t_sec);
    t = t.addMSecs(t_msec);
    QString s_timems = t.toString("HH:mm:ss.zzz");
    QString s_time = t.toString("HH:mm:ss");
    ui->relTimeText->setText(s_timems);

    if (!m_enableNavTime)
    {
        float posRatio = (t_sec * m_bitrate) / (m_streamLength * 8);
        ui->navTimeSlider->setValue((int) (posRatio * 100.0));
    }
}

void DATVModGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &DATVModGUI::on_deltaFrequency_changed);
    QObject::connect(ui->channelMute, &QToolButton::toggled, this, &DATVModGUI::on_channelMute_toggled);
    QObject::connect(ui->dvbStandard, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DATVModGUI::on_dvbStandard_currentIndexChanged);
    QObject::connect(ui->symbolRate, QOverload<int>::of(&QSpinBox::valueChanged), this, &DATVModGUI::on_symbolRate_valueChanged);
    QObject::connect(ui->rfBW, &QSlider::valueChanged, this, &DATVModGUI::on_rfBW_valueChanged);
    QObject::connect(ui->fec, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DATVModGUI::on_fec_currentIndexChanged);
    QObject::connect(ui->modulation, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DATVModGUI::on_modulation_currentIndexChanged);
    QObject::connect(ui->rollOff, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DATVModGUI::on_rollOff_currentIndexChanged);
    QObject::connect(ui->inputSelect, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DATVModGUI::on_inputSelect_currentIndexChanged);
    QObject::connect(ui->tsFileDialog, &QPushButton::clicked, this, &DATVModGUI::on_tsFileDialog_clicked);
    QObject::connect(ui->playFile, &ButtonSwitch::toggled, this, &DATVModGUI::on_playFile_toggled);
    QObject::connect(ui->playLoop, &ButtonSwitch::toggled, this, &DATVModGUI::on_playLoop_toggled);
    QObject::connect(ui->navTimeSlider, &QSlider::valueChanged, this, &DATVModGUI::on_navTimeSlider_valueChanged);
    QObject::connect(ui->udpAddress, &QLineEdit::editingFinished, this, &DATVModGUI::on_udpAddress_editingFinished);
    QObject::connect(ui->udpPort, QOverload<int>::of(&QSpinBox::valueChanged), this, &DATVModGUI::on_udpPort_valueChanged);
}

void DATVModGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
