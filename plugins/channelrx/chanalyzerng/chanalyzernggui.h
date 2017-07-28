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

#ifndef INCLUDE_CHANNELANALYZERNGGUI_H
#define INCLUDE_CHANNELANALYZERNGGUI_H

#include "gui/rollupwidget.h"
#include "plugin/plugingui.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"

class PluginAPI;
class DeviceSourceAPI;

class ThreadedBasebandSampleSink;
class DownChannelizer;
class ChannelAnalyzerNG;
class SpectrumScopeNGComboVis;
class SpectrumVis;
class ScopeVisNG;

namespace Ui {
	class ChannelAnalyzerNGGUI;
}

class ChannelAnalyzerNGGUI : public RollupWidget, public PluginGUI {
	Q_OBJECT

public:
	static ChannelAnalyzerNGGUI* create(PluginAPI* pluginAPI, DeviceSourceAPI *deviceAPI);
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
	void channelizerInputSampleRateChanged();
	void on_deltaFrequency_changed(qint64 value);
    void on_channelSampleRate_changed(quint64 value);
    void on_useRationalDownsampler_toggled(bool checked);
	void on_BW_valueChanged(int value);
	void on_lowCut_valueChanged(int value);
	void on_spanLog2_currentIndexChanged(int index);
	void on_ssb_toggled(bool checked);
	void onWidgetRolled(QWidget* widget, bool rollDown);
	void onMenuDoubleClicked();
	void tick();

private:
	Ui::ChannelAnalyzerNGGUI* ui;
	PluginAPI* m_pluginAPI;
	DeviceSourceAPI* m_deviceAPI;
	ChannelMarker m_channelMarker;
	bool m_basicSettingsShown;
	bool m_doApplySettings;
	int m_rate; //!< sample rate after final in-channel decimation (spanlog2)
	int m_spanLog2;
	MovingAverage<double> m_channelPowerDbAvg;

	ThreadedBasebandSampleSink* m_threadedChannelizer;
	DownChannelizer* m_channelizer;
	ChannelAnalyzerNG* m_channelAnalyzer;
	SpectrumScopeNGComboVis* m_spectrumScopeComboVis;
	SpectrumVis* m_spectrumVis;
	ScopeVisNG* m_scopeVis;

	explicit ChannelAnalyzerNGGUI(PluginAPI* pluginAPI, DeviceSourceAPI *deviceAPI, QWidget* parent = NULL);
	virtual ~ChannelAnalyzerNGGUI();

	int  getRequestedChannelSampleRate();
	int  getEffectiveLowCutoff(int lowCutoff);
	bool setNewFinalRate(int spanLog2); //!< set sample rate after final in-channel decimation
	void setFiltersUIBoundaries();

	void blockApplySettings(bool block);
	void applySettings();
	void displayBandwidth();

	void leaveEvent(QEvent*);
	void enterEvent(QEvent*);
};

#endif // INCLUDE_CHANNELANALYZERNGGUI_H
