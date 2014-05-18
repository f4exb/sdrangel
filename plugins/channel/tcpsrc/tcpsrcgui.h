#ifndef INCLUDE_TCPSRCGUI_H
#define INCLUDE_TCPSRCGUI_H

#include <QHostAddress>
#include "gui/rollupwidget.h"
#include "plugin/plugingui.h"
#include "tcpsrc.h"

class PluginAPI;
class ChannelMarker;
class ThreadedSampleSink;
class Channelizer;
class TCPSrc;
class SpectrumVis;

namespace Ui {
	class TCPSrcGUI;
}

class TCPSrcGUI : public RollupWidget, public PluginGUI {
	Q_OBJECT

public:
	static TCPSrcGUI* create(PluginAPI* pluginAPI);
	void destroy();

	void setName(const QString& name);

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);

	bool handleMessage(Message* message);

private slots:
	void channelMarkerChanged();
	void on_sampleFormat_currentIndexChanged(int index);
	void on_sampleRate_textEdited(const QString& arg1);
	void on_rfBandwidth_textEdited(const QString& arg1);
	void on_tcpPort_textEdited(const QString& arg1);
	void on_applyBtn_clicked();
	void onWidgetRolled(QWidget* widget, bool rollDown);
	void onMenuDoubleClicked();

private:
	Ui::TCPSrcGUI* ui;
	PluginAPI* m_pluginAPI;
	ChannelMarker* m_channelMarker;

	// settings
	TCPSrc::SampleFormat m_sampleFormat;
	Real m_outputSampleRate;
	Real m_rfBandwidth;
	int m_tcpPort;
	bool m_basicSettingsShown;

	// RF path
	ThreadedSampleSink* m_threadedSampleSink;
	Channelizer* m_channelizer;
	TCPSrc* m_tcpSrc;
	SpectrumVis* m_spectrumVis;

	explicit TCPSrcGUI(PluginAPI* pluginAPI, QWidget* parent = NULL);
	~TCPSrcGUI();

	void applySettings();

	void addConnection(quint32 id, const QHostAddress& peerAddress, int peerPort);
	void delConnection(quint32 id);
};

#endif // INCLUDE_TCPSRCGUI_H
