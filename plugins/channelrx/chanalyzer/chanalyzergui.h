///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_CHANNELANALYZERGUI_H
#define INCLUDE_CHANNELANALYZERGUI_H

#include "gui/rollupwidget.h"
#include "plugin/plugingui.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"

class PluginAPI;
class DeviceAPI;

class ThreadedBasebandSampleSink;
class DownChannelizer;
class ChannelAnalyzer;
class SpectrumScopeComboVis;
class SpectrumVis;
class ScopeVis;

namespace Ui {
	class ChannelAnalyzerGUI;
}

class ChannelAnalyzerGUI : public RollupWidget, public PluginGUI {
	Q_OBJECT

public:
	static ChannelAnalyzerGUI* create(PluginAPI* pluginAPI, DeviceAPI *deviceAPI);
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
	void channelSampleRateChanged();
	void on_deltaFrequency_changed(quint64 value);
	void on_deltaMinus_toggled(bool minus);
	void on_BW_valueChanged(int value);
	void on_lowCut_valueChanged(int value);
	void on_spanLog2_valueChanged(int value);
	void on_ssb_toggled(bool checked);
	void onWidgetRolled(QWidget* widget, bool rollDown);
	void onMenuDoubleClicked();
	void tick();

private:
	Ui::ChannelAnalyzerGUI* ui;
	PluginAPI* m_pluginAPI;
	DeviceAPI* m_deviceAPI;
	ChannelMarker m_channelMarker;
	bool m_basicSettingsShown;
	bool m_doApplySettings;
	int m_rate;
	int m_spanLog2;
	MovingAverage<Real> m_channelPowerDbAvg;

	ThreadedBasebandSampleSink* m_threadedChannelizer;
	DownChannelizer* m_channelizer;
	ChannelAnalyzer* m_channelAnalyzer;
	SpectrumScopeComboVis* m_spectrumScopeComboVis;
	SpectrumVis* m_spectrumVis;
	ScopeVis* m_scopeVis;

	explicit ChannelAnalyzerGUI(PluginAPI* pluginAPI, DeviceAPI *deviceAPI, QWidget* parent = NULL);
	virtual ~ChannelAnalyzerGUI();

	int  getEffectiveLowCutoff(int lowCutoff);
	bool setNewRate(int spanLog2);

	void blockApplySettings(bool block);
	void applySettings();

	void leaveEvent(QEvent*);
	void enterEvent(QEvent*);
};

#endif // INCLUDE_CHANNELANALYZERGUI_H
