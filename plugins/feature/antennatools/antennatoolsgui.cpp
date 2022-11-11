///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#include <QResizeEvent>

#include "feature/featureuiset.h"
#include "gui/basicfeaturesettingsdialog.h"
#include "channel/channelwebapiutils.h"
#include "mainwindow.h"
#include "maincore.h"
#include "device/deviceuiset.h"
#include "util/units.h"

#include "ui_antennatoolsgui.h"
#include "antennatools.h"
#include "antennatoolsgui.h"

AntennaToolsGUI* AntennaToolsGUI::create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature)
{
    AntennaToolsGUI* gui = new AntennaToolsGUI(pluginAPI, featureUISet, feature);
    return gui;
}

void AntennaToolsGUI::destroy()
{
    delete this;
}

void AntennaToolsGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray AntennaToolsGUI::serialize() const
{
    return m_settings.serialize();
}

bool AntennaToolsGUI::deserialize(const QByteArray& data)
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

bool AntennaToolsGUI::handleMessage(const Message& message)
{
    if (AntennaTools::MsgConfigureAntennaTools::match(message))
    {
        qDebug("AntennaToolsGUI::handleMessage: AntennaTools::MsgConfigureAntennaTools");
        const AntennaTools::MsgConfigureAntennaTools& cfg = (AntennaTools::MsgConfigureAntennaTools&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);

        return true;
    }

    return false;
}

void AntennaToolsGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()))
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void AntennaToolsGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    RollupContents *rollupContents = getRollupContents();

    rollupContents->saveState(m_rollupState);
    applySettings();
}

AntennaToolsGUI::AntennaToolsGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent) :
    FeatureGUI(parent),
    ui(new Ui::AntennaToolsGUI),
    m_pluginAPI(pluginAPI),
    m_featureUISet(featureUISet),
    m_doApplySettings(true),
    m_deviceSets(0)
{
    m_feature = feature;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/feature/antennatools/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

    m_antennatools = reinterpret_cast<AntennaTools*>(feature);
    m_antennatools->setMessageQueueToGUI(&m_inputMessageQueue);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    // Rather than polling, could we get a message when frequencies change?
    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(500);

    m_settings.setRollupState(&m_rollupState);

    displaySettings();
    applySettings(true);
    makeUIConnections();
}

AntennaToolsGUI::~AntennaToolsGUI()
{
    delete ui;
}

void AntennaToolsGUI::setWorkspaceIndex(int index)
{
    m_settings.m_workspaceIndex = index;
    m_feature->setWorkspaceIndex(index);
}

void AntennaToolsGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void AntennaToolsGUI::displaySettings()
{
    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_settings.m_title);
    setTitle(m_settings.m_title);
    blockApplySettings(true);
    ui->dipoleFrequency->setValue(m_settings.m_dipoleFrequencyMHz);
    ui->dipoleFrequencySelect->setCurrentIndex(m_settings.m_dipoleFrequencySelect);
    ui->dipoleEndEffectFactor->setValue(m_settings.m_dipoleEndEffectFactor);
    ui->dipoleLengthUnits->setCurrentIndex((int)m_settings.m_dipoleLengthUnits);
    ui->dishFrequency->setValue(m_settings.m_dishFrequencyMHz);
    ui->dishFrequencySelect->setCurrentIndex(m_settings.m_dishFrequencySelect);
    ui->dishDiameter->setValue(m_settings.m_dishDiameter);
    ui->dishDepth->setValue(m_settings.m_dishDepth);
    ui->dishLengthUnits->setCurrentIndex((int)m_settings.m_dishLengthUnits);
    ui->dishEfficiency->setValue(m_settings.m_dishEfficiency);
    ui->dishSurfaceError->setValue(m_settings.m_dishSurfaceError);
    blockApplySettings(false);
    calcDipoleLength();
    calcDishFocalLength();
    calcDishBeamwidth();
    calcDishGain();
    calcDishEffectiveArea();
    getRollupContents()->restoreState(m_rollupState);
}

