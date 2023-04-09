///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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
#include <QDateTime>
#include <QTime>
#include <QDebug>

#include "dsp/spectrumvis.h"
#include "dsp/scopevis.h"
#include "dsp/glscopesettings.h"
#include "dsp/dspcommands.h"
#include "device/deviceuiset.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "dsp/dspengine.h"
#include "gui/glspectrum.h"
#include "gui/crightclickenabler.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "maincore.h"

#include "ui_aismodgui.h"
#include "aismodgui.h"
#include "aismodrepeatdialog.h"
#include "aismodtxsettingsdialog.h"
#include "aismodsource.h"

AISModGUI* AISModGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx)
{
    AISModGUI* gui = new AISModGUI(pluginAPI, deviceUISet, channelTx);
    return gui;
}

void AISModGUI::destroy()
{
    delete this;
}

void AISModGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray AISModGUI::serialize() const
{
    return m_settings.serialize();
}

bool AISModGUI::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data)) {
        displaySettings();
        applySettings(true);
        return true;
    } else {
        resetToDefaults();
        return false;
    }
}

bool AISModGUI::handleMessage(const Message& message)
{
    if (AISMod::MsgConfigureAISMod::match(message))
    {
        const AISMod::MsgConfigureAISMod& cfg = (AISMod::MsgConfigureAISMod&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (AISMod::MsgReportData::match(message))
    {
        const AISMod::MsgReportData& report = (AISMod::MsgReportData&) message;
        m_settings.m_data = report.getData();
        ui->message->setText(m_settings.m_data);
        return true;
    }
    else if (DSPSignalNotification::match(message))
    {
        const DSPSignalNotification& notif = (const DSPSignalNotification&) message;
        m_deviceCenterFrequency = notif.getCenterFrequency();
        m_basebandSampleRate = notif.getSampleRate();
        ui->deltaFrequency->setValueRange(false, 7, -m_basebandSampleRate/2, m_basebandSampleRate/2);
        ui->deltaFrequencyLabel->setToolTip(tr("Range %1 %L2 Hz").arg(QChar(0xB1)).arg(m_basebandSampleRate/2));
        updateAbsoluteCenterFrequency();
        return true;
    }
    else
    {
        return false;
    }
}

void AISModGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void AISModGUI::handleSourceMessages()
{
    Message* message;
    MessageQueue *messageQueue = getInputMessageQueue();

    while ((message = messageQueue->pop()) != 0)
    {
        if (handleMessage(*message))
        {
            delete message;
        }
    }
}

void AISModGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings();
}

void AISModGUI::on_mode_currentIndexChanged(int value)
{

    // If m_doApplySettings is set, we are here from a call to displaySettings,
    // so we only want to display the current settings, not update them
    // as though a user had selected a new mode
    if (m_doApplySettings)
    {
        m_settings.m_rfBandwidth = m_settings.getRfBandwidth(value);
        m_settings.m_fmDeviation = m_settings.getFMDeviation(value);
        m_settings.m_bt = m_settings.getBT(value);
    }

    ui->rfBWText->setText(QString("%1k").arg(m_settings.m_rfBandwidth / 1000.0, 0, 'f', 1));
    ui->rfBW->setValue(m_settings.m_rfBandwidth / 100.0);
    ui->fmDevText->setText(QString("%1k").arg(m_settings.m_fmDeviation / 1000.0, 0, 'f', 1));
    ui->fmDev->setValue(m_settings.m_fmDeviation / 100.0);
    ui->btText->setText(QString("%1").arg(m_settings.m_bt, 0, 'f', 1));
    ui->bt->setValue(m_settings.m_bt * 10);
    applySettings();
}

void AISModGUI::on_rfBW_valueChanged(int value)
{
    float bw = value * 100.0f;
    ui->rfBWText->setText(QString("%1k").arg(value / 10.0, 0, 'f', 1));
    m_channelMarker.setBandwidth(bw);
    m_settings.m_rfBandwidth = bw;
    applySettings();
}

void AISModGUI::on_fmDev_valueChanged(int value)
{
    ui->fmDevText->setText(QString("%1k").arg(value / 10.0, 0, 'f', 1));
    m_settings.m_fmDeviation = value * 100.0;
    applySettings();
}

void AISModGUI::on_bt_valueChanged(int value)
{
    ui->btText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
    m_settings.m_bt = value / 10.0;
    applySettings();
}

void AISModGUI::on_gain_valueChanged(int value)
{
    ui->gainText->setText(QString("%1dB").arg(value));
    m_settings.m_gain = value;
    applySettings();
}

void AISModGUI::on_channelMute_toggled(bool checked)
{
    m_settings.m_channelMute = checked;
    applySettings();
}

void AISModGUI::on_insertPosition_clicked()
{
    float latitude = MainCore::instance()->getSettings().getLatitude();
    float longitude = MainCore::instance()->getSettings().getLongitude();

    ui->latitude->setValue(latitude);
    ui->longitude->setValue(longitude);
}

void AISModGUI::on_txButton_clicked()
{
    transmit();
}

void AISModGUI::on_message_returnPressed()
{
    m_settings.m_data = ui->message->text();
    applySettings();
}

void AISModGUI::on_msgId_currentIndexChanged(int index)
{
    m_settings.m_msgType = (AISModSettings::MsgType) index;
    applySettings();
}

void AISModGUI::on_mmsi_editingFinished()
{
    m_settings.m_mmsi = ui->mmsi->text();
    applySettings();
}

void AISModGUI::on_status_currentIndexChanged(int index)
{
    m_settings.m_status = (AISModSettings::Status) index;
    applySettings();
}

void AISModGUI::on_latitude_valueChanged(double value)
{
    m_settings.m_latitude = (float)value;
    applySettings();
}

void AISModGUI::on_longitude_valueChanged(double value)
{
    m_settings.m_longitude = (float)value;
    applySettings();
}

void AISModGUI::on_course_valueChanged(double value)
{
    m_settings.m_course = (float)value;
    applySettings();
}

void AISModGUI::on_speed_valueChanged(double value)
{
    m_settings.m_speed = (float)value;
    applySettings();
}

void AISModGUI::on_heading_valueChanged(int value)
{
    m_settings.m_heading = value;
    applySettings();
}

void AISModGUI::on_message_editingFinished()
{
    m_settings.m_data = ui->message->text();
    applySettings();
}

// Encode the message specified in individual settings in to a hex string (data settings) and put in message field
void AISModGUI::on_encode_clicked()
{
    AISMod::MsgEncode *msg = AISMod::MsgEncode::create();
    m_aisMod->getInputMessageQueue()->push(msg);
}

void AISModGUI::on_repeat_toggled(bool checked)
{
    m_settings.m_repeat = checked;
    applySettings();
}

void AISModGUI::repeatSelect(const QPoint& p)
{
    AISModRepeatDialog dialog(m_settings.m_repeatDelay, m_settings.m_repeatCount);
    dialog.move(p);

    if (dialog.exec() == QDialog::Accepted)
    {
        m_settings.m_repeatDelay = dialog.m_repeatDelay;
        m_settings.m_repeatCount = dialog.m_repeatCount;
        applySettings();
    }
}

void AISModGUI::txSettingsSelect(const QPoint& p)
{
    AISModTXSettingsDialog dialog(m_settings.m_rampUpBits, m_settings.m_rampDownBits,
        m_settings.m_rampRange,
        m_settings.m_baud,
        m_settings.m_symbolSpan,
        m_settings.m_rfNoise,
        m_settings.m_writeToFile);

    dialog.move(p);

    if (dialog.exec() == QDialog::Accepted)
    {
        m_settings.m_rampUpBits = dialog.m_rampUpBits;
        m_settings.m_rampDownBits = dialog.m_rampDownBits;
        m_settings.m_rampRange = dialog.m_rampRange;
        m_settings.m_baud = dialog.m_baud;
        m_settings.m_symbolSpan = dialog.m_symbolSpan;
        m_settings.m_rfNoise = dialog.m_rfNoise;
        m_settings.m_writeToFile = dialog.m_writeToFile;
        displaySettings();
        applySettings();
    }
}

void AISModGUI::on_udpEnabled_clicked(bool checked)
{
    m_settings.m_udpEnabled = checked;
    applySettings();
}

void AISModGUI::on_udpAddress_editingFinished()
{
    m_settings.m_udpAddress = ui->udpAddress->text();
    applySettings();
}

void AISModGUI::on_udpPort_editingFinished()
{
    m_settings.m_udpPort = ui->udpPort->text().toInt();
    applySettings();
}

void AISModGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void AISModGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_aisMod->getNumberOfDeviceStreams());
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

