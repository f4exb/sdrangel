///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#include <QDebug>
#include <QHBoxLayout>
#include <QSizePolicy>

#include "satellitedevicesettingsgui.h"
#include "device/deviceset.h"
#include "settings/mainsettings.h"
#include "maincore.h"
#include "util/messagequeue.h"
#include "plugin/pluginmanager.h"
#include "plugin/pluginapi.h"

SatelliteDeviceSettingsGUI::SatelliteDeviceSettingsGUI(SatelliteTrackerSettings::SatelliteDeviceSettings *devSettings,
                                                       QTableWidget *table)
{
    m_devSettings = devSettings;

    // Device set
    m_deviceSetWidget = new QComboBox();
    m_deviceSetWidget->setEditable(true);
    m_deviceSetWidget->setToolTip(table->horizontalHeaderItem(SAT_DEVICE_COL_DEVICESET)->toolTip());
    m_deviceSetItem = new QWidget();
    layout(m_deviceSetItem, m_deviceSetWidget);
    addDeviceSets();
    int devSetIdx = m_deviceSetWidget->findText(devSettings->m_deviceSet);
    if (devSetIdx != -1)
        m_deviceSetWidget->setCurrentIndex(devSetIdx);
    else
    {
        m_deviceSetWidget->addItem(devSettings->m_deviceSet);
        m_deviceSetWidget->setCurrentIndex(m_deviceSetWidget->count() - 1);
    }

    // Preset
    m_presetWidget = new QComboBox();
    m_presetWidget->setEditable(false);
    m_presetWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_presetWidget->setToolTip(table->horizontalHeaderItem(SAT_DEVICE_COL_PRESET)->toolTip());
    m_presetItem = new QWidget();
    layout(m_presetItem, m_presetWidget);
    addPresets(devSettings->m_deviceSet);

    const MainSettings& mainSettings = MainCore::instance()->getSettings();
    if (!devSettings->m_deviceSet.isEmpty())
    {
        int count = mainSettings.getPresetCount();
        int idx = 0;
        for (int i = 0; i < count; i++)
        {
            const Preset *preset = mainSettings.getPreset(i);
            if (   ((preset->isSourcePreset() && (devSettings->m_deviceSet[0] == "R")))
                || ((preset->isSinkPreset() && (devSettings->m_deviceSet[0] == "T")))
                || ((preset->isMIMOPreset() && (devSettings->m_deviceSet[0] == "M"))))
            {
                if (   (devSettings->m_presetGroup == preset->getGroup())
                    && (devSettings->m_presetFrequency == preset->getCenterFrequency())
                    && (devSettings->m_presetDescription == preset->getDescription()))
                {
                    m_presetWidget->setCurrentIndex(idx);
                    break;
                }
                idx++;
            }
        }
    }

    // Doppler
    m_dopplerWidget =  new QComboBox();
    m_dopplerWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_dopplerWidget->setToolTip(table->horizontalHeaderItem(SAT_DEVICE_COL_DOPPLER)->toolTip());
    m_dopplerItem = new QWidget();
    layout(m_dopplerItem, m_dopplerWidget);
    m_dopplerWidget->setModel(&m_dopplerModel);
    addChannels();

    for (int i = 0; i < devSettings->m_doppler.size(); i++)
        m_dopplerItems[devSettings->m_doppler[i]]->setData(Qt::Checked, Qt::CheckStateRole);

    // Start on AOS
    m_startOnAOSWidget = new QCheckBox();
    m_startOnAOSWidget->setChecked(devSettings->m_startOnAOS);
    m_startOnAOSWidget->setToolTip(table->horizontalHeaderItem(SAT_DEVICE_COL_START)->toolTip());
    m_startOnAOSItem = new QWidget();
    layout(m_startOnAOSItem, m_startOnAOSWidget);

    // Stop on AOS
    m_stopOnLOSWidget = new QCheckBox();
    m_stopOnLOSWidget->setChecked(devSettings->m_stopOnLOS);
    m_stopOnLOSWidget->setToolTip(table->horizontalHeaderItem(SAT_DEVICE_COL_STOP)->toolTip());
    m_stopOnLOSItem = new QWidget();
    layout(m_stopOnLOSItem, m_stopOnLOSWidget);

    // Start file sink
    m_startStopFileSinkWidget = new QCheckBox();
    m_startStopFileSinkWidget->setChecked(devSettings->m_startStopFileSink);
    m_startStopFileSinkWidget->setToolTip(table->horizontalHeaderItem(SAT_DEVICE_COL_START_FILE_SINK)->toolTip());
    m_startStopFileSinkItem = new QWidget();
    layout(m_startStopFileSinkItem, m_startStopFileSinkWidget);

    // Frequency override
    m_frequencyItem = new QTableWidgetItem();
    m_frequencyItem->setToolTip(table->horizontalHeaderItem(SAT_DEVICE_COL_FREQUENCY)->toolTip());
    if (devSettings->m_frequency != 0)
        m_frequencyItem->setData(Qt::DisplayRole, QString("%1").arg(devSettings->m_frequency/1000000.0, 0, 'f', 3, QLatin1Char(' ')));

    // AOS command
    m_aosCommandItem = new QTableWidgetItem();
    m_aosCommandItem->setText(devSettings->m_aosCommand);
    m_aosCommandItem->setToolTip(table->horizontalHeaderItem(SAT_DEVICE_COL_AOS_COMMAND)->toolTip());

    // LOS command
    m_losCommandItem = new QTableWidgetItem();
    m_losCommandItem->setText(devSettings->m_losCommand);
    m_losCommandItem->setToolTip(table->horizontalHeaderItem(SAT_DEVICE_COL_LOS_COMMAND)->toolTip());

    int row = table->rowCount();
    table->setRowCount(row + 1);
    table->setCellWidget(row, SAT_DEVICE_COL_DEVICESET, m_deviceSetItem);
    table->setCellWidget(row, SAT_DEVICE_COL_PRESET, m_presetItem);
    table->setCellWidget(row, SAT_DEVICE_COL_DOPPLER, m_dopplerItem);
    table->setCellWidget(row, SAT_DEVICE_COL_START, m_startOnAOSItem);
    table->setCellWidget(row, SAT_DEVICE_COL_STOP, m_stopOnLOSItem);
    table->setCellWidget(row, SAT_DEVICE_COL_START_FILE_SINK, m_startStopFileSinkItem);
    table->setItem(row, SAT_DEVICE_COL_FREQUENCY, m_frequencyItem);
    table->setItem(row, SAT_DEVICE_COL_AOS_COMMAND, m_aosCommandItem);
    table->setItem(row, SAT_DEVICE_COL_LOS_COMMAND, m_losCommandItem);
    table->resizeColumnsToContents();

    connect(m_deviceSetWidget, SIGNAL(currentTextChanged(const QString &)), this, SLOT(on_m_deviceSetWidget_currentTextChanged(const QString &)));
    connect(m_presetWidget, SIGNAL(currentIndexChanged(int)), this, SLOT(on_m_presetWidget_currentIndexChanged(int)));
}

