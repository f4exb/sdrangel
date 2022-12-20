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
#include <QFileDialog>
#include <QTime>
#include <QDebug>
#include <QMessageBox>

#include "device/deviceuiset.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "dsp/spectrumvis.h"
#include "dsp/scopevis.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "dsp/glscopesettings.h"
#include "gui/glspectrum.h"
#include "gui/crightclickenabler.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "maincore.h"

#include "ui_ieee_802_15_4_modgui.h"
#include "ieee_802_15_4_modgui.h"
#include "ieee_802_15_4_modrepeatdialog.h"
#include "ieee_802_15_4_modtxsettingsdialog.h"


IEEE_802_15_4_ModGUI* IEEE_802_15_4_ModGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx)
{
    IEEE_802_15_4_ModGUI* gui = new IEEE_802_15_4_ModGUI(pluginAPI, deviceUISet, channelTx);
    return gui;
}

void IEEE_802_15_4_ModGUI::destroy()
{
    delete this;
}

void IEEE_802_15_4_ModGUI::setName(const QString& name)
{
    setObjectName(name);
}

QString IEEE_802_15_4_ModGUI::getName() const
{
    return objectName();
}

qint64 IEEE_802_15_4_ModGUI::getCenterFrequency() const {
    return m_channelMarker.getCenterFrequency();
}

void IEEE_802_15_4_ModGUI::setCenterFrequency(qint64 centerFrequency)
{
    m_channelMarker.setCenterFrequency(centerFrequency);
    applySettings();
}

void IEEE_802_15_4_ModGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray IEEE_802_15_4_ModGUI::serialize() const
{
    return m_settings.serialize();
}

bool IEEE_802_15_4_ModGUI::deserialize(const QByteArray& data)
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

bool IEEE_802_15_4_ModGUI::handleMessage(const Message& message)
{
    if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) message;
        m_deviceCenterFrequency = notif.getCenterFrequency();
        m_basebandSampleRate = notif.getSampleRate();
        ui->deltaFrequency->setValueRange(false, 7, -m_basebandSampleRate/2, m_basebandSampleRate/2);
        ui->deltaFrequencyLabel->setToolTip(tr("Range %1 %L2 Hz").arg(QChar(0xB1)).arg(m_basebandSampleRate/2));
        updateAbsoluteCenterFrequency();
        m_scopeVis->setLiveRate(m_basebandSampleRate);
        checkSampleRate();
        return true;
    }
    else if (IEEE_802_15_4_Mod::MsgConfigureIEEE_802_15_4_Mod::match(message))
    {
        qDebug("IEEE_802_15_4_ModGUI::handleMessage: MsgConfigureIEEE_802_15_4_Mod");
        const IEEE_802_15_4_Mod::MsgConfigureIEEE_802_15_4_Mod& cfg = (IEEE_802_15_4_Mod::MsgConfigureIEEE_802_15_4_Mod&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else
    {
        return false;
    }
}

void IEEE_802_15_4_ModGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void IEEE_802_15_4_ModGUI::handleSourceMessages()
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

// Check sample rate is good enough and if not, display a warning in title
void IEEE_802_15_4_ModGUI::checkSampleRate()
{
    int cr = m_settings.getChipRate();

    if ((m_basebandSampleRate % cr) != 0)
    {
        ui->chipRateText->setStyleSheet("QLabel { background:rgb(200,50,50); }");
        ui->chipRateText->setToolTip(QString("Baseband sample rate %1 S/s is not an integer multiple of chip rate %2 S/s").arg(m_basebandSampleRate).arg(cr));
    }
    else if ((m_basebandSampleRate / cr) <= 2)
    {
        ui->chipRateText->setStyleSheet("QLabel { background:rgb(200,50,50); }");
        ui->chipRateText->setToolTip(QString("Baseband sample rate %1 S/s is too low for chip rate %2 S/s").arg(m_basebandSampleRate).arg(cr));
    }
    else
    {
        ui->chipRateText->setStyleSheet("QLabel { background:rgb(79,79,79); }");
        ui->chipRateText->setToolTip("Chip rate");
    }
}

void IEEE_802_15_4_ModGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings();
}

void IEEE_802_15_4_ModGUI::on_phy_currentIndexChanged(int value)
{
    QString phy = ui->phy->currentText();

    // If m_doApplySettings is set, we are here from a call to displaySettings,
    // so we only want to display the current settings, not update them
    // as though a user had selected a new PHY
    if (m_doApplySettings)
        m_settings.setPHY(phy);

    displayRFBandwidth(m_settings.m_rfBandwidth);
    ui->rfBW->setValue(m_settings.m_rfBandwidth / 1000.0);
    ui->glSpectrum->setCenterFrequency(0);
    ui->glSpectrum->setSampleRate(m_settings.m_spectrumRate);
    displayChipRate(m_settings);
    checkSampleRate();
    applySettings();

    // Remove custom PHY when deselected, as we no longer know how to set it
    if (value < 6)
        ui->phy->removeItem(6);
}

