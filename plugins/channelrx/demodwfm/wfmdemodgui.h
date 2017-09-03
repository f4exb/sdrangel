#ifndef INCLUDE_WFMDEMODGUI_H
#define INCLUDE_WFMDEMODGUI_H

#include <plugin/plugininstanceui.h>
#include "gui/rollupwidget.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"

class PluginAPI;
class DeviceSourceAPI;

class ThreadedBasebandSampleSink;
class DownChannelizer;
class WFMDemod;

namespace Ui {
	class WFMDemodGUI;
}

class WFMDemodGUI : public RollupWidget, public PluginInstanceUI {
	Q_OBJECT

public:
	static WFMDemodGUI* create(PluginAPI* pluginAPI, DeviceSourceAPI *deviceAPI);
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
	void on_deltaFrequency_changed(qint64 value);
	void on_rfBW_currentIndexChanged(int index);
	void on_afBW_valueChanged(int value);
	void on_volume_valueChanged(int value);
	void on_squelch_valueChanged(int value);
    void on_audioMute_toggled(bool checked);
	void onWidgetRolled(QWidget* widget, bool rollDown);
	void onMenuDoubleClicked();
	void tick();

private:
	Ui::WFMDemodGUI* ui;
	PluginAPI* m_pluginAPI;
	DeviceSourceAPI* m_deviceAPI;
	ChannelMarker m_channelMarker;
	bool m_basicSettingsShown;
	bool m_doApplySettings;
    bool m_audioMute;
    bool m_squelchOpen;

	ThreadedBasebandSampleSink* m_threadedChannelizer;
	DownChannelizer* m_channelizer;
	WFMDemod* m_wfmDemod;
	MovingAverage<double> m_channelPowerDbAvg;

	static const int m_rfBW[];
    static const int m_nbRfBW;

	explicit WFMDemodGUI(PluginAPI* pluginAPI, DeviceSourceAPI *deviceAPI, QWidget* parent = NULL);
	virtual ~WFMDemodGUI();

    void blockApplySettings(bool block);
	void applySettings();

	void leaveEvent(QEvent*);
	void enterEvent(QEvent*);

	static int requiredBW(int rfBW)
	{
	    if (rfBW <= 48000) {
	        return 48000;
	    } else {
	        return (3*rfBW)/2;
	    }
	}
};

#endif // INCLUDE_WFMDEMODGUI_H
