#ifndef INCLUDE_WFMDEMODGUI_H
#define INCLUDE_WFMDEMODGUI_H

#include "gui/rollupwidget.h"
#include "plugin/plugingui.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"

class PluginAPI;
class DeviceAPI;

class ThreadedSampleSink;
class Channelizer;
class WFMDemod;

namespace Ui {
	class WFMDemodGUI;
}

class WFMDemodGUI : public RollupWidget, public PluginGUI {
	Q_OBJECT

public:
	static WFMDemodGUI* create(PluginAPI* pluginAPI, DeviceAPI *deviceAPI);
	void destroy();

	void setName(const QString& name);
	QString getName() const;
	virtual qint64 getCenterFrequency() const;
	virtual void setCenterFrequency(qint64 centerFrequency);

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);

	virtual bool handleMessage(const Message& message);

private slots:
	void viewChanged();
	void on_deltaFrequency_changed(quint64 value);
	void on_deltaMinus_toggled(bool minus);
	void on_rfBW_valueChanged(int value);
	void on_afBW_valueChanged(int value);
	void on_volume_valueChanged(int value);
	void on_squelch_valueChanged(int value);
	void onWidgetRolled(QWidget* widget, bool rollDown);
	void onMenuDoubleClicked();
	void tick();

private:
	Ui::WFMDemodGUI* ui;
	PluginAPI* m_pluginAPI;
	DeviceAPI* m_deviceAPI;
	ChannelMarker m_channelMarker;
	bool m_basicSettingsShown;
	bool m_doApplySettings;

	ThreadedSampleSink* m_threadedChannelizer;
	Channelizer* m_channelizer;
	WFMDemod* m_wfmDemod;
	MovingAverage<Real> m_channelPowerDbAvg;

	static const int m_rfBW[];

	explicit WFMDemodGUI(PluginAPI* pluginAPI, DeviceAPI *deviceAPI, QWidget* parent = NULL);
	virtual ~WFMDemodGUI();

    void blockApplySettings(bool block);
	void applySettings();

	void leaveEvent(QEvent*);
	void enterEvent(QEvent*);
};

#endif // INCLUDE_WFMDEMODGUI_H
