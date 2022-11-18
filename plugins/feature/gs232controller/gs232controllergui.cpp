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
        m_feature->setWorkspaceIndex(m_settings.m_workspaceIndex);
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
    else if (GS232Controller::MsgReportAvailableChannelOrFeatures::match(message))
    {
        GS232Controller::MsgReportAvailableChannelOrFeatures& report =
            (GS232Controller::MsgReportAvailableChannelOrFeatures&) message;
        updatePipeList(report.getItems());
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

        ui->azimuth->setValue(swgTarget->getAzimuth());
        ui->elevation->setValue(swgTarget->getElevation());
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

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

GS232ControllerGUI::GS232ControllerGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent) :
    FeatureGUI(parent),
    ui(new Ui::GS232ControllerGUI),
    m_pluginAPI(pluginAPI),
    m_featureUISet(featureUISet),
    m_doApplySettings(true),
    m_lastFeatureState(0),
    m_lastOnTarget(false)
{
    m_feature = feature;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/feature/gs232controller/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

    m_gs232Controller = reinterpret_cast<GS232Controller*>(feature);
    m_gs232Controller->setMessageQueueToGUI(&m_inputMessageQueue);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(250);

    ui->azimuthCurrentText->setText("-");
    ui->elevationCurrentText->setText("-");

    updateSerialPortList();
    if (ui->serialPort->currentIndex() >= 0) {
        on_serialPort_currentIndexChanged(ui->serialPort->currentIndex());
    }

    m_settings.setRollupState(&m_rollupState);

    displaySettings();
    applySettings(true);
    makeUIConnections();

    // Get pre-existing pipes
    m_gs232Controller->getInputMessageQueue()->push(GS232Controller::MsgScanAvailableChannelOrFeatures::create());
}

GS232ControllerGUI::~GS232ControllerGUI()
{
    delete ui;
}

void GS232ControllerGUI::setWorkspaceIndex(int index)
{
    m_settings.m_workspaceIndex = index;
    m_feature->setWorkspaceIndex(index);
}

void GS232ControllerGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void GS232ControllerGUI::displaySettings()
{
    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_settings.m_title);
    setTitle(m_settings.m_title);
    blockApplySettings(true);
    ui->azimuth->setValue(m_settings.m_azimuth);
    ui->elevation->setValue(m_settings.m_elevation);
    ui->protocol->setCurrentIndex((int)m_settings.m_protocol);
    ui->connection->setCurrentIndex((int)m_settings.m_connection);
    updateDecimals(m_settings.m_protocol);
    if (m_settings.m_serialPort.length() > 0) {
        ui->serialPort->lineEdit()->setText(m_settings.m_serialPort);
    }
    ui->baudRate->setCurrentText(QString("%1").arg(m_settings.m_baudRate));
    ui->host->setText(m_settings.m_host);
    ui->port->setValue(m_settings.m_port);
    ui->track->setChecked(m_settings.m_track);
    ui->sources->setCurrentIndex(ui->sources->findText(m_settings.m_source));
    ui->azimuthOffset->setValue(m_settings.m_azimuthOffset);
    ui->elevationOffset->setValue(m_settings.m_elevationOffset);
    ui->azimuthMin->setValue(m_settings.m_azimuthMin);
    ui->azimuthMax->setValue(m_settings.m_azimuthMax);
    ui->elevationMin->setValue(m_settings.m_elevationMin);
    ui->elevationMax->setValue(m_settings.m_elevationMax);
    ui->tolerance->setValue(m_settings.m_tolerance);
    getRollupContents()->restoreState(m_rollupState);
    updateConnectionWidgets();
    blockApplySettings(false);
}

