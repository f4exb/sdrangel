#ifndef INCLUDE_UDPSRCGUI_H
#define INCLUDE_UDPSRCGUI_H

#include <QHostAddress>
#include "gui/rollupwidget.h"
#include "plugin/plugingui.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"

#include "udpsrc.h"

class PluginAPI;
class ThreadedSampleSink;
class Channelizer;
class UDPSrc;
class SpectrumVis;

namespace Ui {
	class UDPSrcGUI;
}

class UDPSrcGUI : public RollupWidget, public PluginGUI {
	Q_OBJECT

public:
	static UDPSrcGUI* create(PluginAPI* pluginAPI);
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
	void on_udpAddress_textEdited(const QString& arg1);
	void on_udpPort_textEdited(const QString& arg1);
	void on_applyBtn_clicked();
	void onWidgetRolled(QWidget* widget, bool rollDown);
	void onMenuDoubleClicked();
	void on_boost_valueChanged(int value);
	void tick();

private:
	Ui::UDPSrcGUI* ui;
	PluginAPI* m_pluginAPI;
	UDPSrc* m_udpSrc;
	ChannelMarker m_channelMarker;
	MovingAverage<Real> m_channelPowerDbAvg;

	// settings
	UDPSrc::SampleFormat m_sampleFormat;
	Real m_outputSampleRate;
	Real m_rfBandwidth;
	int m_boost;
	QString m_udpAddress;
	int m_udpPort;
	bool m_basicSettingsShown;
	bool m_doApplySettings;

	// RF path
	ThreadedSampleSink* m_threadedChannelizer;
	Channelizer* m_channelizer;
	SpectrumVis* m_spectrumVis;

	explicit UDPSrcGUI(PluginAPI* pluginAPI, QWidget* parent = 0);
	virtual ~UDPSrcGUI();

    void blockApplySettings(bool block);
	void applySettings();
};

#endif // INCLUDE_UDPSRCGUI_H
