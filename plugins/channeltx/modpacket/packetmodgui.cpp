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

#include "dsp/spectrumvis.h"
#include "device/deviceuiset.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "dsp/dspengine.h"
#include "gui/glspectrum.h"
#include "gui/crightclickenabler.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "gui/fmpreemphasisdialog.h"
#include "mainwindow.h"

#include "ui_packetmodgui.h"
#include "packetmodgui.h"
#include "packetmodrepeatdialog.h"
#include "packetmodtxsettingsdialog.h"
#include "packetmodbpfdialog.h"


PacketModGUI* PacketModGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx)
{
    PacketModGUI* gui = new PacketModGUI(pluginAPI, deviceUISet, channelTx);
    return gui;
}

void PacketModGUI::destroy()
{
    delete this;
}

void PacketModGUI::setName(const QString& name)
{
    setObjectName(name);
}

QString PacketModGUI::getName() const
{
    return objectName();
}

qint64 PacketModGUI::getCenterFrequency() const {
    return m_channelMarker.getCenterFrequency();
}

void PacketModGUI::setCenterFrequency(qint64 centerFrequency)
{
    m_channelMarker.setCenterFrequency(centerFrequency);
    applySettings();
}

void PacketModGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray PacketModGUI::serialize() const
{
    return m_settings.serialize();
}

bool PacketModGUI::deserialize(const QByteArray& data)
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