void GS232ControllerGUI::updateConnectionWidgets()
{
    bool serial = m_settings.m_connection == GS232ControllerSettings::SERIAL;
    ui->serialPortLabel->setVisible(serial);
    ui->serialPort->setVisible(serial);
    ui->baudRateLabel->setVisible(serial);
    ui->baudRate->setVisible(serial);
    ui->hostLabel->setVisible(!serial);
    ui->host->setVisible(!serial);
    ui->portLabel->setVisible(!serial);
    ui->port->setVisible(!serial);
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

void GS232ControllerGUI::updatePipeList(const QList<GS232ControllerSettings::AvailableChannelOrFeature>& sources)
{
    QString currentText = ui->sources->currentText();
    QString newText;
    ui->sources->blockSignals(true);
    ui->sources->clear();

    for (const auto& source : sources)
    {
        QString name = tr("%1%2:%3 %4")
            .arg(source.m_kind)
            .arg(source.m_superIndex)
            .arg(source.m_index)
            .arg(source.m_type);
        ui->sources->addItem(name);
    }

    int index = ui->sources->findText(m_settings.m_source);
    ui->sources->setCurrentIndex(index);

    if (index < 0) // current source is not found
    {
        m_settings.m_source = "";
        ui->targetName->setText("");
        applySettings();
    }

    // if (currentText.isEmpty())
    // {
    //     // Source feature may be loaded after this, so may not have existed when
    //     // displaySettings was called
    //     if (sources.size() > 0) {
    //         ui->sources->setCurrentIndex(ui->sources->findText(m_settings.m_source));
    //     }
    // }
    // else
    // {
    //     ui->sources->setCurrentIndex(ui->sources->findText(currentText));
    // }

    ui->sources->blockSignals(false);

    // QString newText = ui->sources->currentText();

    // if (currentText != newText)
    // {
    //    m_settings.m_source = newText;
    //    ui->targetName->setText("");
    //    applySettings();
    // }
}

void GS232ControllerGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuChannelSettings)
    {
        BasicFeatureSettingsDialog dialog(this);
        dialog.setTitle(m_settings.m_title);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIFeatureSetIndex(m_settings.m_reverseAPIFeatureSetIndex);
        dialog.setReverseAPIFeatureIndex(m_settings.m_reverseAPIFeatureIndex);
        dialog.setDefaultTitle(m_displayedName);

        dialog.move(p);
        dialog.exec();

        m_settings.m_title = dialog.getTitle();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIFeatureSetIndex = dialog.getReverseAPIFeatureSetIndex();
        m_settings.m_reverseAPIFeatureIndex = dialog.getReverseAPIFeatureIndex();

        setTitle(m_settings.m_title);
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

void GS232ControllerGUI::on_connection_currentIndexChanged(int index)
{
    m_settings.m_connection = (GS232ControllerSettings::Connection)index;
    applySettings();
    updateConnectionWidgets();
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

void GS232ControllerGUI::on_host_editingFinished()
{
    m_settings.m_host = ui->host->text();
    applySettings();
}

void GS232ControllerGUI::on_port_valueChanged(int value)
{
    m_settings.m_port = value;
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

void GS232ControllerGUI::on_tolerance_valueChanged(double value)
{
    m_settings.m_tolerance = value;
    applySettings();
}

void GS232ControllerGUI::on_track_stateChanged(int state)
{
    m_settings.m_track = state == Qt::Checked;
    ui->targetName->setEnabled(m_settings.m_track);
    ui->sources->setEnabled(m_settings.m_track);

    if (!m_settings.m_track) {
        ui->targetName->setText("");
    }

    applySettings();
}

void GS232ControllerGUI::on_sources_currentTextChanged(const QString& text)
{
    qDebug("GS232ControllerGUI::on_sources_currentTextChanged: %s", qPrintable(text));
    m_settings.m_source = text;
    ui->targetName->setText("");
    applySettings();
}

void GS232ControllerGUI::updateStatus()
{
    int state = m_gs232Controller->getState();
    bool onTarget = m_gs232Controller->getOnTarget();

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
                if (onTarget) {
                    ui->startStop->setStyleSheet("QToolButton { background-color : green; }");
                } else {
                    ui->startStop->setStyleSheet("QToolButton { background-color : yellow; }");
                }
                m_lastOnTarget = onTarget;
                break;
            case Feature::StError:
                ui->startStop->setStyleSheet("QToolButton { background-color : red; }");
                QMessageBox::critical(this, m_settings.m_title, m_gs232Controller->getErrorMessage());
                break;
            default:
                break;
        }

        m_lastFeatureState = state;
    }
    else if (state == Feature::StRunning)
    {
        if (onTarget != m_lastOnTarget)
        {
            if (onTarget) {
                ui->startStop->setStyleSheet("QToolButton { background-color : green; }");
            } else {
                ui->startStop->setStyleSheet("QToolButton { background-color : yellow; }");
            }
        }
        m_lastOnTarget = onTarget;
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

void GS232ControllerGUI::makeUIConnections()
{
    QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &GS232ControllerGUI::on_startStop_toggled);
    QObject::connect(ui->protocol, qOverload<int>(&QComboBox::currentIndexChanged), this, &GS232ControllerGUI::on_protocol_currentIndexChanged);
    QObject::connect(ui->connection, qOverload<int>(&QComboBox::currentIndexChanged), this, &GS232ControllerGUI::on_connection_currentIndexChanged);
    QObject::connect(ui->serialPort, qOverload<int>(&QComboBox::currentIndexChanged), this, &GS232ControllerGUI::on_serialPort_currentIndexChanged);
    QObject::connect(ui->host, &QLineEdit::editingFinished, this, &GS232ControllerGUI::on_host_editingFinished);
    QObject::connect(ui->port, qOverload<int>(&QSpinBox::valueChanged), this, &GS232ControllerGUI::on_port_valueChanged);
    QObject::connect(ui->baudRate, qOverload<int>(&QComboBox::currentIndexChanged), this, &GS232ControllerGUI::on_baudRate_currentIndexChanged);
    QObject::connect(ui->track, &QCheckBox::stateChanged, this, &GS232ControllerGUI::on_track_stateChanged);
    QObject::connect(ui->azimuth, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &GS232ControllerGUI::on_azimuth_valueChanged);
    QObject::connect(ui->elevation, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &GS232ControllerGUI::on_elevation_valueChanged);
    QObject::connect(ui->sources, &QComboBox::currentTextChanged, this, &GS232ControllerGUI::on_sources_currentTextChanged);
    QObject::connect(ui->azimuthOffset, qOverload<int>(&QSpinBox::valueChanged), this, &GS232ControllerGUI::on_azimuthOffset_valueChanged);
    QObject::connect(ui->elevationOffset, qOverload<int>(&QSpinBox::valueChanged), this, &GS232ControllerGUI::on_elevationOffset_valueChanged);
    QObject::connect(ui->azimuthMin, qOverload<int>(&QSpinBox::valueChanged), this, &GS232ControllerGUI::on_azimuthMin_valueChanged);
    QObject::connect(ui->azimuthMax, qOverload<int>(&QSpinBox::valueChanged), this, &GS232ControllerGUI::on_azimuthMax_valueChanged);
    QObject::connect(ui->elevationMin, qOverload<int>(&QSpinBox::valueChanged), this, &GS232ControllerGUI::on_elevationMin_valueChanged);
    QObject::connect(ui->elevationMax, qOverload<int>(&QSpinBox::valueChanged), this, &GS232ControllerGUI::on_elevationMax_valueChanged);
    QObject::connect(ui->tolerance, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &GS232ControllerGUI::on_tolerance_valueChanged);
}
