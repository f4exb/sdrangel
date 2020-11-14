///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#include <cmath>
#include <QMessageBox>
#include <QLineEdit>
#include <QSerialPortInfo>

#include "feature/featureuiset.h"
#include "gui/basicfeaturesettingsdialog.h"
#include "mainwindow.h"
#include "device/deviceuiset.h"

#include "ui_gs232controllergui.h"
#include "gs232controller.h"
#include "gs232controllergui.h"
#include "gs232controllerreport.h"

GS232ControllerGUI* GS232ControllerGUI::create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature)
{
    GS232ControllerGUI* gui = new GS232ControllerGUI(pluginAPI, featureUISet, feature);
    return gui;
}

void GS232ControllerGUI::destroy()
{
    delete this;
}

void GS232ControllerGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray GS232ControllerGUI::serialize() const
{
    qDebug("GS232ControllerGUI::serialize: %d", m_settings.m_channelIndex);
    return m_settings.serialize();
}

bool GS232ControllerGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        qDebug("GS232ControllerGUI::deserialize: %d", m_settings.m_channelIndex);
        updateDeviceSetList();
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

bool GS232ControllerGUI::handleMessage(const Message& message)
{
    if (GS232Controller::MsgConfigureGS232Controller::match(message))
    {
        qDebug("GS232ControllerGUI::handleMessage: GS232Controller::MsgConfigureGS232Controller");
        const GS232Controller::MsgConfigureGS232Controller& cfg = (GS232Controller::MsgConfigureGS232Controller&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);

        return true;
    }
    else if (GS232ControllerSettings::MsgChannelIndexChange::match(message))
    {
        const GS232ControllerSettings::MsgChannelIndexChange& cfg = (GS232ControllerSettings::MsgChannelIndexChange&) message;
        int newChannelIndex = cfg.getIndex();
        qDebug("GS232ControllerGUI::handleMessage: GS232ControllerSettings::MsgChannelIndexChange: %d", newChannelIndex);
        ui->channel->blockSignals(true);
        ui->channel->setCurrentIndex(newChannelIndex);
        m_settings.m_channelIndex = newChannelIndex;
        ui->channel->blockSignals(false);

        return true;
    } else if (GS232ControllerReport::MsgReportAzAl::match(message))
    {
        GS232ControllerReport::MsgReportAzAl& azAl = (GS232ControllerReport::MsgReportAzAl&) message;
        if (azAl.getType() == GS232ControllerReport::AzAlType::TARGET)
        {
            ui->azimuth->setValue(round(azAl.getAzimuth()));
            ui->elevation->setValue(round(azAl.getElevation()));
        }
        else
        {
            ui->azimuthCurrentText->setText(QString("%1").arg(round(azAl.getAzimuth())));
            ui->elevationCurrentText->setText(QString("%1").arg(round(azAl.getElevation())));
        }
        return true;
    }

    return false;
}

void GS232ControllerGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()))
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void GS232ControllerGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;
}

GS232ControllerGUI::GS232ControllerGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent) :
    FeatureGUI(parent),
    ui(new Ui::GS232ControllerGUI),
    m_pluginAPI(pluginAPI),
    m_featureUISet(featureUISet),
    m_doApplySettings(true),
    m_lastFeatureState(0)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose, true);
    setChannelWidget(false);
    connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    m_gs232Controller = reinterpret_cast<GS232Controller*>(feature);
    m_gs232Controller->setMessageQueueToGUI(&m_inputMessageQueue);

    m_featureUISet->addRollupWidget(this);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(1000);

    updateSerialPortList();
    updateDeviceSetList();
    displaySettings();
    applySettings(true);
}

GS232ControllerGUI::~GS232ControllerGUI()
{
    delete ui;
}

