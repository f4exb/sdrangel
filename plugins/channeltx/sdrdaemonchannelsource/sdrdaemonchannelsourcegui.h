///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// SDRdaemon source channel (Tx) GUI                                             //
//                                                                               //
// SDRdaemon is a detached SDR front end that handles the interface with a       //
// physical device and sends or receives the I/Q samples stream to or from a     //
// SDRangel instance via UDP. It is controlled via a Web REST API.               //
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

#ifndef CHANNEL_TX_SDRDAEMONCHANNELSOURCEGUI_H_
#define CHANNEL_TX_SDRDAEMONCHANNELSOURCEGUI_H_

#include "plugin/plugininstancegui.h"
#include "gui/rollupwidget.h"
#include "dsp/channelmarker.h"
#include "util/messagequeue.h"

#include "sdrdaemonchannelsourcesettings.h"

class PluginAPI;
class DeviceUISet;
class SDRDaemonChannelSource;
class BasebandSampleSource;

namespace Ui {
	class SDRDaemonChannelSourceGUI;
}

class SDRDaemonChannelSourceGUI : public RollupWidget, public PluginInstanceGUI {
	Q_OBJECT

public:
	static SDRDaemonChannelSourceGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceAPI, BasebandSampleSource *channelTx);
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

private:
	Ui::SDRDaemonChannelSourceGUI* ui;
	PluginAPI* m_pluginAPI;
	DeviceUISet* m_deviceUISet;
	ChannelMarker m_channelMarker;
	SDRDaemonChannelSourceSettings m_settings;
	bool m_doApplySettings;
	int m_tickCount;

	SDRDaemonChannelSource* m_sdrDaemonChannelSource;
	MessageQueue m_inputMessageQueue;

	explicit SDRDaemonChannelSourceGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent = 0);
	virtual ~SDRDaemonChannelSourceGUI();

    void blockApplySettings(bool block);
	void applySettings(bool force = false);
	void displaySettings();

    void leaveEvent(QEvent*);
    void enterEvent(QEvent*);	

private slots:
	void handleSourceMessages();
    void on_dataAddress_returnPressed();
    void on_dataPort_returnPressed();
    void on_dataApplyButton_clicked(bool checked);
	void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void tick();	
};

#endif // CHANNEL_TX_SDRDAEMONCHANNELSOURCEGUI_H_
