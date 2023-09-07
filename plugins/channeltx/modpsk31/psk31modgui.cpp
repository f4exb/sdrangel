///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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
#include <QScrollBar>
#include <QDebug>

#include "dsp/spectrumvis.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "device/deviceuiset.h"
#include "plugin/pluginapi.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "util/rtty.h"
#include "util/maidenhead.h"
#include "gui/glspectrum.h"
#include "gui/crightclickenabler.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "gui/fmpreemphasisdialog.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "maincore.h"

#include "ui_psk31modgui.h"
#include "psk31modgui.h"
#include "psk31modrepeatdialog.h"
#include "psk31modtxsettingsdialog.h"

PSK31GUI* PSK31GUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx)
{
    PSK31GUI* gui = new PSK31GUI(pluginAPI, deviceUISet, channelTx);
    return gui;
}

void PSK31GUI::destroy()
{
    delete this;
}

void PSK31GUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray PSK31GUI::serialize() const
{
    return m_settings.serialize();
}

bool PSK31GUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data)) {
        displaySettings();
        applySettings(true);
        return true;
    } else {
        resetToDefaults();
        return false;
    }
}

bool PSK31GUI::handleMessage(const Message& message)
{
    if (PSK31::MsgConfigurePSK31::match(message))
    {
        const PSK31::MsgConfigurePSK31& cfg = (PSK31::MsgConfigurePSK31&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (PSK31::MsgReportTx::match(message))
    {
        const PSK31::MsgReportTx& report = (PSK31::MsgReportTx&)message;
        QString s = report.getText();
        int bufferedCharacters = report.getBufferedCharacters();

        // Turn TX button green when transmitting
        QString tooltip = m_initialToolTip;
        if (bufferedCharacters == 0)
        {
            ui->txButton->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
        }
        else
        {
            ui->txButton->setStyleSheet("QToolButton { background-color : green; }");
            tooltip.append(QString("\n\n%1 characters in buffer").arg(bufferedCharacters));
        }
        ui->txButton->setToolTip(tooltip);

        s = s.replace("\r", ""); // Don't display carriage returns

        if (!s.isEmpty())
        {
            // Is the scroll bar at the bottom?
            int scrollPos = ui->transmittedText->verticalScrollBar()->value();
            bool atBottom = scrollPos >= ui->transmittedText->verticalScrollBar()->maximum();

            // Move cursor to end where we want to append new text
            // (user may have moved it by clicking / highlighting text)
            ui->transmittedText->moveCursor(QTextCursor::End);

            // Restore scroll position
            ui->transmittedText->verticalScrollBar()->setValue(scrollPos);

            // Insert text
            ui->transmittedText->insertPlainText(s);

            // Scroll to bottom, if we we're previously at the bottom
            if (atBottom) {
                ui->transmittedText->verticalScrollBar()->setValue(ui->transmittedText->verticalScrollBar()->maximum());
            }
        }
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

void PSK31GUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void PSK31GUI::handleSourceMessages()
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

void PSK31GUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings();
}

void PSK31GUI::on_rfBW_valueChanged(int value)
{
    int bw = value;
    ui->rfBWText->setText(QString("%1 Hz").arg(bw));
    m_channelMarker.setBandwidth(bw);
    m_settings.m_rfBandwidth = bw;
    applySettings();
}

void PSK31GUI::on_clearTransmittedText_clicked()
{
    ui->transmittedText->clear();
}

void PSK31GUI::on_gain_valueChanged(int value)
{
    ui->gainText->setText(QString("%1dB").arg(value));
    m_settings.m_gain = value;
    applySettings();
}

void PSK31GUI::on_channelMute_toggled(bool checked)
{
    m_settings.m_channelMute = checked;
    applySettings();
}

void PSK31GUI::on_txButton_clicked()
{
    transmit(ui->text->currentText());
}

void PSK31GUI::on_text_returnPressed()
{
    transmit(ui->text->currentText());
    ui->text->setCurrentText("");
}

void PSK31GUI::on_text_editingFinished()
{
    m_settings.m_text = ui->text->currentText();
    applySettings();
}

void PSK31GUI::on_repeat_toggled(bool checked)
{
    m_settings.m_repeat = checked;
    applySettings();
}

void PSK31GUI::repeatSelect(const QPoint& p)
{
    PSK31RepeatDialog dialog(m_settings.m_repeatCount);
    dialog.move(p);
    new DialogPositioner(&dialog, false);

    if (dialog.exec() == QDialog::Accepted)
    {
        m_settings.m_repeatCount = dialog.m_repeatCount;
        applySettings();
    }
}

void PSK31GUI::txSettingsSelect(const QPoint& p)
{
    PSK31TXSettingsDialog dialog(&m_settings);
    dialog.move(p);
    new DialogPositioner(&dialog, false);

    if (dialog.exec() == QDialog::Accepted)
    {
        displaySettings();
        applySettings();
    }
}

void PSK31GUI::on_udpEnabled_clicked(bool checked)
{
    m_settings.m_udpEnabled = checked;
    applySettings();
}

void PSK31GUI::on_udpAddress_editingFinished()
{
    m_settings.m_udpAddress = ui->udpAddress->text();
    applySettings();
}

void PSK31GUI::on_udpPort_editingFinished()
{
    m_settings.m_udpPort = ui->udpPort->text().toInt();
    applySettings();
}

void PSK31GUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void PSK31GUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_psk31Mod->getNumberOfDeviceStreams());
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

PSK31GUI::PSK31GUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::PSK31GUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_basebandSampleRate(1),
    m_doApplySettings(true)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channeltx/modpsk31/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_psk31Mod = (PSK31*) channelTx;
    m_psk31Mod->setMessageQueueToGUI(getInputMessageQueue());

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    m_spectrumVis = m_psk31Mod->getSpectrumVis();
    m_spectrumVis->setGLSpectrum(ui->glSpectrum);

    ui->spectrumGUI->setBuddies(m_spectrumVis, ui->glSpectrum);

    ui->glSpectrum->setCenterFrequency(0);
    ui->glSpectrum->setSampleRate(2000);
    ui->glSpectrum->setLsbDisplay(false);

    SpectrumSettings spectrumSettings = m_spectrumVis->getSettings();
    spectrumSettings.m_ssb = false;
    spectrumSettings.m_displayCurrent = true;
    spectrumSettings.m_displayWaterfall = false;
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
    m_channelMarker.setTitle("PSK31 Modulator");
    m_channelMarker.setSourceOrSinkStream(false);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    m_deviceUISet->addChannelMarker(&m_channelMarker);

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));

    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));
    m_psk31Mod->setLevelMeter(ui->volumeMeter);

    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);

    ui->transmittedText->addAcronyms(Rtty::m_acronyms);

    ui->spectrumContainer->setVisible(false);

    displaySettings();
    makeUIConnections();
    applySettings();
    DialPopup::addPopupsToChildDials(this);

    m_initialToolTip = ui->txButton->toolTip();
}