void IEEE_802_15_4_ModGUI::on_rfBW_valueChanged(int value)
{
    float bw = value * 1000.0f;
    displayRFBandwidth(bw);
    m_channelMarker.setBandwidth(bw);
    m_settings.m_rfBandwidth = bw;
    applySettings();
}

void IEEE_802_15_4_ModGUI::on_gain_valueChanged(int value)
{
    ui->gainText->setText(QString("%1dB").arg(value));
    m_settings.m_gain = value;
    applySettings();
}

void IEEE_802_15_4_ModGUI::on_channelMute_toggled(bool checked)
{
    m_settings.m_channelMute = checked;
    applySettings();
}

void IEEE_802_15_4_ModGUI::on_txButton_clicked()
{
    transmit();
}

void IEEE_802_15_4_ModGUI::on_frame_returnPressed()
{
    transmit();
}

void IEEE_802_15_4_ModGUI::on_frame_editingFinished()
{
    m_settings.m_data = ui->frame->text();
    applySettings();
}

void IEEE_802_15_4_ModGUI::on_repeat_toggled(bool checked)
{
    m_settings.m_repeat = checked;
    applySettings();
}

void IEEE_802_15_4_ModGUI::repeatSelect()
{
    IEEE_802_15_4_ModRepeatDialog dialog(m_settings.m_repeatDelay, m_settings.m_repeatCount);
    if (dialog.exec() == QDialog::Accepted)
    {
        m_settings.m_repeatDelay = dialog.m_repeatDelay;
        m_settings.m_repeatCount = dialog.m_repeatCount;
        applySettings();
    }
}

void IEEE_802_15_4_ModGUI::txSettingsSelect()
{
    IEEE_802_15_4_ModTXSettingsDialog dialog(
        m_settings.m_rampUpBits,
        m_settings.m_rampDownBits,
        m_settings.m_rampRange,
        m_settings.m_modulateWhileRamping,
        m_settings.m_modulation,
        m_settings.m_bitRate,
        m_settings.m_pulseShaping,
        m_settings.m_beta,
        m_settings.m_symbolSpan,
        m_settings.m_scramble,
        m_settings.m_polynomial,
        m_settings.m_lpfTaps,
        m_settings.m_bbNoise,
        m_settings.m_writeToFile
    );
    if (dialog.exec() == QDialog::Accepted)
    {
        m_settings.m_rampUpBits = dialog.m_rampUpBits;
        m_settings.m_rampDownBits = dialog.m_rampDownBits;
        m_settings.m_rampRange = dialog.m_rampRange;
        m_settings.m_modulateWhileRamping = dialog.m_modulateWhileRamping;
        m_settings.m_modulation = static_cast<IEEE_802_15_4_ModSettings::Modulation>(dialog.m_modulation);
        m_settings.m_bitRate = dialog.m_bitRate;
        m_settings.m_pulseShaping = static_cast<IEEE_802_15_4_ModSettings::PulseShaping>(dialog.m_pulseShaping);
        m_settings.m_beta = dialog.m_beta;
        m_settings.m_symbolSpan = dialog.m_symbolSpan;
        m_settings.m_scramble = dialog.m_scramble;
        m_settings.m_polynomial = dialog.m_polynomial;
        m_settings.m_lpfTaps = dialog.m_lpfTaps;
        m_settings.m_bbNoise = dialog.m_bbNoise;
        m_settings.m_writeToFile = dialog.m_writeToFile;
        displaySettings();
        applySettings();
    }
}

void IEEE_802_15_4_ModGUI::on_udpEnabled_clicked(bool checked)
{
    m_settings.m_udpEnabled = checked;
    applySettings();
}

void IEEE_802_15_4_ModGUI::on_udpAddress_editingFinished()
{
    m_settings.m_udpAddress = ui->udpAddress->text();
    applySettings();
}

void IEEE_802_15_4_ModGUI::on_udpPort_editingFinished()
{
    m_settings.m_udpPort = ui->udpPort->text().toInt();
    applySettings();
}

void IEEE_802_15_4_ModGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void IEEE_802_15_4_ModGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_IEEE_802_15_4_Mod->getNumberOfDeviceStreams());
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

