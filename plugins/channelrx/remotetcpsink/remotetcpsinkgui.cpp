///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022-2024 Jon Beniston, M7RCE <jon@beniston.com>                //
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

#include <QHostAddress>
#include <QNetworkInterface>
#include <QTableWidgetItem>
#include <QMessageBox>

#include "device/deviceuiset.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "gui/perioddial.h"
#include "dsp/dspcommands.h"
#include "util/db.h"
#include "maincore.h"

#include "remotetcpsinkgui.h"
#include "remotetcpsink.h"
#include "ui_remotetcpsinkgui.h"
#include "remotetcpsinksettingsdialog.h"

const QString RemoteTCPSinkGUI::m_dateTimeFormat = "yyyy.MM.dd hh:mm:ss";

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
    applyAllSettings();
}

QByteArray RemoteTCPSinkGUI::serialize() const
{
    return m_settings.serialize();
}

bool RemoteTCPSinkGUI::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data)) {
        displaySettings();
        applyAllSettings();
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
        if (posValue < 1000.0f) {
            return tr("%1").arg(QString::number(value, type, precision));
        } else if (posValue < 1000000.0f) {
            return tr("%1%2").arg(QString::number(value / 1000.0, type, precision)).arg(showMult ? "k" : "");
        } else if (posValue < 1000000000.0f) {
            return tr("%1%2").arg(QString::number(value / 1000000.0, type, precision)).arg(showMult ? "M" : "");
        } else if (posValue < 1000000000000.0f) {
            return tr("%1%2").arg(QString::number(value / 1000000000.0, type, precision)).arg(showMult ? "G" : "");
        } else {
            return tr("%1").arg(QString::number(value, 'e', precision));
        }
    }
}

void RemoteTCPSinkGUI::resizeTable()
{
    QDateTime dateTime = QDateTime::currentDateTime();
    QString dateTimeString = dateTime.toString(m_dateTimeFormat);
    int row = ui->connections->rowCount();
    ui->connections->setRowCount(row + 1);
    ui->connections->setItem(row, CONNECTIONS_COL_ADDRESS, new QTableWidgetItem("255.255.255.255"));
    ui->connections->setItem(row, CONNECTIONS_COL_PORT, new QTableWidgetItem("65535"));
    ui->connections->setItem(row, CONNECTIONS_COL_CONNECTED, new QTableWidgetItem(dateTimeString));
    ui->connections->setItem(row, CONNECTIONS_COL_DISCONNECTED, new QTableWidgetItem(dateTimeString));
    ui->connections->setItem(row, CONNECTIONS_COL_TIME, new QTableWidgetItem("1000 d"));
    ui->connections->resizeColumnsToContents();
    ui->connections->removeRow(row);
}

void RemoteTCPSinkGUI::addConnection(const QHostAddress& address, int port)
{
    QDateTime dateTime = QDateTime::currentDateTime();

    int row = ui->connections->rowCount();
    ui->connections->setRowCount(row + 1);

    ui->connections->setItem(row, CONNECTIONS_COL_ADDRESS, new QTableWidgetItem(address.toString()));
    ui->connections->setItem(row, CONNECTIONS_COL_PORT, new QTableWidgetItem(QString::number(port)));
    ui->connections->setItem(row, CONNECTIONS_COL_CONNECTED, new QTableWidgetItem(dateTime.toString(m_dateTimeFormat)));
    ui->connections->setItem(row, CONNECTIONS_COL_DISCONNECTED, new QTableWidgetItem(""));
    ui->connections->setItem(row, CONNECTIONS_COL_TIME, new QTableWidgetItem(""));
}

