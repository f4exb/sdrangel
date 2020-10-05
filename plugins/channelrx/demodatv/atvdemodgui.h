///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4HKW                                                      //
// for F4EXB / SDRAngel                                                          //
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

#ifndef INCLUDE_ATVDEMODGUI_H
#define INCLUDE_ATVDEMODGUI_H

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "util/movingaverage.h"
#include "util/messagequeue.h"

#include "atvdemodsettings.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSink;
class ATVDemod;
class ScopeVis;

namespace Ui
{
	class ATVDemodGUI;
}

class ATVDemodGUI : public ChannelGUI
{
	Q_OBJECT

public:
    static ATVDemodGUI* create(PluginAPI* objPluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
	virtual void destroy();

	void resetToDefaults();
	QByteArray serialize() const;
    bool deserialize(const QByteArray& arrData);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

public slots:
	void channelMarkerChangedByCursor();
    void channelMarkerHighlightedByCursor();

private:
	Ui::ATVDemodGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    ATVDemod* m_atvDemod;
    ATVDemodSettings m_settings;

    bool m_doApplySettings;

    MovingAverageUtil<double, double, 4> m_objMagSqAverage;
    int m_intTickCount;

    ScopeVis* m_scopeVis;

    int m_rfSliderDivisor;
    int m_basebandSampleRate;
    MessageQueue m_inputMessageQueue;

    explicit ATVDemodGUI(PluginAPI* objPluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* objParent = 0);
	virtual ~ATVDemodGUI();

	void applySettings(bool force = false);
    void displaySettings();
    void displayStreamIndex();
    void displayRFBandwidths();
    void applySampleRate();
    void setChannelMarkerBandwidth();
    void setRFFiltersSlidersRange(int sampleRate);
    void lineTimeUpdate();
    void topTimeUpdate();
    bool handleMessage(const Message& objMessage);

	void leaveEvent(QEvent*);
	void enterEvent(QEvent*);

private slots:
    void handleSourceMessages();
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void tick();
    void on_synchLevel_valueChanged(int value);
    void on_blackLevel_valueChanged(int value);
    void on_hSync_clicked();
    void on_vSync_clicked();
    void on_invertVideo_clicked();
    void on_halfImage_clicked();
    void on_modulation_currentIndexChanged(int index);
    void on_nbLines_currentIndexChanged(int index);
    void on_fps_currentIndexChanged(int index);
    void on_standard_currentIndexChanged(int index);
    void on_reset_clicked(bool checked);
    void on_rfBW_valueChanged(int value);
    void on_rfOppBW_valueChanged(int value);
    void on_rfFiltering_toggled(bool checked);
    void on_deltaFrequency_changed(qint64 value);
    void on_bfo_valueChanged(int value);
    void on_fmDeviation_valueChanged(int value);
    void on_amScaleFactor_valueChanged(int value);
    void on_amScaleOffset_valueChanged(int value);
    void on_screenTabWidget_currentChanged(int index);
};

#endif // INCLUDE_ATVDEMODGUI_H
