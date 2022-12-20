///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2022 Jon Beniston, M7RCE                                        //
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

#include "device/deviceuiset.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "dsp/hbfilterchainconverter.h"
#include "dsp/dspcommands.h"
#include "mainwindow.h"

#include "remotetcpsinkgui.h"
#include "remotetcpsink.h"
#include "ui_remotetcpsinkgui.h"

RemoteTCPSinkGUI* RemoteTCPSinkGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *channelRx)
{
    RemoteTCPSinkGUI* gui = new RemoteTCPSinkGUI(pluginAPI, deviceUISet, channelRx);
    return gui;
}

void RemoteTCPSinkGUI::destroy()
{
    delete this;
}

void RemoteTCPSinkGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray RemoteTCPSinkGUI::serialize() const
{
    return m_settings.serialize();
}

bool RemoteTCPSinkGUI::deserialize(const QByteArray& data)
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

QString RemoteTCPSinkGUI::displayScaledF(float value, char type, int precision, bool showMult)
{
    float posValue = (value < 0) ? -value : value;

    if (posValue == 0)
    {
        return tr("%1").arg(QString::number(value, 'f', precision));
    }
    else if (posValue < 1)
    {
        if (posValue > 0.001) {
            return tr("%1%2").arg(QString::number(value * 1000.0, type, precision)).arg(showMult ? "m" : "");
        } else if (posValue > 0.000001) {
            return tr("%1%2").arg(QString::number(value * 1000000.0, type, precision)).arg(showMult ? "u" : "");
        } else if (posValue > 1e-9) {
            return tr("%1%2").arg(QString::number(value * 1e9, type, precision)).arg(showMult ? "n" : "");
        } else if (posValue > 1e-12) {
            return tr("%1%2").arg(QString::number(value * 1e12, type, precision)).arg(showMult ? "p" : "");
        } else {
            return tr("%1").arg(QString::number(value, 'e', precision));
        }
    }
    else
    {
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
}

bool RemoteTCPSinkGUI::handleMessage(const Message& message)
{
    if (RemoteTCPSink::MsgConfigureRemoteTCPSink::match(message))
    {
        const RemoteTCPSink::MsgConfigureRemoteTCPSink& cfg = (RemoteTCPSink::MsgConfigureRemoteTCPSink&) message;
        if ((cfg.getSettings().m_channelSampleRate != m_settings.m_channelSampleRate)
            || (cfg.getSettings().m_sampleBits != m_settings.m_sampleBits)) {
            m_bwAvg.reset();
        }
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);

        return true;
    }
    else if (RemoteTCPSink::MsgReportConnection::match(message))
    {
        const RemoteTCPSink::MsgReportConnection& report = (RemoteTCPSink::MsgReportConnection&) message;
        ui->clients->setText(QString("%1").arg(report.getClients()));
        return true;
    }
    else if (RemoteTCPSink::MsgReportBW::match(message))
    {
        const RemoteTCPSink::MsgReportBW& report = (RemoteTCPSink::MsgReportBW&) message;
        m_bwAvg(report.getBW());
        ui->bw->setText(QString("%1bps").arg(displayScaledF(m_bwAvg.instantAverage(), 'f', 3, true)));
        return true;
    }
    else if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& cfg = (DSPSignalNotification&) message;
        if (cfg.getSampleRate() != m_basebandSampleRate) {
            m_bwAvg.reset();
        }
        m_deviceCenterFrequency = cfg.getCenterFrequency();
        m_basebandSampleRate = cfg.getSampleRate();
        qDebug("RemoteTCPSinkGUI::handleMessage: DSPSignalNotification: m_basebandSampleRate: %d", m_basebandSampleRate);
        displayRateAndShift();
        updateAbsoluteCenterFrequency();

        return true;
    }
    else
    {
        return false;
    }
}

RemoteTCPSinkGUI::RemoteTCPSinkGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *channelrx, QWidget* parent) :
        ChannelGUI(parent),
        ui(new Ui::RemoteTCPSinkGUI),
        m_pluginAPI(pluginAPI),
        m_deviceUISet(deviceUISet),
        m_basebandSampleRate(0),
        m_deviceCenterFrequency(0),
        m_tickCount(0)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/remotetcpsink/readme.md";
    RollupContents *rollupContents = getRollupContents();
    ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
    connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_remoteSink = (RemoteTCPSink*) channelrx;
    m_remoteSink->setMessageQueueToGUI(getInputMessageQueue());
    m_basebandSampleRate = m_remoteSink->getBasebandSampleRate();

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("Remote source");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);

    m_deviceUISet->addChannelMarker(&m_channelMarker);

    ui->channelSampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->channelSampleRate->setValueRange(8, 0, 99999999);
    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));

    displaySettings();
    makeUIConnections();
    applySettings(true);
    DialPopup::addPopupsToChildDials(this);
}