void RemoteTCPSinkGUI::removeConnection(const QHostAddress& address, int port)
{
    QString addressString = address.toString();
    QString portString = QString::number(port);

    for (int row = 0; row < ui->connections->rowCount(); row++)
    {
        if ((ui->connections->item(row, CONNECTIONS_COL_ADDRESS)->text() == addressString)
            && (ui->connections->item(row, CONNECTIONS_COL_PORT)->text() == portString)
            && (ui->connections->item(row, CONNECTIONS_COL_DISCONNECTED)->text().isEmpty()))
        {
            QDateTime connected = QDateTime::fromString(ui->connections->item(row, CONNECTIONS_COL_CONNECTED)->text(), m_dateTimeFormat);
            QDateTime disconnected = QDateTime::currentDateTime();
            QString dateTimeString = disconnected.toString(m_dateTimeFormat);
            QString time;
            int secs = connected.secsTo(disconnected);
            if (secs < 60) {
                time = QString("%1 s").arg(secs);
            } else if (secs < 60 * 60) {
                time = QString("%1 m").arg(secs / 60);
            } else if (secs < 60 * 60 * 24) {
                time = QString("%1 h").arg(secs / 60 / 60);
            } else {
                time = QString("%1 d").arg(secs / 60 / 60 / 24);
            }

            ui->connections->item(row, CONNECTIONS_COL_DISCONNECTED)->setText(dateTimeString);
            ui->connections->item(row, CONNECTIONS_COL_TIME)->setText(time);
            break;
        }
    }
}

bool RemoteTCPSinkGUI::handleMessage(const Message& message)
{
    if (RemoteTCPSink::MsgConfigureRemoteTCPSink::match(message))
    {
        const RemoteTCPSink::MsgConfigureRemoteTCPSink& cfg = (const RemoteTCPSink::MsgConfigureRemoteTCPSink&) message;

        if ((cfg.getSettings().m_channelSampleRate != m_settings.m_channelSampleRate)
            || (cfg.getSettings().m_sampleBits != m_settings.m_sampleBits)) {
            m_bwAvg.reset();
        }
        if (cfg.getForce()) {
            m_settings = cfg.getSettings();
        } else {
            m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
        }
        blockApplySettings(true);
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);

        return true;
    }
    else if (RemoteTCPSink::MsgReportConnection::match(message))
    {
        const RemoteTCPSink::MsgReportConnection& report = (const RemoteTCPSink::MsgReportConnection&) message;

        ui->clients->setText(QString("%1/%2").arg(report.getClients()).arg(m_settings.m_maxClients));
        QString ip = QString("%1:%2").arg(report.getAddress().toString()).arg(report.getPort());
        if (ui->txAddress->findText(ip) == -1) {
            ui->txAddress->addItem(ip);
        }
        addConnection(report.getAddress(), report.getPort());

        return true;
    }
    else if (RemoteTCPSink::MsgReportDisconnect::match(message))
    {
        const RemoteTCPSink::MsgReportDisconnect& report = (const RemoteTCPSink::MsgReportDisconnect&) message;

        ui->clients->setText(QString("%1/%2").arg(report.getClients()).arg(m_settings.m_maxClients));
        QString ip = QString("%1:%2").arg(report.getAddress().toString()).arg(report.getPort());
        int idx = ui->txAddress->findText(ip);
        if (idx != -1) {
            ui->txAddress->removeItem(idx);
        }
        removeConnection(report.getAddress(), report.getPort());

        return true;
    }
    else if (RemoteTCPSink::MsgReportBW::match(message))
    {
        const RemoteTCPSink::MsgReportBW& report = (const RemoteTCPSink::MsgReportBW&) message;

        m_bwAvg(report.getBW());
        m_networkBWAvg(report.getNetworkBW());

        QString text = QString("%1bps").arg(displayScaledF(m_bwAvg.instantAverage(), 'f', 1, true));

        if (!m_settings.m_iqOnly && (report.getBytesUncompressed() > 0))
        {
            float compressionSaving = 1.0f - (report.getBytesCompressed() / (float) report.getBytesUncompressed());
            m_compressionAvg(compressionSaving);

            QString compressionText = QString(" %1%").arg((int) std::round(m_compressionAvg.instantAverage() * 100.0f));
            text.append(compressionText);
        }

        QString networkBWText = QString(" %1bps").arg(displayScaledF(m_networkBWAvg.instantAverage(), 'f', 1, true));
        text.append(networkBWText);

        ui->bw->setText(text);
        return true;
    }
    else if (DSPSignalNotification::match(message))
    {
        const DSPSignalNotification& cfg = (const DSPSignalNotification&) message;
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
    else if (RemoteTCPSink::MsgSendMessage::match(message))
    {
        const RemoteTCPSink::MsgSendMessage& msg = (const RemoteTCPSink::MsgSendMessage&) message;
        QString address = QString("%1:%2").arg(msg.getAddress().toString()).arg(msg.getPort());
        QString callsign = msg.getCallsign();
        QString text = msg.getText();
        bool broadcast = msg.getBroadcast();

        // Display received message in GUI
        ui->messages->addItem(QString("%1/%2> %3").arg(address).arg(callsign).arg(text));
        ui->messages->scrollToBottom();

        // Forward to other clients
        if (broadcast) {
            m_remoteSink->getInputMessageQueue()->push(RemoteTCPSink::MsgSendMessage::create(msg.getAddress(), msg.getPort(), callsign, text, broadcast));
        }

        return true;
    }
    else if (RemoteTCPSink::MsgError::match(message))
    {
        const RemoteTCPSink::MsgError& msg = (const RemoteTCPSink::MsgError&) message;
        QString error = msg.getError();
        QMessageBox::warning(this, "RemoteTCPSink", error, QMessageBox::Ok);
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
	m_tickCount(0),
    m_squelchOpen(false)
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

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("Remote source");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);

    m_deviceUISet->addChannelMarker(&m_channelMarker);

    ui->txAddress->clear();
    ui->txAddress->addItem("All");

    ui->channelSampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->channelSampleRate->setValueRange(8, 0, 99999999);
    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

#ifndef __EMSCRIPTEN__
    // Add all IP addresses
    for (const QHostAddress& address: QNetworkInterface::allAddresses())
    {
        if (address.protocol() == QAbstractSocket::IPv4Protocol && !address.isLoopback()) {
             ui->dataAddress->addItem(address.toString());
        }
    }
#endif

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));

    resizeTable();

    displaySettings();
    makeUIConnections();
    applyAllSettings();
    DialPopup::addPopupsToChildDials(this);
    m_resizer.enableChildMouseTracking();
}

