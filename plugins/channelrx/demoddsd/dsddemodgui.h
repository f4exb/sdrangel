///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
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

#ifndef INCLUDE_DSDDEMODGUI_H
#define INCLUDE_DSDDEMODGUI_H

#include <plugin/plugininstancegui.h>
#include <QMenu>

#include "gui/rollupwidget.h"
#include "dsp/dsptypes.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"
#include "util/messagequeue.h"

#include "dsddemodsettings.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSink;
class ScopeVis;
class DSDDemod;

namespace Ui {
	class DSDDemodGUI;
}

class DSDDemodGUI : public RollupWidget, public PluginInstanceGUI {
	Q_OBJECT

public:
	static DSDDemodGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
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
	typedef enum
	{
	    signalFormatNone,
	    signalFormatDMR,
	    signalFormatDStar,
	    signalFormatDPMR,
		signalFormatYSF
	} SignalFormat;

	Ui::DSDDemodGUI* ui;
	PluginAPI* m_pluginAPI;
	DeviceUISet* m_deviceUISet;
	ChannelMarker m_channelMarker;
	DSDDemodSettings m_settings;
	bool m_doApplySettings;
	char m_formatStatusText[82+1]; //!< Fixed signal format dependent status text
	SignalFormat m_signalFormat;

    ScopeVis* m_scopeVis;

	DSDDemod* m_dsdDemod;
	bool m_enableCosineFiltering;
	bool m_syncOrConstellation;
	bool m_slot1On;
    bool m_slot2On;
    bool m_tdmaStereo;
    bool m_audioMute;
	bool m_squelchOpen;
	uint32_t m_tickCount;

	float m_myLatitude;
	float m_myLongitude;

	MessageQueue m_inputMessageQueue;

	explicit DSDDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = 0);
	virtual ~DSDDemodGUI();

	void blockApplySettings(bool block);
	void applySettings(bool force = false);
    void displaySettings();
	void updateMyPosition();
	void displayUDPAddress();

	void leaveEvent(QEvent*);
	void enterEvent(QEvent*);

private slots:
    void formatStatusText();
    void on_deltaFrequency_changed(qint64 value);
    void on_rfBW_valueChanged(int index);
    void on_demodGain_valueChanged(int value);
    void on_volume_valueChanged(int value);
    void on_baudRate_currentIndexChanged(int index);
    void on_enableCosineFiltering_toggled(bool enable);
    void on_syncOrConstellation_toggled(bool checked);
    void on_slot1On_toggled(bool checked);
    void on_slot2On_toggled(bool checked);
    void on_tdmaStereoSplit_toggled(bool checked);
    void on_fmDeviation_valueChanged(int value);
    void on_squelchGate_valueChanged(int value);
    void on_squelch_valueChanged(int value);
    void on_audioMute_toggled(bool checked);
    void on_symbolPLLLock_toggled(bool checked);
    void on_udpOutput_toggled(bool checked);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void tick();
};

#endif // INCLUDE_DSDDEMODGUI_H
