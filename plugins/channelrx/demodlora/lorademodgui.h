///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019-2020 Edouard Griffiths, F4EXB                              //
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

#ifndef INCLUDE_LoRaDEMODGUI_H
#define INCLUDE_LoRaDEMODGUI_H

#include <plugin/plugininstancegui.h>
#include "gui/rollupwidget.h"
#include "dsp/channelmarker.h"
#include "util/messagequeue.h"

#include "lorademodsettings.h"

class PluginAPI;
class DeviceUISet;
class LoRaDemod;
class SpectrumVis;
class BasebandSampleSink;

namespace Ui {
	class LoRaDemodGUI;
}

class LoRaDemodGUI : public RollupWidget, public PluginInstanceGUI {
	Q_OBJECT

public:
	static LoRaDemodGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceAPI, BasebandSampleSink *rxChannel);
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

private slots:
	void channelMarkerChangedByCursor();
    void on_deltaFrequency_changed(qint64 value);
	void on_BW_valueChanged(int value);
	void on_Spread_valueChanged(int value);
    void on_deBits_valueChanged(int value);
	void onWidgetRolled(QWidget* widget, bool rollDown);
    void channelMarkerHighlightedByCursor();
	void handleInputMessages();

private:
	Ui::LoRaDemodGUI* ui;
	PluginAPI* m_pluginAPI;
	DeviceUISet* m_deviceUISet;
	ChannelMarker m_channelMarker;
	LoRaDemodSettings m_settings;
    int m_basebandSampleRate;
	bool m_doApplySettings;

	LoRaDemod* m_LoRaDemod;
	SpectrumVis* m_spectrumVis;
	MessageQueue m_inputMessageQueue;

	explicit LoRaDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = 0);
	virtual ~LoRaDemodGUI();

    void blockApplySettings(bool block);
	void applySettings(bool force = false);
	void displaySettings();
    void setBandwidths();
};

#endif // INCLUDE_LoRaDEMODGUI_H