void AntennaToolsGUI::makeUIConnections()
{
    QObject::connect(ui->dipoleFrequency, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &AntennaToolsGUI::on_dipoleFrequency_valueChanged);
    QObject::connect(ui->dipoleFrequencySelect, qOverload<int>(&QComboBox::currentIndexChanged), this, &AntennaToolsGUI::on_dipoleFrequencySelect_currentIndexChanged);
    QObject::connect(ui->dipoleEndEffectFactor, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &AntennaToolsGUI::on_dipoleEndEffectFactor_valueChanged);
    QObject::connect(ui->dipoleLengthUnits, qOverload<int>(&QComboBox::currentIndexChanged), this, &AntennaToolsGUI::on_dipoleLengthUnits_currentIndexChanged);
    QObject::connect(ui->dipoleLength, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &AntennaToolsGUI::on_dipoleLength_valueChanged);
    QObject::connect(ui->dipoleElementLength, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &AntennaToolsGUI::on_dipoleElementLength_valueChanged);
    QObject::connect(ui->dishFrequency, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &AntennaToolsGUI::on_dishFrequency_valueChanged);
    QObject::connect(ui->dishFrequencySelect, qOverload<int>(&QComboBox::currentIndexChanged), this, &AntennaToolsGUI::on_dishFrequencySelect_currentIndexChanged);
    QObject::connect(ui->dishDiameter, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &AntennaToolsGUI::on_dishDiameter_valueChanged);
    QObject::connect(ui->dishLengthUnits, qOverload<int>(&QComboBox::currentIndexChanged), this, &AntennaToolsGUI::on_dishLengthUnits_currentIndexChanged);
    QObject::connect(ui->dishDepth, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &AntennaToolsGUI::on_dishDepth_valueChanged);
    QObject::connect(ui->dishEfficiency, qOverload<int>(&QSpinBox::valueChanged), this, &AntennaToolsGUI::on_dishEfficiency_valueChanged);
    QObject::connect(ui->dishSurfaceError, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &AntennaToolsGUI::on_dishSurfaceError_valueChanged);
}

void AntennaToolsGUI::onMenuDialogCalled(const QPoint &p)
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

void AntennaToolsGUI::updateStatus()
{
    // Update device sets listed in frequency select combos
    std::vector<DeviceSet*> deviceSets = MainCore::instance()->getDeviceSets();
    if (deviceSets.size() < m_deviceSets)
    {
        int countToRemove = m_deviceSets - deviceSets.size();
        for (int i = 0; i < countToRemove; i++)
        {
            ui->dipoleFrequencySelect->removeItem(ui->dipoleFrequencySelect->count() - 1);
            ui->dishFrequencySelect->removeItem(ui->dishFrequencySelect->count() - 1);
        }
    }
    else if (deviceSets.size() > m_deviceSets)
    {
        int countToAdd = deviceSets.size() - m_deviceSets;
        for (int i = 0; i < countToAdd; i++)
        {
            ui->dipoleFrequencySelect->addItem(QString("Device set %1").arg(ui->dipoleFrequencySelect->count() - 1));
            ui->dishFrequencySelect->addItem(QString("Device set %1").arg(ui->dishFrequencySelect->count() - 1));
        }
    }
    m_deviceSets = deviceSets.size();

    // Update frequencies to match device set centre frequency
    if (m_settings.m_dipoleFrequencySelect >= 1)
    {
        double frequency = getDeviceSetFrequencyMHz(m_settings.m_dipoleFrequencySelect - 1);
        if (frequency >= 0.0) {
            ui->dipoleFrequency->setValue(frequency);
        }
    }
    if (m_settings.m_dishFrequencySelect >= 1)
    {
        double frequency = getDeviceSetFrequencyMHz(m_settings.m_dishFrequencySelect - 1);
        if (frequency >= 0.0) {
            ui->dishFrequency->setValue(frequency);
        }
    }
}

void AntennaToolsGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        AntennaTools::MsgConfigureAntennaTools* message = AntennaTools::MsgConfigureAntennaTools::create(m_settings, force);
        m_antennatools->getInputMessageQueue()->push(message);
    }
}

void AntennaToolsGUI::calcDipoleLength()
{
    // Length of dipole in freespace is half wavelength
    // End effect depends on ratio of thickness to length, insulation and distance to ground.
    // Is there a formula for this?
    double lengthMetres = 0.5 * 299.792458 * m_settings.m_dipoleEndEffectFactor / m_settings.m_dipoleFrequencyMHz;
    ui->dipoleLength->blockSignals(true);
    ui->dipoleElementLength->blockSignals(true);
    if (m_settings.m_dipoleLengthUnits == AntennaToolsSettings::M)
    {
        ui->dipoleLength->setValue(lengthMetres);
        ui->dipoleElementLength->setValue(lengthMetres/2.0);
    }
    else if (m_settings.m_dipoleLengthUnits == AntennaToolsSettings::CM)
    {
        ui->dipoleLength->setValue(lengthMetres*100.0);
        ui->dipoleElementLength->setValue(lengthMetres/2.0*100.0);
    }
    else
    {
        ui->dipoleLength->setValue(Units::metresToFeet(lengthMetres));
        ui->dipoleElementLength->setValue(Units::metresToFeet(lengthMetres/2.0));
    }
    ui->dipoleLength->blockSignals(false);
    ui->dipoleElementLength->blockSignals(false);
}