RemoteTCPSinkGUI::~RemoteTCPSinkGUI()
{
    QObject::disconnect(ui->dataAddress->lineEdit(), &QLineEdit::editingFinished, this, &RemoteTCPSinkGUI::on_dataAddress_editingFinished);
    delete ui;
}

void RemoteTCPSinkGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void RemoteTCPSinkGUI::applySetting(const QString& settingsKey)
{
    applySettings({settingsKey});
}

void RemoteTCPSinkGUI::applySettings(const QStringList& settingsKeys, bool force)
{
    if (m_doApplySettings)
    {
        setTitleColor(m_channelMarker.getColor());

        RemoteTCPSink::MsgConfigureRemoteTCPSink* message = RemoteTCPSink::MsgConfigureRemoteTCPSink::create(m_settings, settingsKeys, force);
        m_remoteSink->getInputMessageQueue()->push(message);
    }
}

void RemoteTCPSinkGUI::applyAllSettings()
{
    applySettings(QStringList(), true);
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
    if (ui->dataAddress->findText(m_settings.m_dataAddress) == -1) {
        ui->dataAddress->addItem(m_settings.m_dataAddress);
    }
    ui->dataAddress->setCurrentText(m_settings.m_dataAddress);
    ui->dataPort->setValue(m_settings.m_dataPort);
    ui->protocol->setCurrentIndex((int)m_settings.m_protocol);
    ui->remoteControl->setChecked(m_settings.m_remoteControl);
    ui->squelchEnabled->setChecked(m_settings.m_squelchEnabled);
    displayIQOnly();
    displaySquelch();
    getRollupContents()->restoreState(m_rollupState);
    blockApplySettings(false);
}

void RemoteTCPSinkGUI::displayIQOnly()
{
    ui->messagesLayout->setEnabled(!m_settings.m_iqOnly);
    ui->sendMessage->setEnabled(!m_settings.m_iqOnly);
    ui->txAddress->setEnabled(!m_settings.m_iqOnly);
    ui->txMessage->setEnabled(!m_settings.m_iqOnly);
    ui->messagesContainer->setVisible(!m_settings.m_iqOnly);
}