RemoteTCPSinkGUI::~RemoteTCPSinkGUI()
{
    delete ui;
}

void RemoteTCPSinkGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void RemoteTCPSinkGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        setTitleColor(m_channelMarker.getColor());

        RemoteTCPSink::MsgConfigureRemoteTCPSink* message = RemoteTCPSink::MsgConfigureRemoteTCPSink::create(m_settings, force);
        m_remoteSink->getInputMessageQueue()->push(message);
    }
}

void RemoteTCPSinkGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.setBandwidth(m_settings.m_channelSampleRate);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());

    blockApplySettings(true);
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    ui->channelSampleRate->setValue(m_settings.m_channelSampleRate);
    ui->gain->setValue(m_settings.m_gain);
    ui->gainText->setText(tr("%1dB").arg(m_settings.m_gain));
    ui->sampleBits->setCurrentIndex(m_settings.m_sampleBits/8 - 1);
    ui->dataAddress->setText(m_settings.m_dataAddress);
    ui->dataPort->setText(tr("%1").arg(m_settings.m_dataPort));
    ui->protocol->setCurrentIndex((int)m_settings.m_protocol);
    getRollupContents()->restoreState(m_rollupState);
    blockApplySettings(false);
}

void RemoteTCPSinkGUI::displayRateAndShift()
{
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setBandwidth(m_settings.m_channelSampleRate);
}

void RemoteTCPSinkGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void RemoteTCPSinkGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void RemoteTCPSinkGUI::handleSourceMessages()
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

void RemoteTCPSinkGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void RemoteTCPSinkGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_remoteSink->getNumberOfDeviceStreams());
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
        }

        applySettings();
    }

    resetContextMenuType();
}

void RemoteTCPSinkGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void RemoteTCPSinkGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void RemoteTCPSinkGUI::on_deltaFrequency_changed(int index)
{
    m_settings.m_inputFrequencyOffset = index;
    applySettings();
}

void RemoteTCPSinkGUI::on_channelSampleRate_changed(int index)
{
    m_settings.m_channelSampleRate = index;
    m_bwAvg.reset();
    applySettings();
}

void RemoteTCPSinkGUI::on_gain_valueChanged(int value)
{
    m_settings.m_gain = (float)value;
    ui->gainText->setText(tr("%1dB").arg(m_settings.m_gain));
    applySettings();
}

void RemoteTCPSinkGUI::on_sampleBits_currentIndexChanged(int index)
{
    m_settings.m_sampleBits = 8 * (index + 1);
    m_bwAvg.reset();
    applySettings();
}

void RemoteTCPSinkGUI::on_dataAddress_editingFinished()
{
    m_settings.m_dataAddress = ui->dataAddress->text();
    applySettings();
}

void RemoteTCPSinkGUI::on_dataPort_editingFinished()
{
    bool dataOk;
    int dataPort = ui->dataPort->text().toInt(&dataOk);

    if((!dataOk) || (dataPort < 1024) || (dataPort > 65535))
    {
        return;
    }
    else
    {
        m_settings.m_dataPort = dataPort;
    }

    applySettings();
}

void RemoteTCPSinkGUI::on_protocol_currentIndexChanged(int index)
{
    m_settings.m_protocol = (RemoteTCPSinkSettings::Protocol)index;
    applySettings();
}

void RemoteTCPSinkGUI::tick()
{
    if (++m_tickCount == 20) { // once per second
        m_tickCount = 0;
    }
}

void RemoteTCPSinkGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &RemoteTCPSinkGUI::on_deltaFrequency_changed);
    QObject::connect(ui->channelSampleRate, &ValueDial::changed, this, &RemoteTCPSinkGUI::on_channelSampleRate_changed);
    QObject::connect(ui->gain, &QDial::valueChanged, this, &RemoteTCPSinkGUI::on_gain_valueChanged);
    QObject::connect(ui->sampleBits, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RemoteTCPSinkGUI::on_sampleBits_currentIndexChanged);
    QObject::connect(ui->dataAddress, &QLineEdit::editingFinished, this, &RemoteTCPSinkGUI::on_dataAddress_editingFinished);
    QObject::connect(ui->dataPort, &QLineEdit::editingFinished, this, &RemoteTCPSinkGUI::on_dataPort_editingFinished);
    QObject::connect(ui->protocol, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RemoteTCPSinkGUI::on_protocol_currentIndexChanged);
}

void RemoteTCPSinkGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
