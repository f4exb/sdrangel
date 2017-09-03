///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef PLUGINS_CHANNELTX_MODNFM_NFMMODGUI_H_
#define PLUGINS_CHANNELTX_MODNFM_NFMMODGUI_H_

#include <plugin/plugininstanceui.h>
#include "gui/rollupwidget.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"

#include "nfmmod.h"

class PluginAPI;
class DeviceSinkAPI;

class ThreadedBasebandSampleSource;
class UpChannelizer;
class NFMMod;

namespace Ui {
    class NFMModGUI;
}

class NFMModGUI : public RollupWidget, public PluginInstanceUI {
    Q_OBJECT

public:
    static NFMModGUI* create(PluginAPI* pluginAPI, DeviceSinkAPI *deviceAPI);
    void destroy();

    void setName(const QString& name);
    QString getName() const;
    virtual qint64 getCenterFrequency() const;
    virtual void setCenterFrequency(qint64 centerFrequency);

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);

    virtual bool handleMessage(const Message& message);

    static const QString m_channelID;

private slots:
    void viewChanged();
    void handleSourceMessages();

    void on_deltaFrequency_changed(qint64 value);
    void on_rfBW_currentIndexChanged(int index);
    void on_afBW_valueChanged(int value);
    void on_fmDev_valueChanged(int value);
    void on_toneFrequency_valueChanged(int value);
    void on_volume_valueChanged(int value);
    void on_channelMute_toggled(bool checked);
    void on_tone_toggled(bool checked);
    void on_morseKeyer_toggled(bool checked);
    void on_mic_toggled(bool checked);
    void on_play_toggled(bool checked);

    void on_playLoop_toggled(bool checked);
    void on_navTimeSlider_valueChanged(int value);
    void on_showFileDialog_clicked(bool checked);

    void on_ctcss_currentIndexChanged(int index);
    void on_ctcssOn_toggled(bool checked);

    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDoubleClicked();

    void configureFileName();
    void tick();

private:
    Ui::NFMModGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceSinkAPI* m_deviceAPI;
    ChannelMarker m_channelMarker;
    bool m_basicSettingsShown;
    bool m_doApplySettings;

    ThreadedBasebandSampleSource* m_threadedChannelizer;
    UpChannelizer* m_channelizer;
    NFMMod* m_nfmMod;
    MovingAverage<double> m_channelPowerDbAvg;

    QString m_fileName;
    quint32 m_recordLength;
    int m_recordSampleRate;
    int m_samplesCount;
    std::size_t m_tickCount;
    bool m_enableNavTime;
    NFMMod::NFMModInputAF m_modAFInput;

    static const int m_rfBW[];
    static const int m_nbRfBW;

    static const float m_ctcssTones[];
    static const int m_nbCTCSSTones;

    explicit NFMModGUI(PluginAPI* pluginAPI, DeviceSinkAPI *deviceAPI, QWidget* parent = NULL);
    virtual ~NFMModGUI();

    void blockApplySettings(bool block);
    void applySettings();
    void updateWithStreamData();
    void updateWithStreamTime();

    void leaveEvent(QEvent*);
    void enterEvent(QEvent*);
};

#endif /* PLUGINS_CHANNELTX_MODNFM_NFMMODGUI_H_ */
