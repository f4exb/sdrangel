#ifndef INCLUDE_AMDEMODGUI_H
#define INCLUDE_AMDEMODGUI_H

#include "gui/rollupwidget.h"
#include "plugin/plugingui.h"

class PluginAPI;
class ChannelMarker;

class AudioFifo;
class ThreadedSampleSink;
class Channelizer;
class AMDemod;
class NullSink;

namespace Ui {
	class AMDemodGUI;
}

class AMDemodGUI : public RollupWidget, public PluginGUI {
	Q_OBJECT

public:
	static AMDemodGUI* create(PluginAPI* pluginAPI);
	void destroy();

	void setName(const QString& name);
	QString getName() const;
	qint64 getCenterFrequency() const;

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);

	bool handleMessage(Message* message);

private slots:
	void viewChanged();
	void on_deltaFrequency_changed(quint64 value);
	void on_deltaMinus_clicked(bool minus);
	void on_rfBW_valueChanged(int value);
	void on_afBW_valueChanged(int value);
	void on_volume_valueChanged(int value);
	void on_squelch_valueChanged(int value);
	void onWidgetRolled(QWidget* widget, bool rollDown);
	void onMenuDoubleClicked();

private:
	Ui::AMDemodGUI* ui;
	PluginAPI* m_pluginAPI;
	ChannelMarker* m_channelMarker;
	bool m_basicSettingsShown;

	AudioFifo* m_audioFifo;
	ThreadedSampleSink* m_threadedSampleSink;
	Channelizer* m_channelizer;
	AMDemod* m_amDemod;
	NullSink *m_nullSink;

	static const int m_rfBW[];

	explicit AMDemodGUI(PluginAPI* pluginAPI, QWidget* parent = NULL);
	~AMDemodGUI();

	void applySettings();

	void leaveEvent(QEvent*);
	void enterEvent(QEvent*);
};

#endif // INCLUDE_AMDEMODGUI_H
