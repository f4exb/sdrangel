///////////////////////////////////////////////////////////////////////////////////
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

#include <QMessageBox>

#include "feature/featureuiset.h"
#include "device/deviceset.h"
#include "channel/channelapi.h"
#include "gui/basicfeaturesettingsdialog.h"
#include "maincore.h"

#include "ui_afcgui.h"
#include "afcreport.h"
#include "afcgui.h"

AFCGUI* AFCGUI::create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature)
{
	AFCGUI* gui = new AFCGUI(pluginAPI, featureUISet, feature);
	return gui;
}

void AFCGUI::destroy()
{
	delete this;
}

void AFCGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
	applySettings(true);
}

QByteArray AFCGUI::serialize() const
{
    return m_settings.serialize();
}

bool AFCGUI::deserialize(const QByteArray& data)
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

bool AFCGUI::handleMessage(const Message& message)
{
    if (AFC::MsgConfigureAFC::match(message))
    {
        qDebug("AFCGUI::handleMessage: AFC::MsgConfigureAFC");
        const AFC::MsgConfigureAFC& cfg = (AFC::MsgConfigureAFC&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);

        return true;
    }
    else if (AFCReport::MsgUpdateTarget::match(message))
    {
        const AFCReport::MsgUpdateTarget& cfg = (AFCReport::MsgUpdateTarget&) message;

        if (cfg.getFrequencyChanged()) {
            ui->statusIndicator->setStyleSheet("QLabel { background-color: rgb(232, 85, 85); border-radius: 8px; }"); // red
        } else {
            ui->statusIndicator->setStyleSheet("QLabel { background-color: rgb(85, 232, 85); border-radius: 8px; }"); // green
        }

        ui->statusIndicator->setToolTip(tr("%1 Hz").arg(cfg.getFrequencyAdjustment()));
        m_autoTargetStatusTimer.start(500);
        return true;
    }
    else if (AFC::MsgDeviceSetListsReport::match(message))
    {
        const AFC::MsgDeviceSetListsReport& report = (AFC::MsgDeviceSetListsReport&) message;
        updateDeviceSetLists(report);
    }

	return false;
}

void AFCGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()))
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void AFCGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

AFCGUI::AFCGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent) :
	FeatureGUI(parent),
	ui(new Ui::AFCGUI),
	m_pluginAPI(pluginAPI),
    m_featureUISet(featureUISet),
	m_doApplySettings(true),
    m_lastFeatureState(0)
{
    m_feature = feature;
	setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/feature/afc/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

    ui->targetFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->targetFrequency->setValueRange(10, 0, 9999999999L);

    ui->toleranceFrequency->setColorMapper(ColorMapper(ColorMapper::GrayYellow));
    ui->toleranceFrequency->setValueRange(5, 0, 99999L);

    m_afc = reinterpret_cast<AFC*>(feature);
    m_afc->setMessageQueueToGUI(&m_inputMessageQueue);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(1000);

    connect(&m_autoTargetStatusTimer, SIGNAL(timeout()), this, SLOT(resetAutoTargetStatus()));
    m_autoTargetStatusTimer.setSingleShot(true);
    ui->statusIndicator->setStyleSheet("QLabel { background-color: gray; border-radius: 8px; }");

    m_settings.setRollupState(&m_rollupState);

    requestDeviceSetLists();
    displaySettings();
	applySettings(true);
    makeUIConnections();
}

AFCGUI::~AFCGUI()
{
	delete ui;
}

void AFCGUI::setWorkspaceIndex(int index)
{
    m_settings.m_workspaceIndex = index;
    m_feature->setWorkspaceIndex(index);
}

void AFCGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void AFCGUI::displaySettings()
{
    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_settings.m_title);
    setTitle(m_settings.m_title);
    blockApplySettings(true);
    ui->hasTargetFrequency->setChecked(m_settings.m_hasTargetFrequency);
    ui->transverterTarget->setChecked(m_settings.m_transverterTarget);
    ui->targetFrequency->setValue(m_settings.m_targetFrequency);
    ui->toleranceFrequency->setValue(m_settings.m_freqTolerance);
    ui->targetPeriod->setValue(m_settings.m_trackerAdjustPeriod);
    ui->targetPeriodText->setText(tr("%1").arg(m_settings.m_trackerAdjustPeriod));
    getRollupContents()->restoreState(m_rollupState);
    blockApplySettings(false);
}

void AFCGUI::requestDeviceSetLists()
{
    AFC::MsgDeviceSetListsQuery *msg = AFC::MsgDeviceSetListsQuery::create();
    m_afc->getInputMessageQueue()->push(msg);
}

