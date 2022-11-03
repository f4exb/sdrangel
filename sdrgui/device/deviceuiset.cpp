///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#include <QFont>

#include <algorithm>

#include "dsp/spectrumvis.h"
#include "dsp/dspdevicesourceengine.h"
#include "dsp/dspdevicesinkengine.h"
#include "gui/glspectrum.h"
#include "gui/glspectrumview.h"
#include "gui/glspectrumgui.h"
// #include "gui/channelwindow.h"
#include "gui/workspace.h"
#include "gui/rollupcontents.h"
#include "device/devicegui.h"
#include "device/deviceset.h"
#include "device/deviceapi.h"
#include "plugin/pluginapi.h"
#include "plugin/plugininterface.h"
#include "channel/channelutils.h"
#include "channel/channelapi.h"
#include "channel/channelgui.h"
#include "mainspectrum/mainspectrumgui.h"
#include "settings/preset.h"
#include "mainwindow.h"

#include "deviceuiset.h"

DeviceUISet::DeviceUISet(int deviceSetIndex, DeviceSet *deviceSet)
{
    m_spectrum = new GLSpectrum();
    m_spectrum->setIsDeviceSpectrum(true);
    m_spectrumVis = deviceSet->m_spectrumVis;
    m_spectrumVis->setGLSpectrum(m_spectrum);
    m_spectrumGUI = new GLSpectrumGUI;
    m_spectrumGUI->setBuddies(m_spectrumVis, m_spectrum);
    m_mainSpectrumGUI = new MainSpectrumGUI(m_spectrum, m_spectrumGUI);
    // m_channelWindow = new ChannelWindow;
    m_deviceAPI = nullptr;
    m_deviceGUI = nullptr;
    m_deviceSourceEngine = nullptr;
    m_deviceSinkEngine = nullptr;
    m_deviceMIMOEngine = nullptr;
    m_deviceSetIndex = deviceSetIndex;
    m_deviceSet = deviceSet;
    m_nbAvailableRxChannels = 0;   // updated at enumeration for UI selector
    m_nbAvailableTxChannels = 0;   // updated at enumeration for UI selector
    m_nbAvailableMIMOChannels = 0; // updated at enumeration for UI selector

    // m_spectrum needs to have its font to be set since it cannot be inherited from the main window
    QFont font;
    font.setFamily(QStringLiteral("Liberation Sans"));
    font.setPointSize(9);
    m_spectrum->setFont(font);
}

DeviceUISet::~DeviceUISet()
{
    // delete m_channelWindow;
    delete m_mainSpectrumGUI;
    // delete m_spectrumGUI; // done above
    // delete m_spectrum;
}

void DeviceUISet::setIndex(int deviceSetIndex)
{
    m_deviceGUI->setIndex(deviceSetIndex);
    m_mainSpectrumGUI->setIndex(deviceSetIndex);

    for (auto& channelRegistation : m_channelInstanceRegistrations) {
        channelRegistation.m_gui->setDeviceSetIndex(deviceSetIndex);
    }

    m_deviceSetIndex = deviceSetIndex;
}

void DeviceUISet::setSpectrumScalingFactor(float scalef)
{
    m_spectrumVis->setScalef(scalef);
}

void DeviceUISet::addChannelMarker(ChannelMarker* channelMarker)
{
    m_spectrum->addChannelMarker(channelMarker);
}

void DeviceUISet::removeChannelMarker(ChannelMarker* channelMarker)
{
    m_spectrum->removeChannelMarker(channelMarker);
}

void DeviceUISet::registerRxChannelInstance(ChannelAPI *channelAPI, ChannelGUI* channelGUI)
{
    m_channelInstanceRegistrations.append(ChannelInstanceRegistration(channelAPI, channelGUI, 0));
    m_deviceSet->addChannelInstance(channelAPI);
    QObject::connect(
        channelGUI,
        &ChannelGUI::closing,
        this,
        [=](){ this->handleChannelGUIClosing(channelGUI); },
        Qt::QueuedConnection
    );
}

