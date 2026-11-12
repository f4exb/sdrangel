///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#ifndef PLUGINS_CHANNELTX_MODDSE_DSEMODGUI_H_
#define PLUGINS_CHANNELTX_MODDSE_DSEMODGUI_H_

#include <QHash>

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "util/movingaverage.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"

#include "dsemod.h"
#include "dsemodsettings.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSource;
class SpectrumVis;

namespace Ui {
    class DSEModGUI;
}

class DSEModGUI : public ChannelGUI {
    Q_OBJECT

public:
    static DSEModGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx);
    virtual void destroy();

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    virtual void setWorkspaceIndex(int index) { m_settings.m_workspaceIndex = index; };
    virtual int getWorkspaceIndex() const { return m_settings.m_workspaceIndex; };
    virtual void setGeometryBytes(const QByteArray& blob) { m_settings.m_geometryBytes = blob; };
    virtual QByteArray getGeometryBytes() const { return m_settings.m_geometryBytes; };
    virtual QString getTitle() const { return m_settings.m_title; };
    virtual QColor getTitleColor() const  { return m_settings.m_rgbColor; };
    virtual void zetHidden(bool hidden) { m_settings.m_hidden = hidden; }
    virtual bool getHidden() const { return m_settings.m_hidden; }
    virtual ChannelMarker& getChannelMarker() { return m_channelMarker; }
    virtual int getStreamIndex() const { return m_settings.m_streamIndex; }
    virtual void setStreamIndex(int streamIndex) { m_settings.m_streamIndex = streamIndex; }

public slots:
    void channelMarkerChangedByCursor();

private:
    Ui::DSEModGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    RollupState m_rollupState;
    DSEModSettings m_settings;
    //! The file transfer's rate: blocks sent (monotonic), when it began, and the rate so far
    int m_fileRateSent = 0;
    qint64 m_fileRateStart = 0;
    double m_fileRate = 0.0;
    QString m_tunnelStatus = QStringLiteral("Tunnel: off");
    quint64 m_tunnelTxBytes = 0;
    quint64 m_tunnelRxBytes = 0;
    QString m_tunnelRefusedText;
    qint64 m_deviceCenterFrequency;
    int m_basebandSampleRate;
    bool m_doApplySettings;

    SpectrumVis* m_spectrumVis;
    DSEMod* m_dseMod;
    MovingAverageUtil<double, double, 20> m_channelPowerDbAvg;

    quint32 m_recordLength;
    int m_recordSampleRate;
    qint64 m_samplesCount;
    int m_audioSampleRate;
    std::size_t m_tickCount;
    bool m_enableNavTime;
    QList<DSEModSettings::AvailableAMBEFeature> m_availableAMBEFeatures;
    AvailableChannelOrFeatureList m_availableDemodulators; //!< DSD Demodulators the Angel61 rate request can come from
    QStringList m_angel61DestinationOptions;
    //! Destination list entries that are not what is sent: "TG 9 - Local" is TG 9
    QHash<QString, QString> m_angel61DestinationCodes;
    MessageQueue m_inputMessageQueue;

    explicit DSEModGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent = 0);
    virtual ~DSEModGUI();

    bool blockApplySettings(bool block);
    void applySettings(const QStringList& settingsKeys, bool force = false);
    void updateSpectrumSource();
    int m_spectrumRate = 0; //!< Rate the spectrum display is set to
    void displaySettings();
    void displayInputSelection(bool inputChanged = false);
    void updateAMBEFeaturesList();
    void updateDemodulatorsList();
    void updateAngel61Destinations();
    void updateWithStreamData();
    void updateWithStreamTime();
    void channelMarkerUpdate();
    bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();
    void updateTunnelTrafficDisplay();
    void setInput(DSEModSettings::DSEModInputAF input, bool checked);

    void leaveEvent(QEvent*);
    void enterEvent(EnterEventType*);