void GS232ControllerGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void GS232ControllerGUI::displaySettings()
{
    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_settings.m_title);
    blockApplySettings(true);
    ui->azimuth->setValue(m_settings.m_azimuth);
    ui->elevation->setValue(m_settings.m_elevation);
    if (m_settings.m_serialPort.length() > 0)
        ui->serialPort->lineEdit()->setText(m_settings.m_serialPort);
    ui->baudRate->setCurrentText(QString("%1").arg(m_settings.m_baudRate));
    ui->track->setChecked(m_settings.m_track);
    blockApplySettings(false);
}

void GS232ControllerGUI::updateSerialPortList()
{
    ui->serialPort->clear();
    QList<QSerialPortInfo> serialPorts = QSerialPortInfo::availablePorts();
    QListIterator<QSerialPortInfo> i(serialPorts);
    while (i.hasNext())
    {
        QSerialPortInfo info = i.next();
        ui->serialPort->addItem(info.portName());
    }
}

void GS232ControllerGUI::updateDeviceSetList()
{
    MainWindow *mainWindow = MainWindow::getInstance();
    std::vector<DeviceUISet*>& deviceUISets = mainWindow->getDeviceUISets();
    std::vector<DeviceUISet*>::const_iterator it = deviceUISets.begin();

    ui->device->blockSignals(true);

    ui->device->clear();
    unsigned int deviceIndex = 0;

    for (; it != deviceUISets.end(); ++it, deviceIndex++)
    {
        DSPDeviceSourceEngine *deviceSourceEngine =  (*it)->m_deviceSourceEngine;
        DSPDeviceSinkEngine *deviceSinkEngine = (*it)->m_deviceSinkEngine;
        DSPDeviceMIMOEngine *deviceMIMOEngine = (*it)->m_deviceMIMOEngine;

        if (deviceSourceEngine) {
            ui->device->addItem(QString("R%1").arg(deviceIndex), deviceIndex);
        }
    }

    int newDeviceIndex;

    if (it != deviceUISets.begin())
    {
        if (m_settings.m_deviceIndex < 0) {
            ui->device->setCurrentIndex(0);
        } else {
            ui->device->setCurrentIndex(m_settings.m_deviceIndex);
        }

        newDeviceIndex = ui->device->currentData().toInt();
    }
    else
    {
        newDeviceIndex = -1;
    }


    if (newDeviceIndex != m_settings.m_deviceIndex)
    {
        qDebug("GS232ControllerGUI::updateDeviceSetLists: device index changed: %d", newDeviceIndex);
        m_settings.m_deviceIndex = newDeviceIndex;
    }

    updateChannelList();

    ui->device->blockSignals(false);
}

bool GS232ControllerGUI::updateChannelList()
{
    int newChannelIndex;
    ui->channel->blockSignals(true);
    ui->channel->clear();

    if (m_settings.m_deviceIndex < 0)
    {
        newChannelIndex = -1;
    }
    else
    {
        MainWindow *mainWindow = MainWindow::getInstance();
        std::vector<DeviceUISet*>& deviceUISets = mainWindow->getDeviceUISets();
        DeviceUISet *deviceUISet = deviceUISets[m_settings.m_deviceIndex];
        int nbChannels = deviceUISet->getNumberOfChannels();

        for (int ch = 0; ch < nbChannels; ch++) {
            ui->channel->addItem(QString("%1").arg(ch), ch);
        }

        if (nbChannels > 0)
        {
            if (m_settings.m_channelIndex < 0) {
                ui->channel->setCurrentIndex(0);
            } else {
                ui->channel->setCurrentIndex(m_settings.m_channelIndex);
            }

            newChannelIndex = ui->channel->currentIndex();
        }
        else
        {
            newChannelIndex = -1;
        }
    }

    ui->channel->blockSignals(false);

    if (newChannelIndex != m_settings.m_channelIndex)
    {
        qDebug("GS232ControllerGUI::updateChannelList: channel index changed: %d", newChannelIndex);
        m_settings.m_channelIndex = newChannelIndex;
        return true;
    }

    return false;
}

void GS232ControllerGUI::leaveEvent(QEvent*)
{
}