void DeviceUISet::registerTxChannelInstance(ChannelAPI *channelAPI, ChannelGUI* channelGUI)
{
    m_channelInstanceRegistrations.append(ChannelInstanceRegistration(channelAPI, channelGUI, 1));
    m_deviceSet->addChannelInstance(channelAPI);
    QObject::connect(
        channelGUI,
        &ChannelGUI::closing,
        this,
        [=](){ this->handleChannelGUIClosing(channelGUI); },
        Qt::QueuedConnection
    );
}

void DeviceUISet::registerChannelInstance(ChannelAPI *channelAPI, ChannelGUI* channelGUI)
{
    m_channelInstanceRegistrations.append(ChannelInstanceRegistration( channelAPI, channelGUI, 2));
    m_deviceSet->addChannelInstance(channelAPI);
    QObject::connect(
        channelGUI,
        &ChannelGUI::closing,
        this,
        [=](){ this->handleChannelGUIClosing(channelGUI); },
        Qt::QueuedConnection
    );
}

void DeviceUISet::unregisterChannelInstanceAt(int channelIndex)
{
    if ((channelIndex >= 0) && (channelIndex < m_channelInstanceRegistrations.count()))
    {
        m_channelInstanceRegistrations.removeAt(channelIndex);
        m_deviceSet->removeChannelInstanceAt(channelIndex);

        // Renumerate
        for (int i = 0; i < m_channelInstanceRegistrations.count(); i++) {
            m_channelInstanceRegistrations.at(i).m_gui->setIndex(i);
        }
    }
}

void DeviceUISet::freeChannels()
{
    for(int i = 0; i < m_channelInstanceRegistrations.count(); i++)
    {
        qDebug("DeviceUISet::freeChannels: destroying channel [%s]", qPrintable(m_channelInstanceRegistrations[i].m_channelAPI->getURI()));
        m_channelInstanceRegistrations[i].m_gui->destroy();
        m_channelInstanceRegistrations[i].m_channelAPI->destroy();
    }

    m_channelInstanceRegistrations.clear();
    m_deviceSet->clearChannels();
}

void DeviceUISet::deleteChannel(int channelIndex)
{
    if ((channelIndex >= 0) && (channelIndex < m_channelInstanceRegistrations.count()))
    {
        qDebug("DeviceUISet::deleteChannel: delete channel [%s] at %d",
                qPrintable(m_channelInstanceRegistrations[channelIndex].m_channelAPI->getURI()),
                channelIndex);
        m_channelInstanceRegistrations[channelIndex].m_gui->destroy();
        m_channelInstanceRegistrations[channelIndex].m_channelAPI->destroy();
        m_channelInstanceRegistrations.removeAt(channelIndex);
    }

    m_deviceSet->removeChannelInstanceAt(channelIndex);
}

ChannelAPI *DeviceUISet::getChannelAt(int channelIndex)
{
    return m_deviceSet->getChannelAt(channelIndex);
}

ChannelGUI *DeviceUISet::getChannelGUIAt(int channelIndex)
{
    return m_channelInstanceRegistrations[channelIndex].m_gui;
}

void DeviceUISet::loadDeviceSetSettings(
    const Preset* preset,
    PluginAPI *pluginAPI,
    QList<Workspace*> *workspaces,
    Workspace *currentWorkspace
)
{
    qDebug("DeviceUISet::loadDeviceSetSettings: preset: [%s, %s]",
        qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));
    m_spectrumGUI->deserialize(preset->getSpectrumConfig());
    m_mainSpectrumGUI->restoreGeometry(preset->getSpectrumGeometry());
    m_deviceGUI->restoreGeometry(preset->getDeviceGeometry());
    m_deviceAPI->loadSamplingDeviceSettings(preset);

    if (!preset->getShowSpectrum()) {
        m_mainSpectrumGUI->hide();
    }

    if (m_deviceSourceEngine) { // source device
        loadRxChannelSettings(preset, pluginAPI, workspaces, currentWorkspace);
    } else if (m_deviceSinkEngine) { // sink device
        loadTxChannelSettings(preset, pluginAPI, workspaces, currentWorkspace);
    } else if (m_deviceMIMOEngine) { // MIMO device
        loadMIMOChannelSettings(preset, pluginAPI, workspaces, currentWorkspace);
    }
}