AISModGUI::AISModGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::AISModGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_basebandSampleRate(1),
    m_doApplySettings(true)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channeltx/modais/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_aisMod = (AISMod*) channelTx;
    m_aisMod->setMessageQueueToGUI(getInputMessageQueue());

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    m_scopeVis = m_aisMod->getScopeSink();
    m_scopeVis->setGLScope(ui->glScope);
    ui->glScope->connectTimer(MainCore::instance()->getMasterTimer());
    ui->scopeGUI->setBuddies(m_scopeVis->getInputMessageQueue(), m_scopeVis, ui->glScope);

    // Scope settings to display the IQ waveforms
    ui->scopeGUI->setPreTrigger(1);
    GLScopeSettings::TraceData traceDataI, traceDataQ;
    traceDataI.m_projectionType = Projector::ProjectionReal;
    traceDataI.m_amp = 1.0;      // for -1 to +1
    traceDataI.m_ofs = 0.0;      // vertical offset
    traceDataQ.m_projectionType = Projector::ProjectionImag;
    traceDataQ.m_amp = 1.0;
    traceDataQ.m_ofs = 0.0;
    ui->scopeGUI->changeTrace(0, traceDataI);
    ui->scopeGUI->addTrace(traceDataQ);
    ui->scopeGUI->setDisplayMode(GLScopeSettings::DisplayPol);
    ui->scopeGUI->focusOnTrace(0); // re-focus to take changes into account in the GUI

    GLScopeSettings::TriggerData triggerData;
    triggerData.m_triggerLevel = 0.1;
    triggerData.m_triggerLevelCoarse = 10;
    triggerData.m_triggerPositiveEdge = true;
    ui->scopeGUI->changeTrigger(0, triggerData);
    ui->scopeGUI->focusOnTrigger(0); // re-focus to take changes into account in the GUI

    m_scopeVis->setLiveRate(AISModSettings::AISMOD_SAMPLE_RATE);
    //m_scopeVis->setFreeRun(false); // FIXME: add method rather than call m_scopeVis->configure()

    m_spectrumVis = m_aisMod->getSpectrumVis();
    m_spectrumVis->setGLSpectrum(ui->glSpectrum);

    ui->spectrumGUI->setBuddies(m_spectrumVis, ui->glSpectrum);

    // Extra /2 here because SSB?
    ui->glSpectrum->setCenterFrequency(0);
    ui->glSpectrum->setSampleRate(AISModSettings::AISMOD_SAMPLE_RATE);
    ui->glSpectrum->setLsbDisplay(false);

    SpectrumSettings spectrumSettings = m_spectrumVis->getSettings();
    spectrumSettings.m_ssb = true;
    spectrumSettings.m_displayCurrent = true;
    spectrumSettings.m_displayWaterfall  =false;
    spectrumSettings.m_displayMaxHold = false;
    spectrumSettings.m_displayHistogram = false;
    SpectrumVis::MsgConfigureSpectrumVis *msg = SpectrumVis::MsgConfigureSpectrumVis::create(spectrumSettings, false);
    m_spectrumVis->getInputMessageQueue()->push(msg);

    CRightClickEnabler *repeatRightClickEnabler = new CRightClickEnabler(ui->repeat);
    connect(repeatRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(repeatSelect(const QPoint &)));

    CRightClickEnabler *txRightClickEnabler = new CRightClickEnabler(ui->txButton);
    connect(txRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(txSettingsSelect(const QPoint &)));

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::red);
    m_channelMarker.setBandwidth(12500);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("AIS Modulator");
    m_channelMarker.setSourceOrSinkStream(false);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    m_deviceUISet->addChannelMarker(&m_channelMarker);

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));

    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));
    m_aisMod->setLevelMeter(ui->volumeMeter);

    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);

    ui->scopeContainer->setVisible(false);
    ui->spectrumContainer->setVisible(false);

    displaySettings();
    makeUIConnections();
    applySettings();
    DialPopup::addPopupsToChildDials(this);
}

