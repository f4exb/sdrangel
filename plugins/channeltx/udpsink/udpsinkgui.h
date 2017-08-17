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

#include <QObject>

#include "gui/rollupwidget.h"
#include "plugin/plugingui.h"
#include "dsp/channelmarker.h"

#include "udpsink.h"

class PluginAPI;
class DeviceSinkAPI;
class ThreadedBasebandSampleSource;
class UpChannelizer;
class UDPSink;
class SpectrumVis;

namespace Ui {
    class UDPSinkGUI;
}

class UDPSinkGUI : public RollupWidget, public PluginGUI {
    Q_OBJECT

public:
    static UDPSinkGUI* create(PluginAPI* pluginAPI, DeviceSinkAPI *deviceAPI);
    void destroy();

    void setName(const QString& name);
    QString getName() const;
    virtual qint64 getCenterFrequency() const;
    virtual void setCenterFrequency(qint64 centerFrequency);
    virtual void resetToDefaults();
    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);
    virtual bool handleMessage(const Message& message);

    static const QString m_channelID;

private slots:
    void handleSourceMessages();
    void channelMarkerChanged();
    void on_deltaFrequency_changed(qint64 value);
    void on_sampleFormat_currentIndexChanged(int index);
    void on_sampleRate_textEdited(const QString& arg1);
    void on_rfBandwidth_textEdited(const QString& arg1);
    void on_udpAddress_textEdited(const QString& arg1);
    void on_udpPort_textEdited(const QString& arg1);
    void on_fmDeviation_textEdited(const QString& arg1);
    void on_applyBtn_clicked();
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDoubleClicked();
    void on_volume_valueChanged(int value);
    void on_squelch_valueChanged(int value);
    void on_channelMute_toggled(bool checked);
    void tick();

private:
    Ui::UDPSinkGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceSinkAPI* m_deviceAPI;
    ThreadedBasebandSampleSource* m_threadedChannelizer;
    UpChannelizer* m_channelizer;
    SpectrumVis* m_spectrumVis;
    UDPSink* m_udpSink;
    MovingAverage<double> m_channelPowerAvg;
    MovingAverage<double> m_inPowerAvg;
    int m_powDisplayCount;
    ChannelMarker m_channelMarker;

    // settings
    UDPSink::SampleFormat m_sampleFormat;
    Real m_inputSampleRate;
    Real m_rfBandwidth;
    int m_fmDeviation;
    QString m_udpAddress;
    int m_udpPort;
    bool m_basicSettingsShown;
    bool m_doApplySettings;

    explicit UDPSinkGUI(PluginAPI* pluginAPI, DeviceSinkAPI *deviceAPI, QWidget* parent = NULL);
    virtual ~UDPSinkGUI();

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();

    void leaveEvent(QEvent*);
    void enterEvent(QEvent*);
};

#endif /* PLUGINS_CHANNELTX_UDPSINK_UDPSINKGUI_H_ */