void DeviceUISet::saveDeviceSetSettings(Preset* preset) const
{
    qDebug("DeviceUISet::saveDeviceSetSettings: preset: [%s, %s]",
        qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));
    preset->setSpectrumConfig(m_spectrumGUI->serialize());
    preset->setSpectrumWorkspaceIndex(m_mainSpectrumGUI->getWorkspaceIndex());
    preset->setSpectrumGeometry(m_mainSpectrumGUI->saveGeometry());
    preset->setDeviceGeometry(m_deviceGUI->saveGeometry());
    preset->setShowSpectrum(m_spectrumGUI->isVisible());
    preset->setSelectedDevice(Preset::SelectedDevice{
        m_deviceAPI->getSamplingDeviceId(),
        m_deviceAPI->getSamplingDeviceSerial(),
        (int) m_deviceAPI->getSamplingDeviceSequence(),
        (int) m_deviceAPI->getDeviceItemIndex()
    });
    preset->setDeviceWorkspaceIndex(m_deviceGUI->getWorkspaceIndex());
    preset->clearChannels();

    if (m_deviceSourceEngine) // source device
    {
        preset->setSourcePreset();
        saveRxChannelSettings(preset);
    }
    else if (m_deviceSinkEngine) // sink device
    {
        preset->setSinkPreset();
        saveTxChannelSettings(preset);
    }
    else if (m_deviceMIMOEngine) // MIMO device
    {
        preset->setMIMOPreset();
        saveMIMOChannelSettings(preset);
    }

    m_deviceAPI->saveSamplingDeviceSettings(preset);
}

void DeviceUISet::loadRxChannelSettings(const Preset *preset, PluginAPI *pluginAPI, QList<Workspace*> *workspaces, Workspace *currentWorkspace)
{
    if (preset->isSourcePreset())
    {
        qDebug("DeviceUISet::loadRxChannelSettings: Loading preset [%s | %s]",
            qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));

        // Available channel plugins
        PluginAPI::ChannelRegistrations *channelRegistrations = pluginAPI->getRxChannelRegistrations();

        // clear list
        for (int i = 0; i < m_channelInstanceRegistrations.count(); i++)
        {
            qDebug("DeviceUISet::loadRxChannelSettings: destroying old channel [%s]",
                qPrintable(m_channelInstanceRegistrations[i].m_channelAPI->getURI()));
            m_channelInstanceRegistrations[i].m_channelAPI->setMessageQueueToGUI(nullptr); // have channel stop sending messages to its GUI
            m_channelInstanceRegistrations[i].m_gui->destroy();
            m_channelInstanceRegistrations[i].m_channelAPI->destroy();
        }

        m_channelInstanceRegistrations.clear();
        m_deviceSet->clearChannels();
        qDebug("DeviceUISet::loadRxChannelSettings: %d channel(s) in preset", preset->getChannelCount());

        for (int i = 0; i < preset->getChannelCount(); i++)
        {
            const Preset::ChannelConfig& channelConfig = preset->getChannelConfig(i);
            ChannelGUI *rxChannelGUI = nullptr;
            ChannelAPI *channelAPI = nullptr;

            // create channel instance

            for (int i = 0; i < channelRegistrations->count(); i++)
            {
                //if((*channelRegistrations)[i].m_channelIdURI == channelConfig.m_channelIdURI)
                if (ChannelUtils::compareChannelURIs((*channelRegistrations)[i].m_channelIdURI, channelConfig.m_channelIdURI))
                {
                    qDebug("DeviceUISet::loadRxChannelSettings: creating new channel [%s] from config [%s]",
                            qPrintable((*channelRegistrations)[i].m_channelIdURI),
                            qPrintable(channelConfig.m_channelIdURI));
                    BasebandSampleSink *rxChannel;
                    PluginInterface *pluginInterface = (*channelRegistrations)[i].m_plugin;
                    pluginInterface->createRxChannel(m_deviceAPI, &rxChannel, &channelAPI);
                    rxChannelGUI = pluginInterface->createRxChannelGUI(this, rxChannel);
                    rxChannelGUI->setDisplayedame(pluginInterface->getPluginDescriptor().displayedName);
                    registerRxChannelInstance(channelAPI, rxChannelGUI);
                    QObject::connect(
                        rxChannelGUI,
                        &ChannelGUI::closing,
                        this,
                        [=](){ this->handleChannelGUIClosing(rxChannelGUI); },
                        Qt::QueuedConnection
                    );
                    break;
                }
            }

            if (rxChannelGUI && channelAPI)
            {
                qDebug("DeviceUISet::loadRxChannelSettings: deserializing channel [%s]", qPrintable(channelConfig.m_channelIdURI));
                rxChannelGUI->deserialize(channelConfig.m_config);
                int originalWorkspaceIndex = rxChannelGUI->getWorkspaceIndex();

                if (workspaces && (workspaces->size() > 0) && (originalWorkspaceIndex < workspaces->size())) // restore in original workspace
                {
                    (*workspaces)[originalWorkspaceIndex]->addToMdiArea((QMdiSubWindow*) rxChannelGUI);
                }
                else if (currentWorkspace) // restore in current workspace
                {
                    rxChannelGUI->setWorkspaceIndex(currentWorkspace->getIndex());
                    currentWorkspace->addToMdiArea((QMdiSubWindow*) rxChannelGUI);
                }

                if (rxChannelGUI->getHidden()) {
                    rxChannelGUI->hide();
                }

                rxChannelGUI->restoreGeometry(rxChannelGUI->getGeometryBytes());
                rxChannelGUI->getRollupContents()->arrangeRollups();
                rxChannelGUI->setDeviceType(ChannelGUI::DeviceRx);
                rxChannelGUI->setDeviceSetIndex(m_deviceSetIndex);
                rxChannelGUI->setIndex(channelAPI->getIndexInDeviceSet());
                rxChannelGUI->setIndexToolTip(m_deviceAPI->getSamplingDeviceDisplayName());

                QObject::connect(
                    rxChannelGUI,
                    &ChannelGUI::moveToWorkspace,
                    this,
                    [=](int wsIndexDest){ MainWindow::getInstance()->channelMove(rxChannelGUI, wsIndexDest); }
                );
                QObject::connect(
                    rxChannelGUI,
                    &ChannelGUI::duplicateChannelEmitted,
                    this,
                    [=](){ MainWindow::getInstance()->channelDuplicate(rxChannelGUI); }
                );
                QObject::connect(
                    rxChannelGUI,
                    &ChannelGUI::moveToDeviceSet,
                    this,
                    [=](int dsIndexDest){ MainWindow::getInstance()->channelMoveToDeviceSet(rxChannelGUI, dsIndexDest); }
                );
            }
        }
    }
    else
    {
        qDebug("DeviceUISet::loadRxChannelSettings: Loading preset [%s | %s] not a source preset", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));
    }
}