PSK31GUI::~PSK31GUI()
{
    // If we don't disconnect, we can get this signal after this has been deleted!
    QObject::disconnect(ui->text->lineEdit(), &QLineEdit::editingFinished, this, &PSK31GUI::on_text_editingFinished);
    delete ui;
}

void PSK31GUI::transmit(const QString& text)
{
    PSK31::MsgTXText*msg = PSK31::MsgTXText::create(text);
    m_psk31Mod->getInputMessageQueue()->push(msg);
}

void PSK31GUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void PSK31GUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        PSK31::MsgConfigurePSK31 *msg = PSK31::MsgConfigurePSK31::create(m_settings, force);
        m_psk31Mod->getInputMessageQueue()->push(msg);
    }
}

QString PSK31GUI::substitute(const QString& text)
{
    const MainSettings& mainSettings = MainCore::instance()->getSettings();
    QString location = Maidenhead::toMaidenhead(mainSettings.getLatitude(), mainSettings.getLongitude());
    QString s = text;

    s = s.replace("${callsign}", mainSettings.getStationName().toUpper());
    s = s.replace("${location}", location);

    return s;
}

void PSK31GUI::displaySettings()
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

    ui->rfBWText->setText(QString("%1 Hz").arg(m_settings.m_rfBandwidth));
    ui->rfBW->setValue(m_settings.m_rfBandwidth);

    ui->udpEnabled->setChecked(m_settings.m_udpEnabled);
    ui->udpAddress->setText(m_settings.m_udpAddress);
    ui->udpPort->setText(QString::number(m_settings.m_udpPort));

    ui->gainText->setText(QString("%1dB").arg((double)m_settings.m_gain, 0, 'f', 1));
    ui->gain->setValue(m_settings.m_gain);

    ui->channelMute->setChecked(m_settings.m_channelMute);
    ui->repeat->setChecked(m_settings.m_repeat);

    ui->text->clear();
    for (const auto& text : m_settings.m_predefinedTexts) {
        ui->text->addItem(substitute(text));
    }
    ui->text->setCurrentText(m_settings.m_text);

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void PSK31GUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void PSK31GUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void PSK31GUI::tick()
{
    double powDb = CalcDb::dbPower(m_psk31Mod->getMagSq());
    m_channelPowerDbAvg(powDb);
    ui->channelPower->setText(tr("%1 dB").arg(m_channelPowerDbAvg.asDouble(), 0, 'f', 1));
}

void PSK31GUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &PSK31GUI::on_deltaFrequency_changed);
    QObject::connect(ui->rfBW, &QSlider::valueChanged, this, &PSK31GUI::on_rfBW_valueChanged);
    QObject::connect(ui->clearTransmittedText, &QToolButton::clicked, this, &PSK31GUI::on_clearTransmittedText_clicked);
    QObject::connect(ui->gain, &QDial::valueChanged, this, &PSK31GUI::on_gain_valueChanged);
    QObject::connect(ui->channelMute, &QToolButton::toggled, this, &PSK31GUI::on_channelMute_toggled);
    QObject::connect(ui->txButton, &QToolButton::clicked, this, &PSK31GUI::on_txButton_clicked);
    QObject::connect(ui->text->lineEdit(), &QLineEdit::editingFinished, this, &PSK31GUI::on_text_editingFinished);
    QObject::connect(ui->text->lineEdit(), &QLineEdit::returnPressed, this, &PSK31GUI::on_text_returnPressed);
    QObject::connect(ui->repeat, &ButtonSwitch::toggled, this, &PSK31GUI::on_repeat_toggled);
    QObject::connect(ui->udpEnabled, &QCheckBox::clicked, this, &PSK31GUI::on_udpEnabled_clicked);
    QObject::connect(ui->udpAddress, &QLineEdit::editingFinished, this, &PSK31GUI::on_udpAddress_editingFinished);
    QObject::connect(ui->udpPort, &QLineEdit::editingFinished, this, &PSK31GUI::on_udpPort_editingFinished);
}

void PSK31GUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
