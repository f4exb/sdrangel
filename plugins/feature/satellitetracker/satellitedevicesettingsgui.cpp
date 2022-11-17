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
#include <QFormLayout>
#include <QSizePolicy>

#include "satellitedevicesettingsgui.h"
#include "device/deviceset.h"
#include "settings/mainsettings.h"
#include "maincore.h"
#include "util/messagequeue.h"
#include "plugin/pluginmanager.h"
#include "plugin/pluginapi.h"

SatelliteDeviceSettingsGUI::SatelliteDeviceSettingsGUI(SatelliteTrackerSettings::SatelliteDeviceSettings *devSettings,
                                                       QTabWidget *tab, QWidget *parent) :
    QWidget(parent),
    m_tab(tab)
{
    m_devSettings = devSettings;

    QFormLayout *formLayout = new QFormLayout();

    // Device set
    m_deviceSetWidget = new QComboBox();
    m_deviceSetWidget->setEditable(true);
    m_deviceSetWidget->setToolTip("Device set to control");
    formLayout->addRow("Device set", m_deviceSetWidget);

    addDeviceSets();

    if (devSettings->m_deviceSetIndex < m_deviceSetWidget->count()) {
        m_deviceSetWidget->setCurrentIndex(devSettings->m_deviceSetIndex);
    } else {
        m_deviceSetWidget->setCurrentIndex(m_deviceSetWidget->count() - 1);
    }

    // Preset
    m_presetWidget = new QComboBox();
    m_presetWidget->setEditable(false);
    m_presetWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_presetWidget->setToolTip("Preset to load on AOS");
    formLayout->addRow("Preset", m_presetWidget);

    MainCore *mainCore = MainCore::instance();
    const std::vector<DeviceSet*>& deviceSets = mainCore->getDeviceSets();

    if (devSettings->m_deviceSetIndex < (int) deviceSets.size())
    {
        DeviceSet *deviceSet = deviceSets[devSettings->m_deviceSetIndex];

        if (deviceSet->m_deviceSourceEngine) {
            addPresets("R");
        } else if (deviceSet->m_deviceSinkEngine) {
            addPresets("T");
        } else if (deviceSet->m_deviceMIMOEngine) {
            addPresets("M");
        }

        const MainSettings& mainSettings = MainCore::instance()->getSettings();
        int count = mainSettings.getPresetCount();
        int idx = 0;
        for (int i = 0; i < count; i++)
        {
            const Preset *preset = mainSettings.getPreset(i);
            if (((preset->isSourcePreset() && (deviceSet->m_deviceSourceEngine)))
                || ((preset->isSinkPreset() && (deviceSet->m_deviceSinkEngine)))
                || ((preset->isMIMOPreset() && (deviceSet->m_deviceMIMOEngine))))
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
    m_dopplerWidget =  new QListView();
    m_dopplerWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_dopplerWidget->setToolTip("Channels that will have Doppler correction applied");
    formLayout->addRow("Doppler correction", m_dopplerWidget);
    m_dopplerWidget->setModel(&m_dopplerModel);
    addChannels();

    for (int i = 0; i < devSettings->m_doppler.size(); i++)
    {
        int idx = devSettings->m_doppler[i];

        if (idx < m_dopplerItems.size()) {
            m_dopplerItems[idx]->setData(Qt::Checked, Qt::CheckStateRole);
        } else {
            qDebug() << "SatelliteDeviceSettingsGUI - Doppler index " << idx << " out of range: " << m_dopplerItems.size();
        }
    }

    // Start on AOS
    m_startOnAOSWidget = new QCheckBox();
    m_startOnAOSWidget->setChecked(devSettings->m_startOnAOS);
    m_startOnAOSWidget->setToolTip("Start acquisition on AOS");
    formLayout->addRow("Start acquisition on AOS", m_startOnAOSWidget);

    // Stop on AOS
    m_stopOnLOSWidget = new QCheckBox();
    m_stopOnLOSWidget->setChecked(devSettings->m_stopOnLOS);
    m_stopOnLOSWidget->setToolTip("Stop acquisition on LOS");
    formLayout->addRow("Stop acquisition on LOS", m_stopOnLOSWidget);

    // Start file sink
    m_startStopFileSinkWidget = new QCheckBox();
    m_startStopFileSinkWidget->setChecked(devSettings->m_startStopFileSink);
    m_startStopFileSinkWidget->setToolTip("Start file sinks recording on AOS and stop recording on LOS");
    formLayout->addRow("Start/stop file sinks on AOS/LOS", m_startStopFileSinkWidget);

    // Frequency override
    m_frequencyWidget = new QLineEdit();
    m_frequencyWidget->setToolTip("Override the center frequency in the preset with a value specified here in MHz.\nThis allows a single preset to be shared between different satellites that differ only in frequency.");
    // FIXME: Set mask for numeric or blank
    if (devSettings->m_frequency != 0) {
        m_frequencyWidget->setText(QString("%1").arg(devSettings->m_frequency/1000000.0, 0, 'f', 3, QLatin1Char(' ')));
    }
    formLayout->addRow("Override preset frequency (MHz)", m_frequencyWidget);

    // AOS command
    m_aosCommandWidget = new QLineEdit();
    m_aosCommandWidget->setText(devSettings->m_aosCommand);
    m_aosCommandWidget->setToolTip("Command to execute on AOS");
    formLayout->addRow("AOS command", m_aosCommandWidget);

    // LOS command
    m_losCommandWidget = new QLineEdit();
    m_losCommandWidget->setText(devSettings->m_losCommand);
    m_losCommandWidget->setToolTip("Command to execute on LOS");
    formLayout->addRow("LOS command", m_losCommandWidget);

    setLayout(formLayout);

    connect(m_deviceSetWidget, SIGNAL(currentTextChanged(const QString &)), this, SLOT(on_m_deviceSetWidget_currentTextChanged(const QString &)));
    connect(m_presetWidget, SIGNAL(currentIndexChanged(int)), this, SLOT(on_m_presetWidget_currentIndexChanged(int)));
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
        DSPDeviceMIMOEngine *deviceMIMOEngine = (*it)->m_deviceMIMOEngine;

        if (deviceSourceEngine) {
            m_deviceSetWidget->addItem(QString("R%1").arg(deviceIndex), deviceIndex);
        } else if (deviceSinkEngine) {
            m_deviceSetWidget->addItem(QString("T%1").arg(deviceIndex), deviceIndex);
        } else if (deviceMIMOEngine) {
            m_deviceSetWidget->addItem(QString("M%1").arg(deviceIndex), deviceIndex);
        }
    }
}

// Add all available presets for a deviceset to the combobox
void SatelliteDeviceSettingsGUI::addPresets(const QString& deviceSetType)
{
    m_presetWidget->clear();
    const MainSettings& mainSettings = MainCore::instance()->getSettings();
    int count = mainSettings.getPresetCount();
    m_currentPresetType = deviceSetType[0];

    for (int i = 0; i < count; i++)
    {
        const Preset *preset = mainSettings.getPreset(i);
        if (((preset->isSourcePreset() && (m_currentPresetType == 'R')))
            || ((preset->isSinkPreset() && (m_currentPresetType == 'T')))
            || ((preset->isMIMOPreset() && (m_currentPresetType == 'M'))))
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
        if (   ((preset->isSourcePreset() && (m_currentPresetType == 'R')))
            || ((preset->isSinkPreset() && (m_currentPresetType == 'T')))
            || ((preset->isMIMOPreset() && (m_currentPresetType == 'M'))))
        {
            if (listIdx == presetIdx) {
                return preset;
            }
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
        if (text[0] != m_currentPresetType) {
            addPresets(text[0]);
        }

        // Set name of tab to match
        int currentTabIndex = m_tab->currentIndex();
        m_tab->setTabText(currentTabIndex, text);
    }
}

// Update doppler combo, to correspond to selected preset
void SatelliteDeviceSettingsGUI::on_m_presetWidget_currentIndexChanged(int index)
{
    (void) index;
    addChannels();
}

// Update devSettings with current GUI values
void SatelliteDeviceSettingsGUI::accept()
{
    m_devSettings->m_deviceSetIndex = m_deviceSetWidget->currentIndex();
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
        if (m_dopplerItems[i]->checkState() == Qt::Checked) {
            m_devSettings->m_doppler.append(i);
        }
    }

    m_devSettings->m_startOnAOS = m_startOnAOSWidget->isChecked();
    m_devSettings->m_stopOnLOS = m_stopOnLOSWidget->isChecked();
    m_devSettings->m_startStopFileSink = m_startStopFileSinkWidget->isChecked();
    m_devSettings->m_frequency = (quint64)(m_frequencyWidget->text().toDouble() * 1000000.0);
    m_devSettings->m_aosCommand = m_aosCommandWidget->text();
    m_devSettings->m_losCommand = m_losCommandWidget->text();
}