void DeviceUISet::saveRxChannelSettings(Preset *preset) const
{
    if (preset->isSourcePreset())
    {
        for (int i = 0; i < m_channelInstanceRegistrations.count(); i++)
        {
            ChannelGUI *channelGUI = m_channelInstanceRegistrations[i].m_gui;
            qDebug("DeviceUISet::saveRxChannelSettings: saving channel [%s]", qPrintable(m_channelInstanceRegistrations[i].m_channelAPI->getURI()));
            channelGUI->setGeometryBytes(channelGUI->saveGeometry());
            channelGUI->zetHidden(channelGUI->isHidden());
            preset->addChannel(m_channelInstanceRegistrations[i].m_channelAPI->getURI(), channelGUI->serialize());
        }
    }
    else
    {
        qDebug("DeviceUISet::saveRxChannelSettings: not a source preset");
    }
}

void DeviceUISet::loadTxChannelSettings(const Preset *preset, PluginAPI *pluginAPI, QList<Workspace*> *workspaces, Workspace *currentWorkspace)
{
    if (preset->isSinkPreset())
    {
        qDebug("DeviceUISet::loadTxChannelSettings: Loading preset [%s | %s]",
            qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));

        // Available channel plugins
        PluginAPI::ChannelRegistrations *channelRegistrations = pluginAPI->getTxChannelRegistrations();

        //  clear list
        for (int i = 0; i < m_channelInstanceRegistrations.count(); i++)
        {
            qDebug("DeviceUISet::loadTxChannelSettings: destroying old channel [%s]",
                qPrintable(m_channelInstanceRegistrations[i].m_channelAPI->getURI()));
            m_channelInstanceRegistrations[i].m_channelAPI->setMessageQueueToGUI(nullptr); // have channel stop sending messages to its GUI
            m_channelInstanceRegistrations[i].m_gui->destroy();
            m_channelInstanceRegistrations[i].m_channelAPI->destroy();
        }

        m_channelInstanceRegistrations.clear();
        m_deviceSet->clearChannels();
        qDebug("DeviceUISet::loadTxChannelSettings: %d channel(s) in preset", preset->getChannelCount());

        for (int i = 0; i < preset->getChannelCount(); i++)
        {
            const Preset::ChannelConfig& channelConfig = preset->getChannelConfig(i);
            ChannelGUI *txChannelGUI = nullptr;
            ChannelAPI *channelAPI = nullptr;

            // create channel instance

            for (int i = 0; i < channelRegistrations->count(); i++)
            {
                if ((*channelRegistrations)[i].m_channelIdURI == channelConfig.m_channelIdURI)
                {
                    qDebug("DeviceUISet::loadTxChannelSettings: creating new channel [%s] from config [%s]",
                            qPrintable((*channelRegistrations)[i].m_channelIdURI),
                            qPrintable(channelConfig.m_channelIdURI));
                    BasebandSampleSource *txChannel;
                    PluginInterface *pluginInterface = (*channelRegistrations)[i].m_plugin;
                    pluginInterface->createTxChannel(m_deviceAPI, &txChannel, &channelAPI);
                    txChannelGUI = pluginInterface->createTxChannelGUI(this, txChannel);
                    txChannelGUI->setDisplayedame(pluginInterface->getPluginDescriptor().displayedName);
                    registerTxChannelInstance(channelAPI, txChannelGUI);
                    QObject::connect(
                        txChannelGUI,
                        &ChannelGUI::closing,
                        this,
                        [=](){ this->handleChannelGUIClosing(txChannelGUI); },
                        Qt::QueuedConnection
                    );
                    break;
                }
            }

            if (txChannelGUI && channelAPI)
            {
                qDebug("DeviceUISet::loadTxChannelSettings: deserializing channel [%s]", qPrintable(channelConfig.m_channelIdURI));
                txChannelGUI->deserialize(channelConfig.m_config);
                int originalWorkspaceIndex = txChannelGUI->getWorkspaceIndex();

                if (workspaces && (workspaces->size() > 0) && (originalWorkspaceIndex < workspaces->size())) // restore in original workspace
                {
                    (*workspaces)[originalWorkspaceIndex]->addToMdiArea((QMdiSubWindow*) txChannelGUI);
                }
                else if (currentWorkspace) // restore in current workspace
                {
                    txChannelGUI->setWorkspaceIndex(currentWorkspace->getIndex());
                    currentWorkspace->addToMdiArea((QMdiSubWindow*) txChannelGUI);
                }

                if (txChannelGUI->getHidden()) {
                    txChannelGUI->hide();
                }

                txChannelGUI->restoreGeometry(txChannelGUI->getGeometryBytes());
                txChannelGUI->getRollupContents()->arrangeRollups();
                txChannelGUI->setDeviceType(ChannelGUI::DeviceTx);
                txChannelGUI->setDeviceSetIndex(m_deviceSetIndex);
                txChannelGUI->setIndex(channelAPI->getIndexInDeviceSet());
                txChannelGUI->setIndexToolTip(m_deviceAPI->getSamplingDeviceDisplayName());

                QObject::connect(
                    txChannelGUI,
                    &ChannelGUI::moveToWorkspace,
                    this,
                    [=](int wsIndexDest){ MainWindow::getInstance()->channelMove(txChannelGUI, wsIndexDest); }
                );
                QObject::connect(
                    txChannelGUI,
                    &ChannelGUI::duplicateChannelEmitted,
                    this,
                    [=](){ MainWindow::getInstance()->channelDuplicate(txChannelGUI); }
                );
                QObject::connect(
                    txChannelGUI,
                    &ChannelGUI::moveToDeviceSet,
                    this,
                    [=](int dsIndexDest){ MainWindow::getInstance()->channelMoveToDeviceSet(txChannelGUI, dsIndexDest); }
                );
            }
        }
    }
    else
    {
        qDebug("DeviceUISet::loadTxChannelSettings: Loading preset [%s | %s] not a sink preset", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));
    }

}

