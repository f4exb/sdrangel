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

#ifndef INCLUDE_CHANNELANALYZERNGGUI_H
#define INCLUDE_CHANNELANALYZERNGGUI_H

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "dsp/dsptypes.h"
#include "util/movingaverage.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"

#include "chanalyzersettings.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSink;
class ChannelAnalyzer;
class SpectrumVis;
class ScopeVis;

namespace Ui {
	class ChannelAnalyzerGUI;
}

class ChannelAnalyzerGUI : public ChannelGUI {
	Q_OBJECT

public:
	static ChannelAnalyzerGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
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
	void channelMarkerHighlightedByCursor();

private:
	Ui::ChannelAnalyzerGUI* ui;
	PluginAPI* m_pluginAPI;
	DeviceUISet* m_deviceUISet;
	ChannelMarker m_channelMarker;
	RollupState m_rollupState;
	ChannelAnalyzerSettings m_settings;
    qint64 m_deviceCenterFrequency;
	bool m_doApplySettings;
	int m_basebandSampleRate; //!< sample rate after final in-channel decimation (spanlog2)
	MovingAverageUtil<double, double, 40> m_channelPowerAvg;

	ChannelAnalyzer* m_channelAnalyzer;
	SpectrumVis* m_spectrumVis;
	ScopeVis* m_scopeVis;
	MessageQueue m_inputMessageQueue;

	explicit ChannelAnalyzerGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = nullptr);
	virtual ~ChannelAnalyzerGUI();

    int  getSinkSampleRate(); //!< get actual sink sample rate from GUI settings
	void setSinkSampleRate(); //!< set sample rate after full decimation chain
	void setFiltersUIBoundaries();

	void blockApplySettings(bool block);
	void applySettings(bool force = false);
	void displaySettings();
	void displayPLLSettings();
        void setPLLVisibility();
	void setSpectrumDisplay();
	bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();

	void leaveEvent(QEvent*);
	void enterEvent(EnterEventType*);

private slots:
	void on_deltaFrequency_changed(qint64 value);
    void on_rationalDownSamplerRate_changed(quint64 value);
    void on_pll_toggled(bool checked);
    void on_pllType_currentIndexChanged(int index);
    void on_pllPskOrder_currentIndexChanged(int index);
    void on_pllBandwidth_valueChanged(int value);
    void on_pllDampingFactor_valueChanged(int value);
    void on_pllLoopGain_valueChanged(int value);
    void on_useRationalDownsampler_toggled(bool checked);
    void on_signalSelect_currentIndexChanged(int index);
    void on_rrcFilter_toggled(bool checked);
    void on_rrcRolloff_valueChanged(int value);
	void on_BW_valueChanged(int value);
	void on_lowCut_valueChanged(int value);
	void on_log2Decim_currentIndexChanged(int index);
	void on_ssb_toggled(bool checked);
	void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void handleInputMessages();
	void tick();
};

#endif // INCLUDE_CHANNELANALYZERNGGUI_H
