#ifndef INCLUDE_WFMDEMODGUI_H
#define INCLUDE_WFMDEMODGUI_H

#include <plugin/plugininstancegui.h>
#include "gui/rollupwidget.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"
#include "util/messagequeue.h"

#include "wfmdemodsettings.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSink;
class WFMDemod;

namespace Ui {
	class WFMDemodGUI;
}

class WFMDemodGUI : public RollupWidget, public PluginInstanceGUI {
	Q_OBJECT

public:
	static WFMDemodGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
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
	Ui::WFMDemodGUI* ui;
	PluginAPI* m_pluginAPI;
	DeviceUISet* m_deviceUISet;
	ChannelMarker m_channelMarker;
	WFMDemodSettings m_settings;
	bool m_basicSettingsShown;
	bool m_doApplySettings;
    bool m_audioMute;
    bool m_squelchOpen;

	WFMDemod* m_wfmDemod;
	MovingAverage<double> m_channelPowerDbAvg;
	MessageQueue m_inputMessageQueue;

	explicit WFMDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = 0);
	virtual ~WFMDemodGUI();

    void blockApplySettings(bool block);
	void applySettings(bool force = false);
	void displaySettings();
    void displayUDPAddress();

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

private slots:
	void on_deltaFrequency_changed(qint64 value);
	void on_rfBW_currentIndexChanged(int index);
	void on_afBW_valueChanged(int value);
	void on_volume_valueChanged(int value);
	void on_squelch_valueChanged(int value);
    void on_audioMute_toggled(bool checked);
    void on_copyAudioToUDP_toggled(bool copy);
	void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
	void tick();
};

#endif // INCLUDE_WFMDEMODGUI_H
