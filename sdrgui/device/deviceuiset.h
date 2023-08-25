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

#include <QObject>
#include <QTimer>
#include <QByteArray>

#include "settings/serializableinterface.h"
#include "export.h"

class SpectrumVis;
class GLSpectrum;
class GLSpectrumGUI;
class MainSpectrumGUI;
// class ChannelWindow;
class DeviceAPI;
class DeviceSet;
class DSPDeviceSourceEngine;
class DSPDeviceSinkEngine;
class DSPDeviceMIMOEngine;
class ChannelMarker;
class PluginAPI;
class DeviceGUI;
class ChannelAPI;
class ChannelGUI;
class Preset;
class Workspace;

namespace SWGSDRangel {
    class SWGGLSpectrum;
    class SWGSpectrumServer;
    class SWGSuccessResponse;
};

class SDRGUI_API DeviceUISet : public QObject, public SerializableInterface
{
    Q_OBJECT
public:
    SpectrumVis *m_spectrumVis;
    GLSpectrum *m_spectrum;
    GLSpectrumGUI *m_spectrumGUI;
    MainSpectrumGUI *m_mainSpectrumGUI;
    // ChannelWindow *m_channelWindow;
    DeviceAPI *m_deviceAPI;
    DeviceGUI *m_deviceGUI;
    DSPDeviceSourceEngine *m_deviceSourceEngine;
    DSPDeviceSinkEngine *m_deviceSinkEngine;
    DSPDeviceMIMOEngine *m_deviceMIMOEngine;
    QByteArray m_mainWindowState;
    QString m_selectedDeviceId;
    QString m_selectedDeviceSerial;
    int m_selectedDeviceSequence;
    int m_selectedDeviceItemImdex;

    DeviceUISet(int deviceSetIndex, DeviceSet *deviceSet);
    ~DeviceUISet();

    void setIndex(int deviceSetIndex);
    int getIndex() const { return m_deviceSetIndex; }
    GLSpectrum *getSpectrum() { return m_spectrum; }        //!< Direct spectrum getter
    void setSpectrumScalingFactor(float scalef);
    void addChannelMarker(ChannelMarker* channelMarker);    //!< Add channel marker to spectrum
    void removeChannelMarker(ChannelMarker* channelMarker); //!< Remove channel marker from spectrum

    int getNumberOfChannels() const { return m_channelInstanceRegistrations.size(); }
    void freeChannels();
    void deleteChannel(int channelIndex);
    ChannelAPI *getChannelAt(int channelIndex);
    ChannelGUI *getChannelGUIAt(int channelIndex);

    QByteArray serialize() const override;
    bool deserialize(const QByteArray& data) override;

    void loadDeviceSetSettings(
        const Preset* preset,
        PluginAPI *pluginAPI,
        QList<Workspace*> *workspaces,
        Workspace *currentWorkspace
    );
    void saveDeviceSetSettings(Preset* preset) const;

    void registerRxChannelInstance(ChannelAPI *channelAPI, ChannelGUI* channelGUI);
    void registerTxChannelInstance(ChannelAPI *channelAPI, ChannelGUI* channelGUI);
    void registerChannelInstance(ChannelAPI *channelAPI, ChannelGUI* channelGUI);
    void unregisterChannelInstanceAt(int channelIndex);

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
        ChannelAPI *m_channelAPI;
        ChannelGUI* m_gui;
        int m_channelType;

        ChannelInstanceRegistration() :
            m_channelAPI(nullptr),
            m_gui(nullptr),
            m_channelType(0)
        { }

        ChannelInstanceRegistration(ChannelAPI *channelAPI, ChannelGUI* channelGUI, int channelType) :
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
    int m_deviceSetIndex;
    DeviceSet *m_deviceSet;
    int m_nbAvailableRxChannels;   //!< Number of Rx channels available for selection
    int m_nbAvailableTxChannels;   //!< Number of Tx channels available for selection
    int m_nbAvailableMIMOChannels; //!< Number of MIMO channels available for selection

    void loadRxChannelSettings(const Preset* preset, PluginAPI *pluginAPI, QList<Workspace*> *workspaces, Workspace *currentWorkspace);
    void loadTxChannelSettings(const Preset* preset, PluginAPI *pluginAPI, QList<Workspace*> *workspaces, Workspace *currentWorkspace);
    void loadMIMOChannelSettings(const Preset* preset, PluginAPI *pluginAPI, QList<Workspace*> *workspaces, Workspace *currentWorkspace);
    void saveRxChannelSettings(Preset* preset) const;
    void saveTxChannelSettings(Preset* preset) const;
    void saveMIMOChannelSettings(Preset* preset) const;

private slots:
    void handleChannelGUIClosing(ChannelGUI* channelGUI);
    void handleDeleteChannel(ChannelAPI *channelAPI);
};




#endif /* SDRGUI_DEVICE_DEVICEUISET_H_ */