IEEE_802_15_4_ModGUI::IEEE_802_15_4_ModGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::IEEE_802_15_4_ModGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_doApplySettings(true),
    m_basebandSampleRate(12000000)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channeltx/mod802.15.4/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_IEEE_802_15_4_Mod = (IEEE_802_15_4_Mod*) channelTx;
    m_IEEE_802_15_4_Mod->setMessageQueueToGUI(getInputMessageQueue());

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    m_scopeVis = m_IEEE_802_15_4_Mod->getScopeSink();
    m_scopeVis->setGLScope(ui->glScope);
    ui->glScope->connectTimer(MainCore::instance()->getMasterTimer());
    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick())); // 50 ms
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

    m_scopeVis->setLiveRate(m_basebandSampleRate);
    //m_scopeVis->setFreeRun(false); // FIXME: add method rather than call m_scopeVis->configure()

    m_spectrumVis = m_IEEE_802_15_4_Mod->getSpectrumVis();
    m_spectrumVis->setGLSpectrum(ui->glSpectrum);

    ui->glSpectrum->setCenterFrequency(0);
    ui->glSpectrum->setSampleRate(m_settings.m_spectrumRate);
    ui->glSpectrum->setSsbSpectrum(false);
    ui->glSpectrum->setDisplayCurrent(true);
    ui->glSpectrum->setLsbDisplay(false);
    ui->glSpectrum->setDisplayWaterfall(false);
    ui->glSpectrum->setDisplayMaxHold(false);
    ui->glSpectrum->setDisplayHistogram(false);

    CRightClickEnabler *repeatRightClickEnabler = new CRightClickEnabler(ui->repeat);
    connect(repeatRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(repeatSelect()));

    CRightClickEnabler *txRightClickEnabler = new CRightClickEnabler(ui->txButton);
    connect(txRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(txSettingsSelect()));

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::red);
    m_channelMarker.setBandwidth(12500);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("802.15.4 Modulator");
    m_channelMarker.setSourceOrSinkStream(false);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    m_deviceUISet->addChannelMarker(&m_channelMarker);

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));

    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));
    m_IEEE_802_15_4_Mod->setLevelMeter(ui->volumeMeter);

    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);

    ui->spectrumGUI->setBuddies(m_spectrumVis, ui->glSpectrum);

    displaySettings();
    makeUIConnections();
    applySettings();
    DialPopup::addPopupsToChildDials(this);
}

IEEE_802_15_4_ModGUI::~IEEE_802_15_4_ModGUI()
{
    delete ui;
}

void IEEE_802_15_4_ModGUI::transmit()
{
    QString data = ui->frame->text();
    ui->transmittedText->appendPlainText(data);
    IEEE_802_15_4_Mod::MsgTxHexString *msg = IEEE_802_15_4_Mod::MsgTxHexString::create(data);
    m_IEEE_802_15_4_Mod->getInputMessageQueue()->push(msg);
}

void IEEE_802_15_4_ModGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void IEEE_802_15_4_ModGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        IEEE_802_15_4_Mod::MsgConfigureIEEE_802_15_4_Mod *msg = IEEE_802_15_4_Mod::MsgConfigureIEEE_802_15_4_Mod::create(m_settings, force);
        m_IEEE_802_15_4_Mod->getInputMessageQueue()->push(msg);
    }
}

void IEEE_802_15_4_ModGUI::displaySettings()
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
    if ((m_settings.m_bitRate == 20000)
            && m_settings.m_subGHzBand
            && (m_settings.m_pulseShaping == IEEE_802_15_4_ModSettings::RC)
            && (m_settings.m_modulation == IEEE_802_15_4_ModSettings::BPSK))
        ui->phy->setCurrentIndex(0);
    else if ((m_settings.m_bitRate == 40000)
            && m_settings.m_subGHzBand
            && (m_settings.m_pulseShaping == IEEE_802_15_4_ModSettings::RC)
            && (m_settings.m_modulation == IEEE_802_15_4_ModSettings::BPSK))
        ui->phy->setCurrentIndex(1);
    else if ((m_settings.m_bitRate == 100000)
            && m_settings.m_subGHzBand
            && (m_settings.m_modulation == IEEE_802_15_4_ModSettings::OQPSK))
        ui->phy->setCurrentIndex(2);
    else if ((m_settings.m_bitRate == 250000)
            && m_settings.m_subGHzBand
            && (m_settings.m_pulseShaping == IEEE_802_15_4_ModSettings::SINE)
            && (m_settings.m_modulation == IEEE_802_15_4_ModSettings::OQPSK))
        ui->phy->setCurrentIndex(3);
    else if ((m_settings.m_bitRate == 250000)
            && m_settings.m_subGHzBand
            && (m_settings.m_pulseShaping == IEEE_802_15_4_ModSettings::RC)
            && (m_settings.m_modulation == IEEE_802_15_4_ModSettings::OQPSK))
        ui->phy->setCurrentIndex(4);
    else if ((m_settings.m_bitRate == 250000)
            && !m_settings.m_subGHzBand
            && (m_settings.m_pulseShaping == IEEE_802_15_4_ModSettings::SINE)
            && (m_settings.m_modulation == IEEE_802_15_4_ModSettings::OQPSK))
        ui->phy->setCurrentIndex(5);
    else
    {
        ui->phy->removeItem(6);
        ui->phy->addItem(m_settings.getPHY());
        ui->phy->setCurrentIndex(6);
    }
    ui->glSpectrum->setCenterFrequency(0);
    ui->glSpectrum->setSampleRate(m_settings.m_spectrumRate);

    displayRFBandwidth(m_settings.m_rfBandwidth);
    ui->rfBW->setValue(m_settings.m_rfBandwidth / 1000.0);

    displayChipRate(m_settings);

    ui->gainText->setText(QString("%1").arg((double)m_settings.m_gain, 0, 'f', 1));
    ui->gain->setValue(m_settings.m_gain);

    ui->channelMute->setChecked(m_settings.m_channelMute);
    ui->repeat->setChecked(m_settings.m_repeat);

    ui->frame->setText(m_settings.m_data);

    ui->udpEnabled->setChecked(m_settings.m_udpEnabled);
    ui->udpAddress->setText(m_settings.m_udpAddress);
    ui->udpPort->setText(QString::number(m_settings.m_udpPort));

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void IEEE_802_15_4_ModGUI::displayRFBandwidth(int bandwidth)
{
    ui->rfBWText->setText(getDisplayValueWithMultiplier(bandwidth));
}

