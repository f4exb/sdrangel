#ifndef INCLUDE_USBDEMODGUI_H
#define INCLUDE_USBDEMODGUI_H

#include "gui/rollupwidget.h"
#include "plugin/plugingui.h"

class PluginAPI;
class ChannelMarker;

class AudioFifo;
class ThreadedSampleSink;
class Channelizer;
class USBDemod;
class SpectrumVis;

namespace Ui {
	class USBDemodGUI;
}

class USBDemodGUI : public RollupWidget, public PluginGUI {
	Q_OBJECT

public:
	static USBDemodGUI* create(PluginAPI* pluginAPI);
	void destroy();

	void setName(const QString& name);

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);

	bool handleMessage(Message* message);

private slots:
	void viewChanged();
	void on_BW_valueChanged(int value);
	void on_volume_valueChanged(int value);
	void onWidgetRolled(QWidget* widget, bool rollDown);
	void onMenuDoubleClicked();

private:
	Ui::USBDemodGUI* ui;
	PluginAPI* m_pluginAPI;
	ChannelMarker* m_channelMarker;
	bool m_basicSettingsShown;

	AudioFifo* m_audioFifo;
	ThreadedSampleSink* m_threadedSampleSink;
	Channelizer* m_channelizer;
	USBDemod* m_usbDemod;
	SpectrumVis* m_spectrumVis;

	explicit USBDemodGUI(PluginAPI* pluginAPI, QWidget* parent = NULL);
	~USBDemodGUI();

	void applySettings();
};

#endif // INCLUDE_USBDEMODGUI_H
