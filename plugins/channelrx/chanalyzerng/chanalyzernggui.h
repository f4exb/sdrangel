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

#include <plugin/plugininstancegui.h>
#include "gui/rollupwidget.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"
#include "util/messagequeue.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSink;
class ChannelAnalyzerNG;
class SpectrumScopeNGComboVis;
class SpectrumVis;
class ScopeVisNG;

namespace Ui {
	class ChannelAnalyzerNGGUI;
}

class ChannelAnalyzerNGGUI : public RollupWidget, public PluginInstanceGUI {
	Q_OBJECT

public:
	static ChannelAnalyzerNGGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
	virtual void destroy();

	void setName(const QString& name);
	QString getName() const;
	virtual qint64 getCenterFrequency() const;
	virtual void setCenterFrequency(qint64 centerFrequency);

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
	virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
	virtual bool handleMessage(const Message& message);

public slots:
	void channelMarkerChangedByCursor();
	void channelMarkerHighlightedByCursor();

private:
	Ui::ChannelAnalyzerNGGUI* ui;
	PluginAPI* m_pluginAPI;
	DeviceUISet* m_deviceUISet;
	ChannelMarker m_channelMarker;
	bool m_doApplySettings;
	int m_rate; //!< sample rate after final in-channel decimation (spanlog2)
	int m_spanLog2;
	MovingAverage<double> m_channelPowerDbAvg;

	ChannelAnalyzerNG* m_channelAnalyzer;
	SpectrumScopeNGComboVis* m_spectrumScopeComboVis;
	SpectrumVis* m_spectrumVis;
	ScopeVisNG* m_scopeVis;
	MessageQueue m_inputMessageQueue;

	explicit ChannelAnalyzerNGGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = 0);
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

private slots:
	void on_deltaFrequency_changed(qint64 value);
    void on_channelSampleRate_changed(quint64 value);
    void on_useRationalDownsampler_toggled(bool checked);
	void on_BW_valueChanged(int value);
	void on_lowCut_valueChanged(int value);
	void on_spanLog2_currentIndexChanged(int index);
	void on_ssb_toggled(bool checked);
	void onWidgetRolled(QWidget* widget, bool rollDown);
    void handleInputMessages();
	void tick();
};

#endif // INCLUDE_CHANNELANALYZERNGGUI_H