void RemoteTCPSinkGUI::displaySquelch()
{
    ui->squelch->setValue(m_settings.m_squelch);
    ui->squelchText->setText(QString::number(m_settings.m_squelch));
    ui->squelch->setEnabled(m_settings.m_squelchEnabled);
    ui->squelchText->setEnabled(m_settings.m_squelchEnabled);
    ui->squelchUnits->setEnabled(m_settings.m_squelchEnabled);

    ui->squelchGate->setValue(m_settings.m_squelchGate);
    ui->squelchGate->setEnabled(m_settings.m_squelchEnabled);

    ui->audioMute->setEnabled(m_settings.m_squelchEnabled);
}

void RemoteTCPSinkGUI::displayRateAndShift()
{
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setBandwidth(m_settings.m_channelSampleRate);
    //m_channelMarker.setVisible(m_settings.m_channelSampleRate != m_basebandSampleRate); // Hide marker if it takes up full bandwidth
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
    applySetting("rollupState");
}

void RemoteTCPSinkGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuType::ContextMenuChannelSettings)
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

        QList<QString> settingsKeys({
            "m_rgbColor",
            "title",
            "useReverseAPI",
            "reverseAPIAddress",
            "reverseAPIPort",
            "reverseAPIDeviceIndex",
            "reverseAPIChannelIndex"
        });

        if (m_deviceUISet->m_deviceMIMOEngine)
        {
            settingsKeys.append("streamIndex");
            m_settings.m_streamIndex = dialog.getSelectedStreamIndex();
            m_channelMarker.clearStreamIndexes();
            m_channelMarker.addStreamIndex(m_settings.m_streamIndex);
            updateIndexLabel();
        }

        applySettings(settingsKeys);
    }

    resetContextMenuType();
}

void RemoteTCPSinkGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySetting("inputFrequencyOffset");
}

void RemoteTCPSinkGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void RemoteTCPSinkGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = value;
    applySetting("inputFrequencyOffset");
}

void RemoteTCPSinkGUI::on_channelSampleRate_changed(int value)
{
    m_settings.m_channelSampleRate = value;
    m_bwAvg.reset();
    applySetting("channelSampleRate");
    displayRateAndShift();
}

void RemoteTCPSinkGUI::on_gain_valueChanged(int value)
{
    m_settings.m_gain = (float)value;
    ui->gainText->setText(tr("%1dB").arg(m_settings.m_gain));
    applySetting("gain");
}

void RemoteTCPSinkGUI::on_sampleBits_currentIndexChanged(int index)
{
    m_settings.m_sampleBits = 8 * (index + 1);
    m_bwAvg.reset();
    applySetting("sampleBits");
}

void RemoteTCPSinkGUI::on_dataAddress_editingFinished()
{
    m_settings.m_dataAddress = ui->dataAddress->currentText();
    applySetting("dataAddress");
}

void RemoteTCPSinkGUI::on_dataAddress_currentIndexChanged(int index)
{
    (void) index;

    m_settings.m_dataAddress = ui->dataAddress->currentText();
    applySetting("dataAddress");
}

void RemoteTCPSinkGUI::on_dataPort_valueChanged(int value)
{
    m_settings.m_dataPort = value;
    applySetting("dataPort");
}

void RemoteTCPSinkGUI::on_protocol_currentIndexChanged(int index)
{
    m_settings.m_protocol = (RemoteTCPSinkSettings::Protocol)index;
    applySetting("protocol");
}

void RemoteTCPSinkGUI::on_remoteControl_toggled(bool checked)
{
    m_settings.m_remoteControl = checked;
    applySetting("remoteControl");
}

void RemoteTCPSinkGUI::on_squelchEnabled_toggled(bool checked)
{
    m_settings.m_squelchEnabled = checked;
    applySetting("squelchEnabled");
    displaySquelch();
}

void RemoteTCPSinkGUI::on_squelch_valueChanged(int value)
{
    m_settings.m_squelch = value;
    ui->squelchText->setText(QString::number(m_settings.m_squelch));
    applySetting("squelch");
}

void RemoteTCPSinkGUI::on_squelchGate_valueChanged(double value)
{
    m_settings.m_squelchGate = value;
    applySetting("squelchGate");
}

void RemoteTCPSinkGUI::on_displaySettings_clicked()
{
    RemoteTCPSinkSettingsDialog dialog(&m_settings);

    new DialogPositioner(&dialog, true);
    if (dialog.exec() == QDialog::Accepted)
    {
        applySettings(dialog.getSettingsKeys());
        displayIQOnly();
    }
}

