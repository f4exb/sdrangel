///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_CHANNELTX_UDPSINK_UDPSOURCEGUI_H_
#define PLUGINS_CHANNELTX_UDPSINK_UDPSOURCEGUI_H_

#include <QObject>

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "util/messagequeue.h"
#include "util/movingaverage.h"

#include "udpsource.h"
#include "udpsourcesettings.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSource;
class SpectrumVis;

namespace Ui {
    class UDPSourceGUI;
}

class UDPSourceGUI : public ChannelGUI {
    Q_OBJECT

public:
    static UDPSourceGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx);
    virtual void destroy();

    virtual void resetToDefaults();
    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

public slots:
    void channelMarkerChangedByCursor();

private:
    Ui::UDPSourceGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    SpectrumVis* m_spectrumVis;
    UDPSource* m_udpSource;
    MovingAverageUtil<double, double, 4> m_channelPowerAvg;
    MovingAverageUtil<double, double, 4> m_inPowerAvg;
    uint32_t m_tickCount;
    ChannelMarker m_channelMarker;

    // settings
    UDPSourceSettings m_settings;
    bool m_rfBandwidthChanged;
    bool m_doApplySettings;
    MessageQueue m_inputMessageQueue;

    explicit UDPSourceGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent = NULL);
    virtual ~UDPSourceGUI();

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();
    void displayStreamIndex();
    void setSampleFormat(int index);
    void setSampleFormatIndex(const UDPSourceSettings::SampleFormat& sampleFormat);
    bool handleMessage(const Message& message);

    void leaveEvent(QEvent*);
    void enterEvent(QEvent*);

private slots:
    void handleSourceMessages();
    void on_deltaFrequency_changed(qint64 value);
    void on_sampleFormat_currentIndexChanged(int index);
    void on_localUDPAddress_editingFinished();
    void on_localUDPPort_editingFinished();
    void on_multicastAddress_editingFinished();
    void on_multicastJoin_toggled(bool checked);
    void on_sampleRate_textEdited(const QString& arg1);
    void on_rfBandwidth_textEdited(const QString& arg1);
    void on_fmDeviation_textEdited(const QString& arg1);
    void on_amModPercent_textEdited(const QString& arg1);
    void on_applyBtn_clicked();
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void on_gainIn_valueChanged(int value);
    void on_gainOut_valueChanged(int value);
    void on_squelch_valueChanged(int value);
    void on_squelchGate_valueChanged(int value);
    void on_channelMute_toggled(bool checked);
    void on_resetUDPReadIndex_clicked();
    void on_autoRWBalance_toggled(bool checked);
    void on_stereoInput_toggled(bool checked);
    void tick();
};

#endif /* PLUGINS_CHANNELTX_UDPSINK_UDPSOURCEGUI_H_ */
