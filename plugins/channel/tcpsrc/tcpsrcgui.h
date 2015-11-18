#ifndef INCLUDE_TCPSRCGUI_H
#define INCLUDE_TCPSRCGUI_H

#include <QHostAddress>
#include "gui/rollupwidget.h"
#include "plugin/plugingui.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"
#include "tcpsrc.h"

class PluginAPI;
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
	QString getName() const;
	virtual qint64 getCenterFrequency() const;
	virtual void setCenterFrequency(qint64 centerFrequency);

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);

	virtual bool handleMessage(const Message& message);

private slots:
	void channelMarkerChanged();
	void on_deltaFrequency_changed(quint64 value);
	void on_deltaMinus_toggled(bool minus);
	void on_sampleFormat_currentIndexChanged(int index);
	void on_sampleRate_textEdited(const QString& arg1);
	void on_rfBandwidth_textEdited(const QString& arg1);
	void on_tcpPort_textEdited(const QString& arg1);
	void on_applyBtn_clicked();
	void onWidgetRolled(QWidget* widget, bool rollDown);
	void onMenuDoubleClicked();
	void on_boost_valueChanged(int value);
	void tick();

private:
	Ui::TCPSrcGUI* ui;
	PluginAPI* m_pluginAPI;
	TCPSrc* m_tcpSrc;
	ChannelMarker m_channelMarker;
	MovingAverage<Real> m_channelPowerDbAvg;

	// settings
	TCPSrc::SampleFormat m_sampleFormat;
	Real m_outputSampleRate;
	Real m_rfBandwidth;
	int m_boost;
	int m_tcpPort;
	bool m_basicSettingsShown;
	bool m_doApplySettings;

	// RF path
	ThreadedSampleSink* m_threadedChannelizer;
	Channelizer* m_channelizer;
	SpectrumVis* m_spectrumVis;

	explicit TCPSrcGUI(PluginAPI* pluginAPI, QWidget* parent = 0);
	virtual ~TCPSrcGUI();

    void blockApplySettings(bool block);
	void applySettings();

	void addConnection(quint32 id, const QHostAddress& peerAddress, int peerPort);
	void delConnection(quint32 id);
};

#endif // INCLUDE_TCPSRCGUI_H
