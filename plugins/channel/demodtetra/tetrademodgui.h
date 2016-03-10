#ifndef INCLUDE_TETRADEMODGUI_H
#define INCLUDE_TETRADEMODGUI_H

#include "plugin/plugingui.h"

class QDockWidget;

class PluginAPI;
class ChannelMarker;

class ThreadedSampleSink;
class Channelizer;
class TetraDemod;
class SpectrumVis;

namespace Ui {
	class TetraDemodGUI;
}

class TetraDemodGUI : public PluginGUI {
	Q_OBJECT

public:
	static TetraDemodGUI* create(PluginAPI* pluginAPI);
	void destroy();

	void setWidgetName(const QString& name);

	void resetToDefaults();
	QByteArray serializeGeneral() const;
	bool deserializeGeneral(const QByteArray& data);
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);

	bool handleMessage(Message* message);

private slots:
	void on_test_clicked();
	void viewChanged();

private:
	explicit TetraDemodGUI(PluginAPI* pluginAPI, QDockWidget* dockWidget, QWidget* parent = NULL);
	~TetraDemodGUI();

	Ui::TetraDemodGUI* ui;
	PluginAPI* m_pluginAPI;
	QDockWidget* m_dockWidget;
	ChannelMarker* m_channelMarker;

	ThreadedSampleSink* m_threadedSampleSink;
	Channelizer* m_channelizer;
	TetraDemod* m_tetraDemod;
	SpectrumVis* m_spectrumVis;
};

#endif // INCLUDE_TETRADEMODGUI_H