void GS232ControllerGUI::enterEvent(QEvent*)
{
}

void GS232ControllerGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuChannelSettings)
    {
        BasicFeatureSettingsDialog dialog(this);
        dialog.setTitle(m_settings.m_title);
        dialog.setColor(m_settings.m_rgbColor);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIFeatureSetIndex(m_settings.m_reverseAPIFeatureSetIndex);
        dialog.setReverseAPIFeatureIndex(m_settings.m_reverseAPIFeatureIndex);

        dialog.move(p);
        dialog.exec();

        m_settings.m_rgbColor = dialog.getColor().rgb();
        m_settings.m_title = dialog.getTitle();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIFeatureSetIndex = dialog.getReverseAPIFeatureSetIndex();
        m_settings.m_reverseAPIFeatureIndex = dialog.getReverseAPIFeatureIndex();

        setWindowTitle(m_settings.m_title);
        setTitleColor(m_settings.m_rgbColor);

        applySettings();
    }

    resetContextMenuType();
}

void GS232ControllerGUI::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        GS232Controller::MsgStartStop *message = GS232Controller::MsgStartStop::create(checked);
        m_gs232Controller->getInputMessageQueue()->push(message);
    }
}

void GS232ControllerGUI::on_devicesRefresh_clicked()
{
    updateDeviceSetList();
    displaySettings();
    applySettings();
}

void GS232ControllerGUI::on_device_currentIndexChanged(int index)
{
    if (index >= 0)
    {
        m_settings.m_deviceIndex = ui->device->currentData().toInt();
        updateChannelList();
        applySettings();
    }
}

void GS232ControllerGUI::on_channel_currentIndexChanged(int index)
{
    if (index >= 0)
    {
        m_settings.m_channelIndex = index;
        applySettings();
    }
}

void GS232ControllerGUI::on_serialPort_currentIndexChanged(int index)
{
    (void) index;
    m_settings.m_serialPort = ui->serialPort->currentText();
    applySettings();
}

void GS232ControllerGUI::on_baudRate_currentIndexChanged(int index)
{
    (void) index;
    m_settings.m_baudRate = ui->baudRate->currentText().toInt();
    applySettings();
}

void GS232ControllerGUI::on_azimuth_valueChanged(int value)
{
    m_settings.m_azimuth = value;
    applySettings();
}

void GS232ControllerGUI::on_elevation_valueChanged(int value)
{
    m_settings.m_elevation = value;
    applySettings();
}

void GS232ControllerGUI::on_track_stateChanged(int state)
{
    m_settings.m_track = state == Qt::Checked;
    ui->devicesRefresh->setEnabled(m_settings.m_track);
    ui->deviceLabel->setEnabled(m_settings.m_track);
    ui->device->setEnabled(m_settings.m_track);
    ui->channelLabel->setEnabled(m_settings.m_track);
    ui->channel->setEnabled(m_settings.m_track);
    applySettings();
}

void GS232ControllerGUI::updateStatus()
{
    int state = m_gs232Controller->getState();

    if (m_lastFeatureState != state)
    {
        switch (state)
        {
            case Feature::StNotStarted:
                ui->startStop->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
                break;
            case Feature::StIdle:
                ui->startStop->setStyleSheet("QToolButton { background-color : blue; }");
                break;
            case Feature::StRunning:
                ui->startStop->setStyleSheet("QToolButton { background-color : green; }");
                break;
            case Feature::StError:
                ui->startStop->setStyleSheet("QToolButton { background-color : red; }");
                QMessageBox::information(this, tr("Message"), m_gs232Controller->getErrorMessage());
                break;
            default:
                break;
        }

        m_lastFeatureState = state;
    }
}

void GS232ControllerGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        GS232Controller::MsgConfigureGS232Controller* message = GS232Controller::MsgConfigureGS232Controller::create(m_settings, force);
        m_gs232Controller->getInputMessageQueue()->push(message);
    }
}