void DeviceUISet::saveTxChannelSettings(Preset *preset) const
{
    if (preset->isSinkPreset())
    {
        for (int i = 0; i < m_channelInstanceRegistrations.count(); i++)
        {
            ChannelGUI *channelGUI = m_channelInstanceRegistrations[i].m_gui;
            qDebug("DeviceUISet::saveTxChannelSettings: saving channel [%s]", qPrintable(m_channelInstanceRegistrations[i].m_channelAPI->getURI()));
            channelGUI->setGeometryBytes(channelGUI->saveGeometry());
            channelGUI->zetHidden(channelGUI->isHidden());
            preset->addChannel(m_channelInstanceRegistrations[i].m_channelAPI->getURI(), channelGUI->serialize());
        }
    }
    else
    {
        qDebug("DeviceUISet::saveTxChannelSettings: not a sink preset");
    }
}

void DeviceUISet::loadMIMOChannelSettings(const Preset *preset, PluginAPI *pluginAPI, QList<Workspace*> *workspaces, Workspace *currentWorkspace)
{
    if (preset->isMIMOPreset())
    {
        qDebug("DeviceUISet::loadMIMOChannelSettings: Loading preset [%s | %s]",
            qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));

        // clear list
        for (int i = 0; i < m_channelInstanceRegistrations.count(); i++)
        {
            qDebug("DeviceUISet::loadMIMOChannelSettings: destroying old channel [%s]",
                qPrintable(m_channelInstanceRegistrations[i].m_channelAPI->getURI()));
            m_channelInstanceRegistrations[i].m_gui->destroy(); // stop GUI first (issue #1427)
            m_channelInstanceRegistrations[i].m_channelAPI->destroy(); // stop channel before (issue #860)
        }

        m_channelInstanceRegistrations.clear();
        m_deviceSet->clearChannels();
        qDebug("DeviceUISet::loadMIMOChannelSettings: %d channel(s) in preset", preset->getChannelCount());

        for (int i = 0; i < preset->getChannelCount(); i++)
        {
            const Preset::ChannelConfig& channelConfig = preset->getChannelConfig(i);
            ChannelGUI *channelGUI = nullptr;
            ChannelAPI *channelAPI = nullptr;

            // Available MIMO channel plugins
            PluginAPI::ChannelRegistrations *channelMIMORegistrations = pluginAPI->getMIMOChannelRegistrations();
            // create MIMO channel instance
            for (int i = 0; i < channelMIMORegistrations->count(); i++)
            {
                if (ChannelUtils::compareChannelURIs((*channelMIMORegistrations)[i].m_channelIdURI, channelConfig.m_channelIdURI))
                {
                    qDebug("DeviceUISet::loadMIMOChannelSettings: creating new MIMO channel [%s] from config [%s]",
                            qPrintable((*channelMIMORegistrations)[i].m_channelIdURI),
                            qPrintable(channelConfig.m_channelIdURI));
                    MIMOChannel *mimoChannel;
                    PluginInterface *pluginInterface = (*channelMIMORegistrations)[i].m_plugin;
                    pluginInterface->createMIMOChannel(m_deviceAPI, &mimoChannel, &channelAPI);
                    channelGUI = pluginInterface->createMIMOChannelGUI(this, mimoChannel);
                    channelGUI->setDisplayedame(pluginInterface->getPluginDescriptor().displayedName);
                    registerChannelInstance(channelAPI, channelGUI);
                    QObject::connect(
                        channelGUI,
                        &ChannelGUI::closing,
                        this,
                        [=](){ this->handleChannelGUIClosing(channelGUI); },
                        Qt::QueuedConnection
                    );
                    break;
                }
            }

            // Available Rx channel plugins
            PluginAPI::ChannelRegistrations *channelRxRegistrations = pluginAPI->getRxChannelRegistrations();
            // create Rx channel instance
            for (int i = 0; i < channelRxRegistrations->count(); i++)
            {
                if (ChannelUtils::compareChannelURIs((*channelRxRegistrations)[i].m_channelIdURI, channelConfig.m_channelIdURI))
                {
                    qDebug("DeviceUISet::loadMIMOChannelSettings: creating new Rx channel [%s] from config [%s]",
                            qPrintable((*channelRxRegistrations)[i].m_channelIdURI),
                            qPrintable(channelConfig.m_channelIdURI));
                    BasebandSampleSink *rxChannel;
                    PluginInterface *pluginInterface = (*channelRxRegistrations)[i].m_plugin;
                    pluginInterface->createRxChannel(m_deviceAPI, &rxChannel, &channelAPI);
                    channelGUI = pluginInterface->createRxChannelGUI(this, rxChannel);
                    channelGUI->setDisplayedame(pluginInterface->getPluginDescriptor().displayedName);
                    registerRxChannelInstance(channelAPI, channelGUI);
                    QObject::connect(
                        channelGUI,
                        &ChannelGUI::closing,
                        this,
                        [=](){ this->handleChannelGUIClosing(channelGUI); },
                        Qt::QueuedConnection
                    );
                    break;
                }
            }

            // Available Tx channel plugins
            PluginAPI::ChannelRegistrations *channelTxRegistrations = pluginAPI->getTxChannelRegistrations();
            // create Tx channel instance
            for (int i = 0; i < channelTxRegistrations->count(); i++)
            {
                if (ChannelUtils::compareChannelURIs((*channelTxRegistrations)[i].m_channelIdURI, channelConfig.m_channelIdURI))
                {
                    qDebug("DeviceUISet::loadMIMOChannelSettings: creating new Tx channel [%s] from config [%s]",
                            qPrintable((*channelTxRegistrations)[i].m_channelIdURI),
                            qPrintable(channelConfig.m_channelIdURI));
                    BasebandSampleSource *txChannel;
                    PluginInterface *pluginInterface = (*channelTxRegistrations)[i].m_plugin;
                    pluginInterface->createTxChannel(m_deviceAPI, &txChannel, &channelAPI);
                    channelGUI = pluginInterface->createTxChannelGUI(this, txChannel);
                    channelGUI->setDisplayedame(pluginInterface->getPluginDescriptor().displayedName);
                    registerTxChannelInstance(channelAPI, channelGUI);
                    break;
                }
            }

            if (channelGUI && channelAPI)
            {
                qDebug("DeviceUISet::loadMIMOChannelSettings: deserializing channel [%s]", qPrintable(channelConfig.m_channelIdURI));
                channelGUI->deserialize(channelConfig.m_config);
                int originalWorkspaceIndex = channelGUI->getWorkspaceIndex();

                if (workspaces && (workspaces->size() > 0) && (originalWorkspaceIndex < workspaces->size())) // restore in original workspace
                {
                    (*workspaces)[originalWorkspaceIndex]->addToMdiArea((QMdiSubWindow*) channelGUI);
                }
                else if (currentWorkspace) // restore in current workspace
                {
                    channelGUI->setWorkspaceIndex(currentWorkspace->getIndex());
                    currentWorkspace->addToMdiArea((QMdiSubWindow*) channelGUI);
                }

                if (channelGUI->getHidden()) {
                    channelGUI->hide();
                }

                channelGUI->restoreGeometry(channelGUI->getGeometryBytes());
                channelGUI->getRollupContents()->arrangeRollups();
                channelGUI->setDeviceType(ChannelGUI::DeviceMIMO);
                channelGUI->setDeviceSetIndex(m_deviceSetIndex);
                channelGUI->setIndex(channelAPI->getIndexInDeviceSet());
                channelGUI->setIndexToolTip(m_deviceAPI->getSamplingDeviceDisplayName());

                QObject::connect(
                    channelGUI,
                    &ChannelGUI::closing,
                    this,
                    [=](){ this->handleChannelGUIClosing(channelGUI); },
                    Qt::QueuedConnection
                );
                QObject::connect(
                    channelGUI,
                    &ChannelGUI::moveToWorkspace,
                    this,
                    [=](int wsIndexDest){ MainWindow::getInstance()->channelMove(channelGUI, wsIndexDest); }
                );
                QObject::connect(
                    channelGUI,
                    &ChannelGUI::duplicateChannelEmitted,
                    this,
                    [=](){ MainWindow::getInstance()->channelDuplicate(channelGUI); }
                );
                QObject::connect(
                    channelGUI,
                    &ChannelGUI::moveToDeviceSet,
                    this,
                    [=](int dsIndexDest){ MainWindow::getInstance()->channelMoveToDeviceSet(channelGUI, dsIndexDest); }
                );
            }
        }
    }
    else
    {
        qDebug("DeviceUISet::loadMIMOChannelSettings: Loading preset [%s | %s] not a MIMO preset", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));
    }
}

