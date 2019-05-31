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

#ifndef SDRGUI_DEVICE_DEVICEUISET_H_
#define SDRGUI_DEVICE_DEVICEUISET_H_

#include <QTimer>
#include <QByteArray>

#include "export.h"

class SpectrumVis;
class GLSpectrum;
class GLSpectrumGUI;
class ChannelWindow;
class SamplingDeviceControl;
class DeviceAPI;
class DSPDeviceSourceEngine;
class DSPDeviceSinkEngine;
class DSPDeviceMIMOEngine;
class ChannelMarker;
class PluginAPI;
class PluginInstanceGUI;
class Preset;

class SDRGUI_API DeviceUISet
{
public:
    SpectrumVis *m_spectrumVis;
    GLSpectrum *m_spectrum;
    GLSpectrumGUI *m_spectrumGUI;
    ChannelWindow *m_channelWindow;
    SamplingDeviceControl *m_samplingDeviceControl;
    DeviceAPI *m_deviceAPI;
    DSPDeviceSourceEngine *m_deviceSourceEngine;
    DSPDeviceSinkEngine *m_deviceSinkEngine;
    DSPDeviceMIMOEngine *m_deviceMIMOEngine;
    QByteArray m_mainWindowState;

    DeviceUISet(int tabIndex, int deviceType, QTimer& timer);
    ~DeviceUISet();

    GLSpectrum *getSpectrum() { return m_spectrum; }     //!< Direct spectrum getter
    void addChannelMarker(ChannelMarker* channelMarker); //!< Add channel marker to spectrum
    void addRollupWidget(QWidget *widget);               //!< Add rollup widget to channel window

    int getNumberOfRxChannels() const { return m_rxChannelInstanceRegistrations.size(); }
    int getNumberOfTxChannels() const { return m_txChannelInstanceRegistrations.size(); }
    void registerRxChannelInstance(const QString& channelName, PluginInstanceGUI* pluginGUI);
    void registerTxChannelInstance(const QString& channelName, PluginInstanceGUI* pluginGUI);
    void removeRxChannelInstance(PluginInstanceGUI* pluginGUI);
    void removeTxChannelInstance(PluginInstanceGUI* pluginGUI);
    void freeRxChannels();
    void freeTxChannels();
    void deleteRxChannel(int channelIndex);
    void deleteTxChannel(int channelIndex);
    void loadRxChannelSettings(const Preset* preset, PluginAPI *pluginAPI);
    void saveRxChannelSettings(Preset* preset);
    void loadTxChannelSettings(const Preset* preset, PluginAPI *pluginAPI);
    void saveTxChannelSettings(Preset* preset);

    // These are the number of channel types available for selection
    void setNumberOfAvailableRxChannels(int number) { m_nbAvailableRxChannels = number; }
    void setNumberOfAvailableTxChannels(int number) { m_nbAvailableTxChannels = number; }
    void setNumberOfAvailableMIMOChannels(int number) { m_nbAvailableMIMOChannels = number; }
    int getNumberOfAvailableRxChannels() const { return m_nbAvailableRxChannels; }
    int getNumberOfAvailableTxChannels() const { return m_nbAvailableTxChannels; }
    int getNumberOfAvailableMIMOChannels() const { return m_nbAvailableMIMOChannels; }

private:
    struct ChannelInstanceRegistration
    {
        QString m_channelName;
        PluginInstanceGUI* m_gui;

        ChannelInstanceRegistration() :
            m_channelName(),
            m_gui(nullptr)
        { }

        ChannelInstanceRegistration(const QString& channelName, PluginInstanceGUI* pluginGUI) :
            m_channelName(channelName),
            m_gui(pluginGUI)
        { }

        bool operator<(const ChannelInstanceRegistration& other) const;
    };

    typedef QList<ChannelInstanceRegistration> ChannelInstanceRegistrations;

    ChannelInstanceRegistrations m_rxChannelInstanceRegistrations;
    ChannelInstanceRegistrations m_txChannelInstanceRegistrations;
    int m_deviceTabIndex;
    int m_nbAvailableRxChannels;   //!< Number of Rx channels available for selection
    int m_nbAvailableTxChannels;   //!< Number of Tx channels available for selection
    int m_nbAvailableMIMOChannels; //!< Number of MIMO channels available for selection

    void renameRxChannelInstances();
    void renameTxChannelInstances();
    /** Use this function to support possible older identifiers in presets */
    bool compareRxChannelURIs(const QString& registerdChannelURI, const QString& xChannelURI);
};




#endif /* SDRGUI_DEVICE_DEVICEUISET_H_ */