void IEEE_802_15_4_ModGUI::displayChipRate(const IEEE_802_15_4_ModSettings& settings)
{
    ui->chipRateText->setText(getDisplayValueWithMultiplier(settings.getChipRate()));
}

QString IEEE_802_15_4_ModGUI::getDisplayValueWithMultiplier(int value)
{
    if (value < 1000) {
        return QString("%1").arg(value);
    } else if (value < 10000) {
        return QString("%1k").arg(value / 1000.0, 0, 'f', 2);
    } else if (value < 100000) {
        return QString("%1k").arg(value / 1000.0, 0, 'f', 1);
    } else if (value < 1000000) {
        return QString("%1k").arg(value / 1000.0);
    } else if (value < 10000000) {
        return QString("%1M").arg(value / 1000000.0, 0, 'f', 2);
    } else if (value < 100000000) {
        return QString("%1M").arg(value / 1000000.0, 0, 'f', 1);
    } else {
        return QString("%1M").arg(value / 1000000.0);
    }
}

void IEEE_802_15_4_ModGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void IEEE_802_15_4_ModGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void IEEE_802_15_4_ModGUI::tick()
{
    double powDb = CalcDb::dbPower(m_IEEE_802_15_4_Mod->getMagSq());
    m_channelPowerDbAvg(powDb);
    ui->channelPower->setText(tr("%1 dB").arg(m_channelPowerDbAvg.asDouble(), 0, 'f', 1));
}

void IEEE_802_15_4_ModGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &IEEE_802_15_4_ModGUI::on_deltaFrequency_changed);
    QObject::connect(ui->phy, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &IEEE_802_15_4_ModGUI::on_phy_currentIndexChanged);
    QObject::connect(ui->rfBW, &QSlider::valueChanged, this, &IEEE_802_15_4_ModGUI::on_rfBW_valueChanged);
    QObject::connect(ui->gain, &QDial::valueChanged, this, &IEEE_802_15_4_ModGUI::on_gain_valueChanged);
    QObject::connect(ui->channelMute, &QToolButton::toggled, this, &IEEE_802_15_4_ModGUI::on_channelMute_toggled);
    QObject::connect(ui->txButton, &QToolButton::clicked, this, &IEEE_802_15_4_ModGUI::on_txButton_clicked);
    QObject::connect(ui->frame, &QLineEdit::editingFinished, this, &IEEE_802_15_4_ModGUI::on_frame_editingFinished);
    QObject::connect(ui->frame, &QLineEdit::returnPressed, this, &IEEE_802_15_4_ModGUI::on_frame_returnPressed);
    QObject::connect(ui->repeat, &ButtonSwitch::toggled, this, &IEEE_802_15_4_ModGUI::on_repeat_toggled);
    QObject::connect(ui->udpEnabled, &QCheckBox::clicked, this, &IEEE_802_15_4_ModGUI::on_udpEnabled_clicked);
    QObject::connect(ui->udpAddress, &QLineEdit::editingFinished, this, &IEEE_802_15_4_ModGUI::on_udpAddress_editingFinished);
    QObject::connect(ui->udpPort, &QLineEdit::editingFinished, this, &IEEE_802_15_4_ModGUI::on_udpPort_editingFinished);
}

void IEEE_802_15_4_ModGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