AISModGUI::~AISModGUI()
{
    delete ui;
}

void AISModGUI::transmit()
{
    ui->transmittedText->appendPlainText(m_settings.m_data);
    AISMod::MsgTx *msg = AISMod::MsgTx::create();
    m_aisMod->getInputMessageQueue()->push(msg);
}

void AISModGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void AISModGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        AISMod::MsgConfigureAISMod *msg = AISMod::MsgConfigureAISMod::create(m_settings, force);
        m_aisMod->getInputMessageQueue()->push(msg);
    }
}

void AISModGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());
    updateIndexLabel();

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    ui->rfBWText->setText(QString("%1k").arg(m_settings.m_rfBandwidth / 1000.0, 0, 'f', 1));
    ui->rfBW->setValue(m_settings.m_rfBandwidth / 100.0);

    ui->fmDevText->setText(QString("%1k").arg(m_settings.m_fmDeviation / 1000.0, 0, 'f', 1));
    ui->fmDev->setValue(m_settings.m_fmDeviation / 100.0);

    ui->btText->setText(QString("%1").arg(m_settings.m_bt, 0, 'f', 1));
    ui->bt->setValue(m_settings.m_bt * 10);

    ui->gainText->setText(QString("%1").arg((double)m_settings.m_gain, 0, 'f', 1));
    ui->gain->setValue(m_settings.m_gain);

    ui->udpEnabled->setChecked(m_settings.m_udpEnabled);
    ui->udpAddress->setText(m_settings.m_udpAddress);
    ui->udpPort->setText(QString::number(m_settings.m_udpPort));

    ui->channelMute->setChecked(m_settings.m_channelMute);
    ui->repeat->setChecked(m_settings.m_repeat);

    ui->msgId->setCurrentIndex((int) m_settings.m_msgType);
    ui->mmsi->setText(m_settings.m_mmsi);
    ui->status->setCurrentIndex((int) m_settings.m_status);
    ui->latitude->setValue(m_settings.m_latitude);
    ui->longitude->setValue(m_settings.m_longitude);
    ui->course->setValue(m_settings.m_course);
    ui->speed->setValue(m_settings.m_speed);
    ui->heading->setValue(m_settings.m_heading);
    ui->message->setText(m_settings.m_data);

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void AISModGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void AISModGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void AISModGUI::tick()
{
    double powDb = CalcDb::dbPower(m_aisMod->getMagSq());
    m_channelPowerDbAvg(powDb);
    ui->channelPower->setText(tr("%1 dB").arg(m_channelPowerDbAvg.asDouble(), 0, 'f', 1));
}

void AISModGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &AISModGUI::on_deltaFrequency_changed);
    QObject::connect(ui->mode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AISModGUI::on_mode_currentIndexChanged);
    QObject::connect(ui->rfBW, &QSlider::valueChanged, this, &AISModGUI::on_rfBW_valueChanged);
    QObject::connect(ui->fmDev, &QSlider::valueChanged, this, &AISModGUI::on_fmDev_valueChanged);
    QObject::connect(ui->bt, &QSlider::valueChanged, this, &AISModGUI::on_bt_valueChanged);
    QObject::connect(ui->gain, &QDial::valueChanged, this, &AISModGUI::on_gain_valueChanged);
    QObject::connect(ui->channelMute, &QToolButton::toggled, this, &AISModGUI::on_channelMute_toggled);
    QObject::connect(ui->txButton, &QPushButton::clicked, this, &AISModGUI::on_txButton_clicked);
    QObject::connect(ui->encode, &QToolButton::clicked, this, &AISModGUI::on_encode_clicked);
    QObject::connect(ui->msgId, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AISModGUI::on_msgId_currentIndexChanged);
    QObject::connect(ui->mmsi, &QLineEdit::editingFinished, this, &AISModGUI::on_mmsi_editingFinished);
    QObject::connect(ui->status, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AISModGUI::on_status_currentIndexChanged);
    QObject::connect(ui->latitude, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &AISModGUI::on_latitude_valueChanged);
    QObject::connect(ui->longitude, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &AISModGUI::on_longitude_valueChanged);
    QObject::connect(ui->insertPosition, &QToolButton::clicked, this, &AISModGUI::on_insertPosition_clicked);
    QObject::connect(ui->course, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &AISModGUI::on_course_valueChanged);
    QObject::connect(ui->speed, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &AISModGUI::on_speed_valueChanged);
    QObject::connect(ui->heading, QOverload<int>::of(&QSpinBox::valueChanged), this, &AISModGUI::on_heading_valueChanged);
    QObject::connect(ui->message, &QLineEdit::editingFinished, this, &AISModGUI::on_message_editingFinished);
    QObject::connect(ui->message, &QLineEdit::returnPressed, this, &AISModGUI::on_message_returnPressed);
    QObject::connect(ui->repeat, &ButtonSwitch::toggled, this, &AISModGUI::on_repeat_toggled);
    QObject::connect(ui->udpEnabled, &QCheckBox::clicked, this, &AISModGUI::on_udpEnabled_clicked);
    QObject::connect(ui->udpAddress, &QLineEdit::editingFinished, this, &AISModGUI::on_udpAddress_editingFinished);
    QObject::connect(ui->udpPort, &QLineEdit::editingFinished, this, &AISModGUI::on_udpPort_editingFinished);
}

void AISModGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
