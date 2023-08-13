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
#include "gui/dialogpositioner.h"
#include "mainwindow.h"
#include "device/deviceuiset.h"
#include "util/astronomy.h"

#include "ui_gs232controllergui.h"
#include "gs232controller.h"
#include "gs232controllergui.h"
#include "gs232controllerreport.h"
#include "dfmprotocol.h"

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

void GS232ControllerGUI::azElToDisplay(float az, float el, float& coord1, float& coord2) const
{
    AzAlt aa;
    double c1, c2;
    if (m_settings.m_coordinates == GS232ControllerSettings::X_Y_85)
    {
        aa.az = az;
        aa.alt = el;
        Astronomy::azAltToXY85(aa, c1, c2);
        coord1 = (float)c1;
        coord2 = (float)c2;
    }
    else if (m_settings.m_coordinates == GS232ControllerSettings::X_Y_30)
    {
        aa.az = az;
        aa.alt = el;
        Astronomy::azAltToXY30(aa, c1, c2);
        coord1 = (float)c1;
        coord2 = (float)c2;
    }
    else
    {
        coord1 = az;
        coord2 = el;
    }
}

void GS232ControllerGUI::displayToAzEl(float coord1, float coord2)
{
    if (m_settings.m_coordinates == GS232ControllerSettings::X_Y_85)
    {
        AzAlt aa = Astronomy::xy85ToAzAlt(coord1, coord2);
        m_settings.m_azimuth = aa.az;
        m_settings.m_elevation = aa.alt;
    }
    else if (m_settings.m_coordinates == GS232ControllerSettings::X_Y_30)
    {
        AzAlt aa = Astronomy::xy30ToAzAlt(coord1, coord2);
        m_settings.m_azimuth = aa.az;
        m_settings.m_elevation = aa.alt;
    }
    else
    {
        m_settings.m_azimuth = coord1;
        m_settings.m_elevation = coord2;
    }
    m_settingsKeys.append("azimuth");
    m_settingsKeys.append("elevation");
    applySettings();
}

