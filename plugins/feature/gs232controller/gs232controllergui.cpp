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

#include "SWGTargetAzimuthElevation.h"

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
    return m_settings.serialize();
}

bool GS232ControllerGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
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
    else if (PipeEndPoint::MsgReportPipes::match(message))
    {
        PipeEndPoint::MsgReportPipes& report = (PipeEndPoint::MsgReportPipes&) message;
        m_availablePipes = report.getAvailablePipes();
        updatePipeList();
        return true;
    }
    else if (GS232ControllerReport::MsgReportAzAl::match(message))
    {
        GS232ControllerReport::MsgReportAzAl& azAl = (GS232ControllerReport::MsgReportAzAl&) message;
        ui->azimuthCurrentText->setText(QString("%1").arg(azAl.getAzimuth()));
        ui->elevationCurrentText->setText(QString("%1").arg(azAl.getElevation()));
        return true;
    }
    else if (MainCore::MsgTargetAzimuthElevation::match(message))
    {
        MainCore::MsgTargetAzimuthElevation& msg = (MainCore::MsgTargetAzimuthElevation&) message;
        SWGSDRangel::SWGTargetAzimuthElevation *swgTarget = msg.getSWGTargetAzimuthElevation();

        ui->azimuth->setValue(round(swgTarget->getAzimuth()));
        ui->elevation->setValue(round(swgTarget->getElevation()));
        ui->targetName->setText(*swgTarget->getName());
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

    ui->azimuthCurrentText->setText("-");
    ui->elevationCurrentText->setText("-");

    updateSerialPortList();
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
    ui->protocol->setCurrentIndex((int)m_settings.m_protocol);
    updateDecimals(m_settings.m_protocol);
    if (m_settings.m_serialPort.length() > 0)
        ui->serialPort->lineEdit()->setText(m_settings.m_serialPort);
    ui->baudRate->setCurrentText(QString("%1").arg(m_settings.m_baudRate));
    ui->track->setChecked(m_settings.m_track);
    ui->targets->setCurrentIndex(ui->targets->findText(m_settings.m_target));
    ui->azimuthOffset->setValue(m_settings.m_azimuthOffset);
    ui->elevationOffset->setValue(m_settings.m_elevationOffset);
    ui->azimuthMin->setValue(m_settings.m_azimuthMin);
    ui->azimuthMax->setValue(m_settings.m_azimuthMax);
    ui->elevationMin->setValue(m_settings.m_elevationMin);
    ui->elevationMax->setValue(m_settings.m_elevationMax);
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

void GS232ControllerGUI::updatePipeList()
{
    QString currentText = ui->targets->currentText();
    ui->targets->blockSignals(true);
    ui->targets->clear();
    QList<PipeEndPoint::AvailablePipeSource>::const_iterator it = m_availablePipes.begin();

    for (int i = 0; it != m_availablePipes.end(); ++it, i++)
    {
        ui->targets->addItem(it->getName());
    }

    if (currentText.isEmpty())
    {
        if (m_availablePipes.size() > 0)
            ui->targets->setCurrentIndex(0);
    }
    else
        ui->targets->setCurrentIndex(ui->targets->findText(currentText));
    ui->targets->blockSignals(false);

    QString newText = ui->targets->currentText();
    if (currentText != newText)
    {
       m_settings.m_target = newText;
       ui->targetName->setText("");
       applySettings();
    }
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

void GS232ControllerGUI::updateDecimals(GS232ControllerSettings::Protocol protocol)
{
    if (protocol == GS232ControllerSettings::GS232)
    {
        ui->azimuth->setDecimals(0);
        ui->elevation->setDecimals(0);
    }
    else
    {
        ui->azimuth->setDecimals(1);
        ui->elevation->setDecimals(1);
    }
}

void GS232ControllerGUI::on_protocol_currentIndexChanged(int index)
{
    m_settings.m_protocol = (GS232ControllerSettings::Protocol)index;
    updateDecimals(m_settings.m_protocol);
    applySettings();
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

void GS232ControllerGUI::on_azimuth_valueChanged(double value)
{
    m_settings.m_azimuth = (float)value;
    ui->targetName->setText("");
    applySettings();
}

void GS232ControllerGUI::on_elevation_valueChanged(double value)
{
    m_settings.m_elevation = (float)value;
    ui->targetName->setText("");
    applySettings();
}

void GS232ControllerGUI::on_azimuthOffset_valueChanged(int value)
{
    m_settings.m_azimuthOffset = value;
    applySettings();
}

void GS232ControllerGUI::on_elevationOffset_valueChanged(int value)
{
    m_settings.m_elevationOffset = value;
    applySettings();
}

void GS232ControllerGUI::on_azimuthMin_valueChanged(int value)
{
    m_settings.m_azimuthMin = value;
    applySettings();
}

void GS232ControllerGUI::on_azimuthMax_valueChanged(int value)
{
    m_settings.m_azimuthMax = value;
    applySettings();
}

void GS232ControllerGUI::on_elevationMin_valueChanged(int value)
{
    m_settings.m_elevationMin = value;
    applySettings();
}

void GS232ControllerGUI::on_elevationMax_valueChanged(int value)
{
    m_settings.m_elevationMax = value;
    applySettings();
}

void GS232ControllerGUI::on_tolerance_valueChanged(int value)
{
    m_settings.m_tolerance = value;
    applySettings();
}

void GS232ControllerGUI::on_track_stateChanged(int state)
{
    m_settings.m_track = state == Qt::Checked;
    ui->targetsLabel->setEnabled(m_settings.m_track);
    ui->targets->setEnabled(m_settings.m_track);
    if (!m_settings.m_track)
        ui->targetName->setText("");
    applySettings();
}

void GS232ControllerGUI::on_targets_currentTextChanged(const QString& text)
{
    m_settings.m_target = text;
    ui->targetName->setText("");
    applySettings();
}

void GS232ControllerGUI::updateStatus()
{
    int state = m_gs232Controller->getState();

    if (m_lastFeatureState != state)
    {
        // We set checked state of start/stop button, in case it was changed via API
        bool oldState;
        switch (state)
        {
            case Feature::StNotStarted:
                ui->startStop->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
                break;
            case Feature::StIdle:
                oldState = ui->startStop->blockSignals(true);
                ui->startStop->setChecked(false);
                ui->startStop->blockSignals(oldState);
                ui->startStop->setStyleSheet("QToolButton { background-color : blue; }");
                break;
            case Feature::StRunning:
                oldState = ui->startStop->blockSignals(true);
                ui->startStop->setChecked(true);
                ui->startStop->blockSignals(oldState);
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
