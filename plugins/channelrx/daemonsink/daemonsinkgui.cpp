///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include "device/devicesourceapi.h"
#include "device/deviceuiset.h"
#include "gui/basicchannelsettingsdialog.h"
#include "mainwindow.h"

#include "daemonsink.h"
#include "ui_daemonsinkgui.h"
#include "daemonsinkgui.h"

DaemonSinkGUI* DaemonSinkGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *channelRx)
{
    DaemonSinkGUI* gui = new DaemonSinkGUI(pluginAPI, deviceUISet, channelRx);
    return gui;
}

void DaemonSinkGUI::destroy()
{
    delete this;
}

void DaemonSinkGUI::setName(const QString& name)
{
    setObjectName(name);
}

QString DaemonSinkGUI::getName() const
{
    return objectName();
}

qint64 DaemonSinkGUI::getCenterFrequency() const {
    return 0;
}

void DaemonSinkGUI::setCenterFrequency(qint64 centerFrequency __attribute__((unused)))
{
}

void DaemonSinkGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray DaemonSinkGUI::serialize() const
{
    return m_settings.serialize();
}

bool DaemonSinkGUI::deserialize(const QByteArray& data)
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

bool DaemonSinkGUI::handleMessage(const Message& message)
{
    if (DaemonSink::MsgSampleRateNotification::match(message))
    {
        DaemonSink::MsgSampleRateNotification& notif = (DaemonSink::MsgSampleRateNotification&) message;
        m_channelMarker.setBandwidth(notif.getSampleRate());
        m_sampleRate = notif.getSampleRate();
        updateTxDelayTime();
        return true;
    }
    else if (DaemonSink::MsgConfigureDaemonSink::match(message))
    {
        const DaemonSink::MsgConfigureDaemonSink& cfg = (DaemonSink::MsgConfigureDaemonSink&) message;
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

DaemonSinkGUI::DaemonSinkGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *channelrx, QWidget* parent) :
        RollupWidget(parent),
        ui(new Ui::DaemonSinkGUI),
        m_pluginAPI(pluginAPI),
        m_deviceUISet(deviceUISet),
        m_sampleRate(0),
        m_tickCount(0)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose, true);
    connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

    m_daemonSink = (DaemonSink*) channelrx;
    m_daemonSink->setMessageQueueToGUI(getInputMessageQueue());

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("Daemon source");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    m_settings.setChannelMarker(&m_channelMarker);

    m_deviceUISet->registerRxChannelInstance(DaemonSink::m_channelIdURI, this);
    m_deviceUISet->addChannelMarker(&m_channelMarker);
    m_deviceUISet->addRollupWidget(this);

    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));
    //connect(&(m_deviceUISet->m_deviceSourceAPI->getMasterTimer()), SIGNAL(timeout()), this, SLOT(tick()));

    m_time.start();

    displaySettings();
    applySettings(true);
}

DaemonSinkGUI::~DaemonSinkGUI()
{
    m_deviceUISet->removeRxChannelInstance(this);
    delete ui;
}

void DaemonSinkGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void DaemonSinkGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        setTitleColor(m_channelMarker.getColor());

        DaemonSink::MsgConfigureDaemonSink* message = DaemonSink::MsgConfigureDaemonSink::create(m_settings, force);
        m_daemonSink->getInputMessageQueue()->push(message);
    }
}

void DaemonSinkGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.setBandwidth(5000); // TODO
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());

    blockApplySettings(true);
    ui->dataAddress->setText(m_settings.m_dataAddress);
    ui->dataPort->setText(tr("%1").arg(m_settings.m_dataPort));
    QString s = QString::number(128 + m_settings.m_nbFECBlocks, 'f', 0);
    QString s1 = QString::number(m_settings.m_nbFECBlocks, 'f', 0);
    ui->nominalNbBlocksText->setText(tr("%1/%2").arg(s).arg(s1));
    ui->txDelayText->setText(tr("%1%").arg(m_settings.m_txDelay));
    ui->txDelay->setValue(m_settings.m_txDelay);
    updateTxDelayTime();
    blockApplySettings(false);
}

void DaemonSinkGUI::leaveEvent(QEvent*)
{
    m_channelMarker.setHighlighted(false);
}

void DaemonSinkGUI::enterEvent(QEvent*)
{
    m_channelMarker.setHighlighted(true);
}

void DaemonSinkGUI::handleSourceMessages()
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

void DaemonSinkGUI::onWidgetRolled(QWidget* widget __attribute__((unused)), bool rollDown __attribute__((unused)))
{
}

void DaemonSinkGUI::onMenuDialogCalled(const QPoint &p)
{
    BasicChannelSettingsDialog dialog(&m_channelMarker, this);
    dialog.move(p);
    dialog.exec();

    m_settings.m_rgbColor = m_channelMarker.getColor().rgb();
    m_settings.m_title = m_channelMarker.getTitle();

    setWindowTitle(m_settings.m_title);
    setTitleColor(m_settings.m_rgbColor);

    applySettings();
}

void DaemonSinkGUI::on_dataAddress_returnPressed()
{
    m_settings.m_dataAddress = ui->dataAddress->text();
    applySettings();
}

void DaemonSinkGUI::on_dataPort_returnPressed()
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

void DaemonSinkGUI::on_dataApplyButton_clicked(bool checked __attribute__((unused)))
{
    m_settings.m_dataAddress = ui->dataAddress->text();

    bool dataOk;
    int udpDataPort = ui->dataPort->text().toInt(&dataOk);

    if((dataOk) && (udpDataPort >= 1024) && (udpDataPort < 65535))
    {
        m_settings.m_dataPort = udpDataPort;
    }

    applySettings();
}

void DaemonSinkGUI::on_txDelay_valueChanged(int value)
{
    m_settings.m_txDelay = value; // percentage
    ui->txDelayText->setText(tr("%1%").arg(value));
    updateTxDelayTime();
    applySettings();
}

void DaemonSinkGUI::on_nbFECBlocks_valueChanged(int value)
{
    m_settings.m_nbFECBlocks = value;
    int nbOriginalBlocks = 128;
    int nbFECBlocks = value;
    QString s = QString::number(nbOriginalBlocks + nbFECBlocks, 'f', 0);
    QString s1 = QString::number(nbFECBlocks, 'f', 0);
    ui->nominalNbBlocksText->setText(tr("%1/%2").arg(s).arg(s1));
    updateTxDelayTime();
    applySettings();
}

void DaemonSinkGUI::updateTxDelayTime()
{
    double txDelayRatio = m_settings.m_txDelay / 100.0;
    double delay = m_sampleRate == 0 ? 0.0 : (127*127*txDelayRatio) / m_sampleRate;
    delay /= 128 + m_settings.m_nbFECBlocks;
    ui->txDelayTime->setText(tr("%1µs").arg(QString::number(delay*1e6, 'f', 0)));
}

void DaemonSinkGUI::tick()
{
    if (++m_tickCount == 20) { // once per second
        m_tickCount = 0;
    }
}
