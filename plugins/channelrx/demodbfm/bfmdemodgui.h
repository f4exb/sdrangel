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

#ifndef INCLUDE_BFMDEMODGUI_H
#define INCLUDE_BFMDEMODGUI_H

#include "gui/rollupwidget.h"
#include "plugin/plugingui.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"

#include "../../channelrx/demodbfm/rdsparser.h"

class PluginAPI;
class DeviceAPI;

class ThreadedSampleSink;
class DownChannelizer;
class SpectrumVis;
class BFMDemod;

namespace Ui {
	class BFMDemodGUI;
}

class BFMDemodGUI : public RollupWidget, public PluginGUI {
	Q_OBJECT

public:
	static BFMDemodGUI* create(PluginAPI* pluginAPI, DeviceAPI *deviceAPI);
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
	void on_rfBW_valueChanged(int value);
	void on_afBW_valueChanged(int value);
	void on_volume_valueChanged(int value);
	void on_squelch_valueChanged(int value);
	void on_audioStereo_toggled(bool stereo);
	void on_lsbStereo_toggled(bool lsb);
	void on_showPilot_clicked();
	void on_rds_clicked();
	void on_g14ProgServiceNames_currentIndexChanged(int index);
	void on_clearData_clicked(bool checked);
	void on_g00AltFrequenciesBox_activated(int index);
	void on_g14MappedFrequencies_activated(int index);
	void on_g14AltFrequencies_activated(int index);
	void onWidgetRolled(QWidget* widget, bool rollDown);
	void onMenuDoubleClicked();
	void tick();

private:
	Ui::BFMDemodGUI* ui;
	PluginAPI* m_pluginAPI;
	DeviceAPI* m_deviceAPI;
	ChannelMarker m_channelMarker;
	bool m_basicSettingsShown;
	bool m_doApplySettings;
	int m_rdsTimerCount;

	ThreadedSampleSink* m_threadedChannelizer;
	DownChannelizer* m_channelizer;
	SpectrumVis* m_spectrumVis;
	RDSParser m_rdsParser;

	BFMDemod* m_bfmDemod;
	MovingAverage<Real> m_channelPowerDbAvg;
	int m_rate;
	std::vector<unsigned int> m_g14ComboIndex;

	static const int m_rfBW[];

	explicit BFMDemodGUI(PluginAPI* pluginAPI, DeviceAPI *deviceAPI, QWidget* parent = NULL);
	virtual ~BFMDemodGUI();

    void blockApplySettings(bool block);
	void applySettings();
	void rdsUpdate(bool force);
	void rdsUpdateFixedFields();

	void leaveEvent(QEvent*);
	void enterEvent(QEvent*);

	void changeFrequency(qint64 f);
};

#endif // INCLUDE_BFMDEMODGUI_H