void SatelliteDeviceSettingsGUI::layout(QWidget *parent, QWidget *child)
{
    QHBoxLayout* pLayout = new QHBoxLayout(parent);
    pLayout->addWidget(child);
    pLayout->setAlignment(Qt::AlignCenter);
    pLayout->setContentsMargins(0, 0, 0, 0);
    parent->setLayout(pLayout);
}

// Add available devicesets to the combobox
void SatelliteDeviceSettingsGUI::addDeviceSets()
{
    MainCore *mainCore = MainCore::instance();
    std::vector<DeviceSet*>& deviceSets = mainCore->getDeviceSets();
    std::vector<DeviceSet*>::const_iterator it = deviceSets.begin();
    for (unsigned int deviceIndex = 0; it != deviceSets.end(); ++it, deviceIndex++)
    {
        DSPDeviceSourceEngine *deviceSourceEngine =  (*it)->m_deviceSourceEngine;
        DSPDeviceSinkEngine *deviceSinkEngine = (*it)->m_deviceSinkEngine;

        if (deviceSourceEngine) {
            m_deviceSetWidget->addItem(QString("R%1").arg(deviceIndex), deviceIndex);
        } else if (deviceSinkEngine) {
            m_deviceSetWidget->addItem(QString("T%1").arg(deviceIndex), deviceIndex);
        }
    }
}