bool PacketModGUI::handleMessage(const Message& message)
{
    if (PacketMod::MsgConfigurePacketMod::match(message))
    {
        const PacketMod::MsgConfigurePacketMod& cfg = (PacketMod::MsgConfigurePacketMod&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else
    {
        return false;
    }
}

void PacketModGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void PacketModGUI::handleSourceMessages()
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

void PacketModGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void PacketModGUI::on_mode_currentIndexChanged(int value)
{
    QString mode = ui->mode->currentText();

    // If m_doApplySettings is set, we are here from a call to displaySettings,
    // so we only want to display the current settings, not update them
    // as though a user had selected a new mode
    if (m_doApplySettings)
        m_settings.setMode(mode);

    ui->rfBWText->setText(QString("%1k").arg(m_settings.m_rfBandwidth / 1000.0, 0, 'f', 1));
    ui->fmDevText->setText(QString("%1k").arg(m_settings.m_fmDeviation / 1000.0, 0, 'f', 1));
    ui->fmDev->setValue(m_settings.m_fmDeviation / 100.0);
    ui->glSpectrum->setCenterFrequency(m_settings.m_spectrumRate/4);
    ui->glSpectrum->setSampleRate(m_settings.m_spectrumRate/2);
    applySettings();

    // Remove custom mode when deselected, as we no longer know how to set it
    if (value < 2)
        ui->mode->removeItem(2);
}

void PacketModGUI::on_rfBW_valueChanged(int value)
{
    float bw = value * 100.0f;
    ui->rfBWText->setText(QString("%1k").arg(value / 10.0, 0, 'f', 1));
    m_channelMarker.setBandwidth(bw);
    m_settings.m_rfBandwidth = bw;
    applySettings();
}

void PacketModGUI::on_fmDev_valueChanged(int value)
{
    ui->fmDevText->setText(QString("%1k").arg(value / 10.0, 0, 'f', 1));
    m_settings.m_fmDeviation = value * 100.0;
    applySettings();
}

void PacketModGUI::on_gain_valueChanged(int value)
{
    ui->gainText->setText(QString("%1dB").arg(value));
    m_settings.m_gain = value;
    applySettings();
}

void PacketModGUI::on_channelMute_toggled(bool checked)
{
    m_settings.m_channelMute = checked;
    applySettings();
}

void PacketModGUI::on_txButton_clicked(bool checked)
{
    transmit();
}

void PacketModGUI::on_packet_returnPressed()
{
    transmit();
}

void PacketModGUI::on_callsign_editingFinished()
{
    m_settings.m_callsign = ui->callsign->text();
    applySettings();
}

void PacketModGUI::on_to_currentTextChanged(const QString &text)
{
    m_settings.m_to = text;
    applySettings();
}

void PacketModGUI::on_via_currentTextChanged(const QString &text)
{
    m_settings.m_via = text;
    applySettings();
}

void PacketModGUI::on_packet_editingFinished()
{
    m_settings.m_data = ui->packet->text();
    applySettings();
}

void PacketModGUI::on_insertPosition_clicked(bool checked)
{
    float latitude = MainWindow::getInstance()->getMainSettings().getLatitude();
    float longitude = MainWindow::getInstance()->getMainSettings().getLongitude();

    int latDeg, latMin, latFrac, latNorth;
    int longDeg, longMin, longFrac, longEast;
    char latBuf[40];
    char longBuf[40];

    // Convert decimal latitude to degrees, min and hundreths of a minute
    latNorth = latitude >= 0.0f;
    latitude = abs(latitude);
    latDeg = (int)latitude;
    latitude -= (float)latDeg;
    latitude *= 60.0f;
    latMin = (int)latitude;
    latitude -= (float)latMin;
    latitude *= 100.0f;
    latFrac = round(latitude);

    // Convert decimal longitude
    longEast = longitude >= 0.0f;
    longitude = abs(longitude);
    longDeg = (int)longitude;
    longitude -= (float)longDeg;
    longitude *= 60.0f;
    longMin = (int)longitude;
    longitude -= (float)longMin;
    longitude *= 100.0f;
    longFrac = round(longitude);

    // Insert position with house symbol (-) in to data field
    sprintf(latBuf, "%02d%02d.%02d%c", latDeg, latMin, latFrac, latNorth ? 'N' : 'S');
    sprintf(longBuf, "%03d%02d.%02d%c", longDeg, longMin, longFrac, longEast ? 'E' : 'W');
    QString packet = QString("%1/%2-").arg(latBuf).arg(longBuf);
    ui->packet->insert(packet);
}

void PacketModGUI::on_repeat_toggled(bool checked)
{
    m_settings.m_repeat = checked;
    applySettings();
}

void PacketModGUI::on_preEmphasis_toggled(bool checked)
{
    m_settings.m_preEmphasis = checked;
    applySettings();
}

void PacketModGUI::on_bpf_toggled(bool checked)
{
    m_settings.m_bpf = checked;
    applySettings();
}

void PacketModGUI::preEmphasisSelect()
{
    FMPreemphasisDialog dialog(m_settings.m_preEmphasisTau, m_settings.m_preEmphasisHighFreq);
    if (dialog.exec() == QDialog::Accepted)
    {
        m_settings.m_preEmphasisTau = dialog.m_tau;
        m_settings.m_preEmphasisHighFreq = dialog.m_highFreq;
        applySettings();
    }
}

void PacketModGUI::bpfSelect()
{
    PacketModBPFDialog dialog(m_settings.m_bpfLowCutoff, m_settings.m_bpfHighCutoff, m_settings.m_bpfTaps);
    if (dialog.exec() == QDialog::Accepted)
    {
        m_settings.m_bpfLowCutoff = dialog.m_lowFreq;
        m_settings.m_bpfHighCutoff = dialog.m_highFreq;
        m_settings.m_bpfTaps = dialog.m_taps;
        applySettings();
    }
}

void PacketModGUI::repeatSelect()
{
    PacketModRepeatDialog dialog(m_settings.m_repeatDelay, m_settings.m_repeatCount);
    if (dialog.exec() == QDialog::Accepted)
    {
        m_settings.m_repeatDelay = dialog.m_repeatDelay;
        m_settings.m_repeatCount = dialog.m_repeatCount;
        applySettings();
    }
}

void PacketModGUI::txSettingsSelect()
{
    PacketModTXSettingsDialog dialog(m_settings.m_rampUpBits, m_settings.m_rampDownBits,
                                        m_settings.m_rampRange, m_settings.m_modulateWhileRamping,
                                        m_settings.m_modulation, m_settings.m_baud,
                                        m_settings.m_markFrequency, m_settings.m_spaceFrequency,
                                        m_settings.m_pulseShaping, m_settings.m_beta, m_settings.m_symbolSpan,
                                        m_settings.m_scramble, m_settings.m_polynomial,
                                        m_settings.m_ax25PreFlags, m_settings.m_ax25PostFlags,
                                        m_settings.m_ax25Control, m_settings.m_ax25PID,
                                        m_settings.m_lpfTaps,
                                        m_settings.m_bbNoise, m_settings.m_rfNoise,
                                        m_settings.m_writeToFile);
    if (dialog.exec() == QDialog::Accepted)
    {
        m_settings.m_rampUpBits = dialog.m_rampUpBits;
        m_settings.m_rampDownBits = dialog.m_rampDownBits;
        m_settings.m_rampRange = dialog.m_rampRange;
        m_settings.m_modulateWhileRamping = dialog.m_modulateWhileRamping;
        m_settings.m_modulation = static_cast<PacketModSettings::Modulation>(dialog.m_modulation);
        m_settings.m_baud = dialog.m_baud;
        m_settings.m_markFrequency = dialog.m_markFrequency;
        m_settings.m_spaceFrequency = dialog.m_spaceFrequency;
        m_settings.m_pulseShaping = dialog.m_pulseShaping;
        m_settings.m_beta = dialog.m_beta;
        m_settings.m_symbolSpan = dialog.m_symbolSpan;
        m_settings.m_scramble = dialog.m_scramble;
        m_settings.m_polynomial = dialog.m_polynomial;
        m_settings.m_ax25PreFlags = dialog.m_ax25PreFlags;
        m_settings.m_ax25PostFlags = dialog.m_ax25PostFlags;
        m_settings.m_ax25Control = dialog.m_ax25Control;
        m_settings.m_ax25PID = dialog.m_ax25PID;
        m_settings.m_lpfTaps = dialog.m_lpfTaps;
        m_settings.m_bbNoise = dialog.m_bbNoise;
        m_settings.m_rfNoise = dialog.m_rfNoise;
        m_settings.m_writeToFile = dialog.m_writeToFile;
        displaySettings();
        applySettings();
    }
}

void PacketModGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;
}

void PacketModGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuChannelSettings)
    {
        BasicChannelSettingsDialog dialog(&m_channelMarker, this);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIDeviceIndex(m_settings.m_reverseAPIDeviceIndex);
        dialog.setReverseAPIChannelIndex(m_settings.m_reverseAPIChannelIndex);
        dialog.move(p);
        dialog.exec();

        m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
        m_settings.m_rgbColor = m_channelMarker.getColor().rgb();
        m_settings.m_title = m_channelMarker.getTitle();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIDeviceIndex = dialog.getReverseAPIDeviceIndex();
        m_settings.m_reverseAPIChannelIndex = dialog.getReverseAPIChannelIndex();

        setWindowTitle(m_settings.m_title);
        setTitleColor(m_settings.m_rgbColor);

        applySettings();
    }
    else if ((m_contextMenuType == ContextMenuStreamSettings) && (m_deviceUISet->m_deviceMIMOEngine))
    {
        DeviceStreamSelectionDialog dialog(this);
        dialog.setNumberOfStreams(m_packetMod->getNumberOfDeviceStreams());
        dialog.setStreamIndex(m_settings.m_streamIndex);
        dialog.move(p);
        dialog.exec();

        m_settings.m_streamIndex = dialog.getSelectedStreamIndex();
        m_channelMarker.clearStreamIndexes();
        m_channelMarker.addStreamIndex(m_settings.m_streamIndex);
        displayStreamIndex();
        applySettings();
    }

    resetContextMenuType();
}

