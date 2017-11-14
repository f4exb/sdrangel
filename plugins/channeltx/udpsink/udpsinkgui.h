///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_CHANNELTX_UDPSINK_UDPSINKGUI_H_
#define PLUGINS_CHANNELTX_UDPSINK_UDPSINKGUI_H_

#include <plugin/plugininstancegui.h>
#include <QObject>

#include "gui/rollupwidget.h"
#include "dsp/channelmarker.h"
#include "util/messagequeue.h"

#include "udpsink.h"
#include "udpsinksettings.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSource;
class SpectrumVis;

namespace Ui {
    class UDPSinkGUI;
}

class UDPSinkGUI : public RollupWidget, public PluginInstanceGUI {
    Q_OBJECT

public:
    static UDPSinkGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx);
    virtual void destroy();

    void setName(const QString& name);
    QString getName() const;
    virtual qint64 getCenterFrequency() const;
    virtual void setCenterFrequency(qint64 centerFrequency);
    virtual void resetToDefaults();
    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    virtual bool handleMessage(const Message& message);

public slots:
    void channelMarkerChangedByCursor();

private:
    Ui::UDPSinkGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    SpectrumVis* m_spectrumVis;
    UDPSink* m_udpSink;
    MovingAverage<double> m_channelPowerAvg;
    MovingAverage<double> m_inPowerAvg;
    uint32_t m_tickCount;
    ChannelMarker m_channelMarker;

    // settings
    UDPSinkSettings m_settings;
    bool m_rfBandwidthChanged;
    bool m_doApplySettings;
    MessageQueue m_inputMessageQueue;

    explicit UDPSinkGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent = NULL);
    virtual ~UDPSinkGUI();

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();
    void setSampleFormat(int index);
    void setSampleFormatIndex(const UDPSinkSettings::SampleFormat& sampleFormat);

    void leaveEvent(QEvent*);
    void enterEvent(QEvent*);

private slots:
    void handleSourceMessages();
    void on_deltaFrequency_changed(qint64 value);
    void on_sampleFormat_currentIndexChanged(int index);
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

#endif /* PLUGINS_CHANNELTX_UDPSINK_UDPSINKGUI_H_ */