bool GS232ControllerGUI::handleMessage(const Message& message)
{
    if (GS232Controller::MsgConfigureGS232Controller::match(message))
    {
        qDebug("GS232ControllerGUI::handleMessage: GS232Controller::MsgConfigureGS232Controller");
        const GS232Controller::MsgConfigureGS232Controller& cfg = (GS232Controller::MsgConfigureGS232Controller&) message;

        if (cfg.getForce()) {
            m_settings = cfg.getSettings();
        } else {
            m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
        }

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
        float coord1, coord2;
        azElToDisplay(azAl.getAzimuth(), azAl.getElevation(), coord1, coord2);
        ui->coord1CurrentText->setText(QString::number(coord1, 'f', m_settings.m_precision));
        ui->coord2CurrentText->setText(QString::number(coord2, 'f', m_settings.m_precision));
        return true;
    }
    else if (MainCore::MsgTargetAzimuthElevation::match(message))
    {
        MainCore::MsgTargetAzimuthElevation& msg = (MainCore::MsgTargetAzimuthElevation&) message;
        SWGSDRangel::SWGTargetAzimuthElevation *swgTarget = msg.getSWGTargetAzimuthElevation();
        float coord1, coord2;
        azElToDisplay(swgTarget->getAzimuth(), swgTarget->getElevation(), coord1, coord2);
        ui->coord1->setValue(coord1);
        ui->coord2->setValue(coord2);
        ui->targetName->setText(*swgTarget->getName());
        return true;
    }
    else if (GS232Controller::MsgReportSerialPorts::match(message))
    {
        GS232Controller::MsgReportSerialPorts& msg = (GS232Controller::MsgReportSerialPorts&) message;
        updateSerialPortList(msg.getSerialPorts());
        return true;
    }
    else if (DFMProtocol::MsgReportDFMStatus::match(message))
    {
        DFMProtocol::MsgReportDFMStatus& report = (DFMProtocol::MsgReportDFMStatus&) message;
        m_dfmStatusDialog.displayStatus(report.getDFMStatus());
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
    m_lastOnTarget(false),
    m_dfmStatusDialog(),
    m_inputController(nullptr),
    m_inputCoord1(0.0),
    m_inputCoord2(0.0),
    m_inputAzOffset(0.0),
    m_inputElOffset(0.0),
    m_inputUpdate(false)
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

    ui->coord1CurrentText->setText("-");
    ui->coord2CurrentText->setText("-");
    setProtocol(m_settings.m_protocol); // Hide DFM buttons

    updateSerialPortList();
    if (ui->serialPort->currentIndex() >= 0) {
        on_serialPort_currentIndexChanged(ui->serialPort->currentIndex());
    }

    m_settings.setRollupState(&m_rollupState);

    //ui->inputConfigure->setVisible(false);
    ui->inputConfigure->setEnabled(false);
    updateInputControllerList();
    connect(InputControllerManager::instance(), &InputControllerManager::controllersChanged, this, &GS232ControllerGUI::updateInputControllerList);
    connect(&m_inputTimer, &QTimer::timeout, this, &GS232ControllerGUI::checkInputController);

    displaySettings();
    applySettings(true);
    makeUIConnections();

    // Get pre-existing pipes
    m_gs232Controller->getInputMessageQueue()->push(GS232Controller::MsgScanAvailableChannelOrFeatures::create());

    new DialogPositioner(&m_dfmStatusDialog, true);
}

void GS232ControllerGUI::updateInputControllerList()
{
    ui->inputController->blockSignals(true);
    ui->inputController->clear();
    ui->inputController->addItem("None");

    QStringList controllers = InputControllerManager::getAllControllers();
    for (const auto& controller : controllers) {
        ui->inputController->addItem(controller);
    }
    ui->inputController->blockSignals(false);
    int index = ui->inputController->findText(m_settings.m_inputController);
    ui->inputController->setCurrentIndex(index);
}

void GS232ControllerGUI::updateInputController()
{
    delete m_inputController;
    m_inputController = nullptr;

    bool enabled = false;
    if (m_settings.m_inputController != "None")
    {
        m_inputController = InputControllerManager::open(m_settings.m_inputController);
        if (m_inputController)
        {
            connect(m_inputController, &InputController::buttonChanged, this, &GS232ControllerGUI::buttonChanged);
            connect(m_inputController, &InputController::configurationComplete, this, &GS232ControllerGUI::inputConfigurationComplete);
            m_inputTimer.start(20);
            enabled = true;
        }
    }
    else
    {
        m_inputTimer.stop();
    }
    ui->inputConfigure->setEnabled(enabled);
}

void GS232ControllerGUI::buttonChanged(int button, bool released)
{
    if (!released)
    {
        switch (button)
        {
        case INPUTCONTROLLER_BUTTON_RIGHT_TOP:
            ui->startStop->doToggle(!ui->startStop->isChecked());
            break;
        case INPUTCONTROLLER_BUTTON_RIGHT_BOTTOM:
            ui->track->click();
            break;
        case INPUTCONTROLLER_BUTTON_RIGHT_RIGHT:
            ui->enableTargetControl->click();
            break;
        case INPUTCONTROLLER_BUTTON_RIGHT_LEFT:
            ui->enableOffsetControl->click();
            break;
        case INPUTCONTROLLER_BUTTON_R1:
            ui->highSensitivity->click();
            break;
        }
    }
}

void GS232ControllerGUI::checkInputController()
{
    if (m_inputController)
    {
        // If our input device has two sticks (four axes), we use one for target and one for offset
        // If only one stick (two axes), it's used both for target when not tracking and offset, when tracking
        // Use separate variables rather than values in UI, to allow for higher precision

        if (!m_settings.m_track)
        {
            if (m_settings.m_targetControlEnabled)
            {
                m_inputCoord1 += m_extraSensitivity * m_inputController->getAxisCalibratedValue(0, &m_settings.m_inputControllerSettings, m_settings.m_highSensitivity);
                m_inputCoord2 += m_extraSensitivity * -m_inputController->getAxisCalibratedValue(1, &m_settings.m_inputControllerSettings, m_settings.m_highSensitivity);
            }

            if (m_settings.m_coordinates == GS232ControllerSettings::AZ_EL)
            {
                m_inputCoord1 = std::max(m_inputCoord1, (double) m_settings.m_azimuthMin);
                m_inputCoord1 = std::min(m_inputCoord1, (double) m_settings.m_azimuthMax);
                m_inputCoord2 = std::max(m_inputCoord2, (double) m_settings.m_elevationMin);
                m_inputCoord2 = std::min(m_inputCoord2, (double) m_settings.m_elevationMax);
            }
            else
            {
                m_inputCoord1 = std::max(m_inputCoord1, -90.0);
                m_inputCoord1 = std::min(m_inputCoord1, 90.0);
                m_inputCoord2 = std::max(m_inputCoord2, -90.0);
                m_inputCoord2 = std::min(m_inputCoord2, 90.0);
            }
        }

        if ((m_inputController->getNumberOfAxes() < 4) && m_settings.m_track)
        {
            if (m_settings.m_offsetControlEnabled)
            {
                m_inputAzOffset += m_extraSensitivity * m_inputController->getAxisCalibratedValue(0, &m_settings.m_inputControllerSettings, m_settings.m_highSensitivity);
                m_inputElOffset += m_extraSensitivity * -m_inputController->getAxisCalibratedValue(1, &m_settings.m_inputControllerSettings, m_settings.m_highSensitivity);
            }
        }
        else if (m_inputController->getNumberOfAxes() >= 4)
        {
            if (m_settings.m_offsetControlEnabled)
            {
                m_inputAzOffset += m_extraSensitivity * m_inputController->getAxisCalibratedValue(2, &m_settings.m_inputControllerSettings, m_settings.m_highSensitivity);
                m_inputElOffset += m_extraSensitivity * -m_inputController->getAxisCalibratedValue(3, &m_settings.m_inputControllerSettings, m_settings.m_highSensitivity);
            }
        }
        m_inputAzOffset = std::max(m_inputAzOffset, -360.0);
        m_inputAzOffset = std::min(m_inputAzOffset, 360.0);
        m_inputElOffset = std::max(m_inputElOffset, -180.0);
        m_inputElOffset = std::min(m_inputElOffset, 180.0);

        m_inputUpdate = true;
        if (!m_settings.m_track)
        {
            ui->coord1->setValue(m_inputCoord1);
            ui->coord2->setValue(m_inputCoord2);
        }
        if (((m_inputController->getNumberOfAxes() < 4) && m_settings.m_track) || (m_inputController->getNumberOfAxes() >= 4))
        {
            ui->azimuthOffset->setValue(m_inputAzOffset);
            ui->elevationOffset->setValue(m_inputElOffset);
        }
        m_inputUpdate = false;
    }
}

void GS232ControllerGUI::on_inputController_currentIndexChanged(int index)
{
    // Don't update settings if set to -1
    if (index >= 0)
    {
        m_settings.m_inputController = ui->inputController->currentText();
        m_settingsKeys.append("inputController");
        applySettings();
        updateInputController();
    }
}

void GS232ControllerGUI::on_inputConfigure_clicked()
{
    if (m_inputController) {
        m_inputController->configure(&m_settings.m_inputControllerSettings);
    }
}

void GS232ControllerGUI::on_highSensitivity_clicked(bool checked)
{
    m_settings.m_highSensitivity = checked;
    ui->highSensitivity->setText(checked ? "H" : "L");
    m_settingsKeys.append("highSensitivity");
    applySettings();
}

void GS232ControllerGUI::on_enableTargetControl_clicked(bool checked)
{
    m_settings.m_targetControlEnabled = checked;
    m_settingsKeys.append("targetControlEnabled");
    applySettings();
}

void GS232ControllerGUI::on_enableOffsetControl_clicked(bool checked)
{
    m_settings.m_offsetControlEnabled  = checked;
    m_settingsKeys.append("offsetControlEnabled");
    applySettings();
}

void GS232ControllerGUI::inputConfigurationComplete()
{
    m_settingsKeys.append("inputControllerSettings");
    applySettings();
}

GS232ControllerGUI::~GS232ControllerGUI()
{
    m_dfmStatusDialog.close();
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
    ui->precision->setValue(m_settings.m_precision); // Must set before protocol and az/el
    ui->protocol->setCurrentIndex((int)m_settings.m_protocol);
    ui->coordinates->setCurrentIndex((int)m_settings.m_coordinates);
    float coord1, coord2;
    azElToDisplay(m_settings.m_azimuth, m_settings.m_elevation, coord1, coord2);
    ui->coord1->setValue(coord1);
    ui->coord2->setValue(coord2);
    ui->connection->setCurrentIndex((int)m_settings.m_connection);
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
    ui->inputController->setCurrentText(m_settings.m_inputController);
    ui->highSensitivity->setChecked(m_settings.m_highSensitivity);
    ui->enableTargetControl->setChecked(m_settings.m_targetControlEnabled);
    ui->enableOffsetControl->setChecked(m_settings.m_offsetControlEnabled);
    ui->dfmTrack->setChecked(m_settings.m_dfmTrackOn);
    ui->dfmLubePumps->setChecked(m_settings.m_dfmLubePumpsOn);
    ui->dfmBrakes->setChecked(m_settings.m_dfmBrakesOn);
    ui->dfmDrives->setChecked(m_settings.m_dfmDrivesOn);
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

void GS232ControllerGUI::updateSerialPortList(const QStringList& serialPorts)
{
    ui->serialPort->blockSignals(true);
    ui->serialPort->clear();
    for (const auto& serialPort : serialPorts) {
        ui->serialPort->addItem(serialPort);
    }
    if (!m_settings.m_serialPort.isEmpty()) {
        ui->serialPort->setCurrentText(m_settings.m_serialPort);
    }
    ui->serialPort->blockSignals(false);
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
        m_settingsKeys.append("source");
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
        new DialogPositioner(&dialog, false);
        dialog.exec();

        m_settings.m_title = dialog.getTitle();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIFeatureSetIndex = dialog.getReverseAPIFeatureSetIndex();
        m_settings.m_reverseAPIFeatureIndex = dialog.getReverseAPIFeatureIndex();

        setTitle(m_settings.m_title);
        setTitleColor(m_settings.m_rgbColor);

        m_settingsKeys.append("title");
        m_settingsKeys.append("rgbColor");
        m_settingsKeys.append("useReverseAPI");
        m_settingsKeys.append("reverseAPIAddress");
        m_settingsKeys.append("reverseAPIPort");
        m_settingsKeys.append("reverseAPIFeatureSetIndex");
        m_settingsKeys.append("reverseAPIFeatureIndex");

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

void GS232ControllerGUI::setProtocol(GS232ControllerSettings::Protocol protocol)
{
    if (protocol == GS232ControllerSettings::GS232)
    {
        ui->precision->setValue(0);
        ui->precision->setEnabled(false);
        ui->precisionLabel->setEnabled(false);
    }
    else if (protocol == GS232ControllerSettings::SPID)
    {
        ui->precision->setValue(1);
        ui->precision->setEnabled(false);
        ui->precisionLabel->setEnabled(false);
    }
    else
    {
        ui->precision->setEnabled(true);
        ui->precisionLabel->setEnabled(true);
    }
    bool dfm = protocol == GS232ControllerSettings::DFM;
    ui->dfmLine->setVisible(dfm);
    ui->dfmTrack->setVisible(dfm);
    ui->dfmLubePumps->setVisible(dfm);
    ui->dfmBrakes->setVisible(dfm);
    ui->dfmDrives->setVisible(dfm);
    ui->dfmShowStatus->setVisible(dfm);

    // See RemoteControlGUI::createGUI() for additional weirdness in trying
    // to resize a window after widgets are changed
    getRollupContents()->arrangeRollups();
    layout()->activate(); // Recalculate sizeHint
    setMinimumSize(sizeHint());
    setMaximumSize(sizeHint());
    resize(sizeHint());
}

void GS232ControllerGUI::setPrecision()
{
    ui->coord1->setDecimals(m_settings.m_precision);
    ui->coord2->setDecimals(m_settings.m_precision);
    ui->tolerance->setDecimals(m_settings.m_precision);
    ui->azimuthOffset->setDecimals(m_settings.m_precision);
    ui->elevationOffset->setDecimals(m_settings.m_precision);
    double step = pow(10.0, -m_settings.m_precision);
    ui->coord1->setSingleStep(step);
    ui->coord2->setSingleStep(step);
    ui->tolerance->setSingleStep(step);
    ui->azimuthOffset->setSingleStep(step);
    ui->elevationOffset->setSingleStep(step);
}

void GS232ControllerGUI::on_protocol_currentIndexChanged(int index)
{
    m_settings.m_protocol = (GS232ControllerSettings::Protocol)index;
    setProtocol(m_settings.m_protocol);
    m_settingsKeys.append("protocol");
    applySettings();
}

void GS232ControllerGUI::on_connection_currentIndexChanged(int index)
{
    m_settings.m_connection = (GS232ControllerSettings::Connection)index;
    m_settingsKeys.append("connection");
    applySettings();
    updateConnectionWidgets();
}

void GS232ControllerGUI::on_serialPort_currentIndexChanged(int index)
{
    (void) index;
    m_settings.m_serialPort = ui->serialPort->currentText();
    m_settingsKeys.append("serialPort");
    applySettings();
}

void GS232ControllerGUI::on_baudRate_currentIndexChanged(int index)
{
    (void) index;
    m_settings.m_baudRate = ui->baudRate->currentText().toInt();
    m_settingsKeys.append("baudRate");
    applySettings();
}

void GS232ControllerGUI::on_host_editingFinished()
{
    m_settings.m_host = ui->host->text();
    m_settingsKeys.append("host");
    applySettings();
}

void GS232ControllerGUI::on_port_valueChanged(int value)
{
    m_settings.m_port = value;
    m_settingsKeys.append("port");
    applySettings();
}

void GS232ControllerGUI::on_coord1_valueChanged(double value)
{
    if (!m_inputUpdate) {
        m_inputCoord1 = value;
    }
    displayToAzEl(value, ui->coord2->value());
    ui->targetName->setText("");
}

void GS232ControllerGUI::on_coord2_valueChanged(double value)
{
    if (!m_inputUpdate) {
        m_inputCoord2 = value;
    }
    displayToAzEl(ui->coord1->value(), value);
    ui->targetName->setText("");
}

void GS232ControllerGUI::on_azimuthOffset_valueChanged(double value)
{
    if (!m_inputUpdate) {
        m_inputAzOffset = value;
    }
    m_settings.m_azimuthOffset = (float) value;
    m_settingsKeys.append("azimuthOffset");
    applySettings();
}

void GS232ControllerGUI::on_elevationOffset_valueChanged(double value)
{
    if (!m_inputUpdate) {
        m_inputElOffset = value;
    }
    m_settings.m_elevationOffset = (float) value;
    m_settingsKeys.append("elevationOffset");
    applySettings();
}

void GS232ControllerGUI::on_azimuthMin_valueChanged(int value)
{
    m_settings.m_azimuthMin = value;
    m_settingsKeys.append("azimuthMin");
    applySettings();
}

void GS232ControllerGUI::on_azimuthMax_valueChanged(int value)
{
    m_settings.m_azimuthMax = value;
    m_settingsKeys.append("azimuthMax");
    applySettings();
}

void GS232ControllerGUI::on_elevationMin_valueChanged(int value)
{
    m_settings.m_elevationMin = value;
    m_settingsKeys.append("elevationMin");
    applySettings();
}

void GS232ControllerGUI::on_elevationMax_valueChanged(int value)
{
    m_settings.m_elevationMax = value;
    m_settingsKeys.append("elevationMax");
    applySettings();
}

void GS232ControllerGUI::on_tolerance_valueChanged(double value)
{
    m_settings.m_tolerance = value;
    m_settingsKeys.append("tolerance");
    applySettings();
}

void GS232ControllerGUI::on_precision_valueChanged(int value)
{
    m_settings.m_precision = value;
    setPrecision();
    m_settingsKeys.append("precision");
    applySettings();
}

void GS232ControllerGUI::on_coordinates_currentIndexChanged(int index)
{
    m_settings.m_coordinates = (GS232ControllerSettings::Coordinates)index;
    m_settingsKeys.append("coordinates");
    applySettings();

    float coord1, coord2;
    azElToDisplay(m_settings.m_azimuth, m_settings.m_elevation, coord1, coord2);

    ui->coord1->blockSignals(true);
    if (m_settings.m_coordinates == GS232ControllerSettings::AZ_EL)
    {
        ui->coord1->setMinimum(0.0);
        ui->coord1->setMaximum(450.0);
        ui->coord1->setToolTip("Target azimuth in degrees");
        ui->coord1Label->setText("Azimuth");
        ui->coord1CurrentText->setToolTip("Current azimuth in degrees");
    }
    else
    {
        ui->coord1->setMinimum(-90.0);
        ui->coord1->setMaximum(90.0);
        ui->coord1->setToolTip("Target X in degrees");
        ui->coord1Label->setText("X");
        ui->coord1CurrentText->setToolTip("Current X coordinate in degrees");
    }
    ui->coord1->setValue(coord1);
    ui->coord1->blockSignals(false);
    ui->coord2->blockSignals(true);
    if (m_settings.m_coordinates == GS232ControllerSettings::AZ_EL)
    {
        ui->coord2->setMinimum(0.0);
        ui->coord2->setMaximum(180.0);
        ui->coord2->setToolTip("Target elevation in degrees");
        ui->coord2Label->setText("Elevation");
        ui->coord2CurrentText->setToolTip("Current elevation in degrees");
    }
    else
    {
        ui->coord2->setMinimum(-90.0);
        ui->coord2->setMaximum(90.0);
        ui->coord2->setToolTip("Target Y in degrees");
        ui->coord2Label->setText("Y");
        ui->coord2CurrentText->setToolTip("Current Y coordinate in degrees");
    }
    ui->coord2->setValue(coord2);
    ui->coord2->blockSignals(false);
}

void GS232ControllerGUI::on_track_stateChanged(int state)
{
    m_settings.m_track = state == Qt::Checked;
    ui->targetName->setEnabled(m_settings.m_track);
    ui->sources->setEnabled(m_settings.m_track);

    if (!m_settings.m_track) {
        ui->targetName->setText("");
    }

    m_settingsKeys.append("track");
    applySettings();
}

void GS232ControllerGUI::on_sources_currentTextChanged(const QString& text)
{
    qDebug("GS232ControllerGUI::on_sources_currentTextChanged: %s", qPrintable(text));
    m_settings.m_source = text;
    ui->targetName->setText("");
    m_settingsKeys.append("source");
    applySettings();
}

void GS232ControllerGUI::on_dfmTrack_clicked(bool checked)
{
    m_settings.m_dfmTrackOn = checked;
    m_settingsKeys.append("dfmTrackOn");
    applySettings();
}

void GS232ControllerGUI::on_dfmLubePumps_clicked(bool checked)
{
    m_settings.m_dfmLubePumpsOn = checked;
    m_settingsKeys.append("dfmLubePumpsOn");
    applySettings();
}

void GS232ControllerGUI::on_dfmBrakes_clicked(bool checked)
{
    m_settings.m_dfmBrakesOn = checked;
    m_settingsKeys.append("dfmBrakesOn");
    applySettings();
}

void GS232ControllerGUI::on_dfmDrives_clicked(bool checked)
{
    m_settings.m_dfmDrivesOn = checked;
    m_settingsKeys.append("dfmDrivesOn");
    applySettings();
}

void GS232ControllerGUI::on_dfmShowStatus_clicked()
{
    m_dfmStatusDialog.show();
    m_dfmStatusDialog.raise();
    m_dfmStatusDialog.activateWindow();
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
        GS232Controller::MsgConfigureGS232Controller* message = GS232Controller::MsgConfigureGS232Controller::create(m_settings, m_settingsKeys, force);
        m_gs232Controller->getInputMessageQueue()->push(message);
    }

    m_settingsKeys.clear();
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
    QObject::connect(ui->coord1, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &GS232ControllerGUI::on_coord1_valueChanged);
    QObject::connect(ui->coord2, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &GS232ControllerGUI::on_coord2_valueChanged);
    QObject::connect(ui->sources, &QComboBox::currentTextChanged, this, &GS232ControllerGUI::on_sources_currentTextChanged);
    QObject::connect(ui->azimuthOffset, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &GS232ControllerGUI::on_azimuthOffset_valueChanged);
    QObject::connect(ui->elevationOffset, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &GS232ControllerGUI::on_elevationOffset_valueChanged);
    QObject::connect(ui->azimuthMin, qOverload<int>(&QSpinBox::valueChanged), this, &GS232ControllerGUI::on_azimuthMin_valueChanged);
    QObject::connect(ui->azimuthMax, qOverload<int>(&QSpinBox::valueChanged), this, &GS232ControllerGUI::on_azimuthMax_valueChanged);
    QObject::connect(ui->elevationMin, qOverload<int>(&QSpinBox::valueChanged), this, &GS232ControllerGUI::on_elevationMin_valueChanged);
    QObject::connect(ui->elevationMax, qOverload<int>(&QSpinBox::valueChanged), this, &GS232ControllerGUI::on_elevationMax_valueChanged);
    QObject::connect(ui->tolerance, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &GS232ControllerGUI::on_tolerance_valueChanged);
    QObject::connect(ui->precision, qOverload<int>(&QSpinBox::valueChanged), this, &GS232ControllerGUI::on_precision_valueChanged);
    QObject::connect(ui->coordinates, qOverload<int>(&QComboBox::currentIndexChanged), this, &GS232ControllerGUI::on_coordinates_currentIndexChanged);
    QObject::connect(ui->inputController, qOverload<int>(&QComboBox::currentIndexChanged), this, &GS232ControllerGUI::on_inputController_currentIndexChanged);
    QObject::connect(ui->inputConfigure, &QToolButton::clicked, this, &GS232ControllerGUI::on_inputConfigure_clicked);
    QObject::connect(ui->highSensitivity, &QToolButton::clicked, this, &GS232ControllerGUI::on_highSensitivity_clicked);
    QObject::connect(ui->enableTargetControl, &QToolButton::clicked, this, &GS232ControllerGUI::on_enableTargetControl_clicked);
    QObject::connect(ui->enableOffsetControl, &QToolButton::clicked, this, &GS232ControllerGUI::on_enableOffsetControl_clicked);
    QObject::connect(ui->dfmTrack, &QToolButton::toggled, this, &GS232ControllerGUI::on_dfmTrack_clicked);
    QObject::connect(ui->dfmLubePumps, &QToolButton::toggled, this, &GS232ControllerGUI::on_dfmLubePumps_clicked);
    QObject::connect(ui->dfmBrakes, &QToolButton::toggled, this, &GS232ControllerGUI::on_dfmBrakes_clicked);
    QObject::connect(ui->dfmDrives, &QToolButton::toggled, this, &GS232ControllerGUI::on_dfmDrives_clicked);
    QObject::connect(ui->dfmShowStatus, &QToolButton::clicked, this, &GS232ControllerGUI::on_dfmShowStatus_clicked);
}