void AFCGUI::updateDeviceSetLists(const AFC::MsgDeviceSetListsReport& report)
{
    ui->trackerDevice->blockSignals(true);
    ui->trackedDevice->blockSignals(true);

    ui->trackerDevice->clear();
    ui->trackedDevice->clear();

    for (const auto& deviceSetRef : report.getTrackerDevices()) {
        ui->trackerDevice->addItem(QString("R%1").arg(deviceSetRef.m_deviceIndex), deviceSetRef.m_deviceIndex);
    }

    for (const auto& deviceSetRef : report.getTrackedDevices())
    {
        if (deviceSetRef.m_rx) {
            ui->trackedDevice->addItem(QString("R%1").arg(deviceSetRef.m_deviceIndex), deviceSetRef.m_deviceIndex);
        } else {
            ui->trackedDevice->addItem(QString("T%1").arg(deviceSetRef.m_deviceIndex), deviceSetRef.m_deviceIndex);
        }
    }

    int trackedDeviceIndex;
    int trackerDeviceIndex;

    if ((report.getTrackerDevices().size() > 0) && (report.getTrackedDevices().size() > 0))
    {
        if (m_settings.m_trackedDeviceSetIndex < 0) {
            ui->trackedDevice->setCurrentIndex(0);
        } else {
            ui->trackedDevice->setCurrentIndex(m_settings.m_trackedDeviceSetIndex);
        }

        if (m_settings.m_trackerDeviceSetIndex < 0) {
            ui->trackerDevice->setCurrentIndex(0);
        } else {
            ui->trackerDevice->setCurrentIndex(m_settings.m_trackerDeviceSetIndex);
        }

        trackedDeviceIndex = ui->trackedDevice->currentData().toInt();
        trackerDeviceIndex = ui->trackerDevice->currentData().toInt();
    }
    else
    {
        trackedDeviceIndex = -1;
        trackerDeviceIndex = -1;
    }

    if ((trackedDeviceIndex != m_settings.m_trackedDeviceSetIndex) ||
        (trackerDeviceIndex != m_settings.m_trackerDeviceSetIndex))
    {
        qDebug("AFCGUI::updateDeviceSetLists: device index changed: %d:%d", trackerDeviceIndex, trackedDeviceIndex);
        m_settings.m_trackerDeviceSetIndex = trackerDeviceIndex;
        m_settings.m_trackedDeviceSetIndex = trackedDeviceIndex;
        applySettings();
    }

    ui->trackerDevice->blockSignals(false);
    ui->trackedDevice->blockSignals(false);
}

void AFCGUI::onMenuDialogCalled(const QPoint &p)
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

void AFCGUI::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        AFC::MsgStartStop *message = AFC::MsgStartStop::create(checked);
        m_afc->getInputMessageQueue()->push(message);
    }
}

void AFCGUI::on_hasTargetFrequency_toggled(bool checked)
{
    m_settings.m_hasTargetFrequency = checked;
    applySettings();
}

void AFCGUI::on_targetFrequency_changed(quint64 value)
{
    m_settings.m_targetFrequency = value;
    applySettings();
}

void AFCGUI::on_transverterTarget_toggled(bool checked)
{
    m_settings.m_transverterTarget = checked;
    applySettings();
}


void AFCGUI::on_toleranceFrequency_changed(quint64 value)
{
    m_settings.m_freqTolerance = value;
    applySettings();
}

void AFCGUI::on_deviceTrack_clicked()
{
    AFC::MsgDeviceTrack *msg = AFC::MsgDeviceTrack::create();
    m_afc->getInputMessageQueue()->push(msg);
}

void AFCGUI::on_devicesRefresh_clicked()
{
    requestDeviceSetLists();
}

void AFCGUI::on_trackerDevice_currentIndexChanged(int index)
{
    if (index >= 0)
    {
        m_settings.m_trackerDeviceSetIndex = index;
        applySettings();
    }
}

void AFCGUI::on_trackedDevice_currentIndexChanged(int index)
{
    if (index >= 0)
    {
        m_settings.m_trackedDeviceSetIndex = index;
        applySettings();
    }
}

void AFCGUI::on_devicesApply_clicked()
{
    AFC::MsgDevicesApply *msg = AFC::MsgDevicesApply::create();
    m_afc->getInputMessageQueue()->push(msg);
}

void AFCGUI::on_targetPeriod_valueChanged(int value)
{
    m_settings.m_trackerAdjustPeriod = value;
    ui->targetPeriodText->setText(tr("%1").arg(m_settings.m_trackerAdjustPeriod));
    applySettings();
}

void AFCGUI::updateStatus()
{
    int state = m_afc->getState();

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
                QMessageBox::information(this, tr("Message"), m_afc->getErrorMessage());
                break;
            default:
                break;
        }

        m_lastFeatureState = state;
    }
}

void AFCGUI::resetAutoTargetStatus()
{
    ui->statusIndicator->setStyleSheet("QLabel { background-color: gray; border-radius: 8px; }");
}

void AFCGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
	    AFC::MsgConfigureAFC* message = AFC::MsgConfigureAFC::create( m_settings, force);
	    m_afc->getInputMessageQueue()->push(message);
	}
}

void AFCGUI::makeUIConnections()
{
	QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &AFCGUI::on_startStop_toggled);
	QObject::connect(ui->hasTargetFrequency, &ButtonSwitch::toggled, this, &AFCGUI::on_hasTargetFrequency_toggled);
    QObject::connect(ui->targetFrequency, &ValueDial::changed, this, &AFCGUI::on_targetFrequency_changed);
	QObject::connect(ui->transverterTarget, &ButtonSwitch::toggled, this, &AFCGUI::on_transverterTarget_toggled);
	QObject::connect(ui->toleranceFrequency, &ValueDial::changed, this, &AFCGUI::on_toleranceFrequency_changed);
	QObject::connect(ui->deviceTrack, &QPushButton::clicked, this, &AFCGUI::on_deviceTrack_clicked);
	QObject::connect(ui->devicesRefresh, &QPushButton::clicked, this, &AFCGUI::on_devicesRefresh_clicked);
	QObject::connect(ui->trackerDevice, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AFCGUI::on_trackerDevice_currentIndexChanged);
	QObject::connect(ui->trackedDevice, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AFCGUI::on_trackedDevice_currentIndexChanged);
	QObject::connect(ui->devicesApply, &QPushButton::clicked, this, &AFCGUI::on_devicesApply_clicked);
	QObject::connect(ui->targetPeriod, &QDial::valueChanged, this, &AFCGUI::on_targetPeriod_valueChanged);
}