// Add all available presets for a deviceset to the combobox
void SatelliteDeviceSettingsGUI::addPresets(const QString& deviceSet)
{
    m_presetWidget->clear();
    const MainSettings& mainSettings = MainCore::instance()->getSettings();
    int count = mainSettings.getPresetCount();
    m_currentPresets = deviceSet[0];
    for (int i = 0; i < count; i++)
    {
        const Preset *preset = mainSettings.getPreset(i);
        if (   ((preset->isSourcePreset() && (m_currentPresets == "R")))
            || ((preset->isSinkPreset() && (m_currentPresets == "T")))
            || ((preset->isMIMOPreset() && (m_currentPresets == "M"))))
        {
            m_presetWidget->addItem(QString("%1: %2 - %3")
                                    .arg(preset->getGroup())
                                    .arg(preset->getCenterFrequency()/1000000.0, 0, 'f', 3)
                                    .arg(preset->getDescription()));
        }
    }
}

const Preset* SatelliteDeviceSettingsGUI::getSelectedPreset()
{
    int listIdx = m_presetWidget->currentIndex();
    const MainSettings& mainSettings = MainCore::instance()->getSettings();
    int count = mainSettings.getPresetCount();
    int presetIdx = 0;
    for (int i = 0; i < count; i++)
    {
        const Preset *preset = mainSettings.getPreset(i);
        if (   ((preset->isSourcePreset() && (m_currentPresets == "R")))
            || ((preset->isSinkPreset() && (m_currentPresets == "T")))
            || ((preset->isMIMOPreset() && (m_currentPresets == "M"))))
        {
            if (listIdx == presetIdx)
                return preset;
            presetIdx++;
        }
    }
    return nullptr;
}

// Add checkable list of channels from a preset to the combobox
void SatelliteDeviceSettingsGUI::addChannels()
{
    m_dopplerModel.clear();
    m_dopplerItems.clear();
    const PluginManager *pluginManager = MainCore::instance()->getPluginManager();
    const Preset* preset = getSelectedPreset();
    if (preset != nullptr)
    {
        int channels = preset->getChannelCount();
        for (int i = 0; i < channels; i++)
        {
            const Preset::ChannelConfig& channelConfig = preset->getChannelConfig(i);

            QStandardItem *item = new QStandardItem();
            item->setText(pluginManager->uriToId(channelConfig.m_channelIdURI));
            item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
            item->setData(Qt::Unchecked, Qt::CheckStateRole);
            m_dopplerModel.appendRow(item);
            m_dopplerItems.append(item);
        }
    }
}

// Update preset list, to match type of deviceset entered
void SatelliteDeviceSettingsGUI::on_m_deviceSetWidget_currentTextChanged(const QString &text)
{
    if (!text.isEmpty())
    {
        if (text[0] != m_currentPresets)
            addPresets(text[0]);
    }
}

// Update doppler combo, to correspond to selected preset
void SatelliteDeviceSettingsGUI::on_m_presetWidget_currentIndexChanged(int index)
{
    addChannels();
}

// Update devSettings with current GUI values
void SatelliteDeviceSettingsGUI::accept()
{
    m_devSettings->m_deviceSet = m_deviceSetWidget->currentText();
    const Preset* preset = getSelectedPreset();
    if (preset != nullptr)
    {
        m_devSettings->m_presetGroup = preset->getGroup();
        m_devSettings->m_presetFrequency = preset->getCenterFrequency();
        m_devSettings->m_presetDescription = preset->getDescription();
    }
    else
    {
        m_devSettings->m_presetGroup = "";
        m_devSettings->m_presetFrequency = 0;
        m_devSettings->m_presetDescription = "";
    }
    m_devSettings->m_doppler.clear();
    for (int i = 0; i < m_dopplerItems.size(); i++)
    {
        if (m_dopplerItems[i]->checkState() == Qt::Checked)
            m_devSettings->m_doppler.append(i);
    }
    m_devSettings->m_startOnAOS = m_startOnAOSWidget->isChecked();
    m_devSettings->m_stopOnLOS = m_stopOnLOSWidget->isChecked();
    m_devSettings->m_startStopFileSink = m_startStopFileSinkWidget->isChecked();
    m_devSettings->m_frequency = (quint64)(m_frequencyItem->data(Qt::DisplayRole).toDouble() * 1000000.0);
    m_devSettings->m_aosCommand = m_aosCommandItem->text();
    m_devSettings->m_losCommand = m_losCommandItem->text();
}