void DeviceUISet::saveMIMOChannelSettings(Preset *preset) const
{
    if (preset->isMIMOPreset())
    {
        for (int i = 0; i < m_channelInstanceRegistrations.count(); i++)
        {
            ChannelGUI *channelGUI = m_channelInstanceRegistrations[i].m_gui;
            qDebug("DeviceUISet::saveMIMOChannelSettings: saving channel [%s]", qPrintable(m_channelInstanceRegistrations[i].m_channelAPI->getURI()));
            channelGUI->setGeometryBytes(channelGUI->saveGeometry());
            channelGUI->zetHidden(channelGUI->isHidden());
            preset->addChannel(m_channelInstanceRegistrations[i].m_channelAPI->getURI(), channelGUI->serialize());
        }
    }
    else
    {
        qDebug("DeviceUISet::saveMIMOChannelSettings: not a MIMO preset");
    }
}

// sort by increasing delta frequency and type (i.e. name)
bool DeviceUISet::ChannelInstanceRegistration::operator<(const ChannelInstanceRegistration& other) const
{
    if (m_channelAPI && other.m_channelAPI)
    {
        if (m_channelAPI->getCenterFrequency() == other.m_channelAPI->getCenterFrequency())
        {
            return m_channelAPI->getName() < other.m_channelAPI->getName();
        }
        else
        {
            return m_channelAPI->getCenterFrequency() < other.m_channelAPI->getCenterFrequency();
        }
    }
    else
    {
        return false;
    }
}

