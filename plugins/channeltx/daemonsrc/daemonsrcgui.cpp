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

#include "device/devicesinkapi.h"
#include "device/deviceuiset.h"
#include "gui/basicchannelsettingsdialog.h"
#include "mainwindow.h"

#include "daemonsrc.h"
#include "ui_daemonsrcgui.h"
#include "daemonsrcgui.h"

DaemonSrcGUI* DaemonSrcGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx)
{
    DaemonSrcGUI* gui = new DaemonSrcGUI(pluginAPI, deviceUISet, channelTx);
    return gui;
}

void DaemonSrcGUI::destroy()
{
    delete this;
}

void DaemonSrcGUI::setName(const QString& name)
{
    setObjectName(name);
}

QString DaemonSrcGUI::getName() const
{
    return objectName();
}

qint64 DaemonSrcGUI::getCenterFrequency() const {
    return 0;
}

void DaemonSrcGUI::setCenterFrequency(qint64 centerFrequency __attribute__((unused)))
{
}

void DaemonSrcGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray DaemonSrcGUI::serialize() const
{
    return m_settings.serialize();
}

bool DaemonSrcGUI::deserialize(const QByteArray& data)
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

bool DaemonSrcGUI::handleMessage(const Message& message)
{
    if (DaemonSrc::MsgSampleRateNotification::match(message))
    {
        DaemonSrc::MsgSampleRateNotification& notif = (DaemonSrc::MsgSampleRateNotification&) message;
        m_channelMarker.setBandwidth(notif.getSampleRate());
    }

    return false;
}

DaemonSrcGUI::DaemonSrcGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx __attribute__((unused)), QWidget* parent) :
        RollupWidget(parent),
        ui(new Ui::DaemonSrcGUI),
        m_pluginAPI(pluginAPI),
        m_deviceUISet(deviceUISet)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose, true);
    connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

    m_daemonSrc = (DaemonSrc*) channelTx;
    m_daemonSrc->setMessageQueueToGUI(getInputMessageQueue());

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("Daemon source");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    m_settings.setChannelMarker(&m_channelMarker);

    m_deviceUISet->registerTxChannelInstance(DaemonSrc::m_channelIdURI, this);
    m_deviceUISet->addChannelMarker(&m_channelMarker);
    m_deviceUISet->addRollupWidget(this);

    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));

    displaySettings();
    applySettings(true);
}

DaemonSrcGUI::~DaemonSrcGUI()
{
    m_deviceUISet->removeTxChannelInstance(this);
    delete ui;
}

void DaemonSrcGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void DaemonSrcGUI::applySettings(bool force __attribute((unused)))
{
}

void DaemonSrcGUI::displaySettings()
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
    blockApplySettings(false);
}

void DaemonSrcGUI::leaveEvent(QEvent*)
{
    m_channelMarker.setHighlighted(false);
}

void DaemonSrcGUI::enterEvent(QEvent*)
{
    m_channelMarker.setHighlighted(true);
}

void DaemonSrcGUI::handleSourceMessages()
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

void DaemonSrcGUI::onWidgetRolled(QWidget* widget __attribute__((unused)), bool rollDown __attribute__((unused)))
{
}

void DaemonSrcGUI::onMenuDialogCalled(const QPoint &p)
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