PacketModGUI::PacketModGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent) :
    RollupWidget(parent),
    ui(new Ui::PacketModGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_doApplySettings(true)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose, true);

    connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_packetMod = (PacketMod*) channelTx;
    m_packetMod->setMessageQueueToGUI(getInputMessageQueue());

    connect(&MainWindow::getInstance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    m_spectrumVis = m_packetMod->getSpectrumVis();
    m_spectrumVis->setGLSpectrum(ui->glSpectrum);

    // Extra /2 here because SSB?
    ui->glSpectrum->setCenterFrequency(m_settings.m_spectrumRate/4);
    ui->glSpectrum->setSampleRate(m_settings.m_spectrumRate/2);
    ui->glSpectrum->setSsbSpectrum(true);
    ui->glSpectrum->setDisplayCurrent(true);
    ui->glSpectrum->setLsbDisplay(false);
    ui->glSpectrum->setDisplayWaterfall(false);
    ui->glSpectrum->setDisplayMaxHold(false);
    ui->glSpectrum->setDisplayHistogram(false);
    ui->glSpectrum->connectTimer(MainWindow::getInstance()->getMasterTimer());

    CRightClickEnabler *repeatRightClickEnabler = new CRightClickEnabler(ui->repeat);
    connect(repeatRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(repeatSelect()));

    CRightClickEnabler *txRightClickEnabler = new CRightClickEnabler(ui->txButton);
    connect(txRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(txSettingsSelect()));

    CRightClickEnabler *preempRightClickEnabler = new CRightClickEnabler(ui->preEmphasis);
    connect(preempRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(preEmphasisSelect()));

    CRightClickEnabler *bpfRightClickEnabler = new CRightClickEnabler(ui->bpf);
    connect(bpfRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(bpfSelect()));

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::red);
    m_channelMarker.setBandwidth(12500);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("Packet Modulator");
    m_channelMarker.setSourceOrSinkStream(false);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    m_deviceUISet->registerTxChannelInstance(PacketMod::m_channelIdURI, this);
    m_deviceUISet->addChannelMarker(&m_channelMarker);
    m_deviceUISet->addRollupWidget(this);

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));

    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));
    m_packetMod->setLevelMeter(ui->volumeMeter);

    m_settings.setChannelMarker(&m_channelMarker);

    ui->spectrumGUI->setBuddies(m_spectrumVis, ui->glSpectrum);

    displaySettings();
    applySettings();
}