private slots:
    void handleSourceMessages();
    void on_deltaFrequency_changed(qint64 value);
    void displayAPRSSymbol();
    void updateModeControls();
    //! The rate choices of the Angel61 profile selected: NB's two tiers or WB's three
    void updateAngel61Rates();
    void updateFunctionTabs();
    void on_mode_currentIndexChanged(int index);
    QWidget *modeTab(int mode);
    void on_invert_toggled(bool checked);
    void on_transmit_toggled(bool checked);
    void on_volume_valueChanged(int value);
    void on_tone_toggled(bool checked);
    void on_toneFrequency_valueChanged(int value);
    void on_mic_toggled(bool checked);
    void on_play_toggled(bool checked);
    void on_playLoop_toggled(bool checked);
    void on_morseKeyer_toggled(bool checked);
    void on_feedbackEnable_toggled(bool checked);
    void on_spectrumAudio_toggled(bool checked);
    void on_feedbackVolume_valueChanged(int value);
    void on_vocoder_currentIndexChanged(int index);
    void on_vocoderGain_valueChanged(int value);
    void on_toneDetect_toggled(bool checked);
    void on_noiseSuppress_toggled(bool checked);
    void on_dstarMyCall_editingFinished();
    void on_dstarMySuffix_editingFinished();
    void on_dstarUrCall_editingFinished();
    void on_dstarRpt1_editingFinished();
    void on_dstarRpt2_editingFinished();
    void on_dstarText_editingFinished();
    void on_dtmf_editingFinished();
    void on_dtmfSend_clicked();
    void on_dmrColorCode_valueChanged(int value);
    void on_dmrSourceId_valueChanged(int value);
    void on_dmrDestinationId_valueChanged(int value);
    void on_dmrCallType_currentIndexChanged(int index);
    void on_dmrSlot_currentIndexChanged(int index);
    void on_dmrDirect_currentIndexChanged(int index);
    void on_ysfCallsign_editingFinished();
    void on_ysfDestination_editingFinished();
    void on_ysfDownlink_editingFinished();
    void on_ysfUplink_editingFinished();
    void on_ysfMode_currentIndexChanged(int index);
    void on_nxdnRan_valueChanged(int value);
    void on_nxdnSourceId_valueChanged(int value);
    void on_nxdnDestinationId_valueChanged(int value);
    void on_nxdnCallType_currentIndexChanged(int index);
    void on_dpmrColorCode_valueChanged(int value);
    void on_dpmrOwnId_valueChanged(int value);
    void on_dpmrCalledId_valueChanged(int value);
    void on_p25Nac_valueChanged(int value);
    void on_p25TalkGroup_valueChanged(int value);
    void on_p25SourceId_valueChanged(int value);
    void on_angel61Rate_currentIndexChanged(int index);
    void on_angel61Demod_currentIndexChanged(int index);
    void on_angel61Symbol_clicked();
    void on_angel61APRSSend_clicked();
    void on_m17Source_editingFinished();
    void on_m17Destination_editingFinished();
    void on_m17CAN_valueChanged(int value);
    void on_m17Message_editingFinished();
    void on_m17Send_clicked();
    void on_angel61FileName_editingFinished();
    void on_angel61Tunnel_toggled(bool checked);
    void on_angel61TunnelPeer_editingFinished();
    void on_angel61TunnelAddress_editingFinished();
    void on_angel61TunnelRoutes_editingFinished();
    void on_angel61TunnelAllow_editingFinished();
    void on_angel61TunnelGroups_editingFinished();
    void on_angel61TunnelInterface_editingFinished();
    void on_angel61TunnelMTU_valueChanged(int value);
    void on_angel61FileBrowse_clicked();
    void on_angel61FileSend_clicked();
    void on_angel61Source_editingFinished();
    void on_angel61Destination_editingFinished();
    void on_angel61Path_editingFinished();
    void on_sendPosition_toggled(bool checked);
    void on_angel61Message_editingFinished();
    void on_angel61Send_clicked();
    void on_navTimeSlider_valueChanged(int value);
    void on_showFileDialog_clicked(bool checked);

    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);

    void audioSelect(const QPoint& p);
    void audioFeedbackSelect(const QPoint& p);
    void tick();
};

#endif /* PLUGINS_CHANNELTX_MODDSE_DSEMODGUI_H_ */