double AntennaToolsGUI::calcDipoleFrequency(double totalLength)
{
    double lengthMetres;
    if (m_settings.m_dipoleLengthUnits == AntennaToolsSettings::M) {
        lengthMetres = totalLength;
    } else if  (m_settings.m_dipoleLengthUnits == AntennaToolsSettings::CM) {
        lengthMetres = totalLength / 100.0;
    } else {
        lengthMetres = Units::feetToMetres(totalLength);
    }
    return 0.5 * 299.792458 * m_settings.m_dipoleEndEffectFactor / lengthMetres;
}

void AntennaToolsGUI::on_dipoleFrequencySelect_currentIndexChanged(int index)
{
    m_settings.m_dipoleFrequencySelect = index;
    applySettings();
    if (index >= 1)
    {
        double frequency = getDeviceSetFrequencyMHz(index - 1);
        if (frequency >= 0.0) {
            ui->dipoleFrequency->setValue(frequency);
        }
    }
    ui->dipoleFrequency->setReadOnly(index >= 1);
    ui->dipoleLength->setReadOnly(index >= 1);
    ui->dipoleElementLength->setReadOnly(index >= 1);
}

void AntennaToolsGUI::on_dipoleFrequency_valueChanged(double value)
{
    m_settings.m_dipoleFrequencyMHz = value;
    applySettings();
    calcDipoleLength();
}

void AntennaToolsGUI::on_dipoleEndEffectFactor_valueChanged(double value)
{
    m_settings.m_dipoleEndEffectFactor = value;
    applySettings();
    calcDipoleLength();
}

void AntennaToolsGUI::on_dipoleLengthUnits_currentIndexChanged(int index)
{
    m_settings.m_dipoleLengthUnits = (AntennaToolsSettings::LengthUnits)index;
    applySettings();
    calcDipoleLength();
}

void AntennaToolsGUI::on_dipoleLength_valueChanged(double value)
{
    m_settings.m_dipoleFrequencyMHz = calcDipoleFrequency(value);
    applySettings();
    ui->dipoleElementLength->blockSignals(true);
    ui->dipoleElementLength->setValue(value/2.0);
    ui->dipoleElementLength->blockSignals(false);
    ui->dipoleFrequency->blockSignals(true);
    ui->dipoleFrequency->setValue(m_settings.m_dipoleFrequencyMHz);
    ui->dipoleFrequency->blockSignals(false);
}

void AntennaToolsGUI::on_dipoleElementLength_valueChanged(double value)
{
    m_settings.m_dipoleFrequencyMHz = calcDipoleFrequency(value*2.0);
    applySettings();
    ui->dipoleLength->blockSignals(true);
    ui->dipoleLength->setValue(value*2.0);
    ui->dipoleLength->blockSignals(false);
    ui->dipoleFrequency->blockSignals(true);
    ui->dipoleFrequency->setValue(m_settings.m_dipoleFrequencyMHz);
    ui->dipoleFrequency->blockSignals(false);
}

double AntennaToolsGUI::dishLambda() const
{
    return 299.792458 / m_settings.m_dishFrequencyMHz;
}

double AntennaToolsGUI::dishLengthMetres(double length) const
{
    if (m_settings.m_dishLengthUnits == AntennaToolsSettings::CM) {
        return length / 100.0;
    } else if (m_settings.m_dishLengthUnits == AntennaToolsSettings::M) {
        return length;
    } else {
        return Units::feetToMetres(length);
    }
}

double AntennaToolsGUI::dishMetresToLength(double m) const
{
    if (m_settings.m_dishLengthUnits == AntennaToolsSettings::CM) {
        return m * 100.0;
    } else if (m_settings.m_dishLengthUnits == AntennaToolsSettings::M) {
        return m;
    } else {
        return Units::metresToFeet(m);
    }
}

