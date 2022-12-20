///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB                                   //
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

#include "feature/featureuiset.h"
#include "gui/basicfeaturesettingsdialog.h"
#include "gui/crightclickenabler.h"
#include "gui/dialogpositioner.h"

#include "ui_ambegui.h"
#include "ambegui.h"
#include "ambe.h"

AMBEGUI* AMBEGUI::create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature)
{
	AMBEGUI* gui = new AMBEGUI(pluginAPI, featureUISet, feature);
	return gui;
}

void AMBEGUI::destroy()
{
	delete this;
}

AMBEGUI::AMBEGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent) :
	FeatureGUI(parent),
	ui(new Ui::AMBEGUI),
	m_pluginAPI(pluginAPI),
    m_featureUISet(featureUISet),
	m_doApplySettings(true)
{
    m_feature = feature;
	setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/feature/ambe/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

    m_ambe = reinterpret_cast<AMBE*>(feature);
    m_ambe->setMessageQueueToGUI(&m_inputMessageQueue);

    m_settings.setRollupState(&m_rollupState);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    populateSerialList();
    refreshInUseList();
    displaySettings();
    makeUIConnections();
}

AMBEGUI::~AMBEGUI()
{
	delete ui;
}

void AMBEGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
	applySettings(true);
}

QByteArray AMBEGUI::serialize() const
{
    return m_settings.serialize();
}

bool AMBEGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        m_feature->setWorkspaceIndex(m_settings.m_workspaceIndex);
        displaySettings();
        refreshInUseList();
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void AMBEGUI::setWorkspaceIndex(int index)
{
    m_settings.m_workspaceIndex = index;
    m_feature->setWorkspaceIndex(index);
}

void AMBEGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void AMBEGUI::onMenuDialogCalled(const QPoint &p)
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

void AMBEGUI::displaySettings()
{
    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_settings.m_title);
    setTitle(m_settings.m_title);
}

void AMBEGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
	    AMBE::MsgConfigureAMBE* message = AMBE::MsgConfigureAMBE::create( m_settings, m_settingsKeys, force);
	    m_ambe->getInputMessageQueue()->push(message);
	}

    m_settingsKeys.clear();
}

bool AMBEGUI::handleMessage(const Message& message)
{
    if (AMBE::MsgConfigureAMBE::match(message))
    {
        qDebug("AMBEGUI::handleMessage: AMBE::MsgConfigureAMBE");
        const AMBE::MsgConfigureAMBE& cfg = (AMBE::MsgConfigureAMBE&) message;

        if (cfg.getForce()) {
            m_settings = cfg.getSettings();
        } else {
            m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
        }

        displaySettings();
        return true;
    }
    else if (AMBE::MsgReportDevices::match(message))
    {
        qDebug("AMBEGUI::handleMessage: AMBE::MsgReportDevices");
        AMBE::MsgReportDevices& cfg = (AMBE::MsgReportDevices&) message;
        ui->ambeSerialDevices->clear();
        ui->statusText->setText("Updated all devices lists");

        for (const auto& ambeDevice : cfg.getAvailableDevices()) {
            ui->ambeSerialDevices->addItem(ambeDevice);
        }

        ui->ambeDeviceRefs->clear();

        for (const auto& inUseDevice : cfg.getUsedDevices()) {
            ui->ambeDeviceRefs->addItem(
                tr("%1 - %2|%3")
                    .arg(inUseDevice.m_devicePath)
                    .arg(inUseDevice.m_successCount)
                    .arg(inUseDevice.m_failureCount)
            );
        }

        return true;
    }

	return false;
}

void AMBEGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()))
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void AMBEGUI::populateSerialList()
{
    QList<QString> ambeSerialDevices;
    m_ambe->getAMBEEngine()->scan(ambeSerialDevices);
    ui->ambeSerialDevices->clear();

    for (const auto& ambeDevice : ambeSerialDevices) {
        ui->ambeSerialDevices->addItem(ambeDevice);
    }
}

