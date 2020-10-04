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
class DeviceAPI;
class DSPDeviceSourceEngine;
class DSPDeviceSinkEngine;
class DSPDeviceMIMOEngine;
class ChannelMarker;
class PluginAPI;
class PluginInstanceGUI;
class ChannelAPI;
class ChannelGUI;
class Preset;

namespace SWGSDRangel {
    class SWGGLSpectrum;
    class SWGSpectrumServer;
    class SWGSuccessResponse;
};

class SDRGUI_API DeviceUISet
{
public:
    SpectrumVis *m_spectrumVis;
    GLSpectrum *m_spectrum;
    GLSpectrumGUI *m_spectrumGUI;
    ChannelWindow *m_channelWindow;
    DeviceAPI *m_deviceAPI;
    PluginInstanceGUI *m_deviceGUI;
    DSPDeviceSourceEngine *m_deviceSourceEngine;
    DSPDeviceSinkEngine *m_deviceSinkEngine;
    DSPDeviceMIMOEngine *m_deviceMIMOEngine;
    QByteArray m_mainWindowState;

    DeviceUISet(int tabIndex, int deviceType, QTimer& timer);
    ~DeviceUISet();

    GLSpectrum *getSpectrum() { return m_spectrum; }     //!< Direct spectrum getter
    void setSpectrumScalingFactor(float scalef);
    void addChannelMarker(ChannelMarker* channelMarker); //!< Add channel marker to spectrum
    void addRollupWidget(QWidget *widget);               //!< Add rollup widget to channel window

    int getNumberOfChannels() const { return m_channelInstanceRegistrations.size(); }
    void freeChannels();
    void deleteChannel(int channelIndex);
    void loadRxChannelSettings(const Preset* preset, PluginAPI *pluginAPI);
    void saveRxChannelSettings(Preset* preset);
    void loadTxChannelSettings(const Preset* preset, PluginAPI *pluginAPI);
    void saveTxChannelSettings(Preset* preset);
    void loadMIMOChannelSettings(const Preset* preset, PluginAPI *pluginAPI);
    void saveMIMOChannelSettings(Preset* preset);
    void registerRxChannelInstance(const QString& channelName, ChannelAPI *channelAPI, ChannelGUI* channelGUI);
    void registerTxChannelInstance(const QString& channelName, ChannelAPI *channelAPI, ChannelGUI* channelGUI);
    void registerChannelInstance(const QString& channelName, ChannelAPI *channelAPI, ChannelGUI* channelGUI);
    void removeRxChannelInstance(ChannelGUI* channelGUI);
    void removeTxChannelInstance(ChannelGUI* channelGUI);
    void removeChannelInstance(ChannelGUI* channelGUI);

    // These are the number of channel types available for selection
    void setNumberOfAvailableRxChannels(int number) { m_nbAvailableRxChannels = number; }
    void setNumberOfAvailableTxChannels(int number) { m_nbAvailableTxChannels = number; }
    void setNumberOfAvailableMIMOChannels(int number) { m_nbAvailableMIMOChannels = number; }
    int getNumberOfAvailableRxChannels() const { return m_nbAvailableRxChannels; }
    int getNumberOfAvailableTxChannels() const { return m_nbAvailableTxChannels; }
    int getNumberOfAvailableMIMOChannels() const { return m_nbAvailableMIMOChannels; }

    // REST API
    int webapiSpectrumSettingsGet(SWGSDRangel::SWGGLSpectrum& response, QString& errorMessage) const;
    int webapiSpectrumSettingsPutPatch(
            bool force,
            const QStringList& spectrumSettingsKeys,
            SWGSDRangel::SWGGLSpectrum& response, // query + response
            QString& errorMessage);
    int webapiSpectrumServerGet(SWGSDRangel::SWGSpectrumServer& response, QString& errorMessage) const;
    int webapiSpectrumServerPost(SWGSDRangel::SWGSuccessResponse& response, QString& errorMessage);
    int webapiSpectrumServerDelete(SWGSDRangel::SWGSuccessResponse& response, QString& errorMessage);

private:
    struct ChannelInstanceRegistration
    {
        QString m_channelName;
        ChannelAPI *m_channelAPI;
        ChannelGUI* m_gui;
        int m_channelType;

        ChannelInstanceRegistration() :
            m_channelName(),
            m_channelAPI(nullptr),
            m_gui(nullptr),
            m_channelType(0)
        { }

        ChannelInstanceRegistration(const QString& channelName, ChannelAPI *channelAPI, ChannelGUI* channelGUI, int channelType) :
            m_channelName(channelName),
            m_channelAPI(channelAPI),
            m_gui(channelGUI),
            m_channelType(channelType)
        { }

        bool operator<(const ChannelInstanceRegistration& other) const;
    };

    typedef QList<ChannelInstanceRegistration> ChannelInstanceRegistrations;

    // ChannelInstanceRegistrations m_rxChannelInstanceRegistrations;
    // ChannelInstanceRegistrations m_txChannelInstanceRegistrations;
    ChannelInstanceRegistrations m_channelInstanceRegistrations;
    int m_deviceTabIndex;
    int m_nbAvailableRxChannels;   //!< Number of Rx channels available for selection
    int m_nbAvailableTxChannels;   //!< Number of Tx channels available for selection
    int m_nbAvailableMIMOChannels; //!< Number of MIMO channels available for selection
};




#endif /* SDRGUI_DEVICE_DEVICEUISET_H_ */