double AntennaToolsGUI::dishDiameterMetres() const
{
    return dishLengthMetres(m_settings.m_dishDiameter);
}

double AntennaToolsGUI::dishDepthMetres() const
{
    return dishLengthMetres(m_settings.m_dishDepth);
}

double AntennaToolsGUI::dishSurfaceErrorMetres() const
{
    return dishLengthMetres(m_settings.m_dishSurfaceError);
}

void AntennaToolsGUI::calcDishFocalLength()
{
    double d = dishDiameterMetres();
    double focalLength = d * d / (16.0 * dishDepthMetres());
    ui->dishFocalLength->setValue(dishMetresToLength(focalLength));
    double fd = focalLength / d;
    ui->dishFD->setValue(fd);
}

void AntennaToolsGUI::calcDishBeamwidth()
{
    // The constant here depends on the illumination tapering.
    // 1.15 equals about 10dB: (4.14) in Fundamentals of Radio Astronomy
    // 1.2 is also a commonly used value: https://www.cv.nrao.edu/~sransom/web/Ch3.html#E96
    double beamwidth = Units::radiansToDegrees(1.15 * dishLambda() / dishDiameterMetres());
    ui->dishBeamwidth->setValue(beamwidth);
}

void AntennaToolsGUI::calcDishGain()
{
    double t = M_PI * dishDiameterMetres() / dishLambda();
    double gainDB = 10.0 * log10((m_settings.m_dishEfficiency/100.0) * (t*t));
    // Adjust for surface error using Ruze's equation
    t = dishSurfaceErrorMetres() / dishLambda();
    gainDB = gainDB - 685.81 * t * t;
    ui->dishGain->setValue(gainDB);
}

void AntennaToolsGUI::calcDishEffectiveArea()
{
    double gainDB = ui->dishGain->value();
    double g = pow(10.0, gainDB/10.0);
    double lambda = dishLambda();
    double ae = g * lambda * lambda / (4.0 * M_PI);
    ui->dishEffectiveArea->setValue(ae);
}

void AntennaToolsGUI::on_dishFrequency_valueChanged(double value)
{
    m_settings.m_dishFrequencyMHz = value;
    applySettings();
    calcDishBeamwidth();
    calcDishGain();
    calcDishEffectiveArea();
}

void AntennaToolsGUI::on_dishFrequencySelect_currentIndexChanged(int index)
{
    m_settings.m_dishFrequencySelect = index;
    applySettings();
    if (index >= 1)
    {
        double frequency = getDeviceSetFrequencyMHz(index - 1);
        if (frequency >= 0.0) {
            ui->dishFrequency->setValue(frequency);
        }
    }
    ui->dishFrequency->setReadOnly(index >= 1);
}

void AntennaToolsGUI::on_dishDiameter_valueChanged(double value)
{
    m_settings.m_dishDiameter = value;
    applySettings();
    calcDishFocalLength();
    calcDishBeamwidth();
    calcDishGain();
    calcDishEffectiveArea();
}

void AntennaToolsGUI::on_dishLengthUnits_currentIndexChanged(int index)
{
    m_settings.m_dishLengthUnits = (AntennaToolsSettings::LengthUnits)index;
    applySettings();
    calcDishFocalLength();
    calcDishBeamwidth();
    calcDishGain();
    calcDishEffectiveArea();
}

void AntennaToolsGUI::on_dishDepth_valueChanged(double value)
{
    m_settings.m_dishDepth = value;
    applySettings();
    calcDishFocalLength();
}

void AntennaToolsGUI::on_dishEfficiency_valueChanged(int value)
{
    m_settings.m_dishEfficiency = value;
    applySettings();
    calcDishGain();
    calcDishEffectiveArea();
}

void AntennaToolsGUI::on_dishSurfaceError_valueChanged(double value)
{
    m_settings.m_dishSurfaceError= value;
    applySettings();
    calcDishGain();
    calcDishEffectiveArea();
}

double AntennaToolsGUI::getDeviceSetFrequencyMHz(int index)
{
    std::vector<DeviceSet*> deviceSets = MainCore::instance()->getDeviceSets();
    if (index < (int)deviceSets.size())
    {
        double frequencyInHz;
        if (ChannelWebAPIUtils::getCenterFrequency(index, frequencyInHz))
        {
            return frequencyInHz / 1e6;
        }
        else
        {
            return -1.0;
        }
    }
    else
    {
        return -1.0;
    }
}
