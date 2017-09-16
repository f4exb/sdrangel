#ifndef INCLUDE_AMDEMODGUI_H
#define INCLUDE_AMDEMODGUI_H

#include "gui/rollupwidget.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"
#include "plugin/plugininstanceui.h"
#include "util/messagequeue.h"

class PluginAPI;
class DeviceSourceAPI;

class ThreadedBasebandSampleSink;
class DownChannelizer;
class AMDemod;

namespace Ui {
	class AMDemodGUI;
}

class AMDemodGUI : public RollupWidget, public PluginInstanceUI {
	Q_OBJECT

public:
	static AMDemodGUI* create(PluginAPI* pluginAPI, DeviceSourceAPI *deviceAPI);
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

	static const QString m_channelID;

private slots:
	void channelMarkerChanged();
	void on_deltaFrequency_changed(qint64 value);
	void on_bandpassEnable_toggled(bool checked);
	void on_rfBW_valueChanged(int value);
	void on_volume_valueChanged(int value);
	void on_squelch_valueChanged(int value);
	void on_audioMute_toggled(bool checked);
    void on_copyAudioToUDP_toggled(bool copy);
	void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
	void tick();

private:
	Ui::AMDemodGUI* ui;
	PluginAPI* m_pluginAPI;
	DeviceSourceAPI* m_deviceAPI;
	ChannelMarker m_channelMarker;
	bool m_doApplySettings;

	ThreadedBasebandSampleSink* m_threadedChannelizer;
	DownChannelizer* m_channelizer;
	AMDemod* m_amDemod;
	bool m_squelchOpen;
	uint32_t m_tickCount;
	MessageQueue m_inputMessageQueue;

	explicit AMDemodGUI(PluginAPI* pluginAPI, DeviceSourceAPI *deviceAPI, QWidget* parent = NULL);
	virtual ~AMDemodGUI();

    void blockApplySettings(bool block);
	void applySettings(bool force = false);
	void displayUDPAddress();

	void leaveEvent(QEvent*);
	void enterEvent(QEvent*);
};

#endif // INCLUDE_AMDEMODGUI_H