PacketModGUI::~PacketModGUI()
{
    m_deviceUISet->removeTxChannelInstance(this);
    delete m_packetMod; // TODO: check this: when the GUI closes it has to delete the modulator
    delete ui;
}

void PacketModGUI::transmit()
{
    QString callsign = ui->callsign->text();
    QString to = ui->to->currentText();
    QString via = ui->via->currentText();
    QString data = ui->packet->text();
    // TODO: Any validation?
    QString str = callsign + ">" + to + "," + via + ":" + data;
    ui->transmittedText->appendPlainText(str + "\n");
    PacketMod::MsgTXPacketMod *msg = PacketMod::MsgTXPacketMod::create(callsign, to, via, data);
    m_packetMod->getInputMessageQueue()->push(msg);
}

void PacketModGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void PacketModGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        PacketMod::MsgConfigurePacketMod *msg = PacketMod::MsgConfigurePacketMod::create(m_settings, force);
        m_packetMod->getInputMessageQueue()->push(msg);
    }
}

void PacketModGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    displayStreamIndex();

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    if ((m_settings.m_baud == 1200) && (m_settings.m_modulation == PacketModSettings::AFSK))
        ui->mode->setCurrentIndex(0);
    else if ((m_settings.m_baud == 9600) && (m_settings.m_modulation == PacketModSettings::FSK))
        ui->mode->setCurrentIndex(1);
    else
    {
        ui->mode->removeItem(2);
        ui->mode->addItem(m_settings.getMode());
        ui->mode->setCurrentIndex(2);
    }
    ui->glSpectrum->setCenterFrequency(m_settings.m_spectrumRate/4);
    ui->glSpectrum->setSampleRate(m_settings.m_spectrumRate/2);

    ui->rfBWText->setText(QString("%1k").arg(m_settings.m_rfBandwidth / 1000.0, 0, 'f', 1));
    ui->rfBW->setValue(m_settings.m_rfBandwidth / 100.0);

    ui->fmDevText->setText(QString("%1k").arg(m_settings.m_fmDeviation / 1000.0, 0, 'f', 1));
    ui->fmDev->setValue(m_settings.m_fmDeviation / 100.0);

    ui->gainText->setText(QString("%1").arg((double)m_settings.m_gain, 0, 'f', 1));
    ui->gain->setValue(m_settings.m_gain);

    ui->channelMute->setChecked(m_settings.m_channelMute);
    ui->repeat->setChecked(m_settings.m_repeat);

    ui->callsign->setText(m_settings.m_callsign);
    ui->to->lineEdit()->setText(m_settings.m_to);
    ui->via->lineEdit()->setText(m_settings.m_via);
    ui->packet->setText(m_settings.m_data);

    blockApplySettings(false);
}

void PacketModGUI::displayStreamIndex()
{
    if (m_deviceUISet->m_deviceMIMOEngine) {
        setStreamIndicator(tr("%1").arg(m_settings.m_streamIndex));
    } else {
        setStreamIndicator("S"); // single channel indicator
    }
}

void PacketModGUI::leaveEvent(QEvent*)
{
    m_channelMarker.setHighlighted(false);
}

void PacketModGUI::enterEvent(QEvent*)
{
    m_channelMarker.setHighlighted(true);
}

void PacketModGUI::tick()
{
    double powDb = CalcDb::dbPower(m_packetMod->getMagSq());
    m_channelPowerDbAvg(powDb);
    ui->channelPower->setText(tr("%1 dB").arg(m_channelPowerDbAvg.asDouble(), 0, 'f', 1));
}
