#ifndef INCLUDE_AMDEMODGUI_H
#define INCLUDE_AMDEMODGUI_H

#include <QIcon>

#include "plugin/plugininstancegui.h"
#include "gui/rollupwidget.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"
#include "util/messagequeue.h"
#include "amdemodsettings.h"

class PluginAPI;
class DeviceUISet;

class AMDemod;
class BasebandSampleSink;

namespace Ui {
	class AMDemodGUI;
}

class AMDemodGUI : public RollupWidget, public PluginInstanceGUI {
	Q_OBJECT

public:
	static AMDemodGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
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
	Ui::AMDemodGUI* ui;
	PluginAPI* m_pluginAPI;
	DeviceUISet* m_deviceUISet;
	ChannelMarker m_channelMarker;
	AMDemodSettings m_settings;
	bool m_doApplySettings;

	AMDemod* m_amDemod;
	bool m_squelchOpen;
	bool m_samUSB;
	uint32_t m_tickCount;
	MessageQueue m_inputMessageQueue;

    QIcon m_iconDSBUSB;
    QIcon m_iconDSBLSB;

	explicit AMDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = 0);
	virtual ~AMDemodGUI();

    void blockApplySettings(bool block);
	void applySettings(bool force = false);
	void displaySettings();

	void leaveEvent(QEvent*);
	void enterEvent(QEvent*);

private slots:
	void on_deltaFrequency_changed(qint64 value);
	void on_pll_toggled(bool checked);
	void on_ssb_toggled(bool checked);
	void on_bandpassEnable_toggled(bool checked);
	void on_rfBW_valueChanged(int value);
	void on_volume_valueChanged(int value);
	void on_squelch_valueChanged(int value);
	void on_audioMute_toggled(bool checked);
	void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void handleInputMessages();
    void audioSelect();
    void samSSBSelect();
	void tick();
};

#endif // INCLUDE_AMDEMODGUI_H