void RemoteTCPSinkGUI::on_sendMessage_clicked()
{
    QString message = ui->txMessage->text().trimmed();
    if (!message.isEmpty())
    {
        ui->messages->addItem(QString("< %1").arg(message));
        ui->messages->scrollToBottom();
        bool broadcast = ui->txAddress->currentText() == "All";
        QHostAddress address;
        quint16 port = 0;
        if (!broadcast)
        {
            QStringList parts = ui->txAddress->currentText().split(':');
            address = QHostAddress(parts[0]);
            port = parts[1].toInt();
        }
        QString callsign = MainCore::instance()->getSettings().getStationName();
        m_remoteSink->getInputMessageQueue()->push(RemoteTCPSink::MsgSendMessage::create(address, port, callsign, message, broadcast));
    }
}

void RemoteTCPSinkGUI::on_txMessage_returnPressed()
{
    on_sendMessage_clicked();
    ui->txMessage->selectAll();
}

void RemoteTCPSinkGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_remoteSink->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    double powDbAvg = CalcDb::dbPower(magsqAvg);
    double powDbPeak = CalcDb::dbPower(magsqPeak);

    ui->channelPowerMeter->levelChanged(
            (100.0f + powDbAvg) / 100.0f,
            (100.0f + powDbPeak) / 100.0f,
            nbMagsqSamples);

    if (m_tickCount % 4 == 0) {
        ui->channelPower->setText(tr("%1").arg(powDbAvg, 0, 'f', 1));
    }

    bool squelchOpen = m_remoteSink->getSquelchOpen() || !m_settings.m_squelchEnabled;

	if (squelchOpen != m_squelchOpen)
	{
        /*if (squelchOpen) {
			ui->audioMute->setStyleSheet("QToolButton { background-color : green; }");
		} else {
			ui->audioMute->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
		}*/
        ui->audioMute->setChecked(!squelchOpen);
        m_squelchOpen = squelchOpen;
	}

	m_tickCount++;
}

void RemoteTCPSinkGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &RemoteTCPSinkGUI::on_deltaFrequency_changed);
    QObject::connect(ui->channelSampleRate, &ValueDial::changed, this, &RemoteTCPSinkGUI::on_channelSampleRate_changed);
    QObject::connect(ui->gain, &QDial::valueChanged, this, &RemoteTCPSinkGUI::on_gain_valueChanged);
    QObject::connect(ui->sampleBits, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RemoteTCPSinkGUI::on_sampleBits_currentIndexChanged);
    QObject::connect(ui->dataAddress->lineEdit(), &QLineEdit::editingFinished, this, &RemoteTCPSinkGUI::on_dataAddress_editingFinished);
    QObject::connect(ui->dataAddress, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RemoteTCPSinkGUI::on_dataAddress_currentIndexChanged);
    QObject::connect(ui->dataPort, QOverload<int>::of(&QSpinBox::valueChanged), this, &RemoteTCPSinkGUI::on_dataPort_valueChanged);
    QObject::connect(ui->protocol, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RemoteTCPSinkGUI::on_protocol_currentIndexChanged);
    QObject::connect(ui->remoteControl, &ButtonSwitch::toggled, this, &RemoteTCPSinkGUI::on_remoteControl_toggled);
    QObject::connect(ui->squelchEnabled, &ButtonSwitch::toggled, this, &RemoteTCPSinkGUI::on_squelchEnabled_toggled);
    QObject::connect(ui->squelch, &QDial::valueChanged, this, &RemoteTCPSinkGUI::on_squelch_valueChanged);
    QObject::connect(ui->squelchGate, &PeriodDial::valueChanged, this, &RemoteTCPSinkGUI::on_squelchGate_valueChanged);
    QObject::connect(ui->displaySettings, &QToolButton::clicked, this, &RemoteTCPSinkGUI::on_displaySettings_clicked);
    QObject::connect(ui->sendMessage, &QToolButton::clicked, this, &RemoteTCPSinkGUI::on_sendMessage_clicked);
    QObject::connect(ui->txMessage, &QLineEdit::returnPressed, this, &RemoteTCPSinkGUI::on_txMessage_returnPressed);
}

void RemoteTCPSinkGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
