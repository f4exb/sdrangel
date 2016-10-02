///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef INCLUDE_UDPSRCGUI_H
#define INCLUDE_UDPSRCGUI_H

#include <QHostAddress>
#include "gui/rollupwidget.h"
#include "plugin/plugingui.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"

#include "../../channelrx/udpsrc/udpsrc.h"

class PluginAPI;
class DeviceAPI;
class ThreadedSampleSink;
class DownChannelizer;
class UDPSrc;
class SpectrumVis;

namespace Ui {
	class UDPSrcGUI;
}

class UDPSrcGUI : public RollupWidget, public PluginGUI {
	Q_OBJECT

public:
	static UDPSrcGUI* create(PluginAPI* pluginAPI, DeviceAPI *deviceAPI);
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
	void channelMarkerChanged();
	void on_deltaFrequency_changed(quint64 value);
	void on_deltaMinus_toggled(bool minus);
	void on_sampleFormat_currentIndexChanged(int index);
	void on_sampleRate_textEdited(const QString& arg1);
	void on_rfBandwidth_textEdited(const QString& arg1);
	void on_udpAddress_textEdited(const QString& arg1);
	void on_udpPort_textEdited(const QString& arg1);
	void on_audioPort_textEdited(const QString& arg1);
	void on_fmDeviation_textEdited(const QString& arg1);
	void on_audioActive_toggled(bool active);
	void on_audioStereo_toggled(bool stereo);
	void on_applyBtn_clicked();
	void onWidgetRolled(QWidget* widget, bool rollDown);
	void onMenuDoubleClicked();
	void on_boost_valueChanged(int value);
	void on_volume_valueChanged(int value);
	void tick();

private:
	Ui::UDPSrcGUI* ui;
	PluginAPI* m_pluginAPI;
	DeviceAPI* m_deviceAPI;
	UDPSrc* m_udpSrc;
	ChannelMarker m_channelMarker;
	MovingAverage<Real> m_channelPowerDbAvg;

	// settings
	UDPSrc::SampleFormat m_sampleFormat;
	Real m_outputSampleRate;
	Real m_rfBandwidth;
	int m_fmDeviation;
	int m_boost;
	bool m_audioActive;
	bool m_audioStereo;
	int m_volume;
	QString m_udpAddress;
	int m_udpPort;
	int m_audioPort;
	bool m_basicSettingsShown;
	bool m_doApplySettings;

	// RF path
	ThreadedSampleSink* m_threadedChannelizer;
	DownChannelizer* m_channelizer;
	SpectrumVis* m_spectrumVis;

	explicit UDPSrcGUI(PluginAPI* pluginAPI, DeviceAPI *deviceAPI, QWidget* parent = 0);
	virtual ~UDPSrcGUI();

    void blockApplySettings(bool block);
	void applySettings();
	void applySettingsImmediate();

	void leaveEvent(QEvent*);
	void enterEvent(QEvent*);
};

#endif // INCLUDE_UDPSRCGUI_H