void AMBEGUI::refreshInUseList()
{
    QList<AMBEEngine::DeviceRef> inUseDevices;
    m_ambe->getAMBEEngine()->getDeviceRefs(inUseDevices);
    ui->ambeDeviceRefs->clear();

    for (const auto& inUseDevice : inUseDevices)
    {
        qDebug("AMBEGUI::refreshInUseList: %s", qPrintable(inUseDevice.m_devicePath));
        ui->ambeDeviceRefs->addItem(
            tr("%1 - %2|%3")
                .arg(inUseDevice.m_devicePath)
                .arg(inUseDevice.m_successCount)
                .arg(inUseDevice.m_failureCount)
        );
    }
}
void AMBEGUI::on_importSerial_clicked()
{
    QListWidgetItem *serialItem = ui->ambeSerialDevices->currentItem();

    if (!serialItem)
    {
        ui->statusText->setText("No selection");
        return;
    }

    QString serialName = serialItem->text();
    QList<QListWidgetItem*> foundItems = ui->ambeDeviceRefs->findItems(serialName, Qt::MatchFixedString|Qt::MatchCaseSensitive);

    if (foundItems.size() == 0)
    {
        if (m_ambe->getAMBEEngine()->registerController(serialName.toStdString()))
        {
            ui->ambeDeviceRefs->addItem(tr("%1 - 0|0").arg(serialName));
            ui->statusText->setText(tr("%1 added").arg(serialName));
        }
        else
        {
            ui->statusText->setText(tr("Cannot open %1").arg(serialName));
        }
    }
    else
    {
        ui->statusText->setText("Device already in use");
    }
}

void AMBEGUI::on_importAllSerial_clicked()
{
    int count = 0;

    for (int i = 0; i < ui->ambeSerialDevices->count(); i++)
    {
        const QListWidgetItem *serialItem = ui->ambeSerialDevices->item(i);
        QString serialName = serialItem->text();
        QList<QListWidgetItem*> foundItems = ui->ambeDeviceRefs->findItems(serialName, Qt::MatchFixedString|Qt::MatchCaseSensitive);

        if (foundItems.size() == 0)
        {
            if (m_ambe->getAMBEEngine()->registerController(serialName.toStdString()))
            {
                ui->ambeDeviceRefs->addItem(serialName);
                count++;
            }
        }
    }

    ui->statusText->setText(tr("%1 devices added").arg(count));
}

void AMBEGUI::on_removeAmbeDevice_clicked()
{
    QListWidgetItem *deviceItem = ui->ambeDeviceRefs->currentItem();

    if (!deviceItem)
    {
        ui->statusText->setText("No selection");
        return;
    }

    QString deviceName = deviceItem->text().split(" ").at(0);
    m_ambe->getAMBEEngine()->releaseController(deviceName.toStdString());
    ui->statusText->setText(tr("%1 removed").arg(deviceName));
    refreshInUseList();
}

void AMBEGUI::on_refreshAmbeList_clicked()
{
    refreshInUseList();
    ui->statusText->setText("In use refreshed");
}

void AMBEGUI::on_refreshSerial_clicked()
{
    populateSerialList();
    ui->statusText->setText("Serial refreshed");
}

void AMBEGUI::on_clearAmbeList_clicked()
{
    if (ui->ambeDeviceRefs->count() == 0)
    {
        ui->statusText->setText("No active items");
        return;
    }

    m_ambe->getAMBEEngine()->releaseAll();
    ui->ambeDeviceRefs->clear();
    ui->statusText->setText("All items released");
}

void AMBEGUI::on_importAddress_clicked()
{
    QString addressAndPort = ui->ambeAddressText->text();

    QList<QListWidgetItem*> foundItems = ui->ambeDeviceRefs->findItems(addressAndPort, Qt::MatchFixedString|Qt::MatchCaseSensitive);

    if (foundItems.size() == 0)
    {
        if (m_ambe->getAMBEEngine()->registerController(addressAndPort.toStdString()))
        {
            ui->ambeDeviceRefs->addItem(addressAndPort);
            ui->statusText->setText(tr("%1 added").arg(addressAndPort));
        }
        else
        {
            ui->statusText->setText(tr("Cannot open %1").arg(addressAndPort));
        }
    }
    else
    {
        ui->statusText->setText("Address already in use");
    }
}

void AMBEGUI::makeUIConnections()
{
    QObject::connect(ui->importSerial, &QPushButton::clicked, this, &AMBEGUI::on_importSerial_clicked);
    QObject::connect(ui->importAllSerial, &QPushButton::clicked, this, &AMBEGUI::on_importAllSerial_clicked);
    QObject::connect(ui->removeAmbeDevice, &QPushButton::clicked, this, &AMBEGUI::on_removeAmbeDevice_clicked);
    QObject::connect(ui->refreshAmbeList, &QPushButton::clicked, this, &AMBEGUI::on_refreshAmbeList_clicked);
    QObject::connect(ui->refreshSerial, &QPushButton::clicked, this, &AMBEGUI::on_refreshSerial_clicked);
    QObject::connect(ui->clearAmbeList, &QPushButton::clicked, this, &AMBEGUI::on_clearAmbeList_clicked);
    QObject::connect(ui->importAddress, &QPushButton::clicked, this, &AMBEGUI::on_importAddress_clicked);
}