void DeviceUISet::handleChannelGUIClosing(ChannelGUI* channelGUI)
{
    qDebug("DeviceUISet::handleChannelGUIClosing: %s: %d", qPrintable(channelGUI->getTitle()), channelGUI->getIndex());

    for (ChannelInstanceRegistrations::iterator it = m_channelInstanceRegistrations.begin(); it != m_channelInstanceRegistrations.end(); ++it)
    {
        if (it->m_gui == channelGUI)
        {
            ChannelAPI *channelAPI = it->m_channelAPI;
            m_deviceSet->removeChannelInstance(channelAPI);
            QObject::connect(
                channelGUI,
                &ChannelGUI::destroyed,
                this,
                [this, channelAPI](){ this->handleDeleteChannel(channelAPI); }
            );
            m_channelInstanceRegistrations.erase(it);
            break;
        }
    }

    // Renumerate
    for (int i = 0; i < m_channelInstanceRegistrations.count(); i++) {
        m_channelInstanceRegistrations.at(i).m_gui->setIndex(i);
    }
}

void DeviceUISet::handleDeleteChannel(ChannelAPI *channelAPI)
{
    channelAPI->destroy();
}

int DeviceUISet::webapiSpectrumSettingsGet(SWGSDRangel::SWGGLSpectrum& response, QString& errorMessage) const
{
    return m_spectrumVis->webapiSpectrumSettingsGet(response, errorMessage);
}

int DeviceUISet::webapiSpectrumSettingsPutPatch(
    bool force,
    const QStringList& spectrumSettingsKeys,
    SWGSDRangel::SWGGLSpectrum& response, // query + response
    QString& errorMessage)
{
    return m_spectrumVis->webapiSpectrumSettingsPutPatch(force, spectrumSettingsKeys, response, errorMessage);
}

int DeviceUISet::webapiSpectrumServerGet(SWGSDRangel::SWGSpectrumServer& response, QString& errorMessage) const
{
    return m_spectrumVis->webapiSpectrumServerGet(response, errorMessage);
}

int DeviceUISet::webapiSpectrumServerPost(SWGSDRangel::SWGSuccessResponse& response, QString& errorMessage)
{
    return m_spectrumVis->webapiSpectrumServerPost(response, errorMessage);
}

int DeviceUISet::webapiSpectrumServerDelete(SWGSDRangel::SWGSuccessResponse& response, QString& errorMessage)
{
    return m_spectrumVis->webapiSpectrumServerDelete(response, errorMessage);
}
